#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "types.h"
#include "gbmulator.h"
#include "config.h"
#include "ppu.h"
#include "apu.h"
#include "link.h"
#include "utils.h"

// TODO fix this file (it's ugly code).

#define SET_PIXEL_RGBA(buf, x, y, color, alpha) \
    byte_t *_tmp_color_values = ppu_get_color_values((color)); \
    *(buf + ((y) * 160 * 4) + ((x) * 4)) = _tmp_color_values[0]; \
    *(buf + ((y) * 160 * 4) + ((x) * 4) + 1) = _tmp_color_values[1]; \
    *(buf + ((y) * 160 * 4) + ((x) * 4) + 2) = _tmp_color_values[2]; \
    *(buf + ((y) * 160 * 4) + ((x) * 4) + 3) = (alpha);

const byte_t font[0x5F][0x8] = {
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x10, 0x10, 0x10, 0x10, 0x00, 0x10, 0x00 },
    { 0x00, 0x14, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x14, 0x3E, 0x14, 0x3E, 0x14, 0x00 },
    { 0x10, 0x3E, 0x50, 0x3C, 0x12, 0x12, 0x7C, 0x10 },
    { 0x00, 0x20, 0x52, 0x24, 0x08, 0x12, 0x25, 0x02 },
    { 0x00, 0x18, 0x24, 0x18, 0x30, 0x4A, 0x3C, 0x00 },
    { 0x00, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x08, 0x10, 0x10, 0x10, 0x10, 0x08, 0x00 },
    { 0x00, 0x08, 0x04, 0x04, 0x04, 0x04, 0x08, 0x00 },
    { 0x00, 0x14, 0x08, 0x14, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x08, 0x08, 0x3E, 0x08, 0x08, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x10 },
    { 0x00, 0x00, 0x00, 0x00, 0x3E, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00 },
    { 0x00, 0x00, 0x02, 0x04, 0x08, 0x10, 0x20, 0x00 },
    { 0x00, 0x3C, 0x46, 0x4A, 0x52, 0x62, 0x3C, 0x00 },
    { 0x00, 0x08, 0x18, 0x28, 0x08, 0x08, 0x3E, 0x00 },
    { 0x00, 0x3C, 0x42, 0x1C, 0x20, 0x40, 0x7E, 0x00 },
    { 0x00, 0x3C, 0x42, 0x1C, 0x02, 0x42, 0x3C, 0x00 },
    { 0x00, 0x44, 0x44, 0x7E, 0x04, 0x04, 0x04, 0x00 },
    { 0x00, 0x7C, 0x40, 0x7C, 0x02, 0x02, 0x7C, 0x00 },
    { 0x00, 0x3C, 0x40, 0x7C, 0x42, 0x42, 0x3C, 0x00 },
    { 0x00, 0x7E, 0x02, 0x04, 0x08, 0x10, 0x20, 0x00 },
    { 0x00, 0x3C, 0x42, 0x3C, 0x42, 0x42, 0x3C, 0x00 },
    { 0x00, 0x3C, 0x42, 0x42, 0x3E, 0x02, 0x3C, 0x00 },
    { 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x08, 0x00 },
    { 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x08, 0x10 },
    { 0x00, 0x00, 0x06, 0x18, 0x60, 0x18, 0x06, 0x00 },
    { 0x00, 0x00, 0x00, 0x7E, 0x00, 0x7E, 0x00, 0x00 },
    { 0x00, 0x00, 0x60, 0x18, 0x06, 0x18, 0x60, 0x00 },
    { 0x00, 0x38, 0x44, 0x08, 0x10, 0x00, 0x10, 0x00 },
    { 0x00, 0x1C, 0x22, 0x4E, 0x4A, 0x2E, 0x10, 0x00 },
    { 0x00, 0x3C, 0x42, 0x7E, 0x42, 0x42, 0x42, 0x00 },
    { 0x00, 0x7C, 0x42, 0x7C, 0x42, 0x42, 0x7C, 0x00 },
    { 0x00, 0x3C, 0x42, 0x40, 0x40, 0x42, 0x3C, 0x00 },
    { 0x00, 0x7C, 0x42, 0x42, 0x42, 0x42, 0x7C, 0x00 },
    { 0x00, 0x7E, 0x40, 0x7C, 0x40, 0x40, 0x7E, 0x00 },
    { 0x00, 0x7E, 0x40, 0x7C, 0x40, 0x40, 0x40, 0x00 },
    { 0x00, 0x3C, 0x40, 0x4E, 0x42, 0x42, 0x3E, 0x00 },
    { 0x00, 0x42, 0x42, 0x7E, 0x42, 0x42, 0x42, 0x00 },
    { 0x00, 0x1C, 0x08, 0x08, 0x08, 0x08, 0x1C, 0x00 },
    { 0x00, 0x08, 0x08, 0x08, 0x08, 0x48, 0x30, 0x00 },
    { 0x00, 0x48, 0x50, 0x60, 0x50, 0x48, 0x48, 0x00 },
    { 0x00, 0x40, 0x40, 0x40, 0x40, 0x40, 0x7C, 0x00 },
    { 0x00, 0x42, 0x66, 0x5A, 0x42, 0x42, 0x42, 0x00 },
    { 0x00, 0x42, 0x62, 0x52, 0x4A, 0x46, 0x42, 0x00 },
    { 0x00, 0x3C, 0x42, 0x42, 0x42, 0x42, 0x3C, 0x00 },
    { 0x00, 0x7C, 0x42, 0x7C, 0x40, 0x40, 0x40, 0x00 },
    { 0x00, 0x3C, 0x42, 0x42, 0x4A, 0x44, 0x3A, 0x00 },
    { 0x00, 0x7C, 0x42, 0x7C, 0x48, 0x44, 0x42, 0x00 },
    { 0x00, 0x3E, 0x40, 0x3C, 0x02, 0x02, 0x7C, 0x00 },
    { 0x00, 0x7C, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00 },
    { 0x00, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3C, 0x00 },
    { 0x00, 0x42, 0x42, 0x24, 0x24, 0x24, 0x18, 0x00 },
    { 0x00, 0x42, 0x42, 0x42, 0x5A, 0x66, 0x42, 0x00 },
    { 0x00, 0x44, 0x28, 0x10, 0x10, 0x28, 0x44, 0x00 },
    { 0x00, 0x44, 0x44, 0x28, 0x10, 0x10, 0x10, 0x00 },
    { 0x00, 0x7E, 0x04, 0x08, 0x10, 0x20, 0x7E, 0x00 },
    { 0x00, 0x18, 0x10, 0x10, 0x10, 0x10, 0x18, 0x00 },
    { 0x00, 0x00, 0x20, 0x10, 0x08, 0x04, 0x02, 0x00 },
    { 0x00, 0x18, 0x08, 0x08, 0x08, 0x08, 0x18, 0x00 },
    { 0x00, 0x08, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7E, 0x00 },
    { 0x10, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x3C, 0x02, 0x3E, 0x42, 0x3C, 0x00 },
    { 0x00, 0x00, 0x40, 0x40, 0x7C, 0x42, 0x7C, 0x00 },
    { 0x00, 0x00, 0x3C, 0x42, 0x40, 0x42, 0x3C, 0x00 },
    { 0x00, 0x00, 0x02, 0x02, 0x3E, 0x42, 0x3E, 0x00 },
    { 0x00, 0x00, 0x3C, 0x42, 0x7C, 0x40, 0x3C, 0x00 },
    { 0x00, 0x00, 0x08, 0x10, 0x38, 0x10, 0x10, 0x00 },
    { 0x00, 0x00, 0x3E, 0x42, 0x3E, 0x02, 0x3C, 0x00 },
    { 0x00, 0x00, 0x40, 0x40, 0x78, 0x44, 0x44, 0x00 },
    { 0x00, 0x00, 0x10, 0x00, 0x30, 0x10, 0x10, 0x00 },
    { 0x00, 0x00, 0x08, 0x00, 0x18, 0x08, 0x08, 0x38 },
    { 0x00, 0x00, 0x10, 0x10, 0x14, 0x18, 0x14, 0x00 },
    { 0x00, 0x00, 0x10, 0x10, 0x10, 0x10, 0x18, 0x00 },
    { 0x00, 0x00, 0x74, 0x4A, 0x42, 0x42, 0x42, 0x00 },
    { 0x00, 0x00, 0x7C, 0x42, 0x42, 0x42, 0x42, 0x00 },
    { 0x00, 0x00, 0x3C, 0x42, 0x42, 0x42, 0x3C, 0x00 },
    { 0x00, 0x00, 0x7C, 0x42, 0x7C, 0x40, 0x40, 0x40 },
    { 0x00, 0x00, 0x3E, 0x42, 0x3E, 0x02, 0x02, 0x02 },
    { 0x00, 0x00, 0x2E, 0x31, 0x20, 0x20, 0x20, 0x00 },
    { 0x00, 0x00, 0x1E, 0x20, 0x1C, 0x02, 0x3C, 0x00 },
    { 0x00, 0x00, 0x10, 0x38, 0x10, 0x10, 0x08, 0x00 },
    { 0x00, 0x00, 0x42, 0x42, 0x42, 0x42, 0x3E, 0x00 },
    { 0x00, 0x00, 0x42, 0x42, 0x24, 0x24, 0x18, 0x00 },
    { 0x00, 0x00, 0x42, 0x42, 0x42, 0x4A, 0x34, 0x00 },
    { 0x00, 0x00, 0x44, 0x28, 0x10, 0x28, 0x44, 0x00 },
    { 0x00, 0x00, 0x44, 0x28, 0x10, 0x20, 0x40, 0x00 },
    { 0x00, 0x00, 0x7C, 0x08, 0x10, 0x20, 0x7C, 0x00 },
    { 0x00, 0x08, 0x10, 0x30, 0x30, 0x10, 0x08, 0x00 },
    { 0x00, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00 },
    { 0x00, 0x10, 0x08, 0x0C, 0x0C, 0x08, 0x10, 0x00 },
    { 0x00, 0x00, 0x00, 0x10, 0x2A, 0x04, 0x00, 0x00 }
};

