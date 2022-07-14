#pragma once

#include <SDL2/SDL.h>

#include "emulator/emulator.h"

typedef struct menu_t menu_t;
typedef struct menu_entry_t menu_entry_t;

typedef enum {
    UI_ACTION,     // do something when A is pressed
    UI_INPUT,      // add char to text input when any key is pressed (except UP/DOWN/LEFT/RIGHT/A/B)
    UI_KEY_SETTER, // press ACTION key to enter edit mode, accept any unique key as input (swaps value if another KEY_SETTER in the same menu has already the same value), then leave edit mode
    UI_CHOICE,     // change value when LEFT/RIGHT are pressed
    UI_SUBMENU,    // enter a submenu when A is pressed
    UI_BACK        // back to previous menu
} ui_menu_type_t;

struct menu_entry_t {
    char *label;
    byte_t type;
    byte_t disabled;
    menu_t *parent;
    union {
        void (*action)(menu_entry_t *);
        menu_t *submenu;
        struct {
            void (*choose)(menu_entry_t *);
            byte_t length;
            byte_t position;
            char *description;
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
            char *key_name;
            SDL_Keycode *config_key;
        } setter;
    };
};

struct menu_t {
    char *title;
    byte_t position;
    byte_t length;
    menu_t *parent;
    menu_entry_t entries[16]; // 16 max entries per menu (not a variable array to allow declarative syntax)
};

typedef struct {
    menu_t *root_menu;
    menu_t *current_menu;
    byte_t *pixels;
    byte_t blink_counter;
    byte_t w;
    byte_t h;
} ui_t;

ui_t *ui_init(menu_t *menu, int w, int h);

void ui_free(ui_t *ui);

void ui_draw(ui_t *ui);

void ui_set_position(ui_t *ui, int pos, int go_up);

void ui_press_joypad(ui_t *ui, joypad_button_t key);

void ui_keyboard_press(ui_t *ui, SDL_KeyboardEvent *keyevent);

void ui_controller_press(ui_t *ui, int button);

void ui_text_input(ui_t *ui, const char *text);
