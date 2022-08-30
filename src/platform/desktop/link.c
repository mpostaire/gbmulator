#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "emulator/emulator.h"

#ifndef IPPROTO_MPTCP
#define IPPROTO_MPTCP 262
#endif

typedef enum {
    INFO_SEND,
    INFO_SENDING,
    INFO_RECEIVE,
    ROM_SEND,
    ROM_SENDING,
    ROM_RECEIVE_HEADER,
    ROM_RECEIVE_PAYLOAD,
    STATE_SEND,
    STATE_SENDING,
    STATE_RECEIVE_HEADER,
    STATE_RECEIVE_PAYLOAD,
    JOYPAD_CHANGE
} link_state_t;

typedef enum {
    INFO,
    ROM,
    STATE,
    JOYPAD
} pkt_type_t;

int server_sfd = -1;
int client_sfd = -1;
int is_server = 0;
struct sockaddr server_addr;

link_state_t protocol_state = INFO_SEND;
ssize_t last_buf_received_len = 0;
ssize_t last_buf_sent_len = 0;
word_t checksum;

static void print_connected_to(struct sockaddr *addr) {
    char buf[INET6_ADDRSTRLEN];
    int port;
    if (addr->sa_family == AF_INET) {
        struct sockaddr_in *temp = (struct sockaddr_in *) addr;
        inet_ntop(addr->sa_family, &temp->sin_addr, buf, sizeof(buf));
        port = temp->sin_port;
    } else {
        struct sockaddr_in6 *temp = (struct sockaddr_in6 *) addr;
        inet_ntop(addr->sa_family, &temp->sin6_addr, buf, sizeof(buf));
        port = temp->sin6_port;
    }
    printf("Connected to %s on port %d\n", buf, ntohs(port));
}

int link_start_server(const char *port, int is_ipv6, int mptcp_enabled) {
    // if (server_sfd != -1 || is_server)
    //     return 0;

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
        if ((server_sfd = socket(res->ai_family, res->ai_socktype | SOCK_NONBLOCK, mptcp_enabled ? IPPROTO_MPTCP : res->ai_protocol)) == -1)
            continue;

        if (!bind(server_sfd, res->ai_addr, res->ai_addrlen))
            break;

        close(server_sfd);
    }

    if (res == NULL) {
        eprintf("bind: Could not bind\n");
        server_sfd = -1;
        return 0;
    }

    freeaddrinfo(res);

    if (setsockopt(server_sfd, SOL_SOCKET, SO_REUSEADDR, &(int) { 1 }, sizeof(int))) {
        errnoprintf("setsockopt");
        close(server_sfd);
        server_sfd = -1;
        return 0;
    };

    if (listen(server_sfd, 1) == -1) {
        errnoprintf("listen");
        close(server_sfd);
        server_sfd = -1;
        return 0;
    }

    is_server = 1;

    printf("Link server waiting for client on port %s...\n", port);

    return 1;
}

int link_connect_to_server(const char *address, const char *port, int is_ipv6, int mptcp_enabled) {
    // if (server_sfd != -1 || is_server)
    //     return 0;

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
        if ((server_sfd = socket(res->ai_family, res->ai_socktype | SOCK_NONBLOCK, mptcp_enabled ? IPPROTO_MPTCP : res->ai_protocol)) == -1)
            continue;

        ret = connect(server_sfd, res->ai_addr, res->ai_addrlen);
        if (ret == -1 && errno != EINPROGRESS) {

        } else {
            break;
        }

        close(server_sfd);
    }

    if (res == NULL) {
        eprintf("connect: Could not connect\n");
        errnoprintf("connect");
        server_sfd = -1;
        return 0;
    }

    server_addr = *res->ai_addr;
    freeaddrinfo(res);

    is_server = 0;

    printf("Link client connecting to server %s on port %s...\n", address, port);

    return 1;
}

/**
 * Check if a connection has been made without blocking.
 */
