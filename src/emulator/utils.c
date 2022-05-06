#include <stdlib.h>

#include "utils.h"

void *xmalloc(size_t size) {
    char *ptr;
    if (!(ptr = malloc(size))) {
        errnoprintf("malloc");
        exit(EXIT_FAILURE);
    }
    return ptr;
}
