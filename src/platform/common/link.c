#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "../../emulator/emulator.h"

typedef enum {
    PKT_INFO,
    PKT_ROM,
    PKT_STATE,
    PKT_JOYPAD
} pkt_type_t;

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

int link_start_server(const char *port) {
    struct addrinfo hints = { 0 };
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
    struct addrinfo *res;
    int ret;
    if ((ret = getaddrinfo(NULL, port, &hints, &res)) != 0) {
        eprintf("getaddrinfo: %s\n", gai_strerror(ret));
        return -1;
    }

    int server_sfd;
    for (; res != NULL; res = res->ai_next) {
        if ((server_sfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1)
            continue;

        if (setsockopt(server_sfd, SOL_SOCKET, SO_REUSEADDR, &(int) { 1 }, sizeof(int))) {
            errnoprintf("setsockopt");
            close(server_sfd);
            return -1;
        };

        if (bind(server_sfd, res->ai_addr, res->ai_addrlen) == -1) {
            close(server_sfd);
            continue;
        }

        break;
    }

    if (res == NULL) {
        eprintf("bind: Could not bind\n");
        return -1;
    }

    freeaddrinfo(res);

    if (listen(server_sfd, 1) == -1) {
        errnoprintf("listen");
        close(server_sfd);
        return -1;
    }

    printf("Link server waiting for client on port %s...\n", port);

    // wait for a client connection
    socklen_t client_addr_len = sizeof(struct sockaddr_in6); // take the largest possible size regardless of IP version
    struct sockaddr *client_addr = xmalloc(client_addr_len);
    int client_sfd = accept(server_sfd, client_addr, &client_addr_len);
    if (client_sfd == -1)
        return -1;

    close(server_sfd); // don't accept additional connections
    print_connected_to(client_addr);
    free(client_addr);

    return client_sfd;
}

int link_connect_to_server(const char *address, const char *port) {
    struct addrinfo hints = {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
        .ai_protocol = IPPROTO_TCP
    };
    struct addrinfo *res;
    int ret;
    if ((ret = getaddrinfo(address, port, &hints, &res)) != 0) {
        eprintf("getaddrinfo: %s\n", gai_strerror(ret));
        return -1;
    }

    int server_sfd;
    for (; res != NULL; res = res->ai_next) {
        if ((server_sfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1)
            continue;

        if (connect(server_sfd, res->ai_addr, res->ai_addrlen) == -1)
            close(server_sfd);
        else
            break;
    }

    if (res == NULL) {
        errnoprintf("connect %d", ret);
        return -1;
    }

    print_connected_to(res->ai_addr);
    freeaddrinfo(res);
    return server_sfd;
}

static inline int receive(int fd, void *buf, size_t n, int flags) {
    ssize_t total_ret = 0;
    while (total_ret != (ssize_t) n) {
        ssize_t ret = recv(fd, &((char *) buf)[total_ret], n - total_ret, flags);
        if (ret <= 0)
            return 0;
        total_ret += ret;
    }
    return 1;
}

static int exchange_info(int sfd, emulator_t *emu, emulator_mode_t *mode, int *can_compress) {
    // --- SEND PKT_INFO ---

    word_t checksum = emulator_get_cartridge_checksum(emu);

    byte_t pkt[4] = { 0 };
    pkt[0] = PKT_INFO;
    pkt[1] = emulator_get_mode(emu);
    #ifdef __HAVE_ZLIB__
    // SET_BIT(pkt[1], 7); // TODO fix compression
    #endif
    memcpy(&pkt[2], &checksum, 2);

    send(sfd, pkt, 4, 0);

    // --- RECEIVE PKT_INFO ---

    receive(sfd, pkt, 4, 0);

    if (pkt[0] != PKT_INFO) {
        eprintf("received packet type %d but expected %d (ignored)\n", pkt[0], PKT_INFO);
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
    // --- SEND PKT_ROM ---

    // TODO compression
    byte_t *rom = emulator_get_rom(emu, rom_len);

    byte_t *pkt = xcalloc(1, *rom_len + 9);
    pkt[0] = PKT_ROM;
    memcpy(&pkt[1], rom_len, sizeof(size_t));
    memcpy(&pkt[9], &rom, *rom_len);

    send(sfd, pkt, *rom_len + 9, 0);
    free(pkt);

    // --- RECEIVE PKT_ROM ---

    // TODO compression
    byte_t pkt_header[9] = { 0 };
    receive(sfd, pkt_header, 9, 0);

    if (pkt_header[0] != PKT_ROM) {
        eprintf("received packet type %d but expected %d (ignored)\n", pkt_header[0], PKT_ROM);
        return 0;
    }

    memcpy(rom_len, &pkt_header[1], sizeof(size_t));
    *rom_data = xcalloc(1, *rom_len);

    receive(sfd, *rom_data, *rom_len, 0);

    return 1;
}

static int exchange_savestate(int sfd, emulator_t *emu, int can_compress, byte_t **savestate_data, size_t *savestate_len) {
    // --- SEND PKT_STATE ---

    byte_t *local_savestate_data = emulator_get_savestate(emu, savestate_len, can_compress);

    byte_t *pkt = xcalloc(1, *savestate_len + 9);
    pkt[0] = PKT_STATE;
    memcpy(&pkt[1], savestate_len, sizeof(size_t));
    memcpy(&pkt[9], local_savestate_data, *savestate_len);

    send(sfd, pkt, *savestate_len + 9, 0);
    free(pkt);
    free(local_savestate_data);

    // --- RECEIVE PKT_STATE ---

    byte_t pkt_header[9] = { 0 };
    receive(sfd, pkt_header, 9, 0);

    if (pkt_header[0] != PKT_STATE) {
        eprintf("received packet type %d but expected %d (ignored)\n", pkt_header[0], PKT_STATE);
        return 0;
    }

    memcpy(savestate_len, &pkt_header[1], sizeof(size_t));

    *savestate_data = xcalloc(1, *savestate_len);

    receive(sfd, *savestate_data, *savestate_len, 0);

    return 1;
}

int link_init_transfer(int sfd, emulator_t *emu, emulator_t **linked_emu) {
    // TODO connection lost detection (return -1)
    *linked_emu = NULL;
    emulator_mode_t mode = 0;
    int can_compress = 0;
    size_t rom_len;
    byte_t *rom_data = NULL;
    size_t savestate_len;
    byte_t *savestate_data = NULL;

    // TODO handle wrong packet type received
    int ret = exchange_info(sfd, emu, &mode, &can_compress);
    if (ret == 0) {
        exchange_rom(sfd, emu, &rom_data, &rom_len);
    }
    exchange_savestate(sfd, emu, can_compress, &savestate_data, &savestate_len);

    // --- LINK BACKGROUND EMULATOR ---

    emulator_options_t opts = { .mode = mode };
    if (rom_data) {
        *linked_emu = emulator_init(rom_data, rom_len, &opts);
        if (!*linked_emu) {
            eprintf("received invalid or corrupted PKT_ROM\n");
            free(rom_data);
            return 0;
        }
        free(rom_data);
    } else {
        rom_data = emulator_get_rom(emu, &rom_len);
        *linked_emu = emulator_init(rom_data, rom_len, &opts);
    }

    // FIXME this function fails (received invalid or corrupted savestate)
    if (!emulator_load_savestate(*linked_emu, savestate_data, savestate_len)) {
        eprintf("received invalid or corrupted savestate\n");
        return 0;
    }

    emulator_link_connect(emu, *linked_emu);

    free(savestate_data);
    return 1;
}

int link_exchange_joypad(int sfd, emulator_t *emu, emulator_t *linked_emu) {
    char buf[2];
    buf[0] = PKT_JOYPAD;
    buf[1] = emulator_get_joypad_state(emu);
    send(sfd, buf, 2, 0);

    do {
        if (!receive(sfd, buf, 2, 0)) {
            printf("Link cable disconnected\n");
            emulator_link_disconnect(emu);
            emulator_quit(linked_emu);
            close(sfd);
            return 0;
        }
        if (buf[0] != PKT_JOYPAD)
            eprintf("received packet type %d but expected %d (ignored)\n", buf[0], PKT_JOYPAD);
    } while (buf[0] != PKT_JOYPAD);

    emulator_set_joypad_state(linked_emu, buf[1]);
    return 1;
}
