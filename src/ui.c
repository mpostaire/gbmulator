#include <stdlib.h>
#include <string.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include "gbmulator.h"
#include "config.h"
#include "emulator/emulator.h"

// TODO fix this file (it's ugly code).

#define SET_PIXEL_RGBA(buf, x, y, color, alpha) \
    byte_t *_tmp_color_values = emulator_get_color_values((color)); \
    *(buf + ((y) * GB_SCREEN_WIDTH * 4) + ((x) * 4)) = _tmp_color_values[0]; \
    *(buf + ((y) * GB_SCREEN_WIDTH * 4) + ((x) * 4) + 1) = _tmp_color_values[1]; \
    *(buf + ((y) * GB_SCREEN_WIDTH * 4) + ((x) * 4) + 2) = _tmp_color_values[2]; \
    *(buf + ((y) * GB_SCREEN_WIDTH * 4) + ((x) * 4) + 3) = (alpha);

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

byte_t ui_pixels[GB_SCREEN_WIDTH * GB_SCREEN_HEIGHT * 4];

byte_t blink_counter = 0;

enum menu_type {
    ACTION, // do something when A is pressed
    INPUT,  // add char to text input when any key is pressed (except UP/DOWN/LEFT/RIGHT/A/B)
    KEY_SETTER, // press ACTION key to enter edit mode, accept any unique key as input (swaps value if another KEY_SETTER in the same menu has already the same value), then leave edit mode 
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
static void start_link(void);
static void open_rom(void);
static void on_input_set_keybind(menu_entry_t *entry, SDL_Keycode key);
static void back_to_prev_menu(void);

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
        struct {
            byte_t editing;
            SDL_Keycode key;
            char *key_name;
            void (*on_input)(menu_entry_t *, SDL_Keycode);
        } setter;
    };
};

struct menu {
    char *title;
    byte_t position;
    byte_t length;
    menu_entry_t entries[];
};

menu_t link_menu = {
    .title = "Link cable",
    .length = 5,
    .entries = {
        { "Mode:     |server,client", CHOICE, .choices = { choose_link_mode, 2, 0 } },
        { "Host: ", INPUT, .disabled = 1, .user_input.on_input = on_input_link_host },
        { "Port: ", INPUT, .user_input = { .is_numeric = 1, .on_input = on_input_link_port } },
        { "Start link", ACTION, .action = start_link },
        { "Back...", ACTION, .action = back_to_prev_menu }
    }
};

menu_t options_menu = {
    .title = "Options",
    .length = 5,
    .entries = {
        { "Scale:      | 1x , 2x , 3x , 4x , 5x ", CHOICE, .choices = { choose_win_scale, 5, 0 } },
        { "Speed:      |1.0x,1.5x,2.0x,2.5x,3.0x,3.5x,4.0x", CHOICE, .choices = { choose_speed, 7, 0 } },
        { "Sound:      | OFF, 25%, 50%, 75%,100%", CHOICE, .choices = { choose_sound, 5, 4 } },
        { "Color:      |gray,orig", CHOICE, .choices = { choose_color, 2, 0 } },
        { "Back...", ACTION, .action = back_to_prev_menu }
    }
};

menu_t keybindings_menu = {
    .title = "Keybindings",
    .length = 9,
    .entries = {
        { "LEFT:", KEY_SETTER, .setter.on_input = on_input_set_keybind },
        { "RIGHT:", KEY_SETTER, .setter.on_input = on_input_set_keybind },
        { "UP:", KEY_SETTER, .setter.on_input = on_input_set_keybind },
        { "DOWN:", KEY_SETTER, .setter.on_input = on_input_set_keybind },
        { "A:", KEY_SETTER, .setter.on_input = on_input_set_keybind },
        { "B:", KEY_SETTER, .setter.on_input = on_input_set_keybind },
        { "START:", KEY_SETTER, .setter.on_input = on_input_set_keybind },
        { "SELECT:", KEY_SETTER, .setter.on_input = on_input_set_keybind },
        { "Back...", ACTION, .action = back_to_prev_menu }
    }
};

