#include <stdlib.h>
#include <stdio.h>
#include <SDL.h>
#include <android/log.h>
#include <jni.h>

#include "../common/utils.h"
#include "emulator/emulator.h"

#define log(...) __android_log_print(ANDROID_LOG_INFO, "GBmulator", __VA_ARGS__)

// going higher than 2048 starts to add noticeable audio lag
#define APU_SAMPLE_COUNT 2048
#define SCREEN_WIDTH 160
#define SCREEN_HEIGHT 284

// TODO savestates ui
// TODO bluetooth link cable
// TODO test with gamepad controller (PS3, ...)
// TODO replace all printf() by __android_log_print()

// TODO make ui in an android activity with android ui api (no need for keybindings/scale configs but need for landscape/portrait layouts config)

// TODO button (and screen??) layout designer for both landscape and portrait (like drastic ds emulator)
//      add to config file the layouts: each element has a normalized x,y coordinates and width/heigth.
//      --> meaning the coordinates are in percent of the screen (ex: x is in percent of the screen width, ...)

// TODO ???? bigger finger hitbox (circle instead of point --> button pressed is the one closer to the center of the circle)
//      OR maybe just bigger button hitbox (check drastic ds emulator to see how it's done)

SDL_bool is_running;
SDL_bool is_rom_loaded;
SDL_bool is_landscape;

float speed;

SDL_Renderer *renderer;
SDL_Window *window;

SDL_Texture *ppu_texture;
int ppu_texture_pitch;

SDL_AudioDeviceID audio_device;

SDL_GameController *pad;
SDL_bool is_controller_present;

emulator_t *emu;

SDL_Rect gb_screen_rect;

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
            .h = 18,
            .w = 18
        },
        .r = 255,
        .g = 0,
        .b = 0,
        .button = JOYPAD_LEFT
    },
    {
        .shape = {
            .h = 18,
            .w = 18
        },
        .r = 255,
        .g = 0,
        .b = 0,
        .button = JOYPAD_RIGHT
    },
    {
        .shape = {
            .h = 18,
            .w = 18
        },
        .r = 255,
        .g = 0,
        .b = 0,
        .button = JOYPAD_UP
    },
    {
        .shape = {
            .h = 18,
            .w = 18
        },
        .r = 255,
        .g = 0,
        .b = 0,
        .button = JOYPAD_DOWN
    },
    {
        .shape = {
            .h = 18,
            .w = 18
        },
        .r = 255,
        .g = 0,
        .b = 0,
        .button = JOYPAD_A
    },
    {
        .shape = {
            .h = 18,
            .w = 18
        },
        .r = 255,
        .g = 0,
        .b = 0,
        .button = JOYPAD_B
    },
    {
        .shape = {
            .h = 18,
            .w = 18
        },
        .r = 255,
        .g = 0,
        .b = 0,
        .button = JOYPAD_START
    },
    {
        .shape = {
            .h = 18,
            .w = 18
        },
        .r = 255,
        .g = 0,
        .b = 0,
        .button = JOYPAD_SELECT
    },

    {
        .shape = {
            .h = 18,
            .w = 18,
        },
        .r = 0,
        .g = 0,
        .b = 255,
        .button = JOYPAD_SELECT + 1 // up left
    },
    {
        .shape = {
            .h = 18,
            .w = 18
        },
        .r = 0,
        .g = 0,
        .b = 255,
        .button = JOYPAD_SELECT + 2 // up right
    },
    {
        .shape = {
            .h = 18,
            .w = 18
        },
        .r = 0,
        .g = 0,
        .b = 255,
        .button = JOYPAD_SELECT + 3 // down left
    },
    {
        .shape = {
            .h = 18,
            .w = 18
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
        x *= SCREEN_HEIGHT;
        y *= SCREEN_WIDTH;
    } else {
        x *= SCREEN_WIDTH;
        y *= SCREEN_HEIGHT;
    }

    for (int i = 0; i < 12; i++) {
        SDL_Rect *hitbox = &buttons[i].shape;
        if (x > hitbox->x && x < hitbox->x + hitbox->w && y > hitbox->y && y < hitbox->y + hitbox->h)
            return i;
    }

    return -1;
}

