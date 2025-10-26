#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "config.h"
#include "utils.h"
#include "../../core/core.h"

static void parse_config_line(config_t *config, const char *line) {
    uint8_t color_palette, sound_drc, enable_joypad;
    float   speed, sound, joypad_opacity;
    char    link_host[INET6_ADDRSTRLEN], link_port[6];
    uint8_t mode;

    unsigned int keycode;

    if (sscanf(line, "mode=%hhu", &mode)) {
        if (mode == GBMULATOR_MODE_GB || mode == GBMULATOR_MODE_GBC || mode == GBMULATOR_MODE_GBA)
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
    if (sscanf(line, "joypad_opacity=%f", &joypad_opacity)) {
        if (joypad_opacity < 0.0f)
            config->joypad_opacity = 0.0f;
        else if (joypad_opacity > 1.0f)
            config->joypad_opacity = 1.0f;
        else
            config->joypad_opacity = joypad_opacity;
        return;
    }
    if (sscanf(line, "sound_drc=%hhu", &sound_drc)) {
        config->sound_drc = sound_drc;
        return;
    }
    if (sscanf(line, "enable_joypad=%hhu", &enable_joypad)) {
        config->enable_joypad = enable_joypad;
        return;
    }
    if (sscanf(line, "color_palette=%hhu", &color_palette)) {
        if (color_palette < PPU_COLOR_PALETTE_END)
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

    if (sscanf(line, "keyboard_right=%u", &keycode)) {
        if (config->keycode_filter(keycode))
            config->keybindings[GBMULATOR_JOYPAD_RIGHT] = keycode;
        return;
    }
    if (sscanf(line, "keyboard_left=%u", &keycode)) {
        if (config->keycode_filter(keycode))
            config->keybindings[GBMULATOR_JOYPAD_LEFT] = keycode;
        return;
    }
    if (sscanf(line, "keyboard_up=%u", &keycode)) {
        if (config->keycode_filter(keycode))
            config->keybindings[GBMULATOR_JOYPAD_UP] = keycode;
        return;
    }
    if (sscanf(line, "keyboard_down=%u", &keycode)) {
        if (config->keycode_filter(keycode))
            config->keybindings[GBMULATOR_JOYPAD_DOWN] = keycode;
        return;
    }
    if (sscanf(line, "keyboard_a=%u", &keycode)) {
        if (config->keycode_filter(keycode))
            config->keybindings[GBMULATOR_JOYPAD_A] = keycode;
        return;
    }
    if (sscanf(line, "keyboard_b=%u", &keycode)) {
        if (config->keycode_filter(keycode))
            config->keybindings[GBMULATOR_JOYPAD_B] = keycode;
        return;
    }
    if (sscanf(line, "keyboard_select=%u", &keycode)) {
        if (config->keycode_filter(keycode))
            config->keybindings[GBMULATOR_JOYPAD_SELECT] = keycode;
        return;
    }
    if (sscanf(line, "keyboard_start=%u", &keycode)) {
        if (config->keycode_filter(keycode))
            config->keybindings[GBMULATOR_JOYPAD_START] = keycode;
        return;
    }
    if (sscanf(line, "keyboard_l=%u", &keycode)) {
        if (config->keycode_filter(keycode))
            config->keybindings[GBMULATOR_JOYPAD_L] = keycode;
        return;
    }
    if (sscanf(line, "keyboard_r=%u", &keycode)) {
        if (config->keycode_filter(keycode))
            config->keybindings[GBMULATOR_JOYPAD_R] = keycode;
        return;
    }

    if (sscanf(line, "gamepad_right=%u", &keycode)) {
        config->gamepad_bindings[GBMULATOR_JOYPAD_RIGHT] = keycode;
        return;
    }
    if (sscanf(line, "gamepad_left=%u", &keycode)) {
        config->gamepad_bindings[GBMULATOR_JOYPAD_LEFT] = keycode;
        return;
    }
    if (sscanf(line, "gamepad_up=%u", &keycode)) {
        config->gamepad_bindings[GBMULATOR_JOYPAD_UP] = keycode;
        return;
    }
    if (sscanf(line, "gamepad_down=%u", &keycode)) {
        config->gamepad_bindings[GBMULATOR_JOYPAD_DOWN] = keycode;
        return;
    }
    if (sscanf(line, "gamepad_a=%u", &keycode)) {
        config->gamepad_bindings[GBMULATOR_JOYPAD_A] = keycode;
        return;
    }
    if (sscanf(line, "gamepad_b=%u", &keycode)) {
        config->gamepad_bindings[GBMULATOR_JOYPAD_B] = keycode;
        return;
    }
    if (sscanf(line, "gamepad_select=%u", &keycode)) {
        config->gamepad_bindings[GBMULATOR_JOYPAD_SELECT] = keycode;
        return;
    }
    if (sscanf(line, "gamepad_start=%u", &keycode)) {
        config->gamepad_bindings[GBMULATOR_JOYPAD_START] = keycode;
        return;
    }
    if (sscanf(line, "gamepad_l=%u", &keycode)) {
        config->gamepad_bindings[GBMULATOR_JOYPAD_L] = keycode;
        return;
    }
    if (sscanf(line, "gamepad_r=%u", &keycode)) {
        config->gamepad_bindings[GBMULATOR_JOYPAD_R] = keycode;
        return;
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
    static char config_str[512];

    snprintf(config_str, 512,
             "mode=%d\nspeed=%.1f\nsound=%.2f\njoypad_opacity=%.2f\nsound_drc=%d\nenable_joypad=%d\ncolor_palette=%d\nlink_host=%s\nlink_port=%s\n",
             config->mode,
             config->speed,
             config->sound,
             config->joypad_opacity,
             config->sound_drc,
             config->enable_joypad,
             config->color_palette,
             config->link_host,
             config->link_port);

    // separate snprintfs because key_parser() can return a pointer which contents get overwritten at each call
    size_t len = strlen(config_str);
    snprintf(&config_str[len], 512 - len, "keyboard_right=%u\n", config->keybindings[GBMULATOR_JOYPAD_RIGHT]);
    len = strlen(config_str);
    snprintf(&config_str[len], 512 - len, "keyboard_left=%u\n", config->keybindings[GBMULATOR_JOYPAD_LEFT]);
    len = strlen(config_str);
    snprintf(&config_str[len], 512 - len, "keyboard_up=%u\n", config->keybindings[GBMULATOR_JOYPAD_UP]);
    len = strlen(config_str);
    snprintf(&config_str[len], 512 - len, "keyboard_down=%u\n", config->keybindings[GBMULATOR_JOYPAD_DOWN]);
    len = strlen(config_str);
    snprintf(&config_str[len], 512 - len, "keyboard_a=%u\n", config->keybindings[GBMULATOR_JOYPAD_A]);
    len = strlen(config_str);
    snprintf(&config_str[len], 512 - len, "keyboard_b=%u\n", config->keybindings[GBMULATOR_JOYPAD_B]);
    len = strlen(config_str);
    snprintf(&config_str[len], 512 - len, "keyboard_select=%u\n", config->keybindings[GBMULATOR_JOYPAD_SELECT]);
    len = strlen(config_str);
    snprintf(&config_str[len], 512 - len, "keyboard_start=%u\n", config->keybindings[GBMULATOR_JOYPAD_START]);
    len = strlen(config_str);
    snprintf(&config_str[len], 512 - len, "keyboard_l=%u\n", config->keybindings[GBMULATOR_JOYPAD_L]);
    len = strlen(config_str);
    snprintf(&config_str[len], 512 - len, "keyboard_r=%u\n", config->keybindings[GBMULATOR_JOYPAD_R]);
    len = strlen(config_str);

    snprintf(&config_str[len], 512 - len, "gamepad_right=%u\n", config->gamepad_bindings[GBMULATOR_JOYPAD_RIGHT]);
    len = strlen(config_str);
    snprintf(&config_str[len], 512 - len, "gamepad_left=%u\n", config->gamepad_bindings[GBMULATOR_JOYPAD_LEFT]);
    len = strlen(config_str);
    snprintf(&config_str[len], 512 - len, "gamepad_up=%u\n", config->gamepad_bindings[GBMULATOR_JOYPAD_UP]);
    len = strlen(config_str);
    snprintf(&config_str[len], 512 - len, "gamepad_down=%u\n", config->gamepad_bindings[GBMULATOR_JOYPAD_DOWN]);
    len = strlen(config_str);
    snprintf(&config_str[len], 512 - len, "gamepad_a=%u\n", config->gamepad_bindings[GBMULATOR_JOYPAD_A]);
    len = strlen(config_str);
    snprintf(&config_str[len], 512 - len, "gamepad_b=%u\n", config->gamepad_bindings[GBMULATOR_JOYPAD_B]);
    len = strlen(config_str);
    snprintf(&config_str[len], 512 - len, "gamepad_select=%u\n", config->gamepad_bindings[GBMULATOR_JOYPAD_SELECT]);
    len = strlen(config_str);
    snprintf(&config_str[len], 512 - len, "gamepad_start=%u\n", config->gamepad_bindings[GBMULATOR_JOYPAD_START]);
    len = strlen(config_str);
    snprintf(&config_str[len], 512 - len, "gamepad_l=%u\n", config->gamepad_bindings[GBMULATOR_JOYPAD_L]);
    len = strlen(config_str);
    snprintf(&config_str[len], 512 - len, "gamepad_r=%u\n", config->gamepad_bindings[GBMULATOR_JOYPAD_R]);

    return config_str;
}

bool config_load_from_file(config_t *config, const char *path) {
    if (!config || !path)
        return false;

    size_t   len;
    uint8_t *config_str = read_file(path, &len);
    if (!config_str)
        return false;

    config_load_from_string(config, (char *) config_str);
    free(config_str);

    return true;
}

bool config_save_to_file(config_t *config, const char *path) {
    char *config_str = config_save_to_string(config);
    return write_file(path, (uint8_t *) config_str, strlen(config_str));
}
