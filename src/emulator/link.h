#pragma once

void link_init(void);

int link_start_server(const int port);

int link_connect_to_server(const char* address, const int port);

void link_close_connection(void);

void link_step(int cycles);
