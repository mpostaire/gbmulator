#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

#include "utils.h"

void *xmalloc(size_t size) {
    char *ptr;
    if (!(ptr = malloc(size))) {
        errnoprintf("malloc");
        exit(EXIT_FAILURE);
    }
    return ptr;
}

int dir_exists(const char *directory_path) {
    DIR *dir = opendir(directory_path);
	if (dir == NULL) {
		if (errno == ENOENT)
			return 0;
		errnoprintf("opendir");
        exit(EXIT_FAILURE);
	}
	closedir(dir);
	return 1;
}

void mkdirp(const char *directory_path) {
    char buf[256];
    snprintf(buf, sizeof(buf), "%s", directory_path);
    size_t len = strlen(buf);

    if (buf[len - 1] == '/')
        buf[len - 1] = 0;

    for (char *p = buf + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (mkdir(buf, S_IRWXU | S_IRGRP | S_IROTH) && errno != EEXIST) {
                errnoprintf("mkdir");
                exit(EXIT_FAILURE);
            }
            *p = '/';
        }
    }

    if (mkdir(buf, S_IRWXU | S_IRGRP | S_IROTH) && errno != EEXIST) {
        errnoprintf("mkdir");
        exit(EXIT_FAILURE);
    }
}

void make_parent_dirs(const char *filepath) {
    char *last_slash = strrchr(filepath, '/');
    int last_slash_index = last_slash ? (int) (last_slash - filepath) : -1;

    if (last_slash_index != -1) {
        char directory_path[last_slash_index + 1];
        snprintf(directory_path, last_slash_index + 1, "%s", filepath);

        if (!dir_exists(directory_path))
            mkdirp(directory_path);
    }
}
