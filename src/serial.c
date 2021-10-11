#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "cpu.h"
#include "mem.h"

// TODO this is still buggy: tetris works fine, dr mario has glitched game over screen and pokemon red has glitched fight and trade
// TODO fix problems when a emulator is paused during a connection
// TODO handle disconnection
// TODO this may cause glitches in sound emulation due to the blocking send and recv (investigate when sound is implemented)

int cycles_counter = 0;
int bit_counter = 0;
int sfd = -1; // server's socket used only by the server
int is_server = 0;
int other_sfd = -1;
int connected = 0;
struct sockaddr_in other_addr;

int serial_start_server(const char* address, const int port) {
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

    printf("Waiting for a connection...\n");

    return 1;
}

int serial_connect_to_server(const char* address, const int port) {
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

    return 1;
}

void serial_close_connection(void) {
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
                printf("Could not make a connection\n");
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
 */
void serial_step(int cycles) {
    cycles_counter += cycles;

    if (cycles_counter >= 512) { // 8192 Hz clock
        cycles_counter -= 512; // keep leftover cycles (if any)

        if (!connected)
            complete_connection();

        // if connected, check for any receiving data (non blocking in case we don't receive anything)
        if (connected) {
            byte_t buf;
            // if we receive the SB byte of another emulator without sending one first, we are the slave of the connection
            // we know we didn't send a SB byte first because otherwise, we would have been blocked waiting for an answer.
            if (recv(other_sfd, &buf, sizeof(byte_t), MSG_NOSIGNAL | MSG_DONTWAIT) == 1) {
                // send back our SB byte
                send(other_sfd, &mem[SB], sizeof(byte_t), MSG_NOSIGNAL| MSG_DONTWAIT);
                // store the other emulator's SB byte in ours and request interrupt
                mem[SB] = buf;
                cpu_request_interrupt(IRQ_SERIAL);
                return;
            }
        }

        // transfer requested / in progress with internal clock (we are the master of the connection)
        if (CHECK_BIT(mem[SC], 7) && CHECK_BIT(mem[SC], 0)) {
            if (bit_counter < 8) { // emulate 8 bit shifting
                bit_counter++;
            } else if (connected) { // once all the bits have been shifted and we have a connection
                // send our SB byte to the other emulator
                send(other_sfd, &mem[SB], sizeof(byte_t), MSG_NOSIGNAL); // send once all data
                // wait for the other emulator's SB byte and place it in ours.
                if (recv(other_sfd, &mem[SB], sizeof(byte_t), MSG_NOSIGNAL) == 1) {
                    RESET_BIT(mem[SC], 7);
                    cpu_request_interrupt(IRQ_SERIAL);
                    bit_counter = 0;
                }
            } else { // if not connected, receive 0xFF and request interrupt
                RESET_BIT(mem[SC], 7);
                mem[SB] = 0xFF;
                cpu_request_interrupt(IRQ_SERIAL);
                bit_counter = 0;
            }
        }
    }
}
