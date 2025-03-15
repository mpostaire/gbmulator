#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "utils.h"
#include "../../core/gb.h"

static void parse_config_line(config_t *config, const char *line) {
    uint8_t scale, color_palette, sound_drc;
    float speed, sound;
    char link_host[INET6_ADDRSTRLEN], link_port[6];
    uint8_t mode;

    char key_name[16];

    unsigned int key;

    if (sscanf(line, "scale=%hhu", &scale)) {
        if (scale >= 1 && scale <= 5)
            config->scale = scale;
        return;
    }
    if (sscanf(line, "mode=%hhu", &mode)) {
        if (mode == GB_MODE_CGB || mode == GB_MODE_DMG)
            config->mode = mode;
        return;
    }
    if (sscanf(line, "speed=%f", &speed)) {
        if (speed >= 1.0f && speed <= 8.0f)
            config->speed = speed;
        return;
    }
    if (sscanf(line, "sound=%f", &sound)) {
        if (sound < 0.0f)
            config->sound = 0.0f;
        else if (sound > 1.0f)
            config->sound = 1.0f;
        else
            config->sound = sound;
        return;
    }
    if (sscanf(line, "sound_drc=%hhu", &sound_drc)) {
        if (sound_drc < 0.0f)
            config->sound_drc = 0.0f;
        else if (sound_drc > 1.0f)
            config->sound_drc = 1.0f;
        else
            config->sound_drc = sound_drc;
        return;
    }
    if (sscanf(line, "color_palette=%hhu", &color_palette)) {
        if (color_palette < PPU_COLOR_PALETTE_MAX)
            config->color_palette = color_palette;
        return;
    }
    if (sscanf(line, "link_host=%45s", link_host)) {
        strncpy(config->link_host, link_host, INET6_ADDRSTRLEN);
        return;
    }
    if (sscanf(line, "link_port=%5s", link_port)) {
        strncpy(config->link_port, link_port, 6);
        return;
    }

    if (config->keyname_parser) {
        if (sscanf(line, "keyboard_right=%15[^\t\n]", key_name)) {
            key = config->keyname_parser(key_name);
            if (config->keycode_filter(key))
                config->keybindings[JOYPAD_RIGHT] = key;
            return;
        }
        if (sscanf(line, "keyboard_left=%15[^\t\n]", key_name)) {
            key = config->keyname_parser(key_name);
            if (config->keycode_filter(key))
                config->keybindings[JOYPAD_LEFT] = key;
            return;
        }
        if (sscanf(line, "keyboard_up=%15[^\t\n]", key_name)) {
            key = config->keyname_parser(key_name);
            if (config->keycode_filter(key))
                config->keybindings[JOYPAD_UP] = key;
            return;
        }
        if (sscanf(line, "keyboard_down=%15[^\t\n]", key_name)) {
            key = config->keyname_parser(key_name);
            if (config->keycode_filter(key))
                config->keybindings[JOYPAD_DOWN] = key;
            return;
        }
        if (sscanf(line, "keyboard_a=%15[^\t\n]", key_name)) {
            key = config->keyname_parser(key_name);
            if (config->keycode_filter(key))
                config->keybindings[JOYPAD_A] = key;
            return;
        }
        if (sscanf(line, "keyboard_b=%15[^\t\n]", key_name)) {
            key = config->keyname_parser(key_name);
            if (config->keycode_filter(key))
                config->keybindings[JOYPAD_B] = key;
            return;
        }
        if (sscanf(line, "keyboard_select=%15[^\t\n]", key_name)) {
            key = config->keyname_parser(key_name);
            if (config->keycode_filter(key))
                config->keybindings[JOYPAD_SELECT] = key;
            return;
        }
        if (sscanf(line, "keyboard_start=%15[^\t\n]", key_name)) {
            key = config->keyname_parser(key_name);
            if (config->keycode_filter(key))
                config->keybindings[JOYPAD_START] = key;
            return;
        }
    }

    if (config->gamepad_button_parser) {
        if (sscanf(line, "gamepad_right=%15[^\t\n]", key_name)) {
            key = config->gamepad_button_name_parser(key_name);
            config->gamepad_bindings[JOYPAD_RIGHT] = key;
            return;
        }
        if (sscanf(line, "gamepad_left=%15[^\t\n]", key_name)) {
            key = config->gamepad_button_name_parser(key_name);
            config->gamepad_bindings[JOYPAD_LEFT] = key;
            return;
        }
        if (sscanf(line, "gamepad_up=%15[^\t\n]", key_name)) {
            key = config->gamepad_button_name_parser(key_name);
            config->gamepad_bindings[JOYPAD_UP] = key;
            return;
        }
        if (sscanf(line, "gamepad_down=%15[^\t\n]", key_name)) {
            key = config->gamepad_button_name_parser(key_name);
            config->gamepad_bindings[JOYPAD_DOWN] = key;
            return;
        }
        if (sscanf(line, "gamepad_a=%15[^\t\n]", key_name)) {
            key = config->gamepad_button_name_parser(key_name);
            config->gamepad_bindings[JOYPAD_A] = key;
            return;
        }
        if (sscanf(line, "gamepad_b=%15[^\t\n]", key_name)) {
            key = config->gamepad_button_name_parser(key_name);
            config->gamepad_bindings[JOYPAD_B] = key;
            return;
        }
        if (sscanf(line, "gamepad_select=%15[^\t\n]", key_name)) {
            key = config->gamepad_button_name_parser(key_name);
            config->gamepad_bindings[JOYPAD_SELECT] = key;
            return;
        }
        if (sscanf(line, "gamepad_start=%15[^\t\n]", key_name)) {
            key = config->gamepad_button_name_parser(key_name);
            config->gamepad_bindings[JOYPAD_START] = key;
            return;
        }
    }
}

