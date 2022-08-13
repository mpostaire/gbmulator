#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "utils.h"
#include "emulator/emulator.h"

// defaults
struct config config = {
    .mode = CGB,
    .color_palette = PPU_COLOR_PALETTE_ORIG,
    .scale = 2,
    .sound = 0.25f,
    .speed = 1.0f,
    .link_host = "127.0.0.1",
    .link_port = "7777",
    .is_ipv6 = 0,
    .mptcp_enabled = 0,

    .left = SDLK_LEFT,
    .right = SDLK_RIGHT,
    .up = SDLK_UP,
    .down = SDLK_DOWN,
    .a = SDLK_KP_0,
    .b = SDLK_KP_PERIOD,
    .start = SDLK_KP_1,
    .select = SDLK_KP_2
};

int config_verif_key(SDL_Keycode key) {
    switch (key) {
    case SDLK_RETURN: case SDLK_KP_ENTER:
    case SDLK_DELETE: case SDLK_BACKSPACE:
    case SDLK_PAUSE: case SDLK_ESCAPE:
    case SDLK_F1: case SDLK_F2:
    case SDLK_F3: case SDLK_F4:
    case SDLK_F5: case SDLK_F6:
    case SDLK_F7: case SDLK_F8:
        return 0;
    default:
        return 1;
    }
}

static void parse_config_line(const char *line) {
    byte_t scale, color_palette, is_ipv6, mptcp_enabled;
    float speed, sound;
    char link_host[INET6_ADDRSTRLEN], link_port[6];
    byte_t mode;

    char left[16], right[16], up[16], down[16], a[16], b[16], start[16], select[16];

    SDL_KeyCode key;

    if (sscanf(line, "scale=%hhu", &scale)) {
        if (scale >= 1 && scale <= 5)
            config.scale = scale;
    } else if (sscanf(line, "mode=%hhu", &mode)) {
        if (mode == CGB || mode == DMG)
            config.mode = mode;
    } else if (sscanf(line, "speed=%f", &speed)) {
        if (speed == 1.0f || speed == 1.5f || speed == 2.0f ||
            speed == 2.5f || speed == 3.0f || speed == 3.5f ||
            speed == 4.0f) {

            config.speed = speed;
        }
    } else if (sscanf(line, "sound=%f", &sound)) {
        if (sound == 0.0f || sound == 0.25f || sound == 0.5f ||
            sound == 0.75f || sound == 1.0f) {

            config.sound = sound;
        }
    } else if (sscanf(line, "color_palette=%hhu", &color_palette)) {
        if (color_palette < PPU_COLOR_PALETTE_MAX)
            config.color_palette = color_palette;
    } else if (sscanf(line, "link_host=%45s", link_host)) {
        strncpy(config.link_host, link_host, INET6_ADDRSTRLEN);
    } else if (sscanf(line, "link_port=%5s", link_port)) {
        strncpy(config.link_port, link_port, 6);
    } else if (sscanf(line, "ipv6=%hhu", &is_ipv6)) {
        config.is_ipv6 = is_ipv6;
    } else if (sscanf(line, "mptcp=%hhu", &mptcp_enabled)) {
        config.mptcp_enabled = mptcp_enabled;
    } else if (sscanf(line, "left=%15[^\t\n]", left)) {
        key = SDL_GetKeyFromName(left);
        if (config_verif_key(key))
            config.left = key;
    } else if (sscanf(line, "right=%15[^\t\n]", right)) {
        key = SDL_GetKeyFromName(right);
        if (config_verif_key(key))
            config.right = key;
    } else if (sscanf(line, "up=%15[^\t\n]", up)) {
        key = SDL_GetKeyFromName(up);
        if (config_verif_key(key))
            config.up = key;
    } else if (sscanf(line, "down=%15[^\t\n]", down)) {
        key = SDL_GetKeyFromName(down);
        if (config_verif_key(key))
            config.down = key;
    } else if (sscanf(line, "a=%15[^\t\n]", a)) {
        key = SDL_GetKeyFromName(a);
        if (config_verif_key(key))
            config.a = key;
    } else if (sscanf(line, "b=%15[^\t\n]", b)) {
        key = SDL_GetKeyFromName(b);
        if (config_verif_key(key))
            config.b = key;
    } else if (sscanf(line, "start=%15[^\t\n]", start)) {
        key = SDL_GetKeyFromName(start);
        if (config_verif_key(key))
            config.start = key;
    } else if (sscanf(line, "select=%15[^\t\n]", select)) {
        key = SDL_GetKeyFromName(select);
        if (config_verif_key(key))
            config.select = key;
    }
}

void config_load_from_buffer(const char *buf) {
    char *p, *temp;
    p = strtok_r((char *) buf, "\n", &temp);
    do {
        parse_config_line(p);
    } while ((p = strtok_r(NULL, "\n", &temp)) != NULL);
}

char *config_save_to_buffer(size_t *len) {
    char *buf = xmalloc(512);

    snprintf(buf, 512,
        "mode=%d\nscale=%d\nspeed=%.1f\nsound=%.2f\ncolor_palette=%d\nlink_host=%s\nlink_port=%s\nipv6=%d\nmptcp=%d\n",
        config.mode,
        config.scale,
        config.speed,
        config.sound,
        config.color_palette,
        config.link_host,
        config.link_port,
        config.is_ipv6,
        config.mptcp_enabled
    );

    // separate snprintfs as SDL_GetKeyName() returns a pointer which contents get overwritten at each call
    *len = strlen(buf);
    snprintf(&buf[*len], 512 - *len, "left=%s\n", SDL_GetKeyName(config.left));
    *len = strlen(buf);
    snprintf(&buf[*len], 512 - *len, "right=%s\n", SDL_GetKeyName(config.right));
    *len = strlen(buf);
    snprintf(&buf[*len], 512 - *len, "up=%s\n", SDL_GetKeyName(config.up));
    *len = strlen(buf);
    snprintf(&buf[*len], 512 - *len, "down=%s\n", SDL_GetKeyName(config.down));
    *len = strlen(buf);
    snprintf(&buf[*len], 512 - *len, "a=%s\n", SDL_GetKeyName(config.a));
    *len = strlen(buf);
    snprintf(&buf[*len], 512 - *len, "b=%s\n", SDL_GetKeyName(config.b));
    *len = strlen(buf);
    snprintf(&buf[*len], 512 - *len, "start=%s\n", SDL_GetKeyName(config.start));
    *len = strlen(buf);
    snprintf(&buf[*len], 512 - *len, "select=%s\n", SDL_GetKeyName(config.select));
    *len = strlen(buf);

    return buf;
}

void config_load_from_file(const char *path) {
    SDL_RWops *f = SDL_RWFromFile(path, "r");
    if (f) {
        char buf[512];
        SDL_RWread(f, buf, sizeof(buf), 1);
        config_load_from_buffer(buf);
        SDL_RWclose(f);
    }
}

void config_save_to_file(const char *path) {
    size_t len;
    char *config_buf = config_save_to_buffer(&len);
    make_parent_dirs(path);
    SDL_RWops *f = SDL_RWFromFile(path, "w");
    if (!f) {
        errnoprintf("error opening config file");
        free(config_buf);
        return;
    }
    SDL_RWwrite(f, config_buf, len, 1);
    SDL_RWclose(f);
    free(config_buf);
}