menu_t main_menu = {
    .title = "GBmulator",
    .length = 6,
    .entries = {
        { "Resume", ACTION, .action = gbmulator_unpause },
        { "Open ROM...", ACTION, .action = open_rom },
        { "Link cable...", SUBMENU, .submenu = &link_menu },
        { "Options...", SUBMENU, .submenu = &options_menu },
        { "Keybindings...", SUBMENU, .submenu = &keybindings_menu },
        { "Exit", ACTION, .action = gbmulator_exit }
    }
};

menu_t *current_menu = &main_menu;

static SDL_Keycode get_config_val(joypad_button_t button) {
    switch (button) {
        case JOYPAD_LEFT: return config.left;
        case JOYPAD_RIGHT: return config.right;
        case JOYPAD_UP: return config.up;
        case JOYPAD_DOWN: return config.down;
        case JOYPAD_A: return config.a;
        case JOYPAD_B: return config.b;
        case JOYPAD_START: return config.start;
        case JOYPAD_SELECT: return config.select;
        default: return -1;
    }
}

static SDL_Keycode *sdl_key_to_config_pointer(SDL_Keycode key) {
    if (key == config.left) return &config.left;
    if (key == config.right) return &config.right;
    if (key == config.up) return &config.up;
    if (key == config.down) return &config.down;
    if (key == config.a) return &config.a;
    if (key == config.b) return &config.b;
    if (key == config.start) return &config.start;
    if (key == config.select) return &config.select;
    return NULL;
}

static void key_setter_set_key(menu_entry_t *entry, SDL_Keycode key) {
    for (int i = 0; i < keybindings_menu.length - 1; i++) {
        if (entry == &keybindings_menu.entries[i]) {
            SDL_Keycode *config_pointer = sdl_key_to_config_pointer(entry->setter.key);
            if (config_pointer)
                *config_pointer = key;
            break;
        }
    }

    entry->setter.key = key;
    const char *key_name = SDL_GetKeyName(entry->setter.key);
    int l = strlen(key_name);
    if (entry->setter.key_name)
        free(entry->setter.key_name);
    entry->setter.key_name = xmalloc(l + 1);
    snprintf(entry->setter.key_name, l + 1, "%s", key_name);
}

static void choose_win_scale(menu_entry_t *entry) {
    config.scale = entry->choices.position + 1;
}

static void choose_speed(menu_entry_t *entry) {
    config.speed = (entry->choices.position * 0.5f) + 1;
    emulator_set_apu_sampling_freq_multiplier(config.speed);
}

static void choose_sound(menu_entry_t *entry) {
    config.sound = entry->choices.position * 0.25f;
    emulator_set_apu_sound_level(config.sound);
}

static void choose_color(menu_entry_t *entry) {
    emulator_update_pixels_with_palette(entry->choices.position);
    emulator_set_color_palette(entry->choices.position);
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
        success = emulator_connect_to_link(config.link_host, config.link_port);
    else
        success = emulator_start_link(config.link_port);
    
    if (success){
        link_menu.entries[0].disabled = 1;
        link_menu.entries[1].disabled = 1;
        link_menu.entries[2].disabled = 1;
        link_menu.entries[3].disabled = 1;
        link_menu.position = 4;
    }
}

static void open_rom(void) {
    #ifdef __EMSCRIPTEN__
    EM_ASM(
        var file_selector = document.createElement('input');
        file_selector.setAttribute('type', 'file');
        file_selector.setAttribute('onchange','open_file(event)');
        file_selector.setAttribute('accept','.gb,.gbc'); // optional - limit accepted file types 
        file_selector.click();
    );
    #else
    // TODO
    printf("Open rom not implemented yet -- use command line argument instead\n");
    #endif
}

