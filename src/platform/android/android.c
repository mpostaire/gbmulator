#include <stdlib.h>
#include <stdio.h>
#include <SDL.h>
#include <android/log.h>
#include <jni.h>

#include "../common/utils.h"
#include "layout_editor.h"
#include "emulator/emulator.h"

#define log(...) __android_log_print(ANDROID_LOG_INFO, "GBmulator", __VA_ARGS__)

// going higher than 2048 starts to add noticeable audio lag
#define APU_SAMPLE_COUNT 2048
#define SCREEN_WIDTH 160
#define SCREEN_HEIGHT 284

// TODO savestates ui
// TODO bluetooth (and wifi) link cable --> make a link connection pausable and resumeable (because the whole emulator is
//      reset each time we leave the Emulator activity)
// TODO test with gamepad controller (PS3, ...)

SDL_bool is_running;
SDL_bool is_landscape;

float speed;
int frame_skip;

SDL_Renderer *renderer;
SDL_Window *window;

SDL_Texture *ppu_texture;
int ppu_texture_pitch;

SDL_AudioDeviceID audio_device;

SDL_GameController *pad;
SDL_bool is_controller_present;

emulator_t *emu;

SDL_Rect gb_screen_rect;

button_t buttons[] = {
    {
        .shape = {
            .h = 60,
            .w = 60
        }
    },
    {
        .shape = {
            .h = 20,
            .w = 20
        },
        .button = JOYPAD_A
    },
    {
        .shape = {
            .h = 20,
            .w = 20
        },
        .button = JOYPAD_B
    },
    {
        .shape = {
            .h = 10,
            .w = 30
        },
        .button = JOYPAD_START
    },
    {
        .shape = {
            .h = 10,
            .w = 30
        },
        .button = JOYPAD_SELECT
    },
};

static inline s_byte_t is_finger_over_button(float x, float y) {
    if (is_landscape) {
        x *= SCREEN_HEIGHT;
        y *= SCREEN_WIDTH;
    } else {
        x *= SCREEN_WIDTH;
        y *= SCREEN_HEIGHT;
    }

    SDL_Rect *hitbox = &buttons[0].shape;
    if (x > hitbox->x && x < hitbox->x + hitbox->w && y > hitbox->y && y < hitbox->y + hitbox->h) {
        x -= hitbox->x;
        y -= hitbox->y;
        if (x < hitbox->w / 3) {
            if (y < hitbox->h / 3) // up left
                return JOYPAD_SELECT + 1;
            else if (y > 2 * (hitbox->h / 3)) // down left
                return JOYPAD_SELECT + 3;
            else
                return JOYPAD_LEFT;
        } else if (x > 2 * (hitbox->w / 3)) {
            if (y < hitbox->h / 3) // up right
                return JOYPAD_SELECT + 2;
            else if (y > 2 * (hitbox->h / 3)) // down right
                return JOYPAD_SELECT + 4;
            else
                return JOYPAD_RIGHT;
        } else {
            if (y < hitbox->h / 3)
                return JOYPAD_UP;
            else if (y > 2 * (hitbox->h / 3))
                return JOYPAD_DOWN;
            else
                return -1;
        }
    }

    for (s_byte_t i = 1; i < 5; i++) {
        SDL_Rect *hitbox = &buttons[i].shape;
        if (x > hitbox->x && x < hitbox->x + hitbox->w && y > hitbox->y && y < hitbox->y + hitbox->h)
            return buttons[i].button;
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
        break;
    case JOYPAD_SELECT + 2:
        break;
    case JOYPAD_SELECT + 3:
        break;
    case JOYPAD_SELECT + 4:
        break;
    default:
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
        break;
    case JOYPAD_SELECT + 2:
        break;
    case JOYPAD_SELECT + 3:
        break;
    case JOYPAD_SELECT + 4:
        break;
    default:
        break;
    }
}

static inline void touch_press(SDL_TouchFingerEvent *event) {
    s_byte_t hovered = is_finger_over_button(event->x, event->y);
    if (hovered >= 0)
        button_press(emu, hovered);
}

static inline void touch_release(SDL_TouchFingerEvent *event) {
    s_byte_t hovered = is_finger_over_button(event->x, event->y);
    if (hovered >= 0)
        button_release(emu, hovered);
}