byte_t ui_pixels[160 * 144 * 4];

byte_t draw_frames = 0;

enum menu_type {
    ACTION, // do something when A is pressed
    INPUT,  // add char to text input when any key is pressed (except UP/DOWN/LEFT/RIGHT/A/B)
    CHOICE, // change value when LEFT/RIGHT are pressed
    SUBMENU // enter a submenu when A is pressed
};

typedef struct menu menu_t;
typedef struct menu_entry menu_entry_t;

static void choose_win_scale(menu_entry_t *entry);
static void choose_speed(menu_entry_t *entry);
static void choose_sound(menu_entry_t *entry);
static void choose_color(menu_entry_t *entry);
static void choose_link_mode(menu_entry_t *entry);
static void on_input_link_host(menu_entry_t *entry);
static void on_input_link_port(menu_entry_t *entry);
static void start_link();
static void back_to_prev_menu();

struct menu_entry {
    char *label;
    byte_t type;
    byte_t disabled;
    union {
        void (*action)(void);
        menu_t *submenu;
        struct {
            void (*choose)(menu_entry_t *);
            byte_t length;
            byte_t position;
        } choices;
        struct {
            char *input;
            void (*on_input)(menu_entry_t *);
            byte_t cursor;
            byte_t max_length;
            byte_t is_numeric;
            byte_t visible_lo;
            byte_t visible_hi;
        } user_input;
    };
};

