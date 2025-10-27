#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
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
    char *last_slash       = strrchr(filepath, '/');
    int   last_slash_index = last_slash ? (int) (last_slash - filepath) : -1;

    if (last_slash_index != -1) {
        char directory_path[last_slash_index + 1];
        snprintf(directory_path, last_slash_index + 1, "%s", filepath);

        if (!dir_exists(directory_path))
            mkdirp(directory_path);
    }
}

uint8_t *read_file(const char *path, size_t *len) {
    if (!path || !len)
        return NULL;

    FILE *f = fopen(path, "rb");
    if (!f) {
        errnoprintf("fopen(%s)", path);
        return NULL;
    }

    long size = fsize(f);
    if (size < 0) {
        fclose(f);
        return NULL;
    }

    *len = size;

    uint8_t *buf = xmalloc(*len);

    if (!fread(buf, *len, 1, f)) {
        errnoprintf("fread(%s)", path);
        fclose(f);
        free(buf);
        return NULL;
    }

    fclose(f);

    return buf;
}

bool write_file(const char *path, const uint8_t *data, size_t len) {
    if (!path || !data)
        return false;

    make_parent_dirs(path);

    FILE *f = fopen(path, "wb");
    if (!f) {
        errnoprintf("open(%s)", path);
        return false;
    }

    if (!fwrite(data, len, 1, f)) {
        errnoprintf("fwrite(%s)", path);
        fclose(f);
        return false;
    }

    fflush(f);
    fsync(fileno(f));
    fclose(f);

    return true;
}

bool save_battery_to_file(gbmulator_t *emu, const char *path) {
    size_t   save_length;
    uint8_t *save_data = gbmulator_get_save(emu, &save_length);
    if (!save_data)
        return false;

    bool ret = write_file(path, save_data, save_length);
    free(save_data);
    return ret;
}

bool load_battery_from_file(gbmulator_t *emu, const char *path) {
    size_t   len;
    uint8_t *buf = read_file(path, &len);
    if (!buf)
        return false;

    bool ret = gbmulator_load_save(emu, buf, len);
    free(buf);
    return ret;
}

bool save_state_to_file(gbmulator_t *emu, const char *path, int compressed) {
    size_t   savestate_length;
    uint8_t *savestate_data = gbmulator_get_savestate(emu, &savestate_length, compressed);
    if (!savestate_data)
        return false;

    bool ret = write_file(path, savestate_data, savestate_length);
    free(savestate_data);
    return ret;
}

bool load_state_from_file(gbmulator_t *emu, const char *path) {
    size_t   len;
    uint8_t *buf = read_file(path, &len);
    if (!buf)
        return false;

    bool ret = gbmulator_load_savestate(emu, buf, len);
    free(buf);
    return ret;
}

uint8_t *read_rom(const char *path, size_t *rom_size) {
    if (!path || !rom_size)
        return NULL;

    const char *dot = strrchr(path, '.');
    if (!dot ||
        (strncmp(dot, ".gb", MAX(strlen(dot), sizeof(".gb"))) &&
         strncmp(dot, ".gbc", MAX(strlen(dot), sizeof(".gbc"))) &&
         strncmp(dot, ".gba", MAX(strlen(dot), sizeof(".gba"))))) {
        eprintf("%s: wrong file extension (expected .gb, .gbc or .gba)\n", path);
        return NULL;
    }

    // TODO
    // if (!gb_is_rom_valid(rom)) {
    //     eprintf("%s: invalid or unsupported rom\n", path);
    //     free(rom);
    //     return NULL;
    // }

    return read_file(path, rom_size);
}

static char *get_xdg_path(const char *xdg_variable, const char *fallback) {
    char *xdg = getenv(xdg_variable);
    if (xdg)
        return xdg;

    char *home = getenv("HOME");

    static char path[128];
    snprintf(path, sizeof(path), "%s/%s", home, fallback);

    return path;
}

char *get_config_dir(void) {
    static char path[192];
    char       *xdg_config = get_xdg_path("XDG_DATA_HOME", ".config");

    snprintf(path, sizeof(path), "%s/gbmulator", xdg_config);
    return path;
}

char *get_save_dir(void) {
    static char path[192];
    char       *xdg_config = get_xdg_path("XDG_DATA_HOME", ".local/share");

    snprintf(path, sizeof(path), "%s/gbmulator", xdg_config);
    return path;
}

char *get_savestate_dir(void) {
    static char path[192];
    char       *xdg_config = get_xdg_path("XDG_DATA_HOME", ".local/share");

    snprintf(path, sizeof(path), "%s/gbmulator/savestates", xdg_config);
    return path;
}

char *get_config_path(void) {
    char *config_dir = get_config_dir();

    static char path[256];
    snprintf(path, sizeof(path), "%s/gbmulator.conf", config_dir);
    return path;
}

char *get_save_path(char *rom_title) {
    char *save_dir = get_save_dir();

    static char path[256];
    snprintf(path, sizeof(path), "%s/%s.sav", save_dir, rom_title);
    return path;
}

char *get_savestate_path(char *rom_title, int slot) {
    char *savestate_dir = get_savestate_dir();

    static char path[256];
    snprintf(path, sizeof(path), "%s/%s-%d.gbstate", savestate_dir, rom_title, slot);
    return path;
}

void fit_image(uint8_t *dst, const uint8_t *src, int src_width, int src_height, int row_stride, int rotation) {
    // crop top and bottom of largest dimension to get a 1:1 aspect ratio (adjust scale_x or scale_y accordingly)
    int src_start_x = 0, src_start_y = 0;
    int src_dim_diff = src_width - src_height;
    if (src_dim_diff < 0) {
        src_height  = src_width - (src_dim_diff / 2);
        src_start_y = src_dim_diff / 2;
    } else if (src_dim_diff > 0) {
        src_width   = src_height - (src_dim_diff / 2);
        src_start_x = src_dim_diff / 2;
    }

    // scale and rotate
    int dst_width  = GB_CAMERA_SENSOR_WIDTH;
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
