#include <stdlib.h>
#include <errno.h>

#include "utils.h"

void *xmalloc(size_t size) {
    void *ptr;
    if (!(ptr = malloc(size))) {
        LOG_ERROR("malloc: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    return ptr;
}

void *xcalloc(size_t nmemb, size_t size) {
    void *ptr;
    if (!(ptr = calloc(nmemb, size))) {
        LOG_ERROR("calloc: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    return ptr;
}

void *xrealloc(void *ptr, size_t size) {
    void *new_ptr;
    if (!(new_ptr = realloc(ptr, size))) {
        LOG_ERROR("realloc: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    return new_ptr;
}