struct menu {
    char *title;
    byte_t position;
    byte_t length;
    menu_entry_t entries[];
};

menu_t options_menu = {
    .title = "Options",
    .position = 0,
    .length = 5,
    .entries = {
        { "Scale:      | 1x , 2x , 3x , 4x , 5x ", CHOICE, .choices = { choose_win_scale, 5, 0 } },
        { "Speed:      |1.0x,1.5x,2.0x,2.5x,3.0x,3.5x,4.0x", CHOICE, .choices = { choose_speed, 7, 0 } },
        { "Sound:      | OFF, 25%, 50%, 75%,100%", CHOICE, .choices = { choose_sound, 5, 4 } },
        { "Color:      |gray,orig", CHOICE, .choices = { choose_color, 2, 0 } },
        { "Back...", ACTION, .action = back_to_prev_menu }
    }
};

menu_t link_menu = {
    .title = "Link cable",
    .position = 0,
    .length = 5,
    .entries = {
        { "Mode:     |server,client", CHOICE, .choices = { choose_link_mode, 2, 0 } },
        { "Host: ", INPUT, .disabled = 1, .user_input.on_input = on_input_link_host },
        { "Port: ", INPUT, .user_input = { .is_numeric = 1, .on_input = on_input_link_port } },
        { "Start link", ACTION, .action = start_link },
        { "Back...", ACTION, .action = back_to_prev_menu }
    }
};

