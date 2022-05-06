#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>

#include "config.h"
#include "emulator/emulator.h"

struct config config = {
    .scale = 3,
    .speed = 1.0f,
    .link_host = "127.0.0.1",
    .link_port = 7777,

    .left = SDLK_LEFT,
    .right = SDLK_RIGHT,
    .up = SDLK_UP,
    .down = SDLK_DOWN,
    .a = SDLK_KP_0,
    .b = SDLK_KP_PERIOD,
    .start = SDLK_KP_1,
    .select = SDLK_KP_2
};

/**
 * @returns 1 if directory_path is a directory, 0 otherwise.
 */
static int dir_exists(const char *directory_path) {
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

const char *config_load(void) {
    char *xdg_config_home = getenv("XDG_CONFIG_HOME");
    char *prefix = xdg_config_home;
    char *home = getenv("HOME");
    char default_prefix[strlen(home) + 10];

    if (!prefix) {
        snprintf(default_prefix, strlen(home) + 9, "%s%s", home, "/.config");
        prefix = default_prefix;
    }

    char *config_path = xmalloc(strlen(prefix) + 27);
    snprintf(config_path, strlen(prefix) + 26, "%s%s", prefix, "/gbmulator/gbmulator.conf");

    FILE *f = fopen(config_path, "r");
    if (f) {
        printf("Loading config from %s\n", config_path);

        byte_t scale, color_palette;
        float speed, sound;
        char link_host[40];
        int link_port;

        char left[16], right[16], up[16], down[16], a[16], b[16], start[16], select[16];

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
                }
            } else if (sscanf(buf, "sound=%f", &sound)) {
                if (sound == 0.0f || sound == 0.25f || sound == 0.5f ||
                    sound == 0.75f || sound == 1.0f) {

                    config.sound = sound;
                    emulator_set_apu_sound_level(sound);
                }
            } else if (sscanf(buf, "color_palette=%hhu", &color_palette)) {
                emulator_set_ppu_color_palette(color_palette);
            } else if (sscanf(buf, "link_host=%39s", link_host)) {
                strncpy(config.link_host, link_host, 40);
            } else if (sscanf(buf, "link_port=%d", &link_port)) {
                config.link_port = link_port;
            } else if (sscanf(buf, "left=%15[^\t\n]", left)) {
                config.left = SDL_GetKeyFromName(left);
            } else if (sscanf(buf, "right=%15[^\t\n]", right)) {
                config.right = SDL_GetKeyFromName(right);
            } else if (sscanf(buf, "up=%15[^\t\n]", up)) {
                config.up = SDL_GetKeyFromName(up);
            } else if (sscanf(buf, "down=%15[^\t\n]", down)) {
                config.down = SDL_GetKeyFromName(down);
            } else if (sscanf(buf, "a=%15[^\t\n]", a)) {
                config.a = SDL_GetKeyFromName(a);
            } else if (sscanf(buf, "b=%15[^\t\n]", b)) {
                config.b = SDL_GetKeyFromName(b);
            } else if (sscanf(buf, "start=%15[^\t\n]", start)) {
                config.start = SDL_GetKeyFromName(start);
            } else if (sscanf(buf, "select=%15[^\t\n]", select)) {
                config.select = SDL_GetKeyFromName(select);
            }
        }

        fclose (f);
    }

    return config_path;
}

void config_save(const char* config_path) {
    char *last_slash = strrchr(config_path, '/');
    int last_slash_index = (int) (last_slash - config_path);

    char directory_path[last_slash_index + 1];
    snprintf(directory_path, last_slash_index + 1, "%s", config_path);

    if (!dir_exists(directory_path))
        mkdirp(directory_path);

    FILE *f = fopen(config_path, "w");
    if (!f) {
        errnoprintf("opening file");
        return;
    }

    fprintf(f,
        "scale=%d\nspeed=%.1f\nsound=%.2f\ncolor_palette=%d\nlink_host=%s\nlink_port=%d\n" \
        "left=%s\nright=%s\nup=%s\ndown=%s\na=%s\nb=%s\nstart=%s\nselect=%s\n",
        config.scale,
        config.speed,
        config.sound,
        emulator_get_ppu_color_palette(),
        config.link_host,
        config.link_port,
        SDL_GetKeyName(config.left),
        SDL_GetKeyName(config.right),
        SDL_GetKeyName(config.up),
        SDL_GetKeyName(config.down),
        SDL_GetKeyName(config.a),
        SDL_GetKeyName(config.b),
        SDL_GetKeyName(config.start),
        SDL_GetKeyName(config.select)
    );

    printf("Saving config to %s\n", config_path);

    fclose(f);
}
