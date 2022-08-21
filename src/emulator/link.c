#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <stdlib.h>

#include "emulator.h"
#include "cpu.h"
#include "mmu.h"
#include "link.h"

// TODO this is still buggy: tetris works fine, dr mario has glitched game over screen (unrelated?) and pokemon red has glitched fight and trade
// TODO fix problems when a emulator is paused during a connection
// TODO handle disconnection
// TODO this may cause glitches in sound emulation due to the blocking send and recv (investigate when sound is implemented)

// TODO put socket code logic in platform specific code section and leave here only the transfer logic.
//      --> callback when data needs to be transfered, then platform handles this data and the transfer

// The master game boy MUST receive the slave data at the same time it is sending its own data.
// The slave game boy sends its data when it receives the master's data so it's not affected by the network delay.

// --> This wasn’t required for the emulator to Game Boy connection because the emulator could act as the master device and slow down or speed up
// as necessary to maintain the connection.
// ==> so make the master emulator frequency adapt it's speed based on the current network speed

// TODO Alternative way -- but likely also problematic: this is essentially the server streaming the emulation to the client
// https://retrocomputing.stackexchange.com/questions/24636/is-there-an-obstruction-to-emulating-gameboy-link-cable-over-wi-fi
// I could imagine a setup where, user A has rom RA, with save file SA and user B has rom RB with save file SB.
// A sets themself up as the server, and B as the client. The emulation session sends RB and SB to A. A emulates two gameboys under the hood,
// running RA with SA on one and RB with SB on the other. Only the the RA session is displayed to A, and then A sends the sound and current frame of
// the RB session back to B, and polls B for controller inputs. Any time the game is saved A sends SB back over to B, and A deletes their copy
// of RB and SB at the end of the session.

// TODO: I think it's the best way (it expands the alternative way above):
// https://docs.libretro.com/development/retroarch/netplay/
// If I understand it means device A emulates 2 gameboys and device B also.
// They serialize their states (using savestates): A creates SA and sends it to B, B creates SB and sends it to A
// A and B emulate both their own and the other states but only show to the user their own state. 
// A and B send their respective inputs when they are changed (way less packets sent and latency is a small issue compaired to other solutions)
// on reception of an input from the other device, they apply it to their own hidden emulation of the other device
// There is also a need to rewind to resync... learn more about this by reading the link

// TODO set TCP_NODELAY on the sockets to avoid extra delays in the OS.

#ifndef IPPROTO_MPTCP
#define IPPROTO_MPTCP 262
#endif

typedef struct {
    byte_t data;
    byte_t transfer;
} pkt_t;

static void print_connected_to(struct sockaddr *other_addr) {
    char buf[INET6_ADDRSTRLEN];
    int port;
    if (other_addr->sa_family == AF_INET) {
        inet_ntop(other_addr->sa_family, &((struct sockaddr_in *) other_addr)->sin_addr, buf, INET6_ADDRSTRLEN);
        port = ((struct sockaddr_in *) other_addr)->sin_port;
    } else {
        inet_ntop(other_addr->sa_family, &((struct sockaddr_in6 *) other_addr)->sin6_addr, buf, INET6_ADDRSTRLEN);
        port = ((struct sockaddr_in6 *) other_addr)->sin6_port;
    }
    printf("Connected to %s:%d\n", buf, ntohs(port));
}

void link_init(emulator_t *emu) {
    emu->link = xcalloc(1, sizeof(link_t));
    emu->link->other_addr = xcalloc(1, sizeof(struct sockaddr));
    emu->link->other_sfd = -1;
    emu->link->sfd = -1;
}

void link_quit(emulator_t *emu) {
    if (emu->link->other_sfd >= 0)
        close(emu->link->other_sfd);
    if (emu->link->is_server && emu->link->sfd >= 0)
        close(emu->link->sfd);
    free(emu->link);
}

int link_start_server(emulator_t *emu, const char *port, int is_ipv6, int mptcp_enabled) {
    link_t *link = emu->link;

    if (link->sfd != -1 || link->is_server)
        return 0;

    struct addrinfo hints = {
        .ai_family = is_ipv6 ? AF_INET6 : AF_INET,
        .ai_socktype = SOCK_STREAM,
        .ai_protocol = IPPROTO_TCP
    };
    struct addrinfo *res;
    int ret;
    if ((ret = getaddrinfo(NULL, port, &hints, &res)) != 0) {
        eprintf("getaddrinfo: %s\n", gai_strerror(ret));
        return 0;
    }

    for (; res != NULL; res = res->ai_next) {
        // getaddrinfo doesn't work with IPPROTO_MPTCP
        if ((link->sfd = socket(res->ai_family, res->ai_socktype | SOCK_NONBLOCK, mptcp_enabled ? IPPROTO_MPTCP : res->ai_protocol)) == -1)
            continue;

        if (!bind(link->sfd, res->ai_addr, res->ai_addrlen))
            break;

        close(link->sfd);
    }

    if (res == NULL) {
        eprintf("bind: Could not bind\n");
        link->sfd = -1;
        return 0;
    }

    freeaddrinfo(res);

    int option = 1;
    if (setsockopt(link->sfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option))) {
        errnoprintf("setsockopt");
        close(link->sfd);
        link->sfd = -1;
        return 0;
    };

    if (listen(link->sfd, 1) == -1) {
        errnoprintf("listen");
        close(link->sfd);
        link->sfd = -1;
        return 0;
    }

    link->is_server = 1;

    printf("Link server waiting for client on port %s...\n", port);

    return 1;
}

