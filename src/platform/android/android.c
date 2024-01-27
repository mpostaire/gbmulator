#include <stdlib.h>
#include <stdio.h>
#include <SDL.h>
#include <android/log.h>
#include <jni.h>

#include "../common/utils.h"
#include "../common/link.h"
#include "layout_editor.h"
#include "../../core/gb.h"

#define log(...) __android_log_print(ANDROID_LOG_WARN, "GBmulator", __VA_ARGS__)

// TODO savestates ui

static SDL_bool is_running;
static SDL_bool is_landscape;
static SDL_bool show_link_dialog;
static SDL_bool init_handshake;

static int frame_skip;

static int screen_width;
static int screen_height;

static SDL_Renderer *renderer;
static SDL_Window *window;

static SDL_Texture *ppu_texture;
static int ppu_texture_pitch;

#define N_BUFFERS 8
#define N_SAMPLES 1024
#define SAMPLING_RATE 44100
static SDL_AudioDeviceID audio_device;
static gb_apu_sample_t samples[N_SAMPLES];
static int samples_count;

static int link_sfd;

static gb_t *gb;
static gb_t *linked_gb;

static SDL_Rect gb_screen_rect;

static float portrait_dpad_x, portrait_dpad_y, portrait_a_x, portrait_a_y, portrait_b_x, portrait_b_y;
static float portrait_start_x, portrait_start_y, portrait_select_x, portrait_select_y, portrait_link_x, portrait_link_y;

static float landscape_dpad_x, landscape_dpad_y, landscape_a_x, landscape_a_y, landscape_b_x, landscape_b_y;
static float landscape_start_x, landscape_start_y, landscape_select_x, landscape_select_y, landscape_link_x, landscape_link_y;

static int dpad_status;

static SDL_Texture *dpad_textures[16];

static SDL_Texture *a_texture, *b_texture, *start_texture, *select_texture;
static SDL_Texture *a_pressed_texture, *b_pressed_texture, *start_pressed_texture, *select_pressed_texture;
static SDL_Texture *link_texture, *link_pressed_texture;

static int steps_per_frame;

static byte_t *camera_data;
static int camera_width, camera_height, camera_row_stride, camera_rotation;

static button_t buttons[] = {
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
        .button = JOYPAD_SELECT
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
            .h = 16,
            .w = 16
        },
        .button = JOYPAD_START + 5
    }
};

// --- send and recv functions for an android bluetooth connection
// TODO edit ../common/link.h to support custom send/recv functions (need to do this because it isn't possible to
// get the bluetooth socket's file descriptor

// TODO optimize by storing method_id as global variable
// static inline ssize_t send_bt(int fd, const void *buf, size_t n, int flags) {
//     JNIEnv *env = (JNIEnv *) SDL_AndroidGetJNIEnv();
//     jobject activity = (jobject) SDL_AndroidGetActivity();
//     jclass clazz = (*env)->GetObjectClass(env, activity);
//     jmethodID method_id = (*env)->GetMethodID(env, clazz, "linkSendData", "([B)I");

//     //log("SEND_PKT BEFORE");

//     jbyteArray array = (*env)->NewByteArray(env, n);
//     (*env)->SetByteArrayRegion(env, array, 0, n, (jbyte *) buf);
//     ssize_t ret = (*env)->CallIntMethod(env, activity, method_id, array);

//     //log("SEND_PKT AFTER");

//     (*env)->DeleteLocalRef(env, array);
//     (*env)->DeleteLocalRef(env, activity);
//     (*env)->DeleteLocalRef(env, clazz);
//     return ret;
// }

// TODO optimize by storing method_id as global variable
// static inline ssize_t recv_bt(int fd, void *buf, size_t n, int flags) {
//     JNIEnv *env = (JNIEnv *) SDL_AndroidGetJNIEnv();
//     jobject activity = (jobject) SDL_AndroidGetActivity();
//     jclass clazz = (*env)->GetObjectClass(env, activity);
//     jmethodID method_id = (*env)->GetMethodID(env, clazz, "linkReceiveData", "(I)[B");

//     //log("RECV_PKT BEFORE");

//     jbyteArray array = (*env)->CallObjectMethod(env, activity, method_id, n);
//     if (!array) return -1;
//     ssize_t len = (*env)->GetArrayLength(env, array);
//     if (len > n) return -1;

//     //log("RECV_PKT AFTER");

//     (*env)->GetByteArrayRegion(env, array, 0, len, (jbyte *) buf);

//     //log("RECV_PKT AFTER AFTER");