static void on_input_set_keybind(menu_entry_t *entry, SDL_Keycode key) {
    int same = -1;
    for (int i = 0; i < keybindings_menu.length - 1; i++) {
        if (keybindings_menu.position != i && keybindings_menu.entries[i].setter.key == key) {
            same = i;
            break;
        }
    }

    if (same >= 0)
        key_setter_set_key(&keybindings_menu.entries[same], entry->setter.key);

    key_setter_set_key(entry, key);
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
    options_menu.entries[2].choices.position = config.sound * 4;
    options_menu.entries[3].choices.position = emulator_get_color_palette();

    link_menu.entries[1].user_input.input = xmalloc(40);
    snprintf(link_menu.entries[1].user_input.input, sizeof(config.link_host), "%s", config.link_host);

    link_menu.entries[1].user_input.cursor = strlen(config.link_host);
    link_menu.entries[1].user_input.max_length = 39;
    link_menu.entries[1].user_input.visible_hi = 12;

    char **link_port_buf = &link_menu.entries[2].user_input.input;
    *link_port_buf = xmalloc(6);

    link_menu.entries[2].user_input.cursor = snprintf(*link_port_buf, 6, "%d", config.link_port);
    link_menu.entries[2].user_input.input = *link_port_buf;
    link_menu.entries[2].user_input.max_length = 5;
    link_menu.entries[2].user_input.visible_hi = 5;

    for (int i = 0; i < keybindings_menu.length - 1; i++)
        key_setter_set_key(&keybindings_menu.entries[i], get_config_val(i));

    return ui_pixels;
}

static void print_cursor(int x, int y, color_t color) {
    for (int i = 0; i < 8; i++) {
        SET_PIXEL_RGBA(ui_pixels, x, y + i, color, 0xFF);
    }
}

void print_char(const char c, int x, int y, color_t color) {
    int index = c - 32;
    if (index < 0 || index >= 0x5F) return;
    
    const byte_t *char_data = font[index];
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            if (GET_BIT(char_data[j], SDL_abs(i - 7))) {
                SET_PIXEL_RGBA(ui_pixels, x + i, y + j, color, 0xFF);
            }
        }
    }
}

void print_text(const char *text, int x, int y, color_t color) {
    for (int i = 0; text[i]; i++) {
        if (text[i] == '|')
            return;
        print_char(text[i], x + (i * 8), y, color);
    }
}

static void ui_clear(void) {
    for (int i = 0; i < GB_SCREEN_WIDTH; i++) {
        for (int j = 0; j < GB_SCREEN_HEIGHT; j++) {
            SET_PIXEL_RGBA(ui_pixels, i, j, BLACK, 0xD5);
        }
    }
}

static void print_choice(const char *choices, int x, int y, int n, color_t text_color, color_t arrow_color) {
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
    blink_counter++;
    if (blink_counter > 60)
        blink_counter = 0;

    // clear ui pixels
    ui_clear();

    byte_t title_x = 72 - ((strlen(current_menu->title) * 8) / 2);
    byte_t labels_start_y = 72 - ((current_menu->length * 8) / 2);
    if (labels_start_y < 48)
        labels_start_y = 48;

    print_text(current_menu->title, title_x, 32, WHITE);
    print_text(">", 0, labels_start_y + (8 * current_menu->position), WHITE);
    for (byte_t i = 0; i < current_menu->length; i++) {
        menu_entry_t *entry = &current_menu->entries[i];
        byte_t y = labels_start_y + (i * 8);
        color_t text_color = entry->disabled ? DARK_GRAY : WHITE;
        print_text(entry->label, 8, y, text_color);

        int delim_index;
        byte_t x;
        switch (entry->type) {
        case CHOICE:
            delim_index = strcspn(entry->label, "|");
            char *choices = &entry->label[delim_index + 1];
            print_choice(choices, (delim_index * 8) + 8, y, entry->choices.position, text_color, entry->disabled ? DARK_GRAY : LIGHT_GRAY);
            break;
        case KEY_SETTER:
            if (!entry->setter.editing)
                print_text(entry->setter.key_name, GB_SCREEN_WIDTH - (strlen(entry->setter.key_name) * 8) - 8, y, WHITE);
            else if (blink_counter > 30)
                print_text(entry->setter.key_name, GB_SCREEN_WIDTH - (strlen(entry->setter.key_name) * 8) - 8, y, WHITE);
            break;
        case INPUT:
            x = (strlen(entry->label) * 8) + 8;

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
            if (current_menu->position == i && blink_counter < 30)
                print_cursor(x + ((entry->user_input.cursor - entry->user_input.visible_lo) * 8), y, WHITE);
            break;
        }
    }

    // TODO bottom right corner label status: "Link: Inactive, Waiting, Active"
    // for this an access to link status is needed (not possible yet)
    // print_text("Link:", 0, 0, WHITE);
    // print_text("I", 40, 0, WHITE);
}

