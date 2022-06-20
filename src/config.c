#include <stdlib.h>
#include <string.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include "config.h"
#include "emulator/emulator.h"

struct config config = {
    .mode = CGB,
    #ifdef __EMSCRIPTEN__
    .color_palette = PPU_COLOR_PALETTE_ORIG,
    .scale = 2,
    .sound = 0.25f,
    #else
    .color_palette = PPU_COLOR_PALETTE_GRAY,
    .scale = 3,
    .sound = 0.5f,
    #endif
    .speed = 1.0f,
    .link_host = "127.0.0.1",
    .link_port = 7777,

    .left = SDLK_LEFT,
    .right = SDLK_RIGHT,
    .up = SDLK_UP,
    .down = SDLK_DOWN,
    .a = SDLK_KP_0,
    #ifdef __EMSCRIPTEN__
    .b = SDLK_PERIOD,
    #else
    .b = SDLK_KP_PERIOD,
    #endif
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
    byte_t scale, color_palette;
    float speed, sound;
    char link_host[40];
    int link_port, mode;

    char left[16], right[16], up[16], down[16], a[16], b[16], start[16], select[16];

    SDL_KeyCode key;

    if (sscanf(line, "scale=%hhu", &scale)) {
        if (scale >= 1 && scale <= 5)
            config.scale = scale;
    } else if (sscanf(line, "mode=%d", &mode)) {
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
    } else if (sscanf(line, "link_host=%39s", link_host)) {
        strncpy(config.link_host, link_host, 40);
    } else if (sscanf(line, "link_port=%d", &link_port)) {
        config.link_port = link_port;
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

void config_load(const char* config_path) {
    #ifdef __EMSCRIPTEN__
    unsigned char *data = (unsigned char *) EM_ASM_INT({
        var item = localStorage.getItem(UTF8ToString($0));
        if (item === null)
            return null;
        var itemLength = lengthBytesUTF8(item) + 1;

        var ret = _malloc(itemLength);
        stringToUTF8(item, ret, itemLength);
        return ret;
    }, config_path);

    if (!data)
        return;

    char *p, *temp;
    p = strtok_r((char *) data, "\n", &temp);
    do {
        parse_config_line(p);
    } while ((p = strtok_r(NULL, "\n", &temp)) != NULL);

    free(data);
    #else
    FILE *f = fopen(config_path, "r");
    if (f) {
        printf("Loading config from %s\n", config_path);

        char line[64];
        while (fgets(line, sizeof(line), f))
            parse_config_line(line);

        fclose(f);
    }
    #endif
}

void config_save(const char* config_path) {
    #ifdef __EMSCRIPTEN__
    char buf[512];
    snprintf(buf, sizeof(buf),
        "mode=%d\nscale=%d\nspeed=%.1f\nsound=%.2f\ncolor_palette=%d\nlink_host=%s\nlink_port=%d\n" \
        "left=%s\nright=%s\nup=%s\ndown=%s\na=%s\nb=%s\nstart=%s\nselect=%s\n",
        config.mode,
        config.scale,
        config.speed,
        config.sound,
        config.color_palette,
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

    EM_ASM({
        localStorage.setItem(UTF8ToString($0), UTF8ToString($1));
    }, config_path, buf);
    #else
    make_parent_dirs(config_path);

    FILE *f = fopen(config_path, "w");
    if (!f) {
        errnoprintf("opening file");
        return;
    }

    fprintf(f,
        "mode=%d\nscale=%d\nspeed=%.1f\nsound=%.2f\ncolor_palette=%d\nlink_host=%s\nlink_port=%d\n" \
        "left=%s\nright=%s\nup=%s\ndown=%s\na=%s\nb=%s\nstart=%s\nselect=%s\n",
        config.mode,
        config.scale,
        config.speed,
        config.sound,
        config.color_palette,
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
    #endif
}
