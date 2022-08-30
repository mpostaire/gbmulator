#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#ifdef __ANDROID__
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#endif

#include "emulator/emulator.h"
#include "config.h"

int sdl_key_to_joypad(SDL_Keycode key) {
    if (key == config.keybindings[JOYPAD_RIGHT]) return JOYPAD_RIGHT;
    if (key == config.keybindings[JOYPAD_LEFT]) return JOYPAD_LEFT;
    if (key == config.keybindings[JOYPAD_UP]) return JOYPAD_UP;
    if (key == config.keybindings[JOYPAD_DOWN]) return JOYPAD_DOWN;
    if (key == config.keybindings[JOYPAD_A]) return JOYPAD_A;
    if (key == config.keybindings[JOYPAD_B]) return JOYPAD_B;
    if (key == config.keybindings[JOYPAD_SELECT]) return JOYPAD_SELECT;
    if (key == config.keybindings[JOYPAD_START]) return JOYPAD_START;
    return key;
}

int sdl_controller_to_joypad(SDL_GameControllerButton button) {
    switch (button) {
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return JOYPAD_RIGHT;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return JOYPAD_LEFT;
    case SDL_CONTROLLER_BUTTON_DPAD_UP: return JOYPAD_UP;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return JOYPAD_DOWN;
    case SDL_CONTROLLER_BUTTON_A: return JOYPAD_A;
    case SDL_CONTROLLER_BUTTON_B: return JOYPAD_B;
    case SDL_CONTROLLER_BUTTON_BACK:
    case SDL_CONTROLLER_BUTTON_Y:
        return JOYPAD_SELECT;
    case SDL_CONTROLLER_BUTTON_START:
    case SDL_CONTROLLER_BUTTON_X:
        return JOYPAD_START;
    default:
        return SDL_CONTROLLER_BUTTON_INVALID;
    }
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

void save_battery_to_file(emulator_t *emu, const char *path) {
    size_t save_length;
    byte_t *save_data = emulator_get_save(emu, &save_length);
    if (!save_data) return;

    make_parent_dirs(path);

    SDL_RWops *f = SDL_RWFromFile(path, "w");
    if (!f) {
        errnoprintf("error opening save file");
        return;
    }
    SDL_RWwrite(f, save_data, save_length, 1);
    SDL_RWclose(f);
    free(save_data);
}

void load_battery_from_file(emulator_t *emu, const char *path) {
    SDL_RWops *f = SDL_RWFromFile(path, "r");
    if (!f) return;

    size_t save_length = SDL_RWsize(f);
    byte_t *save_data = xmalloc(save_length);
    SDL_RWread(f, save_data, save_length, 1);
    emulator_load_save(emu, save_data, save_length);
    SDL_RWclose(f);
    free(save_data);
}

int save_state_to_file(emulator_t *emu, const char *path, int compressed) {
    make_parent_dirs(path);

    size_t len;
    byte_t *buf = emulator_get_savestate(emu, &len, compressed);

    SDL_RWops *f = SDL_RWFromFile(path, "wb");
    if (!f) {
        errnoprintf("opening %s", path);
        return 0;
    }

    if (!SDL_RWwrite(f, buf, len, 1)) {
        eprintf("writing savestate to %s\n", path);
        SDL_RWclose(f);
        free(buf);
        return 0;
    }

    SDL_RWclose(f);
    free(buf);
    return 1;
}

int load_state_from_file(emulator_t *emu, const char *path) {
    SDL_RWops *f = SDL_RWFromFile(path, "rb");
    if (!f) {
        errnoprintf("opening %s", path);
        return 0;
    }

    size_t len = SDL_RWsize(f);

    byte_t *buf = xmalloc(len);
    if (!SDL_RWread(f, buf, len, 1)) {
        errnoprintf("reading savestate from %s", path);
        SDL_RWclose(f);
        free(buf);
        return 0;
    }

    int ret = emulator_load_savestate(emu, buf, len);
    SDL_RWclose(f);
    free(buf);
    return ret;
}