static void delete_char_at(char **text, byte_t n) {
    if (n >= strlen(*text))
        return;

    int i = n;
    for (; (*text)[i + 1]; i++) {
        (*text)[i] = (*text)[i + 1];
    }
    (*text)[i] = '\0';
}

void ui_press(SDL_Keysym *keysym) {
    int key = sdl_key_to_joypad(keysym->sym);
    int count;
    menu_entry_t *entry = &current_menu->entries[current_menu->position];
    blink_counter = 0;

    if (entry->type == KEY_SETTER && entry->setter.editing) {
        switch (keysym->sym) {
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
        case SDLK_DELETE:
        case SDLK_BACKSPACE:
        case SDLK_PAUSE:
        case SDLK_ESCAPE:
            break;
        default:
            entry->setter.on_input(entry, keysym->sym);
            break;
        }
        entry->setter.editing = 0;
        return;
    }

    int new_pos, new_cursor, len;
    switch (key) {
    case JOYPAD_RIGHT:
    case JOYPAD_LEFT:
        switch (current_menu->entries[current_menu->position].type) {
        case CHOICE:
            new_pos = key == JOYPAD_RIGHT ? entry->choices.position + 1 : entry->choices.position - 1;
            if (new_pos < 0)
                new_pos = entry->choices.length - 1;
            else if (new_pos > entry->choices.length - 1)
                new_pos = 0;
            entry->choices.position = new_pos;
            (entry->choices.choose)(entry);
            break;
        case INPUT:
            new_cursor = entry->user_input.cursor + (key == JOYPAD_RIGHT ? 1 : -1);
            len = strlen(entry->user_input.input);
            if (new_cursor > len)
                new_cursor = len;
            else if (new_cursor < 0)
                new_cursor = 0;
            entry->user_input.cursor = new_cursor;
        }
        break;
    case JOYPAD_UP:
        count = 0;
        do {
            current_menu->position = current_menu->position - 1 < 0 ? current_menu->length - 1 : current_menu->position - 1;
            count++;
        } while(current_menu->entries[current_menu->position].disabled && count < current_menu->length);
        if (current_menu->entries[current_menu->position].type == INPUT)
            current_menu->entries[current_menu->position].user_input.cursor = strlen(current_menu->entries[current_menu->position].user_input.input);
        break;
    case JOYPAD_DOWN:
        count = 0;
        do {
            current_menu->position = (current_menu->position + 1) % current_menu->length;
            count++;
        } while(current_menu->entries[current_menu->position].disabled && count < current_menu->length);
        if (current_menu->entries[current_menu->position].type == INPUT)
            current_menu->entries[current_menu->position].user_input.cursor = strlen(current_menu->entries[current_menu->position].user_input.input);
        break;
    case JOYPAD_A:
        if (entry->type == INPUT)
            break;
    case SDLK_RETURN:
    case SDLK_KP_ENTER:
        switch (entry->type) {
        case ACTION:
            (entry->action)();
            break;
        case KEY_SETTER:
            entry->setter.editing = 1;
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
    case JOYPAD_B:
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
            new_cursor = entry->user_input.cursor - 1;
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