//     (*env)->DeleteLocalRef(env, array);
//     (*env)->DeleteLocalRef(env, activity);
//     (*env)->DeleteLocalRef(env, clazz);
//     return len;
// }

// ---

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
                return JOYPAD_START + 1;
            else if (y > 2 * (hitbox->h / 3)) // down left
                return JOYPAD_START + 3;
            else
                return JOYPAD_LEFT;
        } else if (x > 2 * (hitbox->w / 3)) {
            if (y < hitbox->h / 3) // up right
                return JOYPAD_START + 2;
            else if (y > 2 * (hitbox->h / 3)) // down right
                return JOYPAD_START + 4;
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

    for (s_byte_t i = 1; i < 6; i++) {
        SDL_Rect *hitbox = &buttons[i].shape;
        if (x > hitbox->x && x < hitbox->x + hitbox->w && y > hitbox->y && y < hitbox->y + hitbox->h)
            return buttons[i].button;
    }

    return -1;
}

static void button_press(gb_t *gb, gb_joypad_button_t button) {
    switch ((int) button) { // cast to int to shut compiler warnings
    case JOYPAD_START + 1:
        gb_joypad_press(gb, JOYPAD_UP);
        gb_joypad_press(gb, JOYPAD_LEFT);
        break;
    case JOYPAD_START + 2:
        gb_joypad_press(gb, JOYPAD_UP);
        gb_joypad_press(gb, JOYPAD_RIGHT);
        break;
    case JOYPAD_START + 3:
        gb_joypad_press(gb, JOYPAD_DOWN);
        gb_joypad_press(gb, JOYPAD_LEFT);
        break;
    case JOYPAD_START + 4:
        gb_joypad_press(gb, JOYPAD_DOWN);
        gb_joypad_press(gb, JOYPAD_RIGHT);
        break;
    case JOYPAD_START + 5:
        show_link_dialog = SDL_TRUE;
        break;
    default:
        gb_joypad_press(gb, button);
        break;
    }

    switch ((int) button) { // cast to int to shut compiler warnings
    case JOYPAD_RIGHT:
    case JOYPAD_LEFT:
    case JOYPAD_UP:
    case JOYPAD_DOWN:
        SET_BIT(dpad_status, button);
        break;
    case JOYPAD_A:
        buttons[1].texture = a_pressed_texture;
        return;
    case JOYPAD_B:
        buttons[2].texture = b_pressed_texture;
        return;
    case JOYPAD_SELECT:
        buttons[3].texture = select_pressed_texture;
        return;
    case JOYPAD_START:
        buttons[4].texture = start_pressed_texture;
        return;
    case JOYPAD_START + 1:
        SET_BIT(dpad_status, JOYPAD_UP);
        SET_BIT(dpad_status, JOYPAD_LEFT);
        break;
    case JOYPAD_START + 2:
        SET_BIT(dpad_status, JOYPAD_UP);
        SET_BIT(dpad_status, JOYPAD_RIGHT);
        break;
    case JOYPAD_START + 3:
        SET_BIT(dpad_status, JOYPAD_DOWN);
        SET_BIT(dpad_status, JOYPAD_LEFT);
        break;
    case JOYPAD_START + 4:
        SET_BIT(dpad_status, JOYPAD_DOWN);
        SET_BIT(dpad_status, JOYPAD_RIGHT);
        break;
    case JOYPAD_START + 5:
        buttons[5].texture = link_pressed_texture;
        break;
    }

    buttons[0].texture = dpad_textures[dpad_status];
}

static void button_release(SDL_TouchID touch_id) {
    for (int i = JOYPAD_RIGHT; i <= JOYPAD_START; i++)
        gb_joypad_release(gb, i);
    dpad_status = 0;
    buttons[0].texture = dpad_textures[dpad_status];
    buttons[1].texture = a_texture;
    buttons[2].texture = b_texture;
    buttons[3].texture = select_texture;
    buttons[4].texture = start_texture;
    buttons[5].texture = link_texture;

    for (int i = 0; i < SDL_GetNumTouchFingers(touch_id); i++) {
        SDL_Finger *f = SDL_GetTouchFinger(touch_id, i);
        if (!f) continue;
        s_byte_t hovered = is_finger_over_button(f->x, f->y);
        if (hovered >= 0)
            button_press(gb, hovered);
    }

    if (show_link_dialog) {
        show_link_dialog = SDL_FALSE;

        JNIEnv *env = (JNIEnv *) SDL_AndroidGetJNIEnv();

        jobject activity = (jobject) SDL_AndroidGetActivity();

        jclass clazz = (*env)->GetObjectClass(env, activity);

        jmethodID method_id = (*env)->GetMethodID(env, clazz, "linkMenu", "()V");

        (*env)->CallVoidMethod(env, activity, method_id);

        (*env)->DeleteLocalRef(env, activity);
        (*env)->DeleteLocalRef(env, clazz);
    }
}

