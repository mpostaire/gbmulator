#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "ui.h"
#include "utils.h"
#include "emulator/emulator.h"

// TODO fix this file (it's ugly code with lots of copy pasted repetitions).

#define SET_PIXEL_RGBA(buf, x, y, color, alpha) \
    { byte_t *_tmp_color_values = emulator_get_color_values_from_palette(config.color_palette, (color)); \
    *(buf + ((y) * ui->w * 4) + ((x) * 4)) = _tmp_color_values[0]; \
    *(buf + ((y) * ui->w * 4) + ((x) * 4) + 1) = _tmp_color_values[1]; \
    *(buf + ((y) * ui->w * 4) + ((x) * 4) + 2) = _tmp_color_values[2]; \
    *(buf + ((y) * ui->w * 4) + ((x) * 4) + 3) = (alpha); }

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












static void key_setter_set_key(menu_entry_t *entry, SDL_Keycode key) {
    for (int i = 0; i < entry->parent->length - 1; i++) {
        if (entry == &entry->parent->entries[i]) {
            *entry->setter.config_key = key;
            break;
        }
    }

    const char *key_name = SDL_GetKeyName(*entry->setter.config_key);
    int l = strlen(key_name);
    entry->setter.key_name = xrealloc(entry->setter.key_name, l + 1);
    snprintf(entry->setter.key_name, l + 1, "%s", key_name);
}

static void key_setter_set_keybind(menu_entry_t *entry, SDL_Keycode key) {
    int same = -1;
    for (int i = 0; i < entry->parent->length - 1; i++) {
        if (entry->parent->position != i && *entry->parent->entries[i].setter.config_key == key) {
            same = i;
            break;
        }
    }

    if (same >= 0)
        key_setter_set_key(&entry->parent->entries[same], *entry->setter.config_key);

    key_setter_set_key(entry, key);
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

static void init_keysetters(menu_t *menu) {
    for (int i = 0; i < menu->length; i++) {
        if (menu->entries[i].type == UI_SUBMENU)
            init_keysetters(menu->entries[i].submenu);
        if (menu->entries[i].type == UI_KEY_SETTER)
            key_setter_set_key(&menu->entries[i], *menu->entries[i].setter.config_key);
    }
}

ui_t *ui_init(menu_t *menu, int w, int h) {
    ui_t *ui = xmalloc(sizeof(ui_t));
    ui->pixels = xmalloc(w * h * 4);
    ui->root_menu = menu;
    ui->w = w;
    ui->h = h;
    ui->blink_counter = 0;
    ui->current_menu = ui->root_menu;
    ui->root_menu->parent = NULL;
    menu_set_parents(ui->root_menu);
    init_keysetters(ui->root_menu);
    ui_set_position(ui, 0, 0);
    return ui;
}

void ui_free(ui_t *ui) {
    free(ui->pixels);
    free(ui);
}

static void print_cursor(ui_t *ui, int x, int y, dmg_color_t color) {
    for (int i = 0; i < 8; i++)
        SET_PIXEL_RGBA(ui->pixels, x, y + i, color, 0xFF);
}

static void print_char(ui_t *ui, const char c, int x, int y, dmg_color_t color) {
    int index = c - 32;
    if (index < 0 || index >= 0x5F) return;
    
    const byte_t *char_data = font[index];
    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 8; j++)
            if (GET_BIT(char_data[j], SDL_abs(i - 7)))
                SET_PIXEL_RGBA(ui->pixels, x + i, y + j, color, 0xFF);
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
            SET_PIXEL_RGBA(ui->pixels, i, j, DMG_BLACK, 0xD5);
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
    int key = sdl_key_to_joypad(keysym.sym);
    menu_entry_t *entry = &ui->current_menu->entries[ui->current_menu->position];

    if (entry->type == UI_KEY_SETTER && entry->setter.editing) {
        if (config_verif_key(keysym.sym))
            key_setter_set_keybind(entry, keysym.sym);
        entry->setter.editing = 0;
        return;
    }

    ui_press(ui, key, keyevent->repeat, 0);
}

void ui_controller_press(ui_t *ui, int button) {
    ui_press(ui, sdl_controller_to_joypad(button), 0, 1);
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
