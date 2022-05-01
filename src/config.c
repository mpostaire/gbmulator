#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>

#include "config.h"
#include "utils.h"
#include "emulator/emulator.h"

struct config config = {
    .scale = 3,
    .speed = 1.0f,
    .link_host = "127.0.0.1",
    .link_port = 7777
};

/**
 * @returns 1 if directory_path is a directory, 0 otherwise.
 */
static int dir_exists(const char *directory_path) {
    DIR *dir = opendir(directory_path);
	if (dir == NULL) {
		if (errno == ENOENT)
			return 0;
		perror("ERROR: directory_exists");
        exit(EXIT_FAILURE);
	}
	closedir(dir);
	return 1;
}

/**
 * Creates directory_path and its parents if they don't exist.
 */
static void mkdirp(const char *directory_path) {
    char buf[256];
    snprintf(buf, sizeof(buf), "%s", directory_path);
    size_t len = strlen(buf);

    if (buf[len - 1] == '/')
        buf[len - 1] = 0;

    for (char *p = buf + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (mkdir(buf, S_IRWXU | S_IRGRP | S_IROTH) && errno != EEXIST) {
                perror("ERROR: mkdirp");
                exit(EXIT_FAILURE);
            }
            *p = '/';
        }
    }

    if (mkdir(buf, S_IRWXU | S_IRGRP | S_IROTH) && errno != EEXIST) {
        perror("ERROR: mkdirp");
        exit(EXIT_FAILURE);
    }
}

const char *load_config(void) {
    char *xdg_config_home = getenv("XDG_CONFIG_HOME");
    char *prefix = xdg_config_home;
    char *home = getenv("HOME");
    char default_prefix[strlen(home) + 10];

    if (!prefix) {
        snprintf(default_prefix, strlen(home) + 9, "%s%s", home, "/.config");
        prefix = default_prefix;
    }

    char *config_path = malloc(sizeof(char) * (strlen(prefix) + 27));
    snprintf(config_path, strlen(prefix) + 26, "%s%s", prefix, "/gbmulator/gbmulator.conf");

    FILE *f = fopen(config_path, "r");
    if (f) {
        printf("Loading config from %s\n", config_path);

        byte_t scale;
        byte_t color_palette;
        float speed;
        float sound;
        char link_host[40];
        int link_port;

        char buf[64];
        while (fgets(buf, 64, f)) {
            if (sscanf(buf, "scale=%hhu", &scale)) {
                if (scale >= 1 && scale <= 5)
                    config.scale = scale;
            } else if (sscanf(buf, "speed=%f", &speed)) {
                if (speed == 1.0f || speed == 1.5f || speed == 2.0f ||
                    speed == 2.5f || speed == 3.0f || speed == 3.5f ||
                    speed == 4.0f) {

                    config.speed = speed;
                    apu_set_sampling_speed_multiplier(speed);
                }
            } else if (sscanf(buf, "sound=%f", &sound)) {
                if (sound == 0.0f || sound == 0.25f || sound == 0.5f ||
                    sound == 0.75f || sound == 1.0f) {

                    apu_set_global_sound_level(sound);
                }
            } else if (sscanf(buf, "color_palette=%hhu", &color_palette)) {
                ppu_set_color_palette(color_palette);
            } else if (sscanf(buf, "link_host=%39s", link_host)) {
                strncpy(config.link_host, link_host, 40);
            } else if (sscanf(buf, "link_port=%d", &link_port)) {
                config.link_port = link_port;
            }
        }

        fclose (f);
    }

    return config_path;
}

void save_config(const char* config_path) {
    char *last_slash = strrchr(config_path, '/');
    int last_slash_index = (int) (last_slash - config_path);

    char directory_path[last_slash_index + 1];
    snprintf(directory_path, last_slash_index + 1, "%s", config_path);

    if (!dir_exists(directory_path))
        mkdirp(directory_path);

    FILE *f = fopen(config_path, "w");
    if (!f) {
        perror("ERROR: save_config");
        return;
    }

    fprintf(f, "scale=%d\nspeed=%.1f\nsound=%.2f\ncolor_palette=%d\nlink_host=%s\nlink_port=%d\n",
        config.scale,
        config.speed,
        apu_get_global_sound_level(),
        ppu_get_color_palette(),
        config.link_host,
        config.link_port
    );

    printf("Saving config to %s\n", config_path);

    fclose(f);
}