void config_load_from_string(config_t *config, const char *buf) {
    char *p, *temp;
    p = strtok_r((char *) buf, "\n", &temp);
    do {
        parse_config_line(config, p);
    } while ((p = strtok_r(NULL, "\n", &temp)) != NULL);
}

char *config_save_to_string(config_t *config) {
    char *buf = xmalloc(512);

    snprintf(buf, 512,
        "mode=%d\nscale=%d\nspeed=%.1f\nsound=%.2f\nsound_drc=%d\ncolor_palette=%d\nlink_host=%s\nlink_port=%s\n",
        config->mode,
        config->scale,
        config->speed,
        config->sound,
        config->sound_drc,
        config->color_palette,
        config->link_host,
        config->link_port
    );

    // separate snprintfs because key_parser() can return a pointer which contents get overwritten at each call
    size_t len = strlen(buf);
    if (config->keycode_parser) {
        snprintf(&buf[len], 512 - len, "keyboard_right=%s\n", config->keycode_parser(config->keybindings[JOYPAD_RIGHT]));
        len = strlen(buf);
        snprintf(&buf[len], 512 - len, "keyboard_left=%s\n", config->keycode_parser(config->keybindings[JOYPAD_LEFT]));
        len = strlen(buf);
        snprintf(&buf[len], 512 - len, "keyboard_up=%s\n", config->keycode_parser(config->keybindings[JOYPAD_UP]));
        len = strlen(buf);
        snprintf(&buf[len], 512 - len, "keyboard_down=%s\n", config->keycode_parser(config->keybindings[JOYPAD_DOWN]));
        len = strlen(buf);
        snprintf(&buf[len], 512 - len, "keyboard_a=%s\n", config->keycode_parser(config->keybindings[JOYPAD_A]));
        len = strlen(buf);
        snprintf(&buf[len], 512 - len, "keyboard_b=%s\n", config->keycode_parser(config->keybindings[JOYPAD_B]));
        len = strlen(buf);
        snprintf(&buf[len], 512 - len, "keyboard_select=%s\n", config->keycode_parser(config->keybindings[JOYPAD_SELECT]));
        len = strlen(buf);
        snprintf(&buf[len], 512 - len, "keyboard_start=%s\n", config->keycode_parser(config->keybindings[JOYPAD_START]));
        len = strlen(buf);
    }

    if (config->gamepad_button_parser) {
        snprintf(&buf[len], 512 - len, "gamepad_right=%s\n", config->gamepad_button_parser(config->gamepad_bindings[JOYPAD_RIGHT]));
        len = strlen(buf);
        snprintf(&buf[len], 512 - len, "gamepad_left=%s\n", config->gamepad_button_parser(config->gamepad_bindings[JOYPAD_LEFT]));
        len = strlen(buf);
        snprintf(&buf[len], 512 - len, "gamepad_up=%s\n", config->gamepad_button_parser(config->gamepad_bindings[JOYPAD_UP]));
        len = strlen(buf);
        snprintf(&buf[len], 512 - len, "gamepad_down=%s\n", config->gamepad_button_parser(config->gamepad_bindings[JOYPAD_DOWN]));
        len = strlen(buf);
        snprintf(&buf[len], 512 - len, "gamepad_a=%s\n", config->gamepad_button_parser(config->gamepad_bindings[JOYPAD_A]));
        len = strlen(buf);
        snprintf(&buf[len], 512 - len, "gamepad_b=%s\n", config->gamepad_button_parser(config->gamepad_bindings[JOYPAD_B]));
        len = strlen(buf);
        snprintf(&buf[len], 512 - len, "gamepad_select=%s\n", config->gamepad_button_parser(config->gamepad_bindings[JOYPAD_SELECT]));
        len = strlen(buf);
        snprintf(&buf[len], 512 - len, "gamepad_start=%s\n", config->gamepad_button_parser(config->gamepad_bindings[JOYPAD_START]));
    }

    return buf;
}

void config_load_from_file(config_t *config, const char *path) {
    FILE *f = fopen(path, "r");
    if (f) {
        char buf[512];
        fread(buf, sizeof(buf), 1, f);
        config_load_from_string(config, buf);
        fclose(f);
    }
}

void config_save_to_file(config_t *config, const char *path) {
    char *config_str = config_save_to_string(config);
    make_parent_dirs(path);
    FILE *f = fopen(path, "w");
    if (!f) {
        errnoprintf("error opening config file");
        free(config_str);
        return;
    }
    fwrite(config_str, strlen(config_str), 1, f);
    fclose(f);
    free(config_str);
}