static void button_press(emulator_t *emu, joypad_button_t button) {
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
        SDL_RenderSetLogicalSize(renderer, SCREEN_WIDTH, SCREEN_HEIGHT);
        gb_screen_rect.w = SCREEN_WIDTH;
        gb_screen_rect.h = GB_SCREEN_HEIGHT * (SCREEN_WIDTH / (float) GB_SCREEN_WIDTH);
        gb_screen_rect.x = 0;
        gb_screen_rect.y = 0;

        // physical buttons
        buttons[0].shape.x = 18 - 18;
        buttons[0].shape.y = 178 + 18;

        buttons[1].shape.x = 18 + 18;
        buttons[1].shape.y = 178 + 18;

        buttons[2].shape.x = 18;
        buttons[2].shape.y = 178;

        buttons[3].shape.x = 18;
        buttons[3].shape.y = 178 + 18 * 2;

        buttons[4].shape.x = 160 - 18;
        buttons[4].shape.y = 178 + 18 / 2;

        buttons[5].shape.x = 160 - 18 * 2;
        buttons[5].shape.y = 178 + 18 + 18 / 2;

        buttons[6].shape.x = 160 / 2 + 18;
        buttons[6].shape.y = 284 - 18;

        buttons[7].shape.x = 160 / 2 - 18 * 2;
        buttons[7].shape.y = 284 - 18;

        // virtual buttons
        buttons[8].shape.x = 18 - 18;
        buttons[8].shape.y = 178;

        buttons[9].shape.x = 18 + 18;
        buttons[9].shape.y = 178;

        buttons[10].shape.x = 18 - 18;
        buttons[10].shape.y = 178 + 18 * 2;

        buttons[11].shape.x = 18 + 18;
        buttons[11].shape.y = 178 + 18 * 2;
        break;
    case 1:
        // landscape
        SDL_RenderSetLogicalSize(renderer, SCREEN_HEIGHT, SCREEN_WIDTH);
        gb_screen_rect.w = GB_SCREEN_WIDTH * (SCREEN_WIDTH / (float) GB_SCREEN_HEIGHT);
        gb_screen_rect.h = SCREEN_WIDTH;
        gb_screen_rect.x = SCREEN_HEIGHT / 2 - gb_screen_rect.w / 2;
        gb_screen_rect.y = 0;

        // physical buttons
        buttons[0].shape.x = 18 - 18;
        buttons[0].shape.y = 160 - 18 * 4;

        buttons[1].shape.x = 18 + 18;
        buttons[1].shape.y = 160 - 18 * 4;

        buttons[2].shape.x = 18;
        buttons[2].shape.y = 160 - 18 * 5;

        buttons[3].shape.x = 18;
        buttons[3].shape.y = 160 - 18 * 3;

        buttons[4].shape.x = 284 - 18;
        buttons[4].shape.y = 160 - 18 * 4 - 18 / 2;

        buttons[5].shape.x = 284 - 18 * 2;
        buttons[5].shape.y = 160 - 18 * 3 - 18 / 2;

        buttons[6].shape.x = 284 - 18 * 3 + 1;
        buttons[6].shape.y = 160 - 18;

        buttons[7].shape.x = 18 * 2;
        buttons[7].shape.y = 160 - 18;

        // virtual buttons
        buttons[8].shape.x = 18 - 18;
        buttons[8].shape.y = 160 - 18 * 5;

        buttons[9].shape.x = 18 + 18;
        buttons[9].shape.y = 160 - 18 * 5;

        buttons[10].shape.x = 18 - 18;
        buttons[10].shape.y = 160 - 18 * 3;

        buttons[11].shape.x = 128 + 128;
        buttons[11].shape.y = 1080 - 128 * 3;
        break;
    }
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

    // TODO
    // the culprit is sdl rendercopy is slow on my phone... causing the audio buffer to starve and needing refills as
    // the apu can't function when the rendering is taking place.
    //  --> make the apu vary it's sample freq or sample size depending on the time the previous frame took to render
    //  --> OR make the entire emulator into another thread: may complicate things a lot

    // if (SDL_GetQueuedAudioSize(audio_device) > audio_buffer_size * 4)
    //     log("need delay");
    // else if (SDL_GetQueuedAudioSize(audio_device) == 0)
    //     log("need refill");

    // while (SDL_GetQueuedAudioSize(audio_device) > audio_buffer_size * 4)
    //     SDL_Delay(1);
    SDL_QueueAudio(audio_device, audio_buffer, audio_buffer_size);
}