static inline void touch_press(SDL_TouchFingerEvent *event) {
    s_byte_t hovered = is_finger_over_button(event->x, event->y);
    if (hovered >= 0)
        button_press(gb, hovered);
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
        if (previous >= 0) {
            if (previous == JOYPAD_START + 5)
                show_link_dialog = SDL_FALSE;
            button_release(event->touchId);
        }
        if (hovered >= 0)
            button_press(gb, hovered);
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
        buttons[3].shape.x = portrait_select_x;
        buttons[3].shape.y = portrait_select_y;
        buttons[4].shape.x = portrait_start_x;
        buttons[4].shape.y = portrait_start_y;
        buttons[5].shape.x = portrait_link_x;
        buttons[5].shape.y = portrait_link_y;
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
        buttons[3].shape.x = landscape_select_x;
        buttons[3].shape.y = landscape_select_y;
        buttons[4].shape.x = landscape_start_x;
        buttons[4].shape.y = landscape_start_y;
        buttons[5].shape.x = landscape_link_x;
        buttons[5].shape.y = landscape_link_y;
        break;
    }
}

static void ppu_vblank_cb(const byte_t *pixels) {
    SDL_UpdateTexture(ppu_texture, NULL, pixels, ppu_texture_pitch);
}

#define DRC_TARGET_QUEUE_SIZE ((N_BUFFERS / 4) * N_SAMPLES)
#define DRC_MAX_FREQ_DIFF 0.02
#define DRC_ALPHA 0.1
static int ewma_queue_size;
static inline uint32_t dynamic_rate_control(int queue_size) {
    queue_size /= sizeof(gb_apu_sample_t);
    ewma_queue_size = queue_size * DRC_ALPHA + ewma_queue_size * (1.0 - DRC_ALPHA);

    double diff = (ewma_queue_size - DRC_TARGET_QUEUE_SIZE) / (double) DRC_TARGET_QUEUE_SIZE;
    return SAMPLING_RATE * (1.0 - CLAMP(diff, -1.0, 1.0) * DRC_MAX_FREQ_DIFF);
}
static void apu_new_sample_cb(const gb_apu_sample_t sample, uint32_t *dynamic_sampling_rate) {
    samples[samples_count++] = sample;
    if (samples_count < N_SAMPLES)
        return;
    samples_count = 0;

    Uint32 queue_size = SDL_GetQueuedAudioSize(audio_device);
    if (queue_size > N_BUFFERS * N_SAMPLES * sizeof(sample))
        return;
    SDL_QueueAudio(audio_device, &samples, sizeof(samples));

    *dynamic_sampling_rate = dynamic_rate_control(SDL_GetQueuedAudioSize(audio_device));
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
    ppu_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, GB_SCREEN_WIDTH, GB_SCREEN_HEIGHT);
    ppu_texture_pitch = GB_SCREEN_WIDTH * sizeof(byte_t) * 4;

    SDL_AudioSpec audio_settings = {
        .freq = SAMPLING_RATE,
        .format = AUDIO_S16SYS,
        .channels = 2,
        .samples = N_SAMPLES
    };
    audio_device = SDL_OpenAudioDevice(NULL, 0, &audio_settings, NULL, 0);
    SDL_PauseAudioDevice(audio_device, 0);

    int steps = 0;
    int frame_count = 0;
    while (is_running) {
        if (steps >= steps_per_frame) {
            steps -= steps_per_frame;
            if (frame_count >= frame_skip) {
                SDL_RenderClear(renderer);
                SDL_RenderCopy(renderer, ppu_texture, NULL, &gb_screen_rect);
                // draw buttons
                for (int i = 0; i < 6; i++)
                    SDL_RenderCopy(renderer, buttons[i].texture, NULL, &buttons[i].shape);
                SDL_RenderPresent(renderer);
                frame_count = 0;
            } else {
                SDL_Delay(1.0f / 60.0f);
            }
            frame_count++;
            // handle_input is a slow function: don't call it every step
            handle_input(); // keep this the closest possible before gb_step() to reduce input inaccuracies
            if (init_handshake) {
                init_handshake = SDL_FALSE;
                gb_t *new_linked_gb;
                if (link_init_transfer(link_sfd, gb, &new_linked_gb))
                    linked_gb = new_linked_gb;
            }
            if (linked_gb && !link_exchange_joypad(link_sfd, gb, linked_gb))
                linked_gb = NULL;
        }

        // run one step of the emulator
        gb_step(gb);
        steps++;

        // no delay at the end of the loop because the emulation is audio synced (the audio is what makes the delay).
    }

    if (gb) {
        char buf[512];

        snprintf(buf, sizeof(buf), "%s/%s", SDL_AndroidGetInternalStoragePath(), gb_get_rom_title(gb));
        save_battery_to_file(gb, buf);

        snprintf(buf, sizeof(buf), "%s/resume", SDL_AndroidGetInternalStoragePath());
        save_state_to_file(gb, buf, 0);

        gb_quit(gb);
    }

    if (camera_data)
        free(camera_data);

    SDL_DestroyTexture(ppu_texture);
    SDL_CloseAudioDevice(audio_device);
}

