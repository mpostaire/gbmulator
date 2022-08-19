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

// TODO savestates ui
// TODO bluetooth (and wifi) link cable --> make a link connection pausable and resumeable (because the whole emulator is
//      reset each time we leave the Emulator activity)

SDL_bool is_running;
SDL_bool is_landscape;

float speed;
int frame_skip;

int screen_width;
int screen_height;

SDL_Renderer *renderer;
SDL_Window *window;

SDL_Texture *ppu_texture;
int ppu_texture_pitch;

SDL_AudioDeviceID audio_device;

emulator_t *emu;

SDL_Rect gb_screen_rect;

float portrait_dpad_x;
float portrait_dpad_y;
float portrait_a_x;
float portrait_a_y;
float portrait_b_x;
float portrait_b_y;
float portrait_start_x;
float portrait_start_y;
float portrait_select_x;
float portrait_select_y;

float landscape_dpad_x;
float landscape_dpad_y;
float landscape_a_x;
float landscape_a_y;
float landscape_b_x;
float landscape_b_y;
float landscape_start_x;
float landscape_start_y;
float landscape_select_x;
float landscape_select_y;

int dpad_status;

SDL_Texture *dpad_textures[16];

SDL_Texture *a_texture;
SDL_Texture *b_texture;
SDL_Texture *start_texture;
SDL_Texture *select_texture;
SDL_Texture *a_pressed_texture;
SDL_Texture *b_pressed_texture;
SDL_Texture *start_pressed_texture;
SDL_Texture *select_pressed_texture;

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
        x *= screen_height;
        y *= screen_width;
    } else {
        x *= screen_width;
        y *= screen_height;
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
    case JOYPAD_UP:
    case JOYPAD_DOWN:
    case JOYPAD_LEFT:
    case JOYPAD_RIGHT:
        SET_BIT(dpad_status, button);
        break;
    case JOYPAD_A:
        buttons[1].texture = a_pressed_texture;
        return;
    case JOYPAD_B:
        buttons[2].texture = b_pressed_texture;
        return;
    case JOYPAD_START:
        buttons[3].texture = start_pressed_texture;
        return;
    case JOYPAD_SELECT:
        buttons[4].texture = select_pressed_texture;
        return;
    case JOYPAD_SELECT + 1:
        SET_BIT(dpad_status, JOYPAD_UP);
        SET_BIT(dpad_status, JOYPAD_LEFT);
        break;
    case JOYPAD_SELECT + 2:
        SET_BIT(dpad_status, JOYPAD_UP);
        SET_BIT(dpad_status, JOYPAD_RIGHT);
        break;
    case JOYPAD_SELECT + 3:
        SET_BIT(dpad_status, JOYPAD_DOWN);
        SET_BIT(dpad_status, JOYPAD_LEFT);
        break;
    case JOYPAD_SELECT + 4:
        SET_BIT(dpad_status, JOYPAD_DOWN);
        SET_BIT(dpad_status, JOYPAD_RIGHT);
        break;
    }

    buttons[0].texture = dpad_textures[dpad_status];
}

