#include <SDL.h>
#include <jni.h>
#include <android/log.h>

#include "emulator/emulator.h"
#include "layout_editor.h"

#define log(...) __android_log_print(ANDROID_LOG_INFO, "GBmulator", __VA_ARGS__)

SDL_bool is_running;
SDL_bool is_landscape;

int screen_width;
int screen_height;

button_t *buttons;
int moving;
int moving_offset_x;
int moving_offset_y;

static inline s_byte_t is_finger_over_ui_element(float x, float y) {
    if (is_landscape) {
        x *= screen_height;
        y *= screen_width;
    } else {
        x *= screen_width;
        y *= screen_height;
    }

    for (s_byte_t i = 5; i >= 0; i--) {
        SDL_Rect *hitbox = &buttons[i].shape;
        if (x > hitbox->x && x < hitbox->x + hitbox->w && y > hitbox->y && y < hitbox->y + hitbox->h)
            return i;
    }

    return -1;
}

static inline void touch_press(SDL_TouchFingerEvent *event) {
    if (moving >= 0) return;
    moving = is_finger_over_ui_element(event->x, event->y);

    float x = event->x;
    float y = event->y;
    if (is_landscape) {
        x *= screen_height;
        y *= screen_width;
    } else {
        x *= screen_width;
        y *= screen_height;
    }

    moving_offset_x = x - buttons[moving].shape.x;
    moving_offset_y = y - buttons[moving].shape.y;
}

static inline void touch_release(SDL_TouchFingerEvent *event) {
    moving = -1;
}

static inline void touch_motion(SDL_TouchFingerEvent *event) {
    if (moving < 0) return;

    float x = event->x;
    float y = event->y;
    int max_width;
    int max_height;
    if (is_landscape) {
        x *= screen_height;
        y *= screen_width;
        max_width = screen_height;
        max_height = screen_width;
    } else {
        x *= screen_width;
        y *= screen_height;
        max_width = screen_width;
        max_height = screen_height;
    }

    SDL_Rect *moving_rect = &buttons[moving].shape;
    moving_rect->x = x - moving_offset_x;
    moving_rect->y = y - moving_offset_y;

    if (moving_rect->x < 0)
        moving_rect->x = 0;
    else if (moving_rect->x + moving_rect->w >= max_width)
        moving_rect->x = max_width - moving_rect->w;
    if (moving_rect->y < 0)
        moving_rect->y = 0;
    else if (moving_rect->y + moving_rect->h >= max_height)
        moving_rect->y = max_height - moving_rect->h;
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
        case SDL_APP_WILLENTERBACKGROUND:
            // continue editing layout
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

static void apply_layout_preferences(void) {
    JNIEnv *env = (JNIEnv *) SDL_AndroidGetJNIEnv();

    jobject activity = (jobject) SDL_AndroidGetActivity();

    jclass clazz = (*env)->GetObjectClass(env, activity);

    jmethodID method_id = (*env)->GetMethodID(env, clazz, "applyLayoutPreferences", "(ZFFFFFFFFFFFF)V");

    if (is_landscape) {
        (*env)->CallVoidMethod(
            env, activity, method_id,
            SDL_TRUE,
            buttons[0].shape.x / (float) screen_height, buttons[0].shape.y / (float) screen_width,
            buttons[1].shape.x / (float) screen_height, buttons[1].shape.y / (float) screen_width,
            buttons[2].shape.x / (float) screen_height, buttons[2].shape.y / (float) screen_width,
            buttons[3].shape.x / (float) screen_height, buttons[3].shape.y / (float) screen_width,
            buttons[4].shape.x / (float) screen_height, buttons[4].shape.y / (float) screen_width,
            buttons[5].shape.x / (float) screen_height, buttons[5].shape.y / (float) screen_width);
    } else {
        (*env)->CallVoidMethod(
            env, activity, method_id,
            SDL_FALSE,
            buttons[0].shape.x / (float) screen_width, buttons[0].shape.y / (float) screen_height,
            buttons[1].shape.x / (float) screen_width, buttons[1].shape.y / (float) screen_height,
            buttons[2].shape.x / (float) screen_width, buttons[2].shape.y / (float) screen_height,
            buttons[3].shape.x / (float) screen_width, buttons[3].shape.y / (float) screen_height,
            buttons[4].shape.x / (float) screen_width, buttons[4].shape.y / (float) screen_height,
            buttons[5].shape.x / (float) screen_width, buttons[5].shape.y / (float) screen_height);
    }

    (*env)->DeleteLocalRef(env, activity);
    (*env)->DeleteLocalRef(env, clazz);
}

void start_layout_editor(
    SDL_Renderer *renderer,
    SDL_bool landscape,
    int scr_width,
    int scr_height,
    SDL_Rect *gb_screen_rect,
    button_t *gb_buttons)
{
    is_running = SDL_TRUE;
    is_landscape = landscape;
    screen_width = scr_width;
    screen_height = scr_height;
    moving = -1;
    buttons = gb_buttons;

    SDL_Surface *gb_screen_surface = SDL_LoadBMP("screen.bmp");
    SDL_Texture *gb_screen_texture = SDL_CreateTextureFromSurface(renderer, gb_screen_surface);
    SDL_FreeSurface(gb_screen_surface);
    SDL_SetTextureAlphaMod(gb_screen_texture, 0x42);

    while (is_running) {
        handle_input();

        SDL_RenderClear(renderer);

        SDL_RenderCopy(renderer, gb_screen_texture, NULL, gb_screen_rect);
        for (int i = 0; i < 6; i++)
            SDL_RenderCopy(renderer, buttons[i].texture, NULL, &buttons[i].shape);

        SDL_RenderPresent(renderer);
        SDL_Delay(1.0f / 30.0f);
    }

    SDL_DestroyTexture(gb_screen_texture);

    apply_layout_preferences();
}