static void init_camera(void) {
    JNIEnv *env = (JNIEnv *) SDL_AndroidGetJNIEnv();
    jobject activity = (jobject) SDL_AndroidGetActivity();
    jclass clazz = (*env)->GetObjectClass(env, activity);

    jmethodID method_id = (*env)->GetMethodID(env, clazz, "initCamera", "()V");

    (*env)->CallVoidMethod(env, activity, method_id);

    (*env)->DeleteLocalRef(env, activity);
    (*env)->DeleteLocalRef(env, clazz);
}

JNIEXPORT void JNICALL Java_io_github_mpostaire_gbmulator_Emulator_updateCameraBuffer(
        JNIEnv* env,
        jobject thiz,
        jbyteArray data,
        jsize width,
        jsize height,
        jsize row_stride,
        jsize rotation)
{
    jboolean is_copy;
    jbyte *image = (*env)->GetByteArrayElements(env, data, &is_copy);

    size_t sz = width * height * sizeof(*camera_data);
    if (!camera_data)
        camera_data = xmalloc(sz);
    memcpy(camera_data, image, sz);
    camera_width = width;
    camera_height = height;
    camera_row_stride = row_stride;
    camera_rotation = rotation;

    (*env)->ReleaseByteArrayElements(env, data, image, JNI_ABORT);

    // log("received image %p w=%d h=%d rs=%d rot=%d", camera_data, width, height, row_stride, rotation);
}

static byte_t camera_capture_image_cb(byte_t *pixels) {
    if (camera_data) {
        fit_image(pixels, camera_data, camera_width, camera_height, camera_row_stride, camera_rotation);
        return 1;
    }
    return 0;
}