static void button_release(SDL_TouchID touch_id) {
    for (int i = JOYPAD_LEFT; i <= JOYPAD_SELECT; i++)
        emulator_joypad_release(emu, i);
    dpad_status = 0;
    buttons[0].texture = dpad_textures[dpad_status];
    buttons[1].texture = a_texture;
    buttons[2].texture = b_texture;
    buttons[3].texture = start_texture;
    buttons[4].texture = select_texture;

    for (int i = 0; i < SDL_GetNumTouchFingers(touch_id); i++) {
        SDL_Finger *f = SDL_GetTouchFinger(touch_id, i);
        if (!f) continue;
        s_byte_t hovered = is_finger_over_button(f->x, f->y);
        if (hovered >= 0)
            button_press(emu, hovered);
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
        button_release(event->touchId);
}

static inline void touch_motion(SDL_TouchFingerEvent *event) {
    s_byte_t previous = is_finger_over_button(event->x - event->dx, event->y - event->dy);
    s_byte_t hovered = is_finger_over_button(event->x, event->y);

    if (previous != hovered) {
        if (previous >= 0)
            button_release(event->touchId);
        if (hovered >= 0)
            button_press(emu, hovered);
    }
}

static void set_layout(int layout) {
    is_landscape = layout;

    switch (layout) {
    case 0: // portrait
        SDL_RenderSetLogicalSize(renderer, screen_width, screen_height);
        gb_screen_rect.w = screen_width;
        gb_screen_rect.h = GB_SCREEN_HEIGHT * (gb_screen_rect.w / GB_SCREEN_WIDTH);
        gb_screen_rect.x = 0;
        gb_screen_rect.y = 0;

        buttons[0].shape.x = portrait_dpad_x;
        buttons[0].shape.y = portrait_dpad_y;
        buttons[1].shape.x = portrait_a_x;
        buttons[1].shape.y = portrait_a_y;
        buttons[2].shape.x = portrait_b_x;
        buttons[2].shape.y = portrait_b_y;
        buttons[3].shape.x = portrait_start_x;
        buttons[3].shape.y = portrait_start_y;
        buttons[4].shape.x = portrait_select_x;
        buttons[4].shape.y = portrait_select_y;
        break;
    case 1: // landscape
        SDL_RenderSetLogicalSize(renderer, screen_height, screen_width);
        gb_screen_rect.h = screen_width;
        gb_screen_rect.w = GB_SCREEN_WIDTH * (gb_screen_rect.h / GB_SCREEN_HEIGHT);
        gb_screen_rect.x = screen_height / 2 - gb_screen_rect.w / 2;
        gb_screen_rect.y = 0;

        buttons[0].shape.x = landscape_dpad_x;
        buttons[0].shape.y = landscape_dpad_y;
        buttons[1].shape.x = landscape_a_x;
        buttons[1].shape.y = landscape_a_y;
        buttons[2].shape.x = landscape_b_x;
        buttons[2].shape.y = landscape_b_y;
        buttons[3].shape.x = landscape_start_x;
        buttons[3].shape.y = landscape_start_y;
        buttons[4].shape.x = landscape_select_x;
        buttons[4].shape.y = landscape_select_y;
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
            cycles -= GB_CPU_CYCLES_PER_FRAME * speed;
            if (frame_count >= frame_skip) {
                SDL_RenderClear(renderer);
                SDL_RenderCopy(renderer, ppu_texture, NULL, &gb_screen_rect);
                // draw buttons
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
        jfloat buttons_opacity,
        jfloat port_dpad_x,
        jfloat port_dpad_y,
        jfloat port_a_x,
        jfloat port_a_y,
        jfloat port_b_x,
        jfloat port_b_y,
        jfloat port_start_x,
        jfloat port_start_y,
        jfloat port_select_x,
        jfloat port_select_y,
        jfloat land_dpad_x,
        jfloat land_dpad_y,
        jfloat land_a_x,
        jfloat land_a_y,
        jfloat land_b_x,
        jfloat land_b_y,
        jfloat land_start_x,
        jfloat land_start_y,
        jfloat land_select_x,
        jfloat land_select_y)
{
    jboolean is_copy;
    jbyte *rom_data = (*env)->GetByteArrayElements(env, data, &is_copy);

    load_cartridge((byte_t *) rom_data, size, resume, emu_mode, palette, emu_speed, sound);
    frame_skip = emu_frame_skip;

    (*env)->ReleaseByteArrayElements(env, data, rom_data, JNI_ABORT);

    portrait_dpad_x = port_dpad_x * (float) screen_width;
    portrait_dpad_y = port_dpad_y * (float) screen_height;
    portrait_a_x = port_a_x * (float) screen_width;
    portrait_a_y = port_a_y * (float) screen_height;
    portrait_b_x = port_b_x * (float) screen_width;
    portrait_b_y = port_b_y * (float) screen_height;
    portrait_start_x = port_start_x * (float) screen_width;
    portrait_start_y = port_start_y * (float) screen_height;
    portrait_select_x = port_select_x * (float) screen_width;
    portrait_select_y = port_select_y * (float) screen_height;

    landscape_dpad_x = land_dpad_x * (float) screen_height;
    landscape_dpad_y = land_dpad_y * (float) screen_width;
    landscape_a_x = land_a_x * (float) screen_height;
    landscape_a_y = land_a_y * (float) screen_width;
    landscape_b_x = land_b_x * (float) screen_height;
    landscape_b_y = land_b_y * (float) screen_width;
    landscape_start_x = land_start_x * (float) screen_height;
    landscape_start_y = land_start_y * (float) screen_width;
    landscape_select_x = land_select_x * (float) screen_height;
    landscape_select_y = land_select_y * (float) screen_width;

    set_layout(is_landscape);

    SDL_Surface *surface = SDL_LoadBMP("dpad_pressed_left.bmp");
    dpad_textures[1] = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    surface = SDL_LoadBMP("dpad_pressed_right.bmp");
    dpad_textures[2] = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    surface = SDL_LoadBMP("dpad_pressed_left-right.bmp");
    dpad_textures[3] = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    surface = SDL_LoadBMP("dpad_pressed_up.bmp");
    dpad_textures[4] = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    surface = SDL_LoadBMP("dpad_pressed_up-left.bmp");
    dpad_textures[5] = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    surface = SDL_LoadBMP("dpad_pressed_up-right.bmp");
    dpad_textures[6] = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    surface = SDL_LoadBMP("dpad_pressed_up-left-right.bmp");
    dpad_textures[7] = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    surface = SDL_LoadBMP("dpad_pressed_down.bmp");
    dpad_textures[8] = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    surface = SDL_LoadBMP("dpad_pressed_down-left.bmp");
    dpad_textures[9] = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    surface = SDL_LoadBMP("dpad_pressed_down-right.bmp");
    dpad_textures[10] = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    surface = SDL_LoadBMP("dpad_pressed_down-left-right.bmp");
    dpad_textures[11] = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    surface = SDL_LoadBMP("dpad_pressed_up-down.bmp");
    dpad_textures[12] = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    surface = SDL_LoadBMP("dpad_pressed_up-left-down.bmp");
    dpad_textures[13] = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    surface = SDL_LoadBMP("dpad_pressed_up-right-down.bmp");
    dpad_textures[14] = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    surface = SDL_LoadBMP("dpad_pressed_all.bmp");
    dpad_textures[15] = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    surface = SDL_LoadBMP("a_pressed.bmp");
    a_pressed_texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    surface = SDL_LoadBMP("b_pressed.bmp");
    b_pressed_texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    surface = SDL_LoadBMP("start_pressed.bmp");
    start_pressed_texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    surface = SDL_LoadBMP("select_pressed.bmp");
    select_pressed_texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    SDL_SetTextureAlphaMod(a_pressed_texture, buttons_opacity * 0xFF);
    SDL_SetTextureAlphaMod(b_pressed_texture, buttons_opacity * 0xFF);
    SDL_SetTextureAlphaMod(start_pressed_texture, buttons_opacity * 0xFF);
    SDL_SetTextureAlphaMod(select_pressed_texture, buttons_opacity * 0xFF);
    for (int i = 0; i < 5; i++)
        SDL_SetTextureAlphaMod(buttons[i].texture, buttons_opacity * 0xFF);
    for (int i = 0; i < 16; i++)
        SDL_SetTextureAlphaMod(dpad_textures[i], buttons_opacity * 0xFF);

    start_emulation_loop();
}

JNIEXPORT void JNICALL Java_io_github_mpostaire_gbmulator_Emulator_enterLayoutEditor(
        JNIEnv* env,
        jobject thiz,
        jfloat buttons_opacity,
        jboolean is_landscape,
        jfloat dpad_x,
        jfloat dpad_y,
        jfloat a_x,
        jfloat a_y,
        jfloat b_x,
        jfloat b_y,
        jfloat start_x,
        jfloat start_y,
        jfloat select_x,
        jfloat select_y)
{
    portrait_dpad_x = dpad_x * (float) screen_width;
    portrait_dpad_y = dpad_y * (float) screen_height;
    portrait_a_x = a_x * (float) screen_width;
    portrait_a_y = a_y * (float) screen_height;
    portrait_b_x = b_x * (float) screen_width;
    portrait_b_y = b_y * (float) screen_height;
    portrait_start_x = start_x * (float) screen_width;
    portrait_start_y = start_y * (float) screen_height;
    portrait_select_x = select_x * (float) screen_width;
    portrait_select_y = select_y * (float) screen_height;

    landscape_dpad_x = dpad_x * (float) screen_height;
    landscape_dpad_y = dpad_y * (float) screen_width;
    landscape_a_x = a_x * (float) screen_height;
    landscape_a_y = a_y * (float) screen_width;
    landscape_b_x = b_x * (float) screen_height;
    landscape_b_y = b_y * (float) screen_width;
    landscape_start_x = start_x * (float) screen_height;
    landscape_start_y = start_y * (float) screen_width;
    landscape_select_x = select_x * (float) screen_height;
    landscape_select_y = select_y * (float) screen_width;

    set_layout(is_landscape);

    button_t *bs = xmalloc(sizeof(button_t) * 5);
    for (int i = 0; i < 5; i++) {
        SDL_SetTextureAlphaMod(buttons[i].texture, buttons_opacity * 0xFF);
        bs[i] = buttons[i];
    }
    start_layout_editor(renderer, is_landscape, screen_width, screen_height, &gb_screen_rect, bs);
    free(bs);
}

int main(int argc, char **argv) {
    emu = NULL;

    // initialize global variables here and not at their initialization as they can still have their
    // previous values because of android's activities lifecycle
    is_running = SDL_TRUE;
    speed = 1.0f;

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);

    // get the minimum possible size of the window while keeping the aspect ratio
    SDL_DisplayMode display_mode;
    if (!SDL_GetCurrentDisplayMode(0, &display_mode)) {
        screen_width = GB_SCREEN_WIDTH;
        if (display_mode.h > display_mode.w)
            screen_height = display_mode.h / (float) display_mode.w * screen_width;
        else
            screen_height = display_mode.w / (float) display_mode.h * screen_width;
    } else {
        // couldn't get the device's screen size, assume 1080x1920
        screen_width = GB_SCREEN_WIDTH;
        screen_height = 1920 / (float) 1080 * screen_width; // 284
    }

    window = SDL_CreateWindow(
        EMULATOR_NAME,
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        screen_width,
        screen_height,
        SDL_WINDOW_FULLSCREEN | SDL_WINDOW_RESIZABLE
    );

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    SDL_Surface *surface = SDL_LoadBMP("dpad.bmp");
    buttons[0].texture = SDL_CreateTextureFromSurface(renderer, surface);
    dpad_textures[0] = buttons[0].texture;
    SDL_FreeSurface(surface);

    surface = SDL_LoadBMP("a.bmp");
    a_texture = SDL_CreateTextureFromSurface(renderer, surface);
    buttons[1].texture = a_texture;
    SDL_FreeSurface(surface);

    surface = SDL_LoadBMP("b.bmp");
    b_texture = SDL_CreateTextureFromSurface(renderer, surface);
    buttons[2].texture = b_texture;
    SDL_FreeSurface(surface);

    surface = SDL_LoadBMP("start.bmp");
    start_texture = SDL_CreateTextureFromSurface(renderer, surface);
    buttons[3].texture = start_texture;
    SDL_FreeSurface(surface);

    surface = SDL_LoadBMP("select.bmp");
    select_texture = SDL_CreateTextureFromSurface(renderer, surface);
    buttons[4].texture = select_texture;
    SDL_FreeSurface(surface);

    ready();

    for (int i = 0; i < 5; i++)
        SDL_DestroyTexture(buttons[i].texture);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    SDL_Quit();
    return EXIT_SUCCESS;
}