static inline void touch_motion(SDL_TouchFingerEvent *event) {
    s_byte_t previous = is_finger_over_button(event->x - event->dx, event->y - event->dy);
    s_byte_t hovered = is_finger_over_button(event->x, event->y);

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
        gb_screen_rect.h = GB_SCREEN_HEIGHT * (gb_screen_rect.w / GB_SCREEN_WIDTH);
        gb_screen_rect.x = 0;
        gb_screen_rect.y = 0;

        buttons[0].shape.x = 20 - 20;
        buttons[0].shape.y = 178;

        buttons[1].shape.x = 160 - 20;
        buttons[1].shape.y = 178 + 20 / 2;

        buttons[2].shape.x = 160 - 20 * 2;
        buttons[2].shape.y = 178 + 20 + 20 / 2;

        buttons[3].shape.x = 160 / 2 + 30;
        buttons[3].shape.y = 284 - 30;

        buttons[4].shape.x = 160 / 2 - 30 * 2;
        buttons[4].shape.y = 284 - 30;
        break;
    case 1:
        // landscape
        SDL_RenderSetLogicalSize(renderer, SCREEN_HEIGHT, SCREEN_WIDTH);
        gb_screen_rect.h = SCREEN_WIDTH;
        gb_screen_rect.w = GB_SCREEN_WIDTH * (gb_screen_rect.h / GB_SCREEN_HEIGHT);
        gb_screen_rect.x = SCREEN_HEIGHT / 2 - gb_screen_rect.w / 2;
        gb_screen_rect.y = 0;

        // physical buttons
        buttons[0].shape.x = 20 - 20;
        buttons[0].shape.y = 160 - 20 * 5;

        buttons[1].shape.x = 284 - 20;
        buttons[1].shape.y = 160 - 20 * 4 - 20 / 2;

        buttons[2].shape.x = 284 - 20 * 2;
        buttons[2].shape.y = 160 - 20 * 3 - 20 / 2;

        buttons[3].shape.x = 284 - 30 * 3 + 1;
        buttons[3].shape.y = 160 - 30;

        buttons[4].shape.x = 30 * 2;
        buttons[4].shape.y = 160 - 30;
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

    while (SDL_GetQueuedAudioSize(audio_device) > audio_buffer_size * 4)
         SDL_Delay(1);
    SDL_QueueAudio(audio_device, audio_buffer, audio_buffer_size);
}

static void handle_input(void) {
    SDL_Event event;
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
                is_running = SDL_FALSE;
            break;
        case SDL_CONTROLLERBUTTONDOWN:
            if (event.cbutton.button == SDL_CONTROLLER_BUTTON_GUIDE)
                is_running = SDL_FALSE;
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
            is_running = SDL_FALSE;
            break;
        case SDL_APP_DIDENTERFOREGROUND:
            break;
        case SDL_RENDER_DEVICE_RESET:
            is_running = SDL_FALSE;
            break;
        case SDL_QUIT:
            is_running = SDL_FALSE;
            break;
        }
    }
}

static void start_emulation_loop(void) {
    ppu_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_BGR888, SDL_TEXTUREACCESS_STREAMING, GB_SCREEN_WIDTH, GB_SCREEN_HEIGHT);
    ppu_texture_pitch = GB_SCREEN_WIDTH * sizeof(byte_t) * 4;

    SDL_AudioSpec audio_settings = {
            .freq = GB_APU_SAMPLE_RATE,
            .format = AUDIO_F32SYS,
            .channels = GB_APU_CHANNELS,
            .samples = APU_SAMPLE_COUNT
    };
    audio_device = SDL_OpenAudioDevice(NULL, 0, &audio_settings, NULL, 0);
    SDL_PauseAudioDevice(audio_device, 0);

    int cycles = 0;
    int frame_count = 0;
    while (is_running) {
        if (cycles >= GB_CPU_CYCLES_PER_FRAME * speed) {
            // TODO: cycles -= GB_CPU_CYCLES_PER_FRAME * speed;
            cycles = 0;
            if (frame_count >= frame_skip) {
                SDL_RenderClear(renderer);
                SDL_RenderCopy(renderer, ppu_texture, NULL, &gb_screen_rect);
                // draw buttons
                // TODO different textures when a button is pressed/not pressed
                // TODO customizable button opacity as a sharedpreference
                for (int i = 0; i < 5; i++)
                    SDL_RenderCopy(renderer, buttons[i].texture, NULL, &buttons[i].shape);
                // this SDL_Delay() isn't needed as the audio sync adds it's own delay
                // TODO??? SDL_Delay((1.0f / 60.0f) - time_to_render_last_frame); // even with SDL waiting for vsync, delay here for monitors with different refresh rates than 60Hz
                SDL_RenderPresent(renderer);
                frame_count = 0;
            } else {
                SDL_Delay(1.0f / 60.0f);
            }
            frame_count++;
            // handle_input is a slow function: don't call it every step
            handle_input(); // keep this the closest possible before emulator_step() to reduce input inaccuracies
        }

        // run one step of the emulator
        cycles += emulator_step(emu);

        // no delay at the end of the loop because the emulation is audio synced (the audio is what makes the delay).
    }

    if (emu) {
        save_battery_to_file(emu, emulator_get_rom_title(emu));
        save_state_to_file(emu, "resume");
        emulator_quit(emu);
    }

    if (is_controller_present)
        SDL_GameControllerClose(pad);

    SDL_DestroyTexture(ppu_texture);
    SDL_CloseAudioDevice(audio_device);
}

