#include <stdlib.h>
#include <stdio.h>
#include <SDL.h>
#include <android/log.h>
#include <jni.h>

#define log(...) __android_log_print(ANDROID_LOG_INFO, "GBmulator", __VA_ARGS__)

// going higher than 2048 starts to add noticeable audio lag
#define APU_SAMPLE_COUNT 2048

// TODO make ui in an android activity with android ui api (no need for keybindings/scale configs but need for landscape/portrait layouts config)
// TODO savestates ui
// TODO bluetooth link cable
// TODO test with gamepad controller (PS3, ...)
// TODO replace all printf() by __android_log_print()

// TODO button (and screen??) layout designer for both landscape and portrait (like drastic ds emulator)
//      add to config file the layouts: each element has a normalized x,y coordinates and width/heigth.
//      --> meaning the coordinates are in percent of the screen (ex: x is in percent of the screen width, ...)

// TODO ???? bigger finger hitbox (circle instead of point --> button pressed is the one closer to the center of the circle)
//      OR maybe just bigger button hitbox (check drastic ds emulator to see how it's done)

#include "../common/ui.h"
#include "../common/config.h"
#include "../common/utils.h"
#include "emulator/emulator.h"

SDL_bool is_running;
SDL_bool is_paused;
SDL_bool is_rom_loaded;
SDL_bool is_landscape;

SDL_Window *window;

SDL_Texture *ppu_texture;
int ppu_texture_pitch;

SDL_AudioDeviceID audio_device;

SDL_GameController *pad;
SDL_bool is_controller_present;

char *rom_title;

ui_t *ui;
emulator_t *emu;

SDL_Rect gb_screen_rect = {
    .w = 1080, // TODO make smallest possible size and let SDL stretch to screen OR use phone actual width OR do not change
    .h = GB_SCREEN_HEIGHT * (1080 / GB_SCREEN_WIDTH),
    .x = 0,
    .y = 0,
};

typedef struct {
    SDL_Rect shape;
    joypad_button_t button;
    byte_t r;
    byte_t g;
    byte_t b;
} button_t;

button_t buttons[] = {
    {
        .shape = {
            .h = 128,
            .w = 128
        },
        .r = 255,
        .g = 0,
        .b = 0,
        .button = JOYPAD_LEFT
    },
    {
        .shape = {
            .h = 128,
            .w = 128
        },
        .r = 255,
        .g = 0,
        .b = 0,
        .button = JOYPAD_RIGHT
    },
    {
        .shape = {
            .h = 128,
            .w = 128
        },
        .r = 255,
        .g = 0,
        .b = 0,
        .button = JOYPAD_UP
    },
    {
        .shape = {
            .h = 128,
            .w = 128
        },
        .r = 255,
        .g = 0,
        .b = 0,
        .button = JOYPAD_DOWN
    },
    {
        .shape = {
            .h = 128,
            .w = 128
        },
        .r = 255,
        .g = 0,
        .b = 0,
        .button = JOYPAD_A
    },
    {
        .shape = {
            .h = 128,
            .w = 128
        },
        .r = 255,
        .g = 0,
        .b = 0,
        .button = JOYPAD_B
    },
    {
        .shape = {
            .h = 128,
            .w = 128
        },
        .r = 255,
        .g = 0,
        .b = 0,
        .button = JOYPAD_START
    },
    {
        .shape = {
            .h = 128,
            .w = 128
        },
        .r = 255,
        .g = 0,
        .b = 0,
        .button = JOYPAD_SELECT
    },

    {
        .shape = {
            .h = 128,
            .w = 128,
        },
        .r = 0,
        .g = 0,
        .b = 255,
        .button = JOYPAD_SELECT + 1 // up left
    },
    {
        .shape = {
            .h = 128,
            .w = 128
        },
        .r = 0,
        .g = 0,
        .b = 255,
        .button = JOYPAD_SELECT + 2 // up right
    },
    {
        .shape = {
            .h = 128,
            .w = 128
        },
        .r = 0,
        .g = 0,
        .b = 255,
        .button = JOYPAD_SELECT + 3 // down left
    },
    {
        .shape = {
            .h = 128,
            .w = 128
        },
        .r = 0,
        .g = 0,
        .b = 255,
        .button = JOYPAD_SELECT + 4 // down right
    },
};