int link_complete_connection(void) {
    if (is_server) {
        socklen_t client_addr_len = sizeof(struct sockaddr_in6); // take the largest possible size regardless of IP version
        struct sockaddr *client_addr = xmalloc(client_addr_len);
        client_sfd = accept(server_sfd, client_addr, &client_addr_len);
        if (client_sfd == -1) {
            if (errno != EAGAIN)
                errnoprintf("accept");
            return 0;
        }
        print_connected_to(client_addr);
        free(client_addr);
        return 1;
    } else {
        struct pollfd fds;
        fds.fd = server_sfd;
        fds.events = POLLOUT;
        if (poll(&fds, 1, 0) == 1) {
            char buf;
            if (send(server_sfd, &buf, 0, MSG_NOSIGNAL) == 0) {
                print_connected_to(&server_addr);
                return 1;
            } else {
                close(server_sfd);
                server_sfd = -1;
                errnoprintf("could not make a connection");
            }
        }
    }

    return 0;
}

int send_pkt(int fd, void *buf, size_t n, int flags) {
    struct pollfd fds;
    fds.fd = fd;
    fds.events = POLLOUT;
    if (poll(&fds, 1, 0) != 1)
        return 0;

    ssize_t ret = send(fd, &((char *) buf)[last_buf_sent_len], n - last_buf_sent_len, flags);
    if (ret == -1) {
        errnoprintf();
        return 0;
    }
    last_buf_sent_len += ret;
    if (last_buf_sent_len > n) {
        eprintf("sent too much (expected %ld, sent %ld)\n", n, last_buf_sent_len);
        last_buf_sent_len = 0;
        return 0;
    } else if (last_buf_sent_len == n) {
        last_buf_sent_len = 0;
        return 1;
    }
    return 0;
}

int receive_pkt(int fd, void *buf, size_t n, int flags) {
    struct pollfd fds;
    fds.fd = fd;
    fds.events = POLLIN;
    if (poll(&fds, 1, 0) != 1)
        return 0;

    ssize_t ret = recv(fd, &((char *) buf)[last_buf_received_len], n - last_buf_received_len, flags);
    if (ret == -1) {
        errnoprintf();
        return 0;
    }
    last_buf_received_len += ret;
    if (last_buf_received_len > n) {
        eprintf("received too much (expected %ld, got %ld)\n", n, last_buf_received_len);
        last_buf_received_len = 0;
        return 0;
    } else if (last_buf_received_len == n) {
        last_buf_received_len = 0;
        return 1;
    }
    return 0;
}

