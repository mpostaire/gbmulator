#include <stdlib.h>

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