static void draw_buttons(SDL_Renderer *renderer) {
    // only draw the physical buttons (first 8 of the buttons array)
    for (int i = 0; i < 8; i++) {
        SDL_SetRenderDrawColor(renderer, buttons[i].r, buttons[i].g, buttons[i].b, 255);
        SDL_RenderFillRect(renderer, &buttons[i].shape);
    }

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
}

static inline s_byte_t is_finger_over_button(float x, float y) {
    if (is_landscape) {
        x *= 1980;
        y *= 1080;
    } else {
        x *= 1080;
        y *= 1920;
    }

    for (int i = 0; i < 12; i++) {
        SDL_Rect *hitbox = &buttons[i].shape;
        if (x > hitbox->x && x < hitbox->x + hitbox->w && y > hitbox->y && y < hitbox->y + hitbox->h)
            return i;
    }

    return -1;
}

static void button_press(emulator_t *emu, joypad_button_t button) {
    if (is_paused) {
        if (button < JOYPAD_SELECT)
            ui_press_joypad(ui, button);
    } else {
        switch ((int) button) { // cast to int to shut compiler warnings
        case JOYPAD_SELECT + 1:
            emulator_joypad_press(emu, JOYPAD_UP);
            emulator_joypad_press(emu, JOYPAD_LEFT);
            break;
        case JOYPAD_SELECT + 2:
            emulator_joypad_press(emu, JOYPAD_UP);
            emulator_joypad_press(emu, JOYPAD_RIGHT);
            break;
        case JOYPAD_SELECT + 3:
            emulator_joypad_press(emu, JOYPAD_DOWN);
            emulator_joypad_press(emu, JOYPAD_LEFT);
            break;
        case JOYPAD_SELECT + 4:
            emulator_joypad_press(emu, JOYPAD_DOWN);
            emulator_joypad_press(emu, JOYPAD_RIGHT);
            break;
        default:
            emulator_joypad_press(emu, button);
            break;
        }
    }

    switch ((int) button) { // cast to int to shut compiler warnings
    case JOYPAD_SELECT + 1:
        buttons[JOYPAD_UP].r = 0;
        buttons[JOYPAD_UP].g = 255;
        buttons[JOYPAD_LEFT].r = 0;
        buttons[JOYPAD_LEFT].g = 255;
        break;
    case JOYPAD_SELECT + 2:
        buttons[JOYPAD_UP].r = 0;
        buttons[JOYPAD_UP].g = 255;
        buttons[JOYPAD_RIGHT].r = 0;
        buttons[JOYPAD_RIGHT].g = 255;
        break;
    case JOYPAD_SELECT + 3:
        buttons[JOYPAD_DOWN].r = 0;
        buttons[JOYPAD_DOWN].g = 255;
        buttons[JOYPAD_LEFT].r = 0;
        buttons[JOYPAD_LEFT].g = 255;
        break;
    case JOYPAD_SELECT + 4:
        buttons[JOYPAD_DOWN].r = 0;
        buttons[JOYPAD_DOWN].g = 255;
        buttons[JOYPAD_RIGHT].r = 0;
        buttons[JOYPAD_RIGHT].g = 255;
        break;
    default:
        buttons[button].r = 0;
        buttons[button].g = 255;
        break;
    }
}