menu_t main_menu = {
    .title = "GBmulator",
    .position = 0,
    .length = 4,
    .entries = {
        { "Resume", ACTION, .action = gbmulator_unpause },
        { "Link cable...", SUBMENU, .submenu = &link_menu },
        { "Options...", SUBMENU, .submenu = &options_menu },
        { "Exit", ACTION, .action = gbmulator_exit }
    }
};

menu_t *current_menu = &main_menu;

static void choose_win_scale(menu_entry_t *entry) {
    config.scale = entry->choices.position + 1;
}

static void choose_speed(menu_entry_t *entry) {
    config.speed = (entry->choices.position * 0.5f) + 1;
    apu_set_sampling_speed_multiplier(config.speed);
}

static void choose_sound(menu_entry_t *entry) {
    apu_set_global_sound_level(entry->choices.position * 0.25f);
}

static void choose_color(menu_entry_t *entry) {
    ppu_update_pixels_with_palette(entry->choices.position);
    ppu_set_color_palette(entry->choices.position);
}

static void choose_link_mode(menu_entry_t *entry) {
    link_menu.entries[1].disabled = !entry->choices.position;
}

static void on_input_link_host(menu_entry_t *entry) {
    strncpy(config.link_host, entry->user_input.input, 39);
    config.link_host[39] = '\0';
}

static void on_input_link_port(menu_entry_t *entry) {
    config.link_port = atoi(entry->user_input.input);
}

static void start_link(void) {
    int success;
    if (link_menu.entries[0].choices.position)
        success = link_connect_to_server(config.link_host, config.link_port);
    else
        success = link_start_server(config.link_port);
    
    if (success){
        link_menu.entries[0].disabled = 1;
        link_menu.entries[1].disabled = 1;
        link_menu.entries[2].disabled = 1;
        link_menu.entries[3].disabled = 1;
        link_menu.position = 4;
    }
}

static void back_to_prev_menu(void) {
    if (current_menu == &main_menu) {
        main_menu.position = 0;
        gbmulator_unpause();
    } else {
        current_menu = &main_menu;
    }
}

void ui_back_to_main_menu(void) {
    current_menu = &main_menu;
    main_menu.position = 0;
}

byte_t *ui_init(void) {
    options_menu.entries[0].choices.position = config.scale - 1;
    options_menu.entries[1].choices.position = config.speed / 0.5f - 2;
    options_menu.entries[2].choices.position = apu_get_global_sound_level() * 4;
    options_menu.entries[3].choices.position = ppu_get_color_palette();

    if (!(link_menu.entries[1].user_input.input = malloc(sizeof(char) * 40))) {
        perror("ERROR: ui_init");
        exit(EXIT_FAILURE);
    }
    snprintf(link_menu.entries[1].user_input.input, sizeof(config.link_host), "%s", config.link_host);

    link_menu.entries[1].user_input.cursor = strlen(config.link_host);
    link_menu.entries[1].user_input.max_length = 39;
    link_menu.entries[1].user_input.visible_hi = 12;

    char **buf = &link_menu.entries[2].user_input.input;
    if (!(*buf = malloc(sizeof(char) * 6))) {
        perror("ERROR: ui_init");
        exit(EXIT_FAILURE);
    }

    link_menu.entries[2].user_input.cursor = snprintf(*buf, sizeof(char) * 6, "%d", config.link_port);
    link_menu.entries[2].user_input.input = *buf;
    link_menu.entries[2].user_input.max_length = 5;
    link_menu.entries[2].user_input.visible_hi = 5;

    return ui_pixels;
}

static void print_cursor(int x, int y, color color) {
    for (int i = 0; i < 8; i++) {
        SET_PIXEL_RGBA(ui_pixels, x, y + i, color, 0xFF);
    }
}