static void load_cartridge(const byte_t *rom, size_t rom_size, int resume, int emu_mode, int palette, float emu_speed, float sound) {
    gb_options_t opts = {
        .mode = emu_mode,
        .on_new_sample = apu_new_sample_cb,
        .on_new_frame = ppu_vblank_cb,
        .apu_speed = emu_speed,
        .apu_sampling_rate = SAMPLING_RATE,
        .apu_sound_level = sound,
        .on_camera_capture_image = camera_capture_image_cb,
        .palette = palette
    };
    gb = gb_init(rom, rom_size, &opts);
    if (!gb) return;

    if (gb_has_camera(gb))
        init_camera();

    if (resume) {
        char buf[512];
        snprintf(buf, sizeof(buf), "%s/resume", SDL_AndroidGetInternalStoragePath());
        load_state_from_file(gb, buf);
    } else {
        char buf[512];
        snprintf(buf, sizeof(buf), "%s/%s", SDL_AndroidGetInternalStoragePath(), gb_get_rom_title(gb));
        load_battery_from_file(gb, buf);
    }

    steps_per_frame = GB_CPU_STEPS_PER_FRAME * emu_speed;
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
        jfloat port_link_x,
        jfloat port_link_y,
        jfloat land_dpad_x,
        jfloat land_dpad_y,
        jfloat land_a_x,
        jfloat land_a_y,
        jfloat land_b_x,
        jfloat land_b_y,
        jfloat land_start_x,
        jfloat land_start_y,
        jfloat land_select_x,
        jfloat land_select_y,
        jfloat land_link_x,
        jfloat land_link_y)
{
    jboolean is_copy;
    jbyte *rom = (*env)->GetByteArrayElements(env, data, &is_copy);

    load_cartridge((byte_t *) rom, size, resume, emu_mode, palette, emu_speed, sound);
    frame_skip = emu_frame_skip;

    (*env)->ReleaseByteArrayElements(env, data, rom, JNI_ABORT);

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
    portrait_link_x = port_link_x * (float) screen_width;
    portrait_link_y = port_link_y * (float) screen_height;

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
    landscape_link_x = land_link_x * (float) screen_height;
    landscape_link_y = land_link_y * (float) screen_width;

    set_layout(is_landscape);

    SDL_Surface *surface = SDL_LoadBMP("dpad_pressed_right.bmp");
    dpad_textures[1] = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    surface = SDL_LoadBMP("dpad_pressed_left.bmp");
    dpad_textures[2] = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    surface = SDL_LoadBMP("dpad_pressed_left-right.bmp");
    dpad_textures[3] = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    surface = SDL_LoadBMP("dpad_pressed_up.bmp");
    dpad_textures[4] = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    surface = SDL_LoadBMP("dpad_pressed_up-right.bmp");
    dpad_textures[5] = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    surface = SDL_LoadBMP("dpad_pressed_up-left.bmp");
    dpad_textures[6] = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    surface = SDL_LoadBMP("dpad_pressed_up-left-right.bmp");
    dpad_textures[7] = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    surface = SDL_LoadBMP("dpad_pressed_down.bmp");
    dpad_textures[8] = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    surface = SDL_LoadBMP("dpad_pressed_down-right.bmp");
    dpad_textures[9] = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    surface = SDL_LoadBMP("dpad_pressed_down-left.bmp");
    dpad_textures[10] = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    surface = SDL_LoadBMP("dpad_pressed_down-left-right.bmp");
    dpad_textures[11] = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    surface = SDL_LoadBMP("dpad_pressed_up-down.bmp");
    dpad_textures[12] = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    surface = SDL_LoadBMP("dpad_pressed_up-right-down.bmp");
    dpad_textures[13] = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    surface = SDL_LoadBMP("dpad_pressed_up-left-down.bmp");
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

    surface = SDL_LoadBMP("link_pressed.bmp");
    link_pressed_texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    SDL_SetTextureAlphaMod(a_pressed_texture, buttons_opacity * 0xFF);
    SDL_SetTextureAlphaMod(b_pressed_texture, buttons_opacity * 0xFF);
    SDL_SetTextureAlphaMod(start_pressed_texture, buttons_opacity * 0xFF);
    SDL_SetTextureAlphaMod(select_pressed_texture, buttons_opacity * 0xFF);
    SDL_SetTextureAlphaMod(link_pressed_texture, buttons_opacity * 0xFF);
    for (int i = 0; i < 6; i++)
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
        jfloat select_y,
        jfloat link_x,
        jfloat link_y)
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
    portrait_link_x = link_x * (float) screen_width;
    portrait_link_y = link_y * (float) screen_height;

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
    landscape_link_x = link_x * (float) screen_height;
    landscape_link_y = link_y * (float) screen_width;

    set_layout(is_landscape);

    button_t *bs = xmalloc(sizeof(button_t) * 6);
    for (int i = 0; i < 6; i++) {
        SDL_SetTextureAlphaMod(buttons[i].texture, buttons_opacity * 0xFF);
        bs[i] = buttons[i];
    }
    start_layout_editor(renderer, is_landscape, screen_width, screen_height, &gb_screen_rect, bs);
    free(bs);
}

JNIEXPORT void JNICALL Java_io_github_mpostaire_gbmulator_Emulator_tcpLinkReady(JNIEnv* env, jobject thiz, jint sfd) {
    link_sfd = sfd;
    init_handshake = SDL_TRUE;
}

int main(int argc, char **argv) {
    gb = NULL;
    linked_gb = NULL;
    link_sfd = -1;
    init_handshake = SDL_FALSE;
    camera_data = NULL;

    // initialize global variables here and not at their initialization as they can still have their
    // previous values because of android's activities lifecycle
    is_running = SDL_TRUE;
    show_link_dialog = SDL_FALSE;
    steps_per_frame = GB_CPU_STEPS_PER_FRAME;

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

    surface = SDL_LoadBMP("select.bmp");
    select_texture = SDL_CreateTextureFromSurface(renderer, surface);
    buttons[3].texture = select_texture;
    SDL_FreeSurface(surface);

    surface = SDL_LoadBMP("start.bmp");
    start_texture = SDL_CreateTextureFromSurface(renderer, surface);
    buttons[4].texture = start_texture;
    SDL_FreeSurface(surface);

    surface = SDL_LoadBMP("link.bmp");
    link_texture = SDL_CreateTextureFromSurface(renderer, surface);
    buttons[5].texture = link_texture;
    SDL_FreeSurface(surface);

    ready();

    for (int i = 0; i < 5; i++)
        SDL_DestroyTexture(buttons[i].texture);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    SDL_Quit();
    return EXIT_SUCCESS;
}