static void button_release(emulator_t *emu, joypad_button_t button) {
    if (!is_paused) {
        switch ((int) button) { // cast to int to shut compiler warnings
        case JOYPAD_SELECT + 1:
            emulator_joypad_release(emu, JOYPAD_UP);
            emulator_joypad_release(emu, JOYPAD_LEFT);
            break;
        case JOYPAD_SELECT + 2:
            emulator_joypad_release(emu, JOYPAD_UP);
            emulator_joypad_release(emu, JOYPAD_RIGHT);
            break;
        case JOYPAD_SELECT + 3:
            emulator_joypad_release(emu, JOYPAD_DOWN);
            emulator_joypad_release(emu, JOYPAD_LEFT);
            break;
        case JOYPAD_SELECT + 4:
            emulator_joypad_release(emu, JOYPAD_DOWN);
            emulator_joypad_release(emu, JOYPAD_RIGHT);
            break;
        default:
            emulator_joypad_release(emu, button);
            break;
        }
    }

    switch ((int) button) { // cast to int to shut compiler warnings
    case JOYPAD_SELECT + 1:
        buttons[JOYPAD_UP].r = 255;
        buttons[JOYPAD_UP].g = 0;
        buttons[JOYPAD_LEFT].r = 255;
        buttons[JOYPAD_LEFT].g = 0;
        break;
    case JOYPAD_SELECT + 2:
        buttons[JOYPAD_UP].r = 255;
        buttons[JOYPAD_UP].g = 0;
        buttons[JOYPAD_RIGHT].r = 255;
        buttons[JOYPAD_RIGHT].g = 0;
        break;
    case JOYPAD_SELECT + 3:
        buttons[JOYPAD_DOWN].r = 255;
        buttons[JOYPAD_DOWN].g = 0;
        buttons[JOYPAD_LEFT].r = 255;
        buttons[JOYPAD_LEFT].g = 0;
        break;
    case JOYPAD_SELECT + 4:
        buttons[JOYPAD_DOWN].r = 255;
        buttons[JOYPAD_DOWN].g = 0;
        buttons[JOYPAD_RIGHT].r = 255;
        buttons[JOYPAD_RIGHT].g = 0;
        break;
    default:
        buttons[button].r = 255;
        buttons[button].g = 0;
        break;
    }
}

static inline void touch_press(SDL_TouchFingerEvent *event) {
    int hovered = is_finger_over_button(event->x, event->y);
    if (hovered >= 0)
        button_press(emu, hovered);
}

static inline void touch_release(SDL_TouchFingerEvent *event) {
    int hovered = is_finger_over_button(event->x, event->y);
    if (hovered >= 0)
        button_release(emu, hovered);
}

static inline void touch_motion(SDL_TouchFingerEvent *event) {
    int previous = is_finger_over_button(event->x - event->dx, event->y - event->dy);
    int hovered = is_finger_over_button(event->x, event->y);

    if (previous != hovered) {
        if (previous >= 0)
            button_release(emu, previous);
        if (hovered >= 0)
            button_press(emu, hovered);
    }
}

