#include <stdlib.h>
#include <string.h>

#include "../common/config.h"
#include "../common/utils.h"
#include "ui.h"
#include "../../emulator/emulator.h"

// TODO fix this file (it's ugly code with lots of copy pasted repetitions).

#include "font/font.h"

#define SET_PIXEL_RGBA(buf, config, x, y, color, alpha) \
    { byte_t *_tmp_color_values = emulator_get_color_values_from_palette((config)->color_palette, (color)); \
    *(buf + ((y) * ui->w * 4) + ((x) * 4)) = _tmp_color_values[0]; \
    *(buf + ((y) * ui->w * 4) + ((x) * 4) + 1) = _tmp_color_values[1]; \
    *(buf + ((y) * ui->w * 4) + ((x) * 4) + 2) = _tmp_color_values[2]; \
    *(buf + ((y) * ui->w * 4) + ((x) * 4) + 3) = (alpha); }

static void key_setter_set_key(menu_entry_t *entry, SDL_Keycode key, config_t *config) {
    for (int i = 0; i < entry->parent->length - 1; i++) {
        if (entry == &entry->parent->entries[i]) {
            config->keybindings[entry->setter.button] = key;
            break;
        }
    }

    const char *key_name = SDL_GetKeyName(config->keybindings[entry->setter.button]);
    int l = strlen(key_name);
    entry->setter.key_name = xrealloc(entry->setter.key_name, l + 1);
    snprintf(entry->setter.key_name, l + 1, "%s", key_name);
}

static void key_setter_set_keybind(menu_entry_t *entry, SDL_Keycode key, config_t *config) {
    int same = -1;
    for (int i = 0; i < entry->parent->length - 1; i++) {
        if (entry->parent->position != i && (int) config->keybindings[entry->parent->entries[i].setter.button] == key) {
            same = i;
            break;
        }
    }

    if (same >= 0)
        key_setter_set_key(&entry->parent->entries[same], config->keybindings[entry->setter.button], config);

    key_setter_set_key(entry, key, config);
}

void ui_set_position(ui_t *ui, int pos, int go_up) {
    if (pos < 0)
        ui->current_menu->position = ui->current_menu->length - 1;
    else if (pos > ui->current_menu->length - 1)
        ui->current_menu->position = 0;
    else
        ui->current_menu->position = pos;

    for (byte_t i = 0; ui->current_menu->entries[ui->current_menu->position].disabled && i < ui->current_menu->length; i++) {
        if (go_up)
            ui->current_menu->position = ui->current_menu->position - 1 < 0 ? ui->current_menu->length - 1 : ui->current_menu->position - 1;
        else
            ui->current_menu->position = (ui->current_menu->position + 1) % ui->current_menu->length;
    }
}

void ui_back_to_root_menu(ui_t *ui) {
    if (ui->current_menu->entries[ui->current_menu->position].type == UI_KEY_SETTER)
        ui->current_menu->entries[ui->current_menu->position].setter.editing = 0;
    ui->current_menu = ui->root_menu;
    ui_set_position(ui, 0, 0);
}

static void back_to_prev_menu(ui_t *ui) {
    if (ui->current_menu == ui->root_menu)
        ui_set_position(ui, 0, 0);
    else
        ui->current_menu = ui->current_menu->parent;
}

static void menu_set_parents(menu_t *menu) {
    for (byte_t i = 0; i < menu->length; i++) {
        menu->entries[i].parent = menu;
        if (menu->entries[i].type == UI_SUBMENU) {
            menu->entries[i].submenu->parent = menu;
            menu_set_parents(menu->entries[i].submenu);
        }
    }
}

static void init_keysetters(menu_t *menu, config_t *config) {
    for (int i = 0; i < menu->length; i++) {
        if (menu->entries[i].type == UI_SUBMENU)
            init_keysetters(menu->entries[i].submenu, config);
        if (menu->entries[i].type == UI_KEY_SETTER)
            key_setter_set_key(&menu->entries[i], config->keybindings[menu->entries[i].setter.button], config);
    }
}

