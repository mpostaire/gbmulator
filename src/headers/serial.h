#pragma once

int serial_start_server(const int port);

int serial_connect_to_server(const char* address, const int port);

void serial_close_connection(void);

void serial_step(int cycles);
