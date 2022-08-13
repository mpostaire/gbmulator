#include <stdlib.h>
#define __USE_XOPEN
#include <time.h>

#include "utils.h"

void *xmalloc(size_t size) {
    char *ptr;
    if (!(ptr = malloc(size))) {
        errnoprintf("malloc");
        exit(EXIT_FAILURE);
    }
    return ptr;
}

void *xcalloc(size_t nmemb, size_t size) {
    char *ptr;
    if (!(ptr = calloc(nmemb, size))) {
        errnoprintf("calloc");
        exit(EXIT_FAILURE);
    }
    return ptr;
}

void *xrealloc(void *ptr, size_t size) {
    char *new_ptr;
    if (!(new_ptr = realloc(ptr, size))) {
        errnoprintf("malloc");
        exit(EXIT_FAILURE);
    }
    return new_ptr;
}

char *time_to_string(time_t t, size_t *len) {
    struct tm *tm = gmtime(&t);
    char *buf = xmalloc(256);
    size_t ret = strftime(buf, 256, "%Y-%m-%d %T", tm);
    if (len)
        *len = ret + 1; // include the null terminating character
    return buf;
}

time_t string_to_time(const char *time_str) {
    struct tm tm;
    strptime(time_str, "%Y-%m-%d %T", &tm);
    return mktime(&tm);
}