static void set_layout(int layout) {
    is_landscape = layout;

    switch (layout) {
    case 0:
        // portrait
        gb_screen_rect.w = 1080;
        gb_screen_rect.h = GB_SCREEN_HEIGHT * (1080 / GB_SCREEN_WIDTH);
        gb_screen_rect.x = 0;

        // physical buttons
        buttons[0].shape.x = 128 - 128;
        buttons[0].shape.y = 1200 + 128;

        buttons[1].shape.x = 128 + 128;
        buttons[1].shape.y = 1200 + 128;

        buttons[2].shape.x = 128;
        buttons[2].shape.y = 1200;

        buttons[3].shape.x = 128;
        buttons[3].shape.y = 1200 + 128 * 2;

        buttons[4].shape.x = 1080 - 128;
        buttons[4].shape.y = 1200 + 128 / 2;

        buttons[5].shape.x = 1080 - 128 * 2;
        buttons[5].shape.y = 1200 + 128 + 128 / 2;

        buttons[6].shape.x = 1080 / 2 + 128;
        buttons[6].shape.y = 1920 - 128;

        buttons[7].shape.x = 1080 / 2 - 128 * 2;
        buttons[7].shape.y = 1920 - 128;

        // virtual buttons
        buttons[8].shape.x = 128 - 128;
        buttons[8].shape.y = 1200;

        buttons[9].shape.x = 128 + 128;
        buttons[9].shape.y = 1200;

        buttons[10].shape.x = 128 - 128;
        buttons[10].shape.y = 1200 + 128 * 2;

        buttons[11].shape.x = 128 + 128;
        buttons[11].shape.y = 1200 + 128 * 2;
        break;
    case 1:
        // landscape
        gb_screen_rect.h = 1080;
        gb_screen_rect.w = GB_SCREEN_WIDTH * (1080 / GB_SCREEN_HEIGHT);
        gb_screen_rect.x = 1920 / 2 - gb_screen_rect.w / 2;

        // physical buttons
        buttons[0].shape.x = 128 - 128;
        buttons[0].shape.y = 1080 - 128 * 4;

        buttons[1].shape.x = 128 + 128;
        buttons[1].shape.y = 1080 - 128 * 4;

        buttons[2].shape.x = 128;
        buttons[2].shape.y = 1080 - 128 * 5;

        buttons[3].shape.x = 128;
        buttons[3].shape.y = 1080 - 128 * 3;

        buttons[4].shape.x = 1920 - 128;
        buttons[4].shape.y = 1080 - 128 * 4 - 128 / 2;

        buttons[5].shape.x = 1920 - 128 * 2;
        buttons[5].shape.y = 1080 - 128 * 3 - 128 / 2;

        buttons[6].shape.x = 1920 - 128 * 3;
        buttons[6].shape.y = 1080 - 128;

        buttons[7].shape.x = 128 * 2;
        buttons[7].shape.y = 1080 - 128;

        // virtual buttons
        buttons[8].shape.x = 128 - 128;
        buttons[8].shape.y = 1080 - 128 * 5;

        buttons[9].shape.x = 128 + 128;
        buttons[9].shape.y = 1080 - 128 * 5;

        buttons[10].shape.x = 128 - 128;
        buttons[10].shape.y = 1080 - 128 * 3;

        buttons[11].shape.x = 128 + 128;
        buttons[11].shape.y = 1080 - 128 * 3;
        break;
    }
}





static void gbmulator_exit(menu_entry_t *entry) {
    is_running = SDL_FALSE;
}

static void gbmulator_unpause(menu_entry_t *entry) {
    if (is_rom_loaded)
        is_paused = SDL_FALSE;
}

static void ppu_vblank_cb(byte_t *pixels) {
    SDL_UpdateTexture(ppu_texture, NULL, pixels, ppu_texture_pitch);
}

static void apu_samples_ready_cb(float *audio_buffer, int audio_buffer_size) {
    // FIXME audio crackling and emulation stuttering at 512 samples bigger sample count is better but there is still
    // some crackling... A better audio/video syncing is needed.

    // TODO this is audio_buffer_size * 8 on web and desktop platforms...
    // TODO to help see what's going on:
    // there are a lot of need refill (and no need delay): the emulation doesn't produce samples at a fast enough rate??
    //   ---> compare these results with desktop platform
    // maybe find a way for the apu to produce samples at a varying rate:
    //      increase rate if we hit 'need refill',
    //      decrease rate if we hit 'need delay'
    // find and algorithm to balance this (maybe take the time it took to render a frame a change the sample rate
    //                                      accordingly or don't change the rate but the sample count)
    // ----> changing the sample count seems better than changing the sample rate

    // if (SDL_GetQueuedAudioSize(audio_device) > audio_buffer_size * 4)
    //     log("need delay");
    // else if (SDL_GetQueuedAudioSize(audio_device) == 0)
    //     log("need refill");

    // while (SDL_GetQueuedAudioSize(audio_device) > audio_buffer_size * 4)
    //     SDL_Delay(1);
    SDL_QueueAudio(audio_device, audio_buffer, audio_buffer_size);
}