static void print_char(const char c, int x, int y, color color) {
    int index = c - 32;
    if (index < 0 || index >= 0x5F) return;
    
    const byte_t *char_data = font[index];
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            if (GET_BIT(char_data[j], abs(i - 7))) {
                SET_PIXEL_RGBA(ui_pixels, x + i, y + j, color, 0xFF);
            }
        }
    }
}

static void print_text(const char *text, int x, int y, color color) {
    for (int i = 0; text[i]; i++) {
        if (text[i] == '|')
            return;
        print_char(text[i], x + (i * 8), y, color);
    }
}

static void ui_clear(void) {
    for (int i = 0; i < 160; i++) {
        for (int j = 0; j < 144; j++) {
            SET_PIXEL_RGBA(ui_pixels, i, j, BLACK, 0xD5);
        }
    }
}

static void print_choice(const char *choices, int x, int y, int n, color text_color, color arrow_color) {
    int delim_count = 0;
    int printed_char_count = 1;
    print_char('<', x, y, arrow_color);
    for (int i = 0; choices[i]; i++) {
        if (choices[i] == ',') {
            delim_count++;
            continue;
        }
        if (delim_count < n)
            continue;
        if (delim_count > n)
            break;
        print_char(choices[i], x + (printed_char_count * 8), y, text_color);
        printed_char_count++;
    }
    print_char('>', x + (printed_char_count * 8), y, arrow_color);
}

void ui_draw_menu(void) {
    draw_frames++;
    if (draw_frames > 64)
        draw_frames = 0;

    // clear ui pixels
    ui_clear();

    byte_t title_x = 72 - ((strlen(current_menu->title) * 8) / 2);
    byte_t labels_start_y = 72 - ((current_menu->length * 8) / 2);

    print_text(current_menu->title, title_x, 32, WHITE);
    print_text(">", 0, labels_start_y + (8 * current_menu->position), WHITE);
    for (byte_t i = 0; i < current_menu->length; i++) {
        menu_entry_t *entry = &current_menu->entries[i];
        byte_t y = labels_start_y + (i * 8);
        color text_color = entry->disabled ? DARK_GRAY : WHITE;
        print_text(entry->label, 8, y, text_color);

        switch (entry->type) {
        case CHOICE:
            int delim_index = strcspn(entry->label, "|");
            char *choices = &entry->label[delim_index + 1];
            print_choice(choices, (delim_index * 8) + 8, y, entry->choices.position, text_color, entry->disabled ? DARK_GRAY : LIGHT_GRAY);
            break;
        case INPUT:
            byte_t x = (strlen(entry->label) * 8) + 8;

            if (entry->user_input.cursor > entry->user_input.visible_hi) {
                byte_t diff = entry->user_input.cursor - entry->user_input.visible_hi;
                entry->user_input.visible_lo += diff;
                entry->user_input.visible_hi += diff;
            } else if (entry->user_input.cursor < entry->user_input.visible_lo) {
                byte_t diff = entry->user_input.visible_lo - entry->user_input.cursor;
                entry->user_input.visible_lo -= diff;
                entry->user_input.visible_hi -= diff;
            }

            char visible_input[13];
            byte_t n = (entry->user_input.visible_hi - entry->user_input.visible_lo) + 1;
            snprintf(visible_input, n, "%s", &entry->user_input.input[entry->user_input.visible_lo]);

            print_text(visible_input, x, y, text_color);
            if (current_menu->position == i && draw_frames < 32) {
                print_cursor(x + ((entry->user_input.cursor - entry->user_input.visible_lo) * 8), y, WHITE);
            }
            break;
        }
    }

    // TODO bottom right corner label status: "Link: Inactive, Waiting, Active"
    // for this an access to link status is needed (not possible yet)
    // print_text("Link:", 0, 0, WHITE);
    // print_text("I", 40, 0, WHITE);
}

static void delete_char_at(char **text, byte_t n) {
    if (n < 0 || n >= strlen(*text))
        return;

    int i = n;
    for (; (*text)[i + 1]; i++) {
        (*text)[i] = (*text)[i + 1];
    }
    (*text)[i] = '\0';
}