static void android_back_to_previous_activity(void) {
    save_state_to_file(emu, "resume");

    // retrieve the JNI environment.
    JNIEnv *env = (JNIEnv *) SDL_AndroidGetJNIEnv();

    // retrieve the Java instance of the SDLActivity
    jobject activity = (jobject) SDL_AndroidGetActivity();

    // find the Java class of the activity. It should be SDLActivity or a subclass of it.
    jclass clazz = (*env)->GetObjectClass(env, activity);

    // find the identifier of the method to call
    jmethodID method_id = (*env)->GetMethodID(env, clazz, "finish", "()V");

    // effectively call the Java method
    (*env)->CallVoidMethod(env, activity, method_id);

    // clean up the local references.
    (*env)->DeleteLocalRef(env, activity);
    (*env)->DeleteLocalRef(env, clazz);
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
            if (!event.key.repeat && event.key.keysym.sym == SDLK_AC_BACK)
                android_back_to_previous_activity();
            break;
        case SDL_CONTROLLERBUTTONDOWN:
            if (event.cbutton.button == SDL_CONTROLLER_BUTTON_GUIDE)
                android_back_to_previous_activity();
            else
                button_press(emu, sdl_controller_to_joypad(event.cbutton.button));
            break;
        case SDL_CONTROLLERBUTTONUP:
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
            if (emu)
                save_battery_to_file(emu, emulator_get_rom_title(emu));
            android_back_to_previous_activity();
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

static void load_cartridge(const byte_t *rom_data, size_t rom_size, int emu_mode, int palette) {
    emulator_options_t opts = {
        .apu_sample_count = APU_SAMPLE_COUNT,
        .mode = emu_mode,
        .on_apu_samples_ready = apu_samples_ready_cb,
        .on_new_frame = ppu_vblank_cb,
        // TODO
        // .apu_speed = config.speed,
        // .apu_sound_level = config.sound,
        .palette = palette
    };
    emulator_t *new_emu = emulator_init(rom_data, rom_size, &opts);
    if (!new_emu) return;

    if (emu) {
        save_battery_to_file(emu, emulator_get_rom_title(emu));
        emulator_quit(emu);
    }
    emu = new_emu;

    load_battery_from_file(emu, emulator_get_rom_title(emu));

    is_rom_loaded = SDL_TRUE;
}

static void set_allowed_orientations(void) {
    JNIEnv *env = (JNIEnv *) SDL_AndroidGetJNIEnv();

    jobject activity = (jobject) SDL_AndroidGetActivity();

    jclass clazz = (*env)->GetObjectClass(env, activity);

    jmethodID method_id = (*env)->GetMethodID(env, clazz, "setAllowedOrientations", "()V");

    (*env)->CallVoidMethod(env, activity, method_id);

    (*env)->DeleteLocalRef(env, activity);
    (*env)->DeleteLocalRef(env, clazz);
}

static void request_rom(void) {
    JNIEnv *env = (JNIEnv *) SDL_AndroidGetJNIEnv();

    jobject activity = (jobject) SDL_AndroidGetActivity();

    jclass clazz = (*env)->GetObjectClass(env, activity);

    jmethodID method_id = (*env)->GetMethodID(env, clazz, "requestROM", "()V");

    (*env)->CallVoidMethod(env, activity, method_id);

    (*env)->DeleteLocalRef(env, activity);
    (*env)->DeleteLocalRef(env, clazz);
}

JNIEXPORT void JNICALL Java_io_github_mpostaire_gbmulator_Emulator_receiveROMData(JNIEnv* env, jobject thiz, jbyteArray data, jsize size, jboolean resume, jint emu_mode, jint palette) {
    jboolean is_copy;
    jbyte *rom_data = (*env)->GetByteArrayElements(env, data, &is_copy);

    load_cartridge((byte_t *) rom_data, size, emu_mode, palette);
    if (resume)
        load_state_from_file(emu, "resume");

    (*env)->ReleaseByteArrayElements(env, data, rom_data, JNI_ABORT);
}

int main(int argc, char **argv) {
    emu = NULL;

    // initialize global variables here and not at their initialization as they can still have their
    // previous values because of android's activities lifecycle
    is_running = SDL_TRUE;
    is_rom_loaded = SDL_FALSE;
    is_controller_present = SDL_FALSE;
    speed = 1.0f;

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER);

    window = SDL_CreateWindow(
        EMULATOR_NAME,
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        SCREEN_WIDTH, // TODO use phone actual width
        SCREEN_HEIGHT, // TODO use phone actual height
        SDL_WINDOW_FULLSCREEN | SDL_WINDOW_RESIZABLE
    );

    set_allowed_orientations();
    set_layout(0);



    // TODO vsync or not vsync?? see what audio/video syncing does...
    // TODO also compare desktop and android platforms speeds at emulator speed == 1.0f to see if it's equivalent
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    // supported:
    // SDL_PIXELFORMAT_ARGB8888
    // SDL_PIXELFORMAT_ABGR8888
    // SDL_PIXELFORMAT_RGB888
    // SDL_PIXELFORMAT_BGR888
    // SDL_PIXELFORMAT_YV12
    // SDL_PIXELFORMAT_IYUV
    // SDL_PIXELFORMAT_NV12
    // SDL_PIXELFORMAT_NV21
    // SDL_PIXELFORMAT_UNKNOWN

    // SDL_RendererInfo info;
    // SDL_GetRendererInfo(renderer, &info);
    // log("available pixel formats:");
    // for (int i = 0; i < info.num_texture_formats; i++)
    //     log("%s", SDL_GetPixelFormatName(info.texture_formats[i]));

    ppu_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_BGR888, SDL_TEXTUREACCESS_STREAMING, GB_SCREEN_WIDTH, GB_SCREEN_HEIGHT);
    ppu_texture_pitch = GB_SCREEN_WIDTH * sizeof(byte_t) * 4;
    SDL_Texture *ui_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, GB_SCREEN_WIDTH, GB_SCREEN_HEIGHT);
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

    // TODO don't overcomplicate and make a request java to call a native method...
    //      just make request_rom return the rom as a byte_t buffer
    request_rom();

    // main gbmulator loop
    int cycles = 0;
    int step = 0;
    while (is_running) {
        // handle_input is a slow function: don't call it every step
        if (cycles >= GB_CPU_CYCLES_PER_FRAME * speed) {
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
        save_battery_to_file(emu, emulator_get_rom_title(emu));
        emulator_quit(emu);
    }

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
