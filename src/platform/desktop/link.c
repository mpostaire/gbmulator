#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "emulator/emulator.h"

#ifndef IPPROTO_MPTCP
#define IPPROTO_MPTCP 262
#endif

typedef enum {
    INFO,
    ROM,
    STATE,
    JOYPAD
} pkt_type_t;

int server_sfd = -1;
int client_sfd = -1;
int is_server = 0;

static void print_connected_to(struct sockaddr *addr) {
    char buf[INET6_ADDRSTRLEN];
    int port;
    if (addr->sa_family == AF_INET) {
        inet_ntop(addr->sa_family, &((struct sockaddr_in *) addr)->sin_addr, buf, sizeof(buf));
        port = ((struct sockaddr_in *) addr)->sin_port;
    } else {
        inet_ntop(addr->sa_family, &((struct sockaddr_in6 *) addr)->sin6_addr, buf, sizeof(buf));
        port = ((struct sockaddr_in6 *) addr)->sin6_port;
    }
    printf("Link cable connected to %s on port %d\n", buf, ntohs(port));
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
        if ((server_sfd = socket(res->ai_family, res->ai_socktype, mptcp_enabled ? IPPROTO_MPTCP : res->ai_protocol)) == -1)
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

    // wait for a client connection
    socklen_t client_addr_len = sizeof(struct sockaddr_in6); // take the largest possible size regardless of IP version
    struct sockaddr *client_addr = xmalloc(client_addr_len);
    client_sfd = accept(server_sfd, client_addr, &client_addr_len);
    if (client_sfd == -1)
        return 0;
    print_connected_to(client_addr);
    free(client_addr);
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
        if ((server_sfd = socket(res->ai_family, res->ai_socktype, mptcp_enabled ? IPPROTO_MPTCP : res->ai_protocol)) == -1)
            continue;

        if (connect(server_sfd, res->ai_addr, res->ai_addrlen) == -1)
            close(server_sfd);
        else
            break;
    }

    if (res == NULL) {
        eprintf("connect: Could not connect\n");
        errnoprintf("connect");
        server_sfd = -1;
        return 0;
    }

    is_server = 0;

    printf("Link client connecting to server %s on port %s...\n", address, port);

    // confirm connection to the server
    char buf;
    if (send(server_sfd, &buf, 0, MSG_NOSIGNAL) == -1) {
        close(server_sfd);
        server_sfd = -1;
        errnoprintf("could not make a connection");
        return 0;
    }
    print_connected_to(res->ai_addr);
    freeaddrinfo(res);
    return 1;
}

static inline int receive(int fd, void *buf, size_t n, int flags) {
    ssize_t total_ret = 0;
    while (total_ret != n) {
        ssize_t ret = recv(fd, &((char *) buf)[total_ret], n - total_ret, flags);
        if (ret <= 0)
            return 0;
        total_ret += ret;
    }
    return 1;
}

static int exchange_info(int sfd, emulator_t *emu, emulator_mode_t *mode, int *can_compress) {
    // --- SEND INFO ---

    word_t checksum = 0;
    for (int i = 0; i < sizeof(emu->mmu->cartridge); i += 2)
        checksum = checksum - (emu->mmu->cartridge[i] + emu->mmu->cartridge[i + 1]) - 1;

    byte_t pkt[4];
    pkt[0] = INFO;
    pkt[1] = emu->mode;
    #ifdef __HAVE_ZLIB__
    // SET_BIT(pkt[1], 7); // TODO fix compression
    #endif
    memcpy(&pkt[2], &checksum, 2);

    send(sfd, pkt, 4, 0);

    // --- RECEIVE INFO ---

    receive(sfd, pkt, 4, 0);

    if (pkt[0] != INFO) {
        eprintf("received packet type %d but expected %d (ignored)\n", pkt[0], INFO);
        return -1;
    }

    *mode = pkt[1] & 0x03;
    #ifdef __HAVE_ZLIB__
    *can_compress = GET_BIT(pkt[1], 7);
    #endif

    word_t received_checksum = 0;
    memcpy(&received_checksum, &pkt[2], 2);
    if (received_checksum == checksum) {
        return 1;
    } else {
        printf("checksum mismatch ('%x' != '%x'): exchanging ROMs\n", checksum, received_checksum);
        return 0;
    }
}