void ui_press(SDL_Keysym *keysym) {
    SDL_Keycode key = keysym->sym;
    int count;
    menu_entry_t *entry = &current_menu->entries[current_menu->position];
    draw_frames = 0;

    switch (key) {
    case RIGHT:
    case LEFT:
        switch (current_menu->entries[current_menu->position].type) {
        case CHOICE:
            int new_pos = key == RIGHT ? entry->choices.position + 1 : entry->choices.position - 1;
            if (new_pos < 0)
                new_pos = entry->choices.length - 1;
            else if (new_pos > entry->choices.length - 1)
                new_pos = 0;
            entry->choices.position = new_pos;
            (entry->choices.choose)(entry);
            break;
        case INPUT:
            int new_cursor = entry->user_input.cursor + (key == RIGHT ? 1 : -1);
            int len = strlen(entry->user_input.input);
            if (new_cursor > len)
                new_cursor = len;
            else if (new_cursor < 0)
                new_cursor = 0;
            entry->user_input.cursor = new_cursor;
        }
        break;
    case UP:
        count = 0;
        do {
            current_menu->position = current_menu->position - 1 < 0 ? current_menu->length - 1 : current_menu->position - 1;
            count++;
        } while(current_menu->entries[current_menu->position].disabled && count < current_menu->length);
        if (current_menu->entries[current_menu->position].type == INPUT)
            current_menu->entries[current_menu->position].user_input.cursor = strlen(current_menu->entries[current_menu->position].user_input.input);
        break;
    case DOWN:
        count = 0;
        do {
            current_menu->position = (current_menu->position + 1) % current_menu->length;
            count++;
        } while(current_menu->entries[current_menu->position].disabled && count < current_menu->length);
        if (current_menu->entries[current_menu->position].type == INPUT)
            current_menu->entries[current_menu->position].user_input.cursor = strlen(current_menu->entries[current_menu->position].user_input.input);
        break;
    case A:
        if (entry->type == INPUT)
            break;
    case SDLK_RETURN:
    case SDLK_KP_ENTER:
        switch (entry->type) {
        case ACTION:
            (entry->action)();
            break;
        case SUBMENU:
            // if I ever decide to add more than 1 level of depth of menus/submenus, make a stack of menus and push here
            current_menu = entry->submenu;
            current_menu->position = 0;
            while(current_menu->entries[current_menu->position].disabled && current_menu->position < current_menu->length) {
                current_menu->position++;
            }
            break;
        }
        break;
    case B:
        // if I ever decide to add more than 1 level of depth of menus/submenus, make a stack of menus and pop here
        if (entry->type != INPUT)
            back_to_prev_menu();
        break;
    }

    if (entry->type == INPUT) {
        if (key == SDLK_DELETE) {
            delete_char_at(&entry->user_input.input, entry->user_input.cursor);
            entry->user_input.on_input(entry);

            if (entry->user_input.visible_lo > 0) {
                entry->user_input.visible_lo -= 1;
                entry->user_input.visible_hi -= 1;
            }
        } else if (key == SDLK_BACKSPACE) {
            int new_cursor = entry->user_input.cursor - 1;
            delete_char_at(&entry->user_input.input, new_cursor);
            entry->user_input.on_input(entry);

            if (new_cursor < 0)
                new_cursor = 0;
            entry->user_input.cursor = new_cursor;

            if (entry->user_input.visible_lo > 0) {
                entry->user_input.visible_lo -= 1;
                entry->user_input.visible_hi -= 1;
            }
        }
    }
}

void ui_text_input(const char *text) {
    menu_entry_t *entry = &current_menu->entries[current_menu->position];
    
    if (entry->type != INPUT)
        return;
    
    char key = text[0];

    if (entry->user_input.is_numeric && (key < '0' || key > '9'))
        return;
    if (strlen(entry->user_input.input) >= entry->user_input.max_length)
        return;

    char c = entry->user_input.input[entry->user_input.cursor];
    entry->user_input.input[entry->user_input.cursor++] = key;
    for (int i = entry->user_input.cursor; i <= entry->user_input.max_length; i++) {
        char temp = entry->user_input.input[i];
        entry->user_input.input[i] = c;
        c = temp;
    }

    entry->user_input.on_input(entry);
}
