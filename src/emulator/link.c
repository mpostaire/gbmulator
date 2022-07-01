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

int link_start_server(emulator_t *emu, const char *port) {
    link_t *link = emu->link;

    if (link->sfd != -1 || link->is_server)
        return 0;

    struct addrinfo hints = {
        .ai_family = AF_UNSPEC,
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
        // TODO MPTCP
        if ((link->sfd = socket(res->ai_family, res->ai_socktype | SOCK_NONBLOCK, res->ai_protocol)) == -1)
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
int link_connect_to_server(emulator_t *emu, const char *address, const char *port) {
    link_t *link = emu->link;

    if (link->other_sfd != -1 || link->is_server)
        return 0;

    struct addrinfo hints = {
        .ai_family = AF_UNSPEC,
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
        // TODO MPTCP
        if ((link->other_sfd = socket(res->ai_family, res->ai_socktype | SOCK_NONBLOCK, res->ai_protocol)) == -1)
            continue;

        int ret = connect(link->other_sfd, res->ai_addr, res->ai_addrlen);
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