static int exchange_rom(int sfd, emulator_t *emu, byte_t **rom_data, size_t *rom_len) {
    // --- SEND ROM ---

    // TODO compression
    byte_t *rom = emulator_get_rom(emu, rom_len);

    byte_t *pkt = xcalloc(1, *rom_len + 9);
    pkt[0] = ROM;
    memcpy(&pkt[1], rom_len, 8);
    memcpy(&pkt[9], &rom, *rom_len);

    send(sfd, pkt, *rom_len + 9, 0);
    free(pkt);

    // --- RECEIVE ROM ---

    // TODO compression
    byte_t pkt_header[9];
    receive(sfd, pkt_header, 9, 0);

    if (pkt_header[0] != ROM) {
        eprintf("received packet type %d but expected %d (ignored)\n", pkt_header[0], ROM);
        return 0;
    }

    memcpy(rom_len, &pkt_header[1], 8);
    *rom_data = xcalloc(1, *rom_len);

    receive(sfd, *rom_data, *rom_len, 0);

    return 1;
}

static int exchange_savestate(int sfd, emulator_t *emu, int can_compress, byte_t **savestate_data, size_t *savestate_len) {
    // --- SEND SAVESTATE ---

    byte_t *local_savestate_data = emulator_get_savestate(emu, savestate_len, can_compress);

    byte_t *pkt = xcalloc(1, *savestate_len + 9);
    pkt[0] = STATE;
    memcpy(&pkt[1], savestate_len, 8);
    memcpy(&pkt[9], local_savestate_data, *savestate_len);

    send(sfd, pkt, *savestate_len + 9, 0);
    free(pkt);
    free(local_savestate_data);

    // --- RECEIVE SAVESTATE ---

    byte_t pkt_header[9];
    receive(sfd, pkt_header, 9, 0);

    if (pkt_header[0] != STATE) {
        eprintf("received packet type %d but expected %d (ignored)\n", pkt_header[0], STATE);
        return 0;
    }

    memcpy(savestate_len, &pkt_header[1], 8);

    *savestate_data = xcalloc(1, *savestate_len);

    receive(sfd, *savestate_data, *savestate_len, 0);

    return 1;
}

int link_init_transfer(emulator_t *emu, emulator_t **linked_emu) {
    // TODO connection lost detection (return -1)
    int sfd = is_server ? client_sfd : server_sfd;
    *linked_emu = NULL;
    emulator_mode_t mode = 0;
    int can_compress = 0;
    size_t rom_len;
    byte_t *rom_data = NULL;
    size_t savestate_len;
    byte_t *savestate_data = NULL;

    // TODO handle wrong packet type received
    int ret = exchange_info(sfd, emu, &mode, &can_compress);
    if (ret == 0)
        exchange_rom(sfd, emu, &rom_data, &rom_len);
    exchange_savestate(sfd, emu, can_compress, &savestate_data, &savestate_len);

    // --- LINK BACKGROUND EMUALATOR ---

    emulator_options_t opts = { .mode = mode };
    if (rom_data) {
        *linked_emu = emulator_init(rom_data, rom_len, &opts);
        if (!*linked_emu) {
            eprintf("received invalid or corrupted ROM\n");
            free(rom_data);
            return 0;
        }
        free(rom_data);
    } else {
        rom_data = emulator_get_rom(emu, &rom_len);
        *linked_emu = emulator_init(rom_data, rom_len, &opts);
    }
    if (!emulator_load_savestate(*linked_emu, savestate_data, savestate_len)) {
        eprintf("received invalid or corrupted savestate\n");
        return 0;
    }
    emulator_link_connect(emu, *linked_emu);

    free(savestate_data);
    return 1;
}

int link_exchange_joypad(emulator_t *emu, emulator_t *linked_emu) {
    int sfd = is_server ? client_sfd : server_sfd;
    char buf[2];
    buf[0] = JOYPAD;
    buf[1] = emulator_get_joypad_state(emu);
    send(sfd, buf, 2, 0);

    do {
        if (!receive(sfd, buf, 2, 0)) {
            printf("Link cable disconnected\n");
            emulator_link_disconnect(emu);
            emulator_quit(linked_emu);
            return 0;
        }
        if (buf[0] != JOYPAD)
            eprintf("received packet type %d but expected %d (ignored)\n", buf[0], JOYPAD);
    } while (buf[0] != JOYPAD);

    emulator_set_joypad_state(linked_emu, buf[1]);
    return 1;
}