byte_t can_compress = 0;
emulator_mode_t mode;
byte_t *savestate;
byte_t *rom_data;
uint64_t savestate_len;
size_t rom_len;
byte_t small_buf[9];
byte_t *buf;
int link_run_init_transfer(emulator_t *emu, emulator_t **linked_emu) {
    // TODO connection lost detection (return -1)



    int sfd = is_server ? client_sfd : server_sfd;
    *linked_emu = NULL;
    switch (protocol_state) {
    case INFO_SEND:
        // cartridge checksum
        checksum = 0;
        for (int i = 0; i < sizeof(emu->mmu->cartridge); i += 2)
            checksum = checksum - (emu->mmu->cartridge[i] + emu->mmu->cartridge[i + 1]) - 1;

        small_buf[0] = INFO;
        small_buf[1] = emu->mode;
        #ifdef __HAVE_ZLIB__
        // SET_BIT(small_buf[1], 7); // TODO fix compression
        #endif
        memcpy(&small_buf[2], &checksum, 2);

        protocol_state = INFO_SENDING;
        break;
    case INFO_SENDING:
        if (!send_pkt(sfd, small_buf, 4, 0))
            return 0;
        protocol_state = INFO_RECEIVE;
        break;
    case INFO_RECEIVE:
        if (!receive_pkt(sfd, small_buf, 4, 0))
            return 0;

        if (small_buf[0] != INFO) {
            eprintf("received packet type %d but expected %d (ignored)\n", small_buf[0], INFO);
            return 0;
        }

        mode = small_buf[1] & 0x03;
        #ifdef __HAVE_ZLIB__
        can_compress = GET_BIT(small_buf[1], 7);
        #endif

        word_t received_checksum = 0;
        memcpy(&received_checksum, &small_buf[2], 2);
        if (received_checksum == checksum) {
            protocol_state = STATE_SEND;
        } else {
            printf("received checksum '%x' but expected '%x': exchanging ROMs\n", received_checksum, checksum);
            protocol_state = ROM_SEND;
        }
        break;
    case ROM_SEND:
        // TODO compression
        byte_t *rom = emulator_get_rom(emu, &rom_len);

        buf = xcalloc(1, rom_len + 9);
        buf[0] = ROM;
        memcpy(&buf[1], &rom_len, 8);
        memcpy(&buf[9], &rom, rom_len);

        protocol_state = ROM_SENDING;
        break;
    case ROM_SENDING:
        if (!send_pkt(sfd, buf, rom_len + 9, 0))
            return 0;
        free(buf);
        protocol_state = ROM_RECEIVE_HEADER;
        break;
    case ROM_RECEIVE_HEADER:
        // TODO compression
        if (!receive_pkt(sfd, small_buf, 9, 0))
            return 0;

        if (small_buf[0] != ROM) {
            eprintf("received packet type %d but expected %d (ignored)\n", small_buf[0], ROM);
            return 0;
        }

        memcpy(&rom_len, &small_buf[1], 8);
        rom_data = xcalloc(1, rom_len);

        protocol_state = ROM_RECEIVE_PAYLOAD;
        break;
    case ROM_RECEIVE_PAYLOAD:
        if (!receive_pkt(sfd, rom_data, rom_len, 0))
            return 0;
        protocol_state = STATE_SEND;
    case STATE_SEND:
        savestate = emulator_get_savestate(emu, &savestate_len, can_compress);

        buf = xcalloc(1, savestate_len + 9);
        buf[0] = STATE;
        memcpy(&buf[1], &savestate_len, 8);
        memcpy(&buf[9], savestate, savestate_len);

        protocol_state = STATE_SENDING;
        break;
    case STATE_SENDING:
        if (!send_pkt(sfd, buf, savestate_len + 9, 0))
            return 0;
        free(buf);
        protocol_state = STATE_RECEIVE_HEADER;
        break;
    case STATE_RECEIVE_HEADER:
        if (!receive_pkt(sfd, small_buf, 9, 0))
            return 0;

        if (small_buf[0] != STATE) {
            eprintf("received packet type %d but expected %d (ignored)\n", small_buf[0], STATE);
            return 0;
        }

        memcpy(&savestate_len, &small_buf[1], 8);

        savestate = xrealloc(savestate, savestate_len);

        protocol_state = STATE_RECEIVE_PAYLOAD;
        break;
    case STATE_RECEIVE_PAYLOAD:
        if (!receive_pkt(sfd, savestate, savestate_len, 0))
            return 0;

        emulator_options_t opts = { .mode = mode };
        if (rom_data) {
            *linked_emu = emulator_init(rom_data, rom_len, &opts);
            // if (!*linked_emu) { // TODO error handling but instead of restarting from INFO_SEND send a special ERROR packet
            //     eprintf("received invalid or corrupted ROM, retrying connection...\n");
            //     protocol_state = INFO_SEND;
            //     free(rom_data);
            //     free(savestate);
            //     return 0;
            // }
            free(rom_data);
        } else {
            rom_data = emulator_get_rom(emu, &rom_len);
            *linked_emu = emulator_init(rom_data, rom_len, &opts);
        }
        emulator_load_savestate(*linked_emu, savestate, savestate_len);
        // if (!emulator_load_savestate(*linked_emu, savestate, savestate_len)) { // TODO error handling but instead of restarting from INFO_SEND send a special ERROR packet
        //     eprintf("received invalid or corrupted savestate, retrying connection...\n");
        //     protocol_state = INFO_SEND;
        //     return 0;
        // }
        emulator_link_connect(emu, *linked_emu);

        free(savestate);

        protocol_state = JOYPAD_CHANGE;
        return 1;
    default:
        break;
    }

    return 0;
}

void link_send_joypad(byte_t joypad) {
    int sfd = is_server ? client_sfd : server_sfd;
    char buf[2];
    buf[0] = JOYPAD;
    buf[1] = joypad;
    send_pkt(sfd, buf, 2, 0);
}

void link_poll_joypad(emulator_t *emu) {
    int sfd = is_server ? client_sfd : server_sfd;
    char buf[2];
    if (!receive_pkt(sfd, buf, 2, 0))
        return;
    if (buf[0] != JOYPAD) {
        eprintf("received packet type %d but expected %d (ignored)\n", buf[0], JOYPAD);
        return;
    }
    emulator_set_joypad_state(emu, buf[1]);
}