void load_config(char *path) {
    SDL_RWops *f = SDL_RWFromFile(path, "r");
    if (f) {
        char buf[512];
        SDL_RWread(f, buf, sizeof(buf), 1);
        config_load_from_buffer(buf);
        SDL_RWclose(f);
    }
}

static void save_config(const char *path) {
    size_t len;
    char *config_buf = config_save_to_buffer(&len);
    SDL_RWops *f = SDL_RWFromFile(path, "w");
    if (!f) {
        log("error opening config file");
        free(config_buf);
        return;
    }
    SDL_RWwrite(f, config_buf, len, 1);
    SDL_RWclose(f);
    free(config_buf);
}

// static char *get_savestate_path(const char *rom_filepath, int slot) {
//     char *xdg_data = get_xdg_path("XDG_DATA_HOME", ".local/share");

//     char *last_slash = strrchr(rom_filepath, '/');
//     char *last_period = strrchr(last_slash ? last_slash : rom_filepath, '.');
//     int last_period_index = last_period ? (int) (last_period - last_slash) : strlen(rom_filepath);

//     size_t len = strlen(xdg_data) + strlen(last_slash);
//     char *save_path = xmalloc(len + 33);
//     snprintf(save_path, len + 32, "%s/gbmulator/savestates%.*s-%d.gbstate", xdg_data, last_period_index, last_slash, slot);

//     free(xdg_data);
//     return save_path;
// }

static void save(void) {
    size_t save_length;
    byte_t *save_data = emulator_get_save_data(emu, &save_length);
    if (!save_data) return;

    SDL_RWops *f = SDL_RWFromFile(rom_title, "w");
    if (!f) {
        log("error opening save file");
        return;
    }
    SDL_RWwrite(f, save_data, save_length, 1);
    SDL_RWclose(f);
    free(save_data);
}

static void handle_input(void) {
    SDL_Event event;
    char *savestate_path;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_FINGERDOWN:
            touch_press(&event.tfinger);
            break;
        case SDL_FINGERUP:
            touch_release(&event.tfinger);
            break;
        case SDL_FINGERMOTION:
            touch_motion(&event.tfinger);
            break;
        case SDL_KEYDOWN:
            if (!event.key.repeat && event.key.keysym.sym == SDLK_AC_BACK) {
                if (is_rom_loaded)
                    is_paused = !is_paused;
                ui_back_to_root_menu(ui);
            }
            break;
        case SDL_CONTROLLERBUTTONDOWN:
            if (is_paused) {
                if (event.cbutton.button == SDL_CONTROLLER_BUTTON_GUIDE) {
                    if (is_rom_loaded)
                        is_paused = SDL_FALSE;
                    else
                        ui_back_to_root_menu(ui);
                } else {
                    ui_controller_press(ui, event.cbutton.button);
                }
                break;
            }
            if (event.cbutton.button == SDL_CONTROLLER_BUTTON_GUIDE) {
                is_paused = SDL_TRUE;
                ui_back_to_root_menu(ui);
                break;
            }
            if (!is_paused)
                button_press(emu, sdl_controller_to_joypad(event.cbutton.button));
            break;
        case SDL_CONTROLLERBUTTONUP:
            if (!is_paused)
                button_release(emu, sdl_controller_to_joypad(event.cbutton.button));
            break;
        case SDL_CONTROLLERDEVICEADDED:
            if (!is_controller_present) {
                pad = SDL_GameControllerOpen(event.cdevice.which);
                is_controller_present = SDL_TRUE;
                log("%s connected\n", SDL_GameControllerName(pad));
            }
            break;
        case SDL_CONTROLLERDEVICEREMOVED:
            if (is_controller_present) {
                SDL_GameControllerClose(pad);
                is_controller_present = SDL_FALSE;
                log("%s disconnected\n", SDL_GameControllerName(pad));
            }
            break;
        case SDL_WINDOWEVENT:
            if(event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                int w = event.window.data1;
                int h = event.window.data2;
                set_layout(w > h);
            }
            break;
        case SDL_APP_WILLENTERBACKGROUND:
            is_paused = SDL_TRUE;
            ui_back_to_root_menu(ui);
            if (emu)
                save();
            save_config("config");
            break;
        case SDL_APP_DIDENTERFOREGROUND:
            break;
        case SDL_RENDER_DEVICE_RESET:
            // TODO
            // However, there's a chance (on older hardware, or on systems under heavy load),
            // where the GL context can not be restored. In that case you have to listen for a specific
            // message (SDL_RENDER_DEVICE_RESET) and restore your textures manually or quit the app.
            log("SDL_RENDER_DEVICE_RESET");
            break;
        case SDL_QUIT:
            is_running = SDL_FALSE;
            break;
        }
    }
}

