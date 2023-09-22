#include <stdlib.h>
#define __USE_XOPEN
#include <time.h>

#include "utils.h"

void *xmalloc(size_t size) {
    void *ptr;
    if (!(ptr = malloc(size))) {
        errnoprintf("malloc");
        exit(EXIT_FAILURE);
    }
    return ptr;
}

void *xcalloc(size_t nmemb, size_t size) {
    void *ptr;
    if (!(ptr = calloc(nmemb, size))) {
        errnoprintf("calloc");
        exit(EXIT_FAILURE);
    }
    return ptr;
}

void *xrealloc(void *ptr, size_t size) {
    void *new_ptr;
    if (!(new_ptr = realloc(ptr, size))) {
        errnoprintf("realloc");
        exit(EXIT_FAILURE);
    }
    return new_ptr;
}

size_t time_to_string_length(void) {
    return 20;
}

char *time_to_string(time_t t, size_t *len) {
    struct tm *tm = localtime(&t);
    char *buf = xmalloc(time_to_string_length());
    size_t ret = strftime(buf, time_to_string_length(), "%Y-%m-%d %T", tm);
    if (len)
        *len = ret + 1; // include the null terminating character
    return buf;
}

time_t string_to_time(const char *time_str) {
    struct tm tm = { 0 };
    tm.tm_isdst = -1;
    strptime(time_str, "%Y-%m-%d %T", &tm);
    return mktime(&tm);
}