static void init_choices(menu_t *menu) {
    for (int i = 0; i < menu->length; i++) {
        if (menu->entries[i].type == UI_SUBMENU)
            init_choices(menu->entries[i].submenu);
        if (menu->entries[i].type == UI_CHOICE) {
            menu->entries[i].choices.length = 1;
            char *str = strchr(menu->entries[i].label, '|');
            for (int j = 0; str[j]; j++)
                menu->entries[i].choices.length += str[j] == ',';
        }
    }
}

ui_t *ui_init(menu_t *menu, int w, int h, config_t *config) {
    ui_t *ui = xmalloc(sizeof(ui_t));
    ui->config = config;
    ui->pixels = xmalloc(w * h * 4);
    ui->root_menu = menu;
    ui->w = w;
    ui->h = h;
    ui->blink_counter = 0;
    ui->current_menu = ui->root_menu;
    ui->root_menu->parent = NULL;
    menu_set_parents(ui->root_menu);
    init_keysetters(ui->root_menu, config);
    init_choices(ui->root_menu);
    ui_set_position(ui, 0, 0);
    return ui;
}

void ui_free(ui_t *ui) {
    free(ui->pixels);
    free(ui);
}

static void print_cursor(ui_t *ui, int x, int y, dmg_color_t color) {
    for (int i = 0; i < 8; i++)
        SET_PIXEL_RGBA(ui->pixels, ui->config, x, y + i, color, 0xFF);
}

static void print_char(ui_t *ui, const char c, int x, int y, dmg_color_t color) {
    int index = c - 32;
    if (index < 0 || index >= 0x5F) return;
    
    const byte_t *char_data = font[index];
    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 8; j++)
            if (GET_BIT(char_data[j], SDL_abs(i - 7)))
                SET_PIXEL_RGBA(ui->pixels, ui->config, x + i, y + j, color, 0xFF);
}

static void print_text(ui_t *ui, const char *text, int x, int y, dmg_color_t color) {
    for (int i = 0; text[i]; i++) {
        if (text[i] == '|')
            return;
        print_char(ui, text[i], x + (i * 8), y, color);
    }
}

static void ui_clear(ui_t *ui) {
    for (int i = 0; i < ui->w; i++)
        for (int j = 0; j < ui->h; j++)
            SET_PIXEL_RGBA(ui->pixels, ui->config, i, j, DMG_BLACK, 0xD5);
}

static void print_choice(ui_t *ui, const char *choices, int x, int y, int n, dmg_color_t text_color, dmg_color_t arrow_color) {
    int delim_count = 0;
    int printed_char_count = 1;
    print_char(ui, '<', x, y, arrow_color);
    for (int i = 0; choices[i]; i++) {
        if (choices[i] == ',') {
            delim_count++;
            continue;
        }
        if (delim_count < n)
            continue;
        if (delim_count > n)
            break;
        print_char(ui, choices[i], x + (printed_char_count * 8), y, text_color);
        printed_char_count++;
    }
    print_char(ui, '>', x + (printed_char_count * 8), y, arrow_color);
}