static void load_cartridge(const byte_t *rom_data, size_t rom_size, char *new_rom_title) {
    emulator_t *new_emu = emulator_init_from_data(config.mode, rom_data, rom_size, APU_SAMPLE_COUNT, ppu_vblank_cb, apu_samples_ready_cb);
    if (!new_emu) return;

    if (emu) {
        save();
        free(rom_title);
        emulator_quit(emu);
    }
    rom_title = new_rom_title;
    emu = new_emu;

    SDL_RWops *f = SDL_RWFromFile(rom_title, "r");
    if (f) {
        size_t save_length = SDL_RWsize(f);
        byte_t *save_data = xmalloc(save_length);
        SDL_RWread(f, save_data, save_length, 1);
        emulator_load_save_data(emu, save_data, save_length);
        SDL_RWclose(f);
        free(save_data);
    }

    emulator_set_apu_speed(emu, config.speed);
    emulator_set_apu_sound_level(emu, config.sound);
    emulator_set_color_palette(emu, config.color_palette);

    is_rom_loaded = SDL_TRUE;
    is_paused = SDL_FALSE;

    ui->root_menu->entries[0].disabled = 0; // enable resume menu entry
    ui->root_menu->entries[2].disabled = 0; // enable reset rom menu entry
}

static void set_allowed_orientations(void) {
    // retrieve the JNI environment.
    JNIEnv *env = (JNIEnv *) SDL_AndroidGetJNIEnv();

    // retrieve the Java instance of the SDLActivity
    jobject activity = (jobject) SDL_AndroidGetActivity();

    // find the Java class of the activity. It should be SDLActivity or a subclass of it.
    jclass clazz = (*env)->GetObjectClass(env, activity);

    // find the identifier of the method to call
    jmethodID method_id = (*env)->GetMethodID(env, clazz, "setAllowedOrientations", "()V");

    // effectively call the Java method
    (*env)->CallVoidMethod(env, activity, method_id);

    // clean up the local references.
    (*env)->DeleteLocalRef(env, activity);
    (*env)->DeleteLocalRef(env, clazz);
}

















static void choose_speed(menu_entry_t *entry) {
    config.speed = (entry->choices.position * 0.5f) + 1;
    if (emu)
        emulator_set_apu_speed(emu, config.speed);
}

static void choose_sound(menu_entry_t *entry) {
    config.sound = entry->choices.position * 0.25f;
    if (emu)
        emulator_set_apu_sound_level(emu, config.sound);
}

static void choose_color(menu_entry_t *entry) {
    config.color_palette = entry->choices.position;
    if (emu) {
        emulator_update_pixels_with_palette(emu, config.color_palette);
        emulator_set_color_palette(emu, config.color_palette);
    }
}

static void choose_mode(menu_entry_t *entry) {
    config.mode = entry->choices.position;
}






JNIEXPORT void JNICALL Java_com_mpostaire_gbmulator_GBmulator_receiveROMData(JNIEnv* env, jobject thiz, jbyteArray data, jsize size) {
    jboolean is_copy;
    jbyte *rom_data = (*env)->GetByteArrayElements(env, data, &is_copy);

    char *new_rom_title = emulator_get_rom_title_from_data((byte_t *) rom_data, size);
    for (int i = 0; i < 16; i++)
        if (new_rom_title[i] == ' ')
            new_rom_title[i] = '_';

    load_cartridge((byte_t *) rom_data, size, new_rom_title);

    (*env)->ReleaseByteArrayElements(env, data, rom_data, JNI_ABORT);
}