static void load_cartridge(const byte_t *rom_data, size_t rom_size, int resume, int emu_mode, int palette, float emu_speed, float sound) {
    emulator_options_t opts = {
        .apu_sample_count = APU_SAMPLE_COUNT,
        .mode = emu_mode,
        .on_apu_samples_ready = apu_samples_ready_cb,
        .on_new_frame = ppu_vblank_cb,
        .apu_speed = emu_speed,
        .apu_sound_level = sound,
        .palette = palette
    };
    // TODO frame skip user setting
    emu = emulator_init(rom_data, rom_size, &opts);
    if (!emu) return;

    if (resume)
        load_state_from_file(emu, "resume");
    else
        load_battery_from_file(emu, emulator_get_rom_title(emu));

    speed = emu_speed;
}

static void ready(void) {
    JNIEnv *env = (JNIEnv *) SDL_AndroidGetJNIEnv();

    jobject activity = (jobject) SDL_AndroidGetActivity();

    jclass clazz = (*env)->GetObjectClass(env, activity);

    jmethodID method_id = (*env)->GetMethodID(env, clazz, "onNativeAppReady", "()V");

    (*env)->CallVoidMethod(env, activity, method_id);

    (*env)->DeleteLocalRef(env, activity);
    (*env)->DeleteLocalRef(env, clazz);
}

JNIEXPORT void JNICALL Java_io_github_mpostaire_gbmulator_Emulator_receiveROMData(
        JNIEnv* env,
        jobject thiz,
        jbyteArray data,
        jsize size,
        jboolean resume,
        jint emu_mode,
        jint palette,
        jfloat emu_speed,
        jfloat sound,
        jint emu_frame_skip,
        jfloat portrait_screen_x,
        jfloat portrait_screen_y,
        jfloat portrait_screen_size,
        jfloat landscape_screen_x,
        jfloat landscape_screen_y,
        jfloat landscape_screen_size
) {
    jboolean is_copy;
    jbyte *rom_data = (*env)->GetByteArrayElements(env, data, &is_copy);

    load_cartridge((byte_t *) rom_data, size, resume, emu_mode, palette, emu_speed, sound);
    frame_skip = emu_frame_skip;

    (*env)->ReleaseByteArrayElements(env, data, rom_data, JNI_ABORT);

    set_layout(is_landscape);
    start_emulation_loop();
}

JNIEXPORT void JNICALL Java_io_github_mpostaire_gbmulator_Emulator_enterLayoutEditor(
        JNIEnv* env,
        jobject thiz,
        jboolean is_landscape,
        jfloat screen_x,
        jfloat screen_y,
        jfloat screen_size
) {
    set_layout(is_landscape);

    button_t *bs = xmalloc(sizeof(button_t) * 5);
    for (int i = 0; i < 5; i++)
        bs[i] = buttons[i];
    start_layout_editor(renderer, is_landscape, SCREEN_WIDTH, SCREEN_HEIGHT, &gb_screen_rect, bs);
    free(bs);
}

int main(int argc, char **argv) {
    emu = NULL;

    // initialize global variables here and not at their initialization as they can still have their
    // previous values because of android's activities lifecycle
    is_running = SDL_TRUE;
    is_controller_present = SDL_FALSE;
    speed = 1.0f;

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER);

    window = SDL_CreateWindow(
        EMULATOR_NAME,
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        SDL_WINDOW_FULLSCREEN | SDL_WINDOW_RESIZABLE
    );

    // TODO vsync or not vsync?? see what audio/video syncing does...
    // TODO also compare desktop and android platforms speeds at emulator speed == 1.0f to see if it's equivalent
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    SDL_Surface *surface = SDL_LoadBMP("dpad.bmp");
    buttons[0].texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    surface = SDL_LoadBMP("a.bmp");
    buttons[1].texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    surface = SDL_LoadBMP("b.bmp");
    buttons[2].texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    surface = SDL_LoadBMP("start.bmp");
    buttons[3].texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    surface = SDL_LoadBMP("select.bmp");
    buttons[4].texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    ready(); // TODO this should create all textures and audio etc only if in emulator mode

    for (int i = 0; i < 5; i++)
        SDL_DestroyTexture(buttons[i].texture);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    SDL_Quit();
    return EXIT_SUCCESS;
}