void ui_draw(ui_t *ui) {
    ui->blink_counter++;
    if (ui->blink_counter > 60)
        ui->blink_counter = 0;

    // clear ui pixels
    ui_clear(ui);

    byte_t title_x = (ui->w / 2) - ((strlen(ui->current_menu->title) * 8) / 2);
    byte_t labels_start_y = 72 - ((ui->current_menu->length * 8) / 2);
    if (labels_start_y < 48)
        labels_start_y = 48;

    print_text(ui, ui->current_menu->title, title_x, 32, DMG_WHITE);
    print_text(ui, ">", 0, labels_start_y + (8 * ui->current_menu->position), DMG_WHITE);
    for (byte_t i = 0; i < ui->current_menu->length; i++) {
        menu_entry_t *entry = &ui->current_menu->entries[i];
        byte_t y = labels_start_y + (i * 8);
        dmg_color_t text_color = entry->disabled ? DMG_DARK_GRAY : DMG_WHITE;
        print_text(ui, entry->label, 8, y, text_color);

        int delim_index;
        byte_t x;
        switch (entry->type) {
        case UI_CHOICE:
            delim_index = strcspn(entry->label, "|");
            char *choices = &entry->label[delim_index + 1];
            print_choice(ui, choices, (delim_index * 8) + 8, y, entry->choices.position, text_color, entry->disabled ? DMG_DARK_GRAY : DMG_LIGHT_GRAY);
            if (ui->current_menu->position == i && entry->choices.description)
                print_text(ui, entry->choices.description, (ui->w / 2) - ((strlen(entry->choices.description) * 8) / 2), ui->h - 16, DMG_LIGHT_GRAY);
            break;
        case UI_KEY_SETTER:
            if (!entry->setter.editing)
                print_text(ui, entry->setter.key_name, ui->w - (strlen(entry->setter.key_name) * 8) - 8, y, DMG_WHITE);
            else if (ui->blink_counter > 30)
                print_text(ui, entry->setter.key_name, ui->w - (strlen(entry->setter.key_name) * 8) - 8, y, DMG_WHITE);
            break;
        case UI_INPUT:
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

            print_text(ui, visible_input, x, y, text_color);
            if (ui->current_menu->position == i && ui->blink_counter < 30)
                print_cursor(ui, x + ((entry->user_input.cursor - entry->user_input.visible_lo) * 8), y, DMG_WHITE);
            break;
        }
    }

    // TODO bottom right corner label status: "Link: Inactive, Waiting, Active"
    // for this an access to link status is needed (not possible yet)
    // print_text("Link:", 0, 0, DMG_WHITE);
    // print_text("I", 40, 0, DMG_WHITE);
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








// TODO universal ui_press function that first converts keyboard/controller inputs into ui inputs
// needs a controller to test so this needs to wait until I can get my controller back
typedef enum {
    VALIDATE, // enter menu, activate action entry, etc.
    CANCEL, // leave menu
    LEFT,
    RIGHT,
    UP,
    DOWN,
} ui_input_t;




void ui_press_joypad(ui_t *ui, joypad_button_t key) {
    int new_pos, new_cursor, len;
    menu_entry_t *entry = &ui->current_menu->entries[ui->current_menu->position];

    if (entry->type == UI_KEY_SETTER && entry->setter.editing)
        return;

    switch (key) {
    case JOYPAD_RIGHT:
    case JOYPAD_LEFT:
        switch (ui->current_menu->entries[ui->current_menu->position].type) {
        case UI_CHOICE:
            new_pos = key == JOYPAD_RIGHT ? entry->choices.position + 1 : entry->choices.position - 1;
            if (new_pos < 0)
                new_pos = entry->choices.length - 1;
            else if (new_pos > entry->choices.length - 1)
                new_pos = 0;
            entry->choices.position = new_pos;
            (entry->choices.choose)(entry);
            break;
        case UI_INPUT:
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
        ui_set_position(ui, ui->current_menu->position - 1, 1);
        if (ui->current_menu->entries[ui->current_menu->position].type == UI_INPUT)
            ui->current_menu->entries[ui->current_menu->position].user_input.cursor = strlen(ui->current_menu->entries[ui->current_menu->position].user_input.input);
        break;
    case JOYPAD_DOWN:
        ui_set_position(ui, ui->current_menu->position + 1, 0);
        if (ui->current_menu->entries[ui->current_menu->position].type == UI_INPUT)
            ui->current_menu->entries[ui->current_menu->position].user_input.cursor = strlen(ui->current_menu->entries[ui->current_menu->position].user_input.input);
        break;
    case JOYPAD_A:
        if (entry->type == UI_INPUT)
            break;
        switch (entry->type) {
        case UI_ACTION:
            (entry->action)(entry);
            break;
        case UI_KEY_SETTER:
            entry->setter.editing = 1;
            break;
        case UI_SUBMENU:
            ui->current_menu = entry->submenu;
            ui_set_position(ui, 0, 0);
            break;
        case UI_BACK:
            back_to_prev_menu(ui);
            break;
        }
        break;
    case JOYPAD_B:
        if (entry->type != UI_INPUT)
            back_to_prev_menu(ui);
        break;
    default:
        break;
    }
}

static void ui_press(ui_t *ui, int key, int repeat, int is_controller) {
    menu_entry_t *entry = &ui->current_menu->entries[ui->current_menu->position];
    ui->blink_counter = 0;

    int new_pos, new_cursor, len;
    switch (key) {
    case JOYPAD_RIGHT:
    case JOYPAD_LEFT:
        switch (ui->current_menu->entries[ui->current_menu->position].type) {
        case UI_CHOICE:
            new_pos = key == JOYPAD_RIGHT ? entry->choices.position + 1 : entry->choices.position - 1;
            if (new_pos < 0)
                new_pos = entry->choices.length - 1;
            else if (new_pos > entry->choices.length - 1)
                new_pos = 0;
            entry->choices.position = new_pos;
            (entry->choices.choose)(entry);
            break;
        case UI_INPUT:
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
        ui_set_position(ui, ui->current_menu->position - 1, 1);
        if (ui->current_menu->entries[ui->current_menu->position].type == UI_INPUT)
            ui->current_menu->entries[ui->current_menu->position].user_input.cursor = strlen(ui->current_menu->entries[ui->current_menu->position].user_input.input);
        break;
    case JOYPAD_DOWN:
        ui_set_position(ui, ui->current_menu->position + 1, 0);
        if (ui->current_menu->entries[ui->current_menu->position].type == UI_INPUT)
            ui->current_menu->entries[ui->current_menu->position].user_input.cursor = strlen(ui->current_menu->entries[ui->current_menu->position].user_input.input);
        break;
    case JOYPAD_A:
        if (entry->type == UI_INPUT)
            break;
        // fall through
    case SDLK_RETURN:
    case SDLK_KP_ENTER:
        switch (entry->type) {
        case UI_ACTION:
            (entry->action)(entry);
            break;
        case UI_KEY_SETTER:
            if (!is_controller)
                entry->setter.editing = 1;
            break;
        case UI_SUBMENU:
            ui->current_menu = entry->submenu;
            ui_set_position(ui, 0, 0);
            break;
        case UI_BACK:
            back_to_prev_menu(ui);
            break;
        }
        break;
    case JOYPAD_B:
        if (entry->type != UI_INPUT)
            back_to_prev_menu(ui);
        break;
    }

    if (entry->type == UI_INPUT) {
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

void ui_keyboard_press(ui_t *ui, SDL_KeyboardEvent *keyevent) {
    SDL_Keysym keysym = keyevent->keysym;
    int key = keycode_to_joypad(ui->config, keysym.sym);
    if (key < 0) key = keysym.sym;
    menu_entry_t *entry = &ui->current_menu->entries[ui->current_menu->position];

    if (entry->type == UI_KEY_SETTER && entry->setter.editing) {
        if (ui->config->keycode_filter(keysym.sym))
            key_setter_set_keybind(entry, keysym.sym, ui->config);
        entry->setter.editing = 0;
        return;
    }

    ui_press(ui, key, keyevent->repeat, 0);
}

void ui_controller_press(ui_t *ui, int button) {
    ui_press(ui, button, 0, 1);
}

void ui_text_input(ui_t *ui, const char *text) {
    menu_entry_t *entry = &ui->current_menu->entries[ui->current_menu->position];
    
    if (entry->type != UI_INPUT)
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