static void open_rom(menu_entry_t *entry) {
    // retrieve the JNI environment.
    JNIEnv *env = (JNIEnv *) SDL_AndroidGetJNIEnv();

    // retrieve the Java instance of the SDLActivity
    jobject activity = (jobject) SDL_AndroidGetActivity();

    // find the Java class of the activity. It should be SDLActivity or a subclass of it.
    jclass clazz = (*env)->GetObjectClass(env, activity);

    // find the identifier of the method to call
    jmethodID method_id = (*env)->GetMethodID(env, clazz, "filePicker", "()V");

    // effectively call the Java method
    (*env)->CallVoidMethod(env, activity, method_id);

    // clean up the local references.
    (*env)->DeleteLocalRef(env, activity);
    (*env)->DeleteLocalRef(env, clazz);
}

static void reset_rom(menu_entry_t *entry) {
    if (!emu)
        return;

    size_t rom_size;
    byte_t *cart = emulator_get_rom_data(emu, &rom_size);
    byte_t *rom_data = xmalloc(rom_size);
    memcpy(rom_data, cart, rom_size);

    char *new_rom_title = emulator_get_rom_title_from_data(rom_data, rom_size);
    for (int i = 0; i < 16; i++)
        if (new_rom_title[i] == ' ')
            new_rom_title[i] = '_';

    load_cartridge(rom_data, rom_size, new_rom_title);
    free(rom_data);
}

menu_t options_menu = {
    .title = "Options",
    .length = 5,
    .entries = {
        { "Speed:      |1.0x,1.5x,2.0x,2.5x,3.0x,3.5x,4.0x", UI_CHOICE, .choices = { choose_speed, 0 } },
        { "Sound:      | OFF, 25%, 50%, 75%,100%", UI_CHOICE, .choices = { choose_sound, 0 } },
        { "Color:      |gray,orig", UI_CHOICE, .choices = { choose_color, 0 } },
        { "Mode:       | DMG, CGB", UI_CHOICE, .choices = { choose_mode, 0 } },
        { "Back...", UI_BACK }
    }
};

menu_t main_menu = {
    .title = "GBmulator",
    .length = 5,
    .entries = {
        { "Resume", UI_ACTION, .disabled = 1, .action = gbmulator_unpause },
        { "Open ROM...", UI_ACTION, .action = open_rom },
        { "Reset ROM", UI_ACTION, .disabled = 1, .action = reset_rom },
        { "Options...", UI_SUBMENU, .submenu = &options_menu },
        { "Exit", UI_ACTION, .action = gbmulator_exit }
    }
};







