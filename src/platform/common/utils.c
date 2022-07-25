#ifdef __ANDROID__
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#endif

#include "emulator/emulator.h"
#include "config.h"

int sdl_key_to_joypad(SDL_Keycode key) {
    if (key == config.left) return JOYPAD_LEFT;
    if (key == config.right) return JOYPAD_RIGHT;
    if (key == config.up) return JOYPAD_UP;
    if (key == config.down) return JOYPAD_DOWN;
    if (key == config.a) return JOYPAD_A;
    if (key == config.b) return JOYPAD_B;
    if (key == config.start) return JOYPAD_START;
    if (key == config.select) return JOYPAD_SELECT;
    return key;
}

int sdl_controller_to_joypad(SDL_GameControllerButton button) {
    switch (button) {
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return JOYPAD_LEFT;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return JOYPAD_RIGHT;
    case SDL_CONTROLLER_BUTTON_DPAD_UP: return JOYPAD_UP;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return JOYPAD_DOWN;
    case SDL_CONTROLLER_BUTTON_A: return JOYPAD_A;
    case SDL_CONTROLLER_BUTTON_B: return JOYPAD_B;
    case SDL_CONTROLLER_BUTTON_START:
    case SDL_CONTROLLER_BUTTON_X:
        return JOYPAD_START;
    case SDL_CONTROLLER_BUTTON_BACK:
    case SDL_CONTROLLER_BUTTON_Y:
        return JOYPAD_SELECT;
    default:
        return SDL_CONTROLLER_BUTTON_INVALID;
    }
}
