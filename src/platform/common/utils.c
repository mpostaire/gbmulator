#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

#include "../../core/gb.h"
#include "config.h"

static long fsize(FILE *f) {
    if (fseek(f, 0, SEEK_END) < 0) {
        errnoprintf("fseek");
        return -1;
    }
    long len = ftell(f);
    if (len < 0) {
        errnoprintf("ftell");
        return -1;
    }
    fseek(f, 0, SEEK_SET);
    return len;
}

int keycode_to_joypad(config_t *config, unsigned int keycode) {
    if (keycode == config->keybindings[JOYPAD_RIGHT]) return JOYPAD_RIGHT;
    if (keycode == config->keybindings[JOYPAD_LEFT]) return JOYPAD_LEFT;
    if (keycode == config->keybindings[JOYPAD_UP]) return JOYPAD_UP;
    if (keycode == config->keybindings[JOYPAD_DOWN]) return JOYPAD_DOWN;
    if (keycode == config->keybindings[JOYPAD_A]) return JOYPAD_A;
    if (keycode == config->keybindings[JOYPAD_B]) return JOYPAD_B;
    if (keycode == config->keybindings[JOYPAD_SELECT]) return JOYPAD_SELECT;
    if (keycode == config->keybindings[JOYPAD_START]) return JOYPAD_START;
    return -1;
}

int button_to_joypad(config_t *config, unsigned int button) {
    if (button == config->gamepad_bindings[JOYPAD_RIGHT]) return JOYPAD_RIGHT;
    if (button == config->gamepad_bindings[JOYPAD_LEFT]) return JOYPAD_LEFT;
    if (button == config->gamepad_bindings[JOYPAD_UP]) return JOYPAD_UP;
    if (button == config->gamepad_bindings[JOYPAD_DOWN]) return JOYPAD_DOWN;
    if (button == config->gamepad_bindings[JOYPAD_A]) return JOYPAD_A;
    if (button == config->gamepad_bindings[JOYPAD_B]) return JOYPAD_B;
    if (button == config->gamepad_bindings[JOYPAD_SELECT]) return JOYPAD_SELECT;
    if (button == config->gamepad_bindings[JOYPAD_START]) return JOYPAD_START;
    return -1;
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

void save_battery_to_file(gb_t *gb, const char *path) {
    size_t save_length;
    byte_t *save_data = gb_get_save(gb, &save_length);
    if (!save_data) return;

    make_parent_dirs(path);

    FILE *f = fopen(path, "w");
    if (!f) {
        errnoprintf("error opening save file");
        return;
    }
    fwrite(save_data, save_length, 1, f);
    fclose(f);
    free(save_data);
}

void load_battery_from_file(gb_t *gb, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return;

    long save_length = fsize(f);
    if (save_length < 0)
        return;

    byte_t *save_data = xmalloc(save_length);
    fread(save_data, save_length, 1, f);
    gb_load_save(gb, save_data, save_length);
    fclose(f);
    free(save_data);
}

int save_state_to_file(gb_t *gb, const char *path, int compressed) {
    make_parent_dirs(path);

    size_t len;
    byte_t *buf = gb_get_savestate(gb, &len, compressed);

    FILE *f = fopen(path, "wb");
    if (!f) {
        errnoprintf("opening %s", path);
        return 0;
    }

    if (!fwrite(buf, len, 1, f)) {
        eprintf("writing savestate to %s\n", path);
        fclose(f);
        free(buf);
        return 0;
    }

    fclose(f);
    free(buf);
    return 1;
}

int load_state_from_file(gb_t *gb, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        errnoprintf("opening %s", path);
        return 0;
    }

    long len = fsize(f);
    if (len < 0)
        return 0;

    byte_t *buf = xmalloc(len);
    if (!fread(buf, len, 1, f)) {
        errnoprintf("reading savestate from %s", path);
        fclose(f);
        free(buf);
        return 0;
    }

    int ret = gb_load_savestate(gb, buf, len);
    fclose(f);
    free(buf);
    return ret;
}
