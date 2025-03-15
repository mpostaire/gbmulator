#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "../../core/core.h"

typedef enum {
    PKT_INFO,
    PKT_ROM,
    PKT_STATE,
    PKT_JOYPAD
} pkt_type_t;

static int server_sfd = -1;

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

void link_cancel(void) {
    shutdown(server_sfd, SHUT_RD);
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

    server_sfd = -1;

    struct addrinfo *ai = res;
    for (; ai != NULL; ai = ai->ai_next) {
        if ((server_sfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol)) == -1) {
            errnoprintf("socket");
            continue;
        }

        int yes = 1;
        if (setsockopt(server_sfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
            errnoprintf("setsockopt");
            close(server_sfd);
            server_sfd = -1;
            return -1;
        };

        if (bind(server_sfd, ai->ai_addr, ai->ai_addrlen) == -1) {
            close(server_sfd);
            server_sfd = -1;
            continue;
        }

        break;
    }

    if (ai == NULL) {
        errnoprintf("bind");
        return -1;
    }

    freeaddrinfo(res);

    if (listen(server_sfd, 1) == -1) {
        errnoprintf("listen");
        close(server_sfd);
        server_sfd = -1;
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
    server_sfd = -1;
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

    for (; res != NULL; res = res->ai_next) {
        if ((server_sfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1)
            continue;

        if (connect(server_sfd, res->ai_addr, res->ai_addrlen) == -1) {
            close(server_sfd);
            server_sfd = -1;
        } else {
            break;
        }
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

static int exchange_info(int sfd, gb_t *gb, gb_mode_t *mode, int *can_compress, int *is_cable_link, int *is_ir_link) {
    // --- SEND PKT_INFO ---

    uint16_t checksum = gb_get_cartridge_checksum(gb);

    uint8_t pkt[4] = { 0 };
    pkt[0] = PKT_INFO;
    pkt[1] = gb_is_cgb(gb);
    SET_BIT(pkt[1], 1); // cable-link
    SET_BIT(pkt[1], 2); // ir-link
    SET_BIT(pkt[1], 7); // compress
    memcpy(&pkt[2], &checksum, 2);

    send(sfd, pkt, 4, 0);

    // --- RECEIVE PKT_INFO ---

    receive(sfd, pkt, 4, 0);

    if (pkt[0] != PKT_INFO) {
        eprintf("received packet type %d but expected %d (ignored)\n", pkt[0], PKT_INFO);
        return -1;
    }

    *mode = CHECK_BIT(pkt[1], 0) ? GB_MODE_CGB : GB_MODE_DMG;
    *is_cable_link = CHECK_BIT(pkt[1], 1); // cable-link
    *is_ir_link = CHECK_BIT(pkt[1], 2); // ir-link
    *can_compress = GET_BIT(pkt[1], 7); // compress

    uint16_t received_checksum = 0;
    memcpy(&received_checksum, &pkt[2], 2);
    if (received_checksum == checksum) {
        return 1;
    } else {
        printf("checksum mismatch ('%x' != '%x'): exchanging ROMs\n", checksum, received_checksum);
        return 0;
    }
}

static int exchange_rom(int sfd, gb_t *gb, uint8_t **other_rom, size_t *rom_len) {
    // --- SEND PKT_ROM ---

    // TODO compression
    uint8_t *this_rom = gb_get_rom(gb, rom_len);

    uint8_t *pkt = xcalloc(1, *rom_len + 9);
    pkt[0] = PKT_ROM;
    memcpy(&pkt[1], rom_len, sizeof(size_t));
    memcpy(&pkt[9], this_rom, *rom_len); // causes segfault

    send(sfd, pkt, *rom_len + 9, 0);
    free(pkt);

    // --- RECEIVE PKT_ROM ---

    // TODO compression
    uint8_t pkt_header[9] = { 0 };
    receive(sfd, pkt_header, 9, 0);

    if (pkt_header[0] != PKT_ROM) {
        eprintf("received packet type %d but expected %d (ignored)\n", pkt_header[0], PKT_ROM);
        return 0;
    }

    memcpy(rom_len, &pkt_header[1], sizeof(size_t));
    *other_rom = xcalloc(1, *rom_len);

    receive(sfd, *other_rom, *rom_len, 0);

    return 1;
}

static int exchange_savestate(int sfd, gb_t *gb, int can_compress, uint8_t **savestate_data, size_t *savestate_len) {
    // --- SEND PKT_STATE ---

    uint8_t *local_savestate_data = gb_get_savestate(gb, savestate_len, can_compress);

    uint8_t *pkt = xcalloc(1, *savestate_len + 9);
    pkt[0] = PKT_STATE;
    memcpy(&pkt[1], savestate_len, sizeof(size_t));
    memcpy(&pkt[9], local_savestate_data, *savestate_len);

    send(sfd, pkt, *savestate_len + 9, 0);
    free(pkt);
    free(local_savestate_data);

    // --- RECEIVE PKT_STATE ---

    uint8_t pkt_header[9] = { 0 };
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

int link_init_transfer(int sfd, gb_t *gb, gb_t **linked_gb) {
    // TODO connection lost detection (return -1)
    *linked_gb = NULL;
    gb_mode_t mode = 0;
    int can_compress = 0;
    int is_cable_link = 0;
    int is_ir_link = 0;
    size_t rom_len;
    uint8_t *rom = NULL;
    size_t savestate_len;
    uint8_t *savestate_data = NULL;

    // TODO handle wrong packet type received
    int ret = exchange_info(sfd, gb, &mode, &can_compress, &is_cable_link, &is_ir_link);
    if (ret == 0)
        exchange_rom(sfd, gb, &rom, &rom_len);
    exchange_savestate(sfd, gb, can_compress, &savestate_data, &savestate_len);

    // --- LINK BACKGROUND EMULATOR ---

    gb_options_t opts = { .mode = mode };
    if (rom) {
        *linked_gb = gb_init(rom, rom_len, &opts);
        if (!*linked_gb) {
            eprintf("received invalid or corrupted PKT_ROM\n");
            free(rom);
            close(sfd);
            return 0;
        }
        free(rom);
    } else {
        rom = gb_get_rom(gb, &rom_len);
        *linked_gb = gb_init(rom, rom_len, &opts);
    }

    if (!gb_load_savestate(*linked_gb, savestate_data, savestate_len)) {
        eprintf("received invalid or corrupted savestate\n");
        close(sfd);
        return 0;
    }

    if (is_cable_link)
        gb_link_connect_gb(gb, *linked_gb);
    if (is_ir_link)
        gb_ir_connect(gb, *linked_gb);

    free(savestate_data);
    return 1;
}

int link_exchange_joypad(int sfd, gb_t *gb, gb_t *linked_gb) {
    char buf[2];
    buf[0] = PKT_JOYPAD;
    buf[1] = gb_get_joypad_state(gb);
    send(sfd, buf, 2, 0);

    do {
        if (!receive(sfd, buf, 2, 0)) {
            printf("Link cable disconnected\n");
            gb_link_disconnect(gb);
            gb_quit(linked_gb);
            close(sfd);
            return 0;
        }
        if (buf[0] != PKT_JOYPAD)
            eprintf("received packet type %d but expected %d (ignored)\n", buf[0], PKT_JOYPAD);
    } while (buf[0] != PKT_JOYPAD);

    gb_set_joypad_state(linked_gb, buf[1]);
    return 1;
}
