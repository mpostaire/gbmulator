#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

#include "../../core/core.h"
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

joypad_button_t keycode_to_joypad(config_t *config, unsigned int keycode) {
    if (keycode == config->keybindings[JOYPAD_A]) return JOYPAD_A;
    if (keycode == config->keybindings[JOYPAD_B]) return JOYPAD_B;
    if (keycode == config->keybindings[JOYPAD_SELECT]) return JOYPAD_SELECT;
    if (keycode == config->keybindings[JOYPAD_START]) return JOYPAD_START;
    if (keycode == config->keybindings[JOYPAD_RIGHT]) return JOYPAD_RIGHT;
    if (keycode == config->keybindings[JOYPAD_LEFT]) return JOYPAD_LEFT;
    if (keycode == config->keybindings[JOYPAD_UP]) return JOYPAD_UP;
    if (keycode == config->keybindings[JOYPAD_DOWN]) return JOYPAD_DOWN;
    return -1;
}

joypad_button_t button_to_joypad(config_t *config, unsigned int button) {
    if (button == config->gamepad_bindings[JOYPAD_A]) return JOYPAD_A;
    if (button == config->gamepad_bindings[JOYPAD_B]) return JOYPAD_B;
    if (button == config->gamepad_bindings[JOYPAD_SELECT]) return JOYPAD_SELECT;
    if (button == config->gamepad_bindings[JOYPAD_START]) return JOYPAD_START;
    if (button == config->gamepad_bindings[JOYPAD_RIGHT]) return JOYPAD_RIGHT;
    if (button == config->gamepad_bindings[JOYPAD_LEFT]) return JOYPAD_LEFT;
    if (button == config->gamepad_bindings[JOYPAD_UP]) return JOYPAD_UP;
    if (button == config->gamepad_bindings[JOYPAD_DOWN]) return JOYPAD_DOWN;
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

void save_battery_to_file(gbmulator_t *emu, const char *path) {
    size_t save_length;
    uint8_t *save_data = gbmulator_get_save(emu, &save_length);
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

void load_battery_from_file(gbmulator_t *emu, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return;

    long save_length = fsize(f);
    if (save_length < 0) {
        fclose(f);
        return;
    }

    uint8_t *save_data = xmalloc(save_length);
    fread(save_data, save_length, 1, f);
    gbmulator_load_save(emu, save_data, save_length);
    fclose(f);
    free(save_data);
}

int save_state_to_file(gbmulator_t *emu, const char *path, int compressed) {
    make_parent_dirs(path);

    size_t len;
    uint8_t *buf = gbmulator_get_savestate(emu, &len, compressed);

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

int load_state_from_file(gbmulator_t *emu, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        errnoprintf("opening %s", path);
        return 0;
    }

    long len = fsize(f);
    if (len < 0)
    {
        fclose(f);
        return 0;
    }

    uint8_t *buf = xmalloc(len);
    if (!fread(buf, len, 1, f)) {
        errnoprintf("reading savestate from %s", path);
        fclose(f);
        free(buf);
        return 0;
    }

    int ret = gbmulator_load_savestate(emu, buf, len);
    fclose(f);
    free(buf);
    return ret;
}

uint8_t *get_rom(const char *path, size_t *rom_size) {
    const char *dot = strrchr(path, '.');
    if (!dot ||
        (strncmp(dot, ".gb", MAX(strlen(dot), sizeof(".gb"))) &&
        strncmp(dot, ".gbc", MAX(strlen(dot), sizeof(".gbc")) &&
        strncmp(dot, ".gba", MAX(strlen(dot), sizeof(".gba")))))
    ) {
        eprintf("%s: wrong file extension (expected .gb, .gbc or .gba)\n", path);
        return NULL;
    }

    FILE *f = fopen(path, "rb");
    if (!f) {
        errnoprintf("opening file %s", path);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    size_t len = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t *rom = xmalloc(len);
    if (!fread(rom, len, 1, f)) {
        errnoprintf("reading %s", path);
        fclose(f);
        return NULL;
    }
    fclose(f);

    // TODO
    // if (!gb_is_rom_valid(rom)) {
    //     eprintf("%s: invalid or unsupported rom\n", path);
    //     free(rom);
    //     return NULL;
    // }

    if (rom_size)
        *rom_size = len;
    return rom;
}

char *get_xdg_path(const char *xdg_variable, const char *fallback) {
    char *xdg = getenv(xdg_variable);
    if (xdg) return xdg;

    char *home = getenv("HOME");
    size_t home_len = strlen(home);
    size_t fallback_len = strlen(fallback);
    char *buf = xmalloc(home_len + fallback_len + 3);
    snprintf(buf, home_len + fallback_len + 2, "%s/%s", home, fallback);
    return buf;
}

char *get_config_path(void) {
    char *xdg_config = get_xdg_path("XDG_CONFIG_HOME", ".config");

    char *path = xmalloc(strlen(xdg_config) + 27);
    snprintf(path, strlen(xdg_config) + 26, "%s%s", xdg_config, "/gbmulator/gbmulator.conf");

    free(xdg_config);
    return path;
}

char *get_save_path(const char *rom_filepath) {
    char *xdg_data = get_xdg_path("XDG_DATA_HOME", ".local/share");

    char *last_slash = strrchr(rom_filepath, '/');
    char *last_period = strrchr(last_slash ? last_slash : rom_filepath, '.');
    int last_period_index = last_period ? (int) (last_period - last_slash) : (int) strlen(rom_filepath);

    size_t len = strlen(xdg_data) + strlen(last_slash ? last_slash : rom_filepath);
    char *save_path = xmalloc(len + 13);
    snprintf(save_path, len + 12, "%s/gbmulator%.*s.sav", xdg_data, last_period_index, last_slash);

    free(xdg_data);
    return save_path;
}

char *get_savestate_path(const char *rom_filepath, int slot) {
    char *xdg_data = get_xdg_path("XDG_DATA_HOME", ".local/share");

    char *last_slash = strrchr(rom_filepath, '/');
    char *last_period = strrchr(last_slash ? last_slash : rom_filepath, '.');
    int last_period_index = last_period ? (int) (last_period - last_slash) : (int) strlen(rom_filepath);

    size_t len = strlen(xdg_data) + strlen(last_slash);
    char *save_path = xmalloc(len + 33);
    snprintf(save_path, len + 32, "%s/gbmulator/savestates%.*s-%d.gbstate", xdg_data, last_period_index, last_slash, slot);

    free(xdg_data);
    return save_path;
}

void fit_image(uint8_t *dst, const uint8_t *src, int src_width, int src_height, int row_stride, int rotation) {
    // crop top and bottom of largest dimension to get a 1:1 aspect ratio (adjust scale_x or scale_y accordingly)
    int src_start_x = 0, src_start_y = 0;
    int src_dim_diff = src_width - src_height;
    if (src_dim_diff < 0) {
        src_height = src_width - (src_dim_diff / 2);
        src_start_y = src_dim_diff / 2;
    } else if (src_dim_diff > 0) {
        src_width = src_height - (src_dim_diff / 2);
        src_start_x = src_dim_diff / 2;
    }

    // scale and rotate
    int dst_width = GB_CAMERA_SENSOR_WIDTH;
    int dst_height = GB_CAMERA_SENSOR_HEIGHT;

    float scale_x = (float) src_width / (float) dst_width;
    float scale_y = (float) src_height / (float) dst_height;

    for (int y = 0; y < dst_height; y++) {
        for (int x = 0; x < dst_width; x++) {
            int src_index = ((int) ((y + src_start_y) * scale_y) * row_stride) + (int) ((x + src_start_x) * scale_x);

            if (rotation == 0)
                dst[y * dst_width + x] = src[src_index];
            else if (rotation == 90)
                dst[x * dst_height + ((dst_height - 1) - y)] = src[src_index];
            else if (rotation == 180)
                dst[((dst_height - 1) - y) * dst_width + ((dst_width - 1) - x)] = src[src_index];
            else // if (rotation == 270)
                dst[((dst_width - 1) - x) * dst_height + y] = src[src_index];
        }
    }
}
