#pragma once

#include <arpa/inet.h>

typedef struct {
    int cycles_counter;
    int bit_counter;
    int sfd; // server's socket used only by the server
    int is_server;
    int other_sfd;
    int connected;
    struct sockaddr_in other_addr;
} link_t;

extern link_t serial_link;

void link_init(void);

int emulator_link_start_server(const int port);

int emulator_link_connect_to_server(const char* address, const int port);

void link_close_connection(void);

void link_step(int cycles);