// TODO make network code separate form the emulator core
// TODO make ipv4 or ipv6 option in link options menu 
int link_connect_to_server(emulator_t *emu, const char *address, const char *port, int is_ipv6, int mptcp_enabled) {
    link_t *link = emu->link;

    if (link->other_sfd != -1 || link->is_server)
        return 0;

    struct addrinfo hints = {
        .ai_family = is_ipv6 ? AF_INET6 : AF_INET,
        .ai_socktype = SOCK_STREAM,
        .ai_protocol = IPPROTO_TCP
    };
    struct addrinfo *res;
    int ret;
    if ((ret = getaddrinfo(address, port, &hints, &res)) != 0) {
        eprintf("getaddrinfo: %s\n", gai_strerror(ret));
        return 0;
    }

    for (; res != NULL; res = res->ai_next) {
        // getaddrinfo doesn't work with IPPROTO_MPTCP
        if ((link->other_sfd = socket(res->ai_family, res->ai_socktype | SOCK_NONBLOCK, mptcp_enabled ? IPPROTO_MPTCP : res->ai_protocol)) == -1)
            continue;

        ret = connect(link->other_sfd, res->ai_addr, res->ai_addrlen);
        if (ret == -1 && errno != EINPROGRESS) {

        } else {
            break;
        }

        close(link->other_sfd);
    }

    if (res == NULL) {
        eprintf("connect: Could not connect\n");
        errnoprintf("connect");
        link->other_sfd = -1;
        return 0;
    }

    link->other_addr = res->ai_addr;
    freeaddrinfo(res);

    link->is_server = 0;

    printf("Link client connecting to server %s:%s...\n", address, port);

    return 1;
}

/**
 * Check if a connection has been made without blocking.
 */
static void complete_connection(link_t *link) {
    if (link->is_server) {
        socklen_t other_addr_len = link->other_addr->sa_family == AF_INET ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);
        link->other_sfd = accept(link->sfd, link->other_addr, &other_addr_len);
        if (link->other_sfd == -1) {
            if (errno != EAGAIN)
                errnoprintf("accept");
            return;
        }
        fcntl(link->other_sfd, F_SETFL, 0); // make other_sfd blocking
        link->connected = 1;
        print_connected_to(link->other_addr);
    } else {
        struct pollfd fds;
        fds.fd = link->other_sfd;
        fds.events = POLLOUT;
        if (poll(&fds, 1, 0) == 1) {
            char buf;
            if (send(link->other_sfd, &buf, 0, MSG_NOSIGNAL) == 0) {
                link->connected = 1;
                print_connected_to(link->other_addr);
                fcntl(link->other_sfd, F_SETFL, 0); // make other_sfd blocking
            } else {
                close(link->other_sfd);
                link->other_sfd = -1;
                errnoprintf("could not make a connection");
            }
        }
    }
}

/**
 * This implementation may cause bugs if the 2 emulators wants to communicate using the internal clock
 * at the same time or during the cycles needed to shift the 8 bits of SB. This should be a rare case
 * so it's not really a problem.
 * 
 * Cause of glitches because the 2 emulators are not completely synchronized?
 * Or maybe messages aren't sent/received due to errors? <-- test this first
 * 
 * TODO /!\ MUST DO ------> compare trace of bgb serial link packets with this one (at cycles_counter >= 512) to spot difference
 * if none, its a cycles sync problem and its bad...
 * 
 * I'll have to implement a cpu sync by forcing each iteration to take 4 cycles
 * (passing memory timings of blargg's tests) and send the current cycle to the serial link for comparison to do the interrupt at the same
 * time (same cycle)
 * maybe with 512 as limit value, problems are due to the cpu not using the SB byte and being interrupted before it has the time... but unlikely
 * as the interrupts disable other interrupts as they are processed
 * 
 * If emulator on 1 computer faster than the other, may cause problems --> cpu cycle sync important?
 */
void link_step(emulator_t *emu, int cycles) {
    link_t *link = emu->link;
    mmu_t *mmu = emu->mmu;

    link->cycles_counter += cycles;

    // TODO this should be a 8192 Hz clock (cycles_counter >= 512)... but as it's too fast and cause bugs, this will do for now
    if (link->cycles_counter >= 4096) {
        link->cycles_counter -= 4096; // keep leftover cycles (if any)

        if (!link->connected)
            complete_connection(link);

        // transfer requested / in progress with internal clock (we are the master of the connection)
        byte_t transfer = 0;
        if (CHECK_BIT(mmu->mem[SC], 7) && CHECK_BIT(mmu->mem[SC], 0)) {
            if (link->bit_counter < 8) { // emulate 8 bit shifting
                link->bit_counter++;
            } else {
                link->bit_counter = 0;
                transfer = 1;
            }
        }

        if (link->connected) {
            // send our SB register and transfer flag
            pkt_t outbuf = { .data = mmu->mem[SB], .transfer = transfer };
            send(link->other_sfd, &outbuf, sizeof(outbuf), MSG_NOSIGNAL);

            // receive their SB register and transfer flag
            pkt_t inbuf = { 0 };
            recv(link->other_sfd, &inbuf, sizeof(inbuf), MSG_NOSIGNAL);

            // if either one of us initiate the transfer (transfer flag is true), swap our SB registers and request interrupt
            if (outbuf.transfer || inbuf.transfer) {
                // printf("send %02X, recv %02X\n", outbuf.data, inbuf.data);
                mmu->mem[SB] = inbuf.data;
                RESET_BIT(mmu->mem[SC], 7);
                cpu_request_interrupt(emu, IRQ_SERIAL);
            }
        } else if (transfer) { // not connected but still transfer pending
            mmu->mem[SB] = 0xFF;
            RESET_BIT(mmu->mem[SC], 7);
            cpu_request_interrupt(emu, IRQ_SERIAL);
        }
    }
}