int main(int argc, char **argv) {
    char *config_path = "config";
    // load_config() must be called before emulator_init() and ui_init()
    load_config(config_path);
    emu = NULL;

    // initialize global variables here and not at their initialization as they can still have their
    // previous values because of android's activities lifecycle
    is_running = SDL_TRUE;
    is_paused = SDL_TRUE;
    is_rom_loaded = SDL_FALSE;
    is_controller_present = SDL_FALSE;





    ui = ui_init(&main_menu, GB_SCREEN_WIDTH, GB_SCREEN_HEIGHT);
    ui->root_menu->entries[0].disabled = 1; // disable resume menu entry
    ui->root_menu->entries[2].disabled = 1; // disable reset rom menu entry
    options_menu.entries[0].choices.position = config.speed / 0.5f - 2;
    options_menu.entries[1].choices.position = config.sound * 4;
    options_menu.entries[2].choices.position = config.color_palette;
    options_menu.entries[3].choices.position = config.mode;
    options_menu.entries[3].choices.description = xmalloc(16);
    snprintf(options_menu.entries[3].choices.description, 16, "Effect on reset");
    ui_back_to_root_menu(ui);






    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER);

    window = SDL_CreateWindow(
        EMULATOR_NAME,
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        1080, // TODO use phone actual width
        1920, // TODO use phone actual height
        SDL_WINDOW_FULLSCREEN | SDL_WINDOW_RESIZABLE
    );

    set_allowed_orientations();
    set_layout(0);



    // TODO vsync or not vsync?? see what audio/video syncing does...
    // TODO also compare desktop and android platforms speeds at config.speed == 1.0f to see if it's equivalent
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    ppu_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, GB_SCREEN_WIDTH, GB_SCREEN_HEIGHT);
    ppu_texture_pitch = GB_SCREEN_WIDTH * sizeof(byte_t) * 3;
    SDL_Texture *ui_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, GB_SCREEN_WIDTH, GB_SCREEN_HEIGHT);
    int ui_texture_pitch = GB_SCREEN_WIDTH * sizeof(byte_t) * 4;
    SDL_SetTextureBlendMode(ui_texture, SDL_BLENDMODE_BLEND);

    SDL_AudioSpec audio_settings = {
        .freq = GB_APU_SAMPLE_RATE,
        .format = AUDIO_F32SYS,
        .channels = GB_APU_CHANNELS,
        .samples = APU_SAMPLE_COUNT
    };
    audio_device = SDL_OpenAudioDevice(NULL, 0, &audio_settings, NULL, 0);
    SDL_PauseAudioDevice(audio_device, 0);

    // main gbmulator loop
    int cycles = 0;
    int step = 0;
    while (is_running) {
        // emulation paused
        if (is_paused) {
            handle_input();
            if (!is_paused) // if user input disable pause, don't draw ui
                continue;

            // display ui
            ui_draw(ui);

            SDL_RenderClear(renderer);

            // update ppu_texture to show color palette changes behind the menu
            if (is_rom_loaded) {
                SDL_UpdateTexture(ppu_texture, NULL, emulator_get_pixels(emu), ppu_texture_pitch);
                SDL_RenderCopy(renderer, ppu_texture, NULL, &gb_screen_rect);
            }

            SDL_UpdateTexture(ui_texture, NULL, ui->pixels, ui_texture_pitch);
            SDL_RenderCopy(renderer, ui_texture, NULL, &gb_screen_rect);

            draw_buttons(renderer);

            SDL_RenderPresent(renderer);
            // wait for the next event (not consuming it) to avoid CPU usage
            // TODO this makes weird screen size until a key is pressed... in the mean time, use SDL_Delay()
            // SDL_WaitEvent(NULL);
            SDL_Delay(1.0f / 30.0f);
            continue;
        }

        // handle_input is a slow function: don't call it every step
        if (cycles >= GB_CPU_CYCLES_PER_FRAME * config.speed) {
            cycles = 0;
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, ppu_texture, NULL, &gb_screen_rect);
            draw_buttons(renderer);
            // this SDL_Delay() isn't needed as the audio sync adds it's own delay
            // SDL_Delay(1.0f / 60.0f); // even with SDL waiting for vsync, delay here for monitors with different refresh rates than 60Hz
            SDL_RenderPresent(renderer);
            handle_input(); // keep this the closest possible before emulator_step() to reduce input inaccuracies
        }

        // run one step of the emulator
        cycles += emulator_step(emu);

        // no delay at the end of the loop because the emulation is audio synced (the audio is what makes the delay).
    }

    if (emu) {
        save();
        emulator_quit(emu);
    }

    if (rom_title)
        free(rom_title);

    // save config
    save_config("config");

    ui_free(ui);

    if (is_controller_present)
        SDL_GameControllerClose(pad);

    SDL_DestroyWindow(window);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyTexture(ppu_texture);
    SDL_DestroyTexture(ui_texture);

    SDL_CloseAudioDevice(audio_device);

    SDL_Quit();
    return EXIT_SUCCESS;
}
