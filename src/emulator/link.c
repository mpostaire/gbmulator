#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "utils.h"
#include "cpu.h"
#include "mmu.h"

// TODO this is still buggy: tetris works fine, dr mario has glitched game over screen (unrelated?) and pokemon red has glitched fight and trade
// TODO fix problems when a emulator is paused during a connection
// TODO handle disconnection
// TODO this may cause glitches in sound emulation due to the blocking send and recv (investigate when sound is implemented)

struct pkt {
    byte_t data;
    byte_t transfer;
};

int cycles_counter = 0;
int bit_counter = 0;
int sfd = -1; // server's socket used only by the server
int is_server = 0;
int other_sfd = -1;
int connected = 0;
struct sockaddr_in other_addr;

int link_start_server(const int port) {
    if (sfd != -1 || is_server)
        return 0;

    sfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (sfd == -1) {
        perror("socket");
        return 0;
    }
    int option = 1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

    struct sockaddr_in addr = { 0 };
    addr.sin_addr.s_addr = htonl(INADDR_ANY); // use INADDR_LOOPBACK for local host only
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (bind(sfd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
        perror("bind");
        close(sfd);
        sfd = -1;
        return 0;
    }

    if (listen(sfd, 1) == -1) {
        perror("listen");
        close(sfd);
        sfd = -1;
        return 0;
    }

    is_server = 1;

    printf("Link server waiting for client on port %d...\n", port);

    return 1;
}

int link_connect_to_server(const char* address, const int port) {
    if (other_sfd != -1 || is_server)
        return 0;

    other_sfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (other_sfd == -1) {
        perror("socket");
        return 0;
    }

    other_addr.sin_addr.s_addr = inet_addr(address); // local host
    other_addr.sin_family = AF_INET;
    other_addr.sin_port = htons(port);

    int ret = connect(other_sfd, (struct sockaddr *) &other_addr, sizeof(other_addr));
    if (ret == -1 && errno != EINPROGRESS) {
        perror("connect");
        close(other_sfd);
        other_sfd = -1;
        return 0;
    }

    is_server = 0;

    printf("Link client connecting to server %s:%d...\n", address, port);

    return 1;
}

void link_close_connection(void) {
    close(other_sfd);
    if (is_server)
        close(sfd);
}

/**
 * Check if a connection has been made without blocking.
 */
static void complete_connection(void) {
    if (is_server) {
        socklen_t other_addr_len = sizeof(other_addr);
        other_sfd = accept(sfd, (struct sockaddr *) &other_addr, &other_addr_len);
        if (other_sfd == -1) {
            if (errno != EAGAIN)
                perror("accept");
            return;
        }
        fcntl(other_sfd, F_SETFL, 0); // make other_sfd blocking
        connected = 1;
        printf("Connected to %s:%d\n", inet_ntoa(other_addr.sin_addr), ntohs(other_addr.sin_port));
    } else {
        struct pollfd fds;
        fds.fd = other_sfd;
        fds.events = POLLOUT;
        if (poll(&fds, 1, 0) == 1) {
            char buf;
            if (send(other_sfd, &buf, 0, MSG_NOSIGNAL) == 0) {
                connected = 1;
                printf("Connected to %s:%d\n", inet_ntoa(other_addr.sin_addr), ntohs(other_addr.sin_port));
                fcntl(other_sfd, F_SETFL, 0); // make other_sfd blocking
            } else {
                close(other_sfd);
                other_sfd = -1;
                perror("Could not make a connection");
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
void link_step(int cycles) {
    cycles_counter += cycles;

    // TODO this should be a 8192 Hz clock (cycles_counter >= 512)... but as it's too fast and cause bugs, this will do for now
    if (cycles_counter >= 4096) {
        cycles_counter -= 4096; // keep leftover cycles (if any)

        if (!connected)
            complete_connection();

        // transfer requested / in progress with internal clock (we are the master of the connection)
        byte_t transfer = 0;
        if (CHECK_BIT(mem[SC], 7) && CHECK_BIT(mem[SC], 0)) {
            if (bit_counter < 8) { // emulate 8 bit shifting
                bit_counter++;
            } else {
                bit_counter = 0;
                transfer = 1;
            }
        }

        if (connected) {
            // send our SB register and transfer flag
            struct pkt outbuf = { .data = mem[SB], .transfer = transfer };
            send(other_sfd, &outbuf, sizeof(outbuf), MSG_NOSIGNAL);

            // receive their SB register and transfer flag
            struct pkt inbuf = { 0 };
            recv(other_sfd, &inbuf, sizeof(inbuf), MSG_NOSIGNAL);

            // if either one of us initiate the transfer (transfer flag is true), swap our SB registers and request interrupt
            if (outbuf.transfer || inbuf.transfer) {
                // printf("send %02X, recv %02X\n", outbuf.data, inbuf.data);
                mem[SB] = inbuf.data;
                RESET_BIT(mem[SC], 7);
                cpu_request_interrupt(IRQ_SERIAL);
            }
        } else if (transfer) { // not connected but still transfer pending
            mem[SB] = 0xFF;
            RESET_BIT(mem[SC], 7);
            cpu_request_interrupt(IRQ_SERIAL);
        }
    }
}