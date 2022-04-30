#include "config.h"
#include "utils.h"

struct config config = {
    .scale = 3,
    .speed = 1.0f,
    .sound = 1.0f,
    .color_palette = 0,
    .link_host = "127.0.0.1",
    .link_port = 7777
};

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

        struct config parsed = { 0 };

        // TODO test a long line in config to see how it breaks this
        char buf[64];
        while (fgets(buf, 64, f)) {
            if (sscanf(buf, "scale=%hhd", &parsed.scale)) {
                if (parsed.scale >= 1 && parsed.scale <= 5)
                    config.scale = parsed.scale;
            } else if (sscanf(buf, "speed=%f", &parsed.speed)) {
                if (parsed.speed == 1.0f || parsed.speed == 1.5f || parsed.speed == 2.0f ||
                    parsed.speed == 2.5f || parsed.speed == 3.0f || parsed.speed == 3.5f ||
                    parsed.speed == 4.0f)

                    config.speed = parsed.speed;
            } else if (sscanf(buf, "sound=%f", &parsed.sound)) {
                if (parsed.sound == 0.0f || parsed.sound == 0.25f || parsed.sound == 0.5f ||
                    parsed.sound == 0.75f || parsed.sound == 1.0f)

                    config.sound = parsed.sound;
            } else if (sscanf(buf, "color_palette=%hhd", &parsed.color_palette)) {
                if (parsed.color_palette >= 0 && parsed.color_palette <= 1)
                    config.color_palette = parsed.color_palette;
            } else if (sscanf(buf, "link_host=%40s", parsed.link_host)) {
                strncpy(config.link_host, parsed.link_host, 40);
            } else if (sscanf(buf, "link_port=%d", &parsed.link_port)) {
                config.link_port = parsed.link_port;
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
        config.scale, config.speed, config.sound, config.color_palette, config.link_host, config.link_port
    );

    printf("Saving config to %s\n", config_path);

    fclose(f);
}
