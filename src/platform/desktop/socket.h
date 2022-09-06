#pragma once

#include <sys/socket.h>

static inline ssize_t send_pkt(int fd, const void *buf, size_t n, int flags) {
    return send(fd, buf, n, flags);
}

static inline ssize_t recv_pkt(int fd, void *buf, size_t n, int flags) {
    return recv(fd, buf, n, flags);
}
