#include <jni.h>

#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <game-activity/GameActivity.h>
#include <unistd.h>
#include <cassert>
#include <thread>
#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include "AndroidOut.h"

extern "C" {
#include "../../../../../common/app.h"
#include "../../../../../common/utils.h"
}

//! executes glGetString and outputs the result to logcat
#define PRINT_GL_STRING(s) {aout << #s": "<< glGetString(s) << std::endl;}

/*!
 * @brief if glGetString returns a space separated list of elements, prints each one on a new line
 *
 * This works by creating an istringstream of the input c-style string. Then that is used to create
 * a vector -- each element of the vector is a new element in the input string. Finally a foreach
 * loop consumes this and outputs it to logcat using @a aout
 */
#define PRINT_GL_STRING_AS_LIST(s) { \
std::istringstream extensionStream((const char *) glGetString(s));\
std::vector<std::string> extensionList(\
        std::istream_iterator<std::string>{extensionStream},\
        std::istream_iterator<std::string>());\
aout << #s":\n";\
for (auto& extension: extensionList) {\
    aout << extension << "\n";\
}\
aout << std::endl;\
}

EGLDisplay display_ = EGL_NO_DISPLAY;
EGLSurface surface_ = EGL_NO_SURFACE;
EGLContext context_ = EGL_NO_CONTEXT;
EGLint width_ = 0;
EGLint height_ = 0;
bool init_gl_context(android_app *app) {
    // Choose your render attributes
    constexpr EGLint attribs[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_BLUE_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_RED_SIZE, 8,
            EGL_DEPTH_SIZE, 24,
            EGL_NONE
    };

    // The default display is probably what you want on Android
    auto display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(display, nullptr, nullptr);

    // figure out how many configs there are
    EGLint numConfigs;
    eglChooseConfig(display, attribs, nullptr, 0, &numConfigs);

    // get the list of configurations
    std::unique_ptr<EGLConfig[]> supportedConfigs(new EGLConfig[numConfigs]);
    eglChooseConfig(display, attribs, supportedConfigs.get(), numConfigs, &numConfigs);

    // Find a config we like.
    // Could likely just grab the first if we don't care about anything else in the config.
    // Otherwise hook in your own heuristic
    auto config = *std::find_if(
            supportedConfigs.get(),
            supportedConfigs.get() + numConfigs,
            [&display](const EGLConfig &config) -> bool {
                EGLint red, green, blue, depth;
                if (eglGetConfigAttrib(display, config, EGL_RED_SIZE, &red)
                    && eglGetConfigAttrib(display, config, EGL_GREEN_SIZE, &green)
                    && eglGetConfigAttrib(display, config, EGL_BLUE_SIZE, &blue)
                    && eglGetConfigAttrib(display, config, EGL_DEPTH_SIZE, &depth)) {

                    aout << "Found config with " << red << ", " << green << ", " << blue << ", "
                         << depth << std::endl;
                    return red == 8 && green == 8 && blue == 8 && depth == 24;
                }
                return false;
            });

    aout << "Found " << numConfigs << " configs" << std::endl;
    aout << "Chose " << config << std::endl;

    // create the proper window surface
    EGLint format;
    eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format);
    EGLSurface surface = eglCreateWindowSurface(display, config, app->window, nullptr);

    // Create a GLES 3 context
    EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    EGLContext context = eglCreateContext(display, config, nullptr, contextAttribs);

    // get some window metrics
    auto madeCurrent = eglMakeCurrent(display, surface, surface, context);
    assert(madeCurrent);

    display_ = display;
    surface_ = surface;
    context_ = context;

    // make width and height invalid so it gets updated the first frame in @a updateRenderArea()
    width_ = -1;
    height_ = -1;

    PRINT_GL_STRING(GL_VENDOR);
    PRINT_GL_STRING(GL_RENDERER);
    PRINT_GL_STRING(GL_VERSION);
    PRINT_GL_STRING_AS_LIST(GL_EXTENSIONS);

    return true;
}

void deinit_gl_context() {
    if (display_ != EGL_NO_DISPLAY) {
        eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (context_ != EGL_NO_CONTEXT) {
            eglDestroyContext(display_, context_);
            context_ = EGL_NO_CONTEXT;
        }
        if (surface_ != EGL_NO_SURFACE) {
            eglDestroySurface(display_, surface_);
            surface_ = EGL_NO_SURFACE;
        }
        eglTerminate(display_);
        display_ = EGL_NO_DISPLAY;
    }
}

extern "C" {

static config_t config = {};
static float gRefreshRate = 60.0f;

JNIEXPORT void JNICALL Java_io_github_mpostaire_gbmulator_EmulatorActivity_updateCameraBuffer(
        JNIEnv* env,
        jobject thiz,
        jbyteArray data,
        jsize width,
        jsize height,
        jsize row_stride,
        jsize rotation)
{
    jboolean is_copy;
    jbyte *image = env->GetByteArrayElements(data, &is_copy);

    app_update_camera_buffer(reinterpret_cast<uint8_t *>(image), width, height, row_stride, rotation);

    env->ReleaseByteArrayElements(data, image, JNI_ABORT);
}

void showSocketDialogFromNative(android_app *pApp) {
    JNIEnv* env = nullptr;
    pApp->activity->vm->AttachCurrentThread(&env, nullptr);

    jclass cls = env->GetObjectClass(pApp->activity->javaGameActivity);
    jmethodID mid = env->GetMethodID(cls, "showSocketDialog", "()I");
    jint sfd = env->CallIntMethod(pApp->activity->javaGameActivity, mid);

    pApp->activity->vm->DetachCurrentThread();
}

static void init_camera(android_app *pApp) {
    JNIEnv* env = nullptr;
    pApp->activity->vm->AttachCurrentThread(&env, nullptr);

    jclass cls = env->GetObjectClass(pApp->activity->javaGameActivity);
    jmethodID mid = env->GetMethodID(cls, "initCamera", "()V");
    env->CallVoidMethod(pApp->activity->javaGameActivity, mid);
}

void init_activity(android_app *pApp, config_t *config) {
    JNIEnv* env = nullptr;
    pApp->activity->vm->AttachCurrentThread(&env, nullptr);

    jclass cls = env->GetObjectClass(pApp->activity->javaGameActivity);
    jmethodID mid = env->GetMethodID(cls, "getEmuParams", "()Lio/github/mpostaire/gbmulator/EmuParams;");
    jobject emuParamsObj = env->CallObjectMethod(pApp->activity->javaGameActivity, mid);

    if (!emuParamsObj) {
        aout << "Failed getting EmuParams" << std::endl;
        GameActivity_finish(pApp->activity);
        return;
    }

    jclass paramsCls = env->GetObjectClass(emuParamsObj);

    jfieldID romFdField = env->GetFieldID(paramsCls, "romFd", "I");
    jfieldID resetField = env->GetFieldID(paramsCls, "reset", "Z");
    jfieldID refreshRateField = env->GetFieldID(paramsCls, "refreshRate", "F");
    jfieldID modeField = env->GetFieldID(paramsCls, "mode", "I");
    jfieldID soundField = env->GetFieldID(paramsCls, "sound", "F");
    jfieldID speedField = env->GetFieldID(paramsCls, "speed", "F");
    jfieldID joypadOpacityField = env->GetFieldID(paramsCls, "joypadOpacity", "F");
    jfieldID paletteField = env->GetFieldID(paramsCls, "palette", "I");

    jint rom_fd = env->GetIntField(emuParamsObj, romFdField);
    jboolean reset = env->GetBooleanField(emuParamsObj, resetField);

    gRefreshRate = env->GetFloatField(emuParamsObj, refreshRateField);

    config->mode = (gbmulator_mode_t ) env->GetIntField(emuParamsObj, modeField);
    config->sound = env->GetFloatField(emuParamsObj, soundField);
    config->speed = env->GetFloatField(emuParamsObj, speedField);
    config->joypad_opacity = env->GetFloatField(emuParamsObj, joypadOpacityField);
    config->color_palette = (gb_color_palette_t) env->GetIntField(emuParamsObj, paletteField);

    config->sound_drc = true;
    config->enable_joypad = true;
    config->disable_save_config_to_file = true;

    aout << "Display refresh rate is " << gRefreshRate << " Hz" << std::endl;
    aout << "mode=" << config->mode << " sound=" << config->sound << " speed=" << config->speed << " joypad_opacity=" << config->joypad_opacity << " palette=" << config->color_palette << std::endl;

    pApp->activity->vm->DetachCurrentThread();

    if (rom_fd < 0) {
        aout << "Invalid file descriptor" << std::endl;
        return;
    }

    config->on_link_button_touched_user_data = pApp;
    config->on_link_button_touched = reinterpret_cast<link_touch_button_cb_t>(showSocketDialogFromNative);

    setenv("XDG_DATA_HOME", pApp->activity->externalDataPath, 1);
    aout << "savestate_dir: " << get_savestate_dir() << std::endl;
    aout << "save_dir: " << get_save_dir() << std::endl;

    app_init();

    app_load_config(config);

    FILE* f = fdopen(rom_fd, "r");

    size_t len;
    uint8_t *rom = read_file_f(f, &len);
    fclose(f);

    if (!app_load_cartridge(rom, len)) {
        GameActivity_finish(pApp->activity);
        return;
    }

    if (!reset)
        app_load_state(0);

    if (app_has_camera())
        init_camera(pApp);

    app_set_pause(false);
}

/*!
 * Handles commands sent to this Android application
 * @param pApp the app the commands are coming from
 * @param cmd the command to handle
 */
void handle_cmd(android_app *pApp, int32_t cmd) {
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            aout << "APP_CMD_INIT_WINDOW" << std::endl;
            // A new window is created.
            init_gl_context(pApp);
            init_activity(pApp, &config);
            pApp->userData = (void *) 1;
            break;
        case APP_CMD_TERM_WINDOW:
            aout << "APP_CMD_TERM_WINDOW" << std::endl;
            // The window is being destroyed. Use this to clean up your userData to avoid leaking
            // resources.

            pApp->userData = nullptr;

            app_save_state(0);
            app_quit();
            deinit_gl_context();

            GameActivity_finish(pApp->activity);
            break;
        default:
            break;
    }
}

/*!
 * Enable the motion events you want to handle; not handled events are
 * passed back to OS for further processing. For this example case,
 * only pointer and joystick devices are enabled.
 *
 * @param motionEvent the newly arrived GameActivityMotionEvent.
 * @return true if the event is from a pointer or joystick device,
 *         false for all other input devices.
 */
bool motion_event_filter_func(const GameActivityMotionEvent *motionEvent) {
    auto sourceClass = motionEvent->source & AINPUT_SOURCE_CLASS_MASK;
    return (sourceClass == AINPUT_SOURCE_CLASS_POINTER ||
            sourceClass == AINPUT_SOURCE_CLASS_JOYSTICK);
}

void handle_input(android_app *app) {
    // handle all queued inputs
    auto *inputBuffer = android_app_swap_input_buffers(app);
    if (!inputBuffer) {
        // no inputs yet.
        return;
    }

    // handle motion events (motionEventsCounts can be 0).
    for (auto i = 0; i < inputBuffer->motionEventsCount; i++) {
        auto &motionEvent = inputBuffer->motionEvents[i];
        auto action = motionEvent.action;

        // Find the pointer index, mask and bitshift to turn it into a readable value.
        auto pointerIndex = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
                >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;

        // get the x and y position of this event if it is not ACTION_MOVE.
        auto &pointer = motionEvent.pointers[pointerIndex];
        auto x = GameActivityPointerAxes_getX(&pointer);
        auto y = GameActivityPointerAxes_getY(&pointer);

        // determine the action type and process the event accordingly.
        switch (action & AMOTION_EVENT_ACTION_MASK) {
            case AMOTION_EVENT_ACTION_DOWN:
            case AMOTION_EVENT_ACTION_POINTER_DOWN:
                app_touch_press(pointer.id, x, y);
                break;

            case AMOTION_EVENT_ACTION_CANCEL:
                // treat the CANCEL as an UP event: doing nothing in the app, except
                // removing the pointer from the cache if pointers are locally saved.
                // code pass through on purpose.
            case AMOTION_EVENT_ACTION_UP:
            case AMOTION_EVENT_ACTION_POINTER_UP:
                app_touch_release(pointer.id, x, y);
                break;

            case AMOTION_EVENT_ACTION_MOVE:
                // There is no pointer index for ACTION_MOVE, only a snapshot of
                // all active pointers; app needs to cache previous active pointers
                // to figure out which ones are actually moved.
                for (auto index = 0; index < motionEvent.pointerCount; index++) {
                    pointer = motionEvent.pointers[index];
                    x = GameActivityPointerAxes_getX(&pointer);
                    y = GameActivityPointerAxes_getY(&pointer);
                }
                app_touch_move(pointer.id, (uint32_t) x, (uint32_t) y);
                break;
            default:
                aout << "Unknown MotionEvent Action: " << action << std::endl;
                break;
        }
    }

    // handle input key events.
    for (auto i = 0; i < inputBuffer->keyEventsCount; i++) {
        auto &keyEvent = inputBuffer->keyEvents[i];
        switch (keyEvent.action) {
            case AKEY_EVENT_ACTION_DOWN:
                if (keyEvent.keyCode == AKEYCODE_BACK)
                    GameActivity_finish(app->activity);
                break;
            default:
                aout << "Unknown KeyEvent Action: " << keyEvent.action << std::endl;
                break;
        }
    }

    // clear the motion input count in this buffer for main thread to re-use.
    android_app_clear_motion_events(inputBuffer);
    android_app_clear_key_events(inputBuffer);
}

static inline void resize_if_needed() {
    EGLint width;
    EGLint height;
    eglQuerySurface(display_, surface_, EGL_WIDTH, &width);
    eglQuerySurface(display_, surface_, EGL_HEIGHT, &height);

    if (width != width_ || height != height_) {
        width_ = width;
        height_ = height;
        app_set_size(width_, height_);
        aout << "Viewport resize: w=" << width_ << " h=" << height_ << std::endl;
    }
}

/*!
 * This the main entry point for a native activity
 */
void android_main(struct android_app *pApp) {
    display_ = EGL_NO_DISPLAY;
    surface_ = EGL_NO_SURFACE;
    context_ = EGL_NO_CONTEXT;
    width_ = 0;
    height_ = 0;
    gRefreshRate = 60.0f;

    memset(&config, 0, sizeof(config));

    // Register an event handler for Android events
    pApp->onAppCmd = handle_cmd;

    // Set input event filters (set it to NULL if the app wants to process all inputs).
    // Note that for key inputs, this example uses the default default_key_filter()
    // implemented in android_native_app_glue.c.
    android_app_set_motion_event_filter(pApp, motion_event_filter_func);

    auto nextEmuTime = std::chrono::steady_clock::now();
    auto nextDrawTime = nextEmuTime;

    double emulatorFrameTime = 1000.0 / 60.0;      // emulator fixed
    double displayFrameTime = 1000.0 / gRefreshRate; // device actual

    // This sets up a typical game/event loop. It will run until the app is destroyed.
    do {
        // Process all pending events before running game logic.
        bool done = false;
        while (!done) {
            // 0 is non-blocking.
            int timeout = 0;
            int events;
            android_poll_source *pSource;
            int result = ALooper_pollOnce(timeout, nullptr, &events,
                                          reinterpret_cast<void**>(&pSource));
            switch (result) {
                case ALOOPER_POLL_TIMEOUT:
                    [[clang::fallthrough]];
                case ALOOPER_POLL_WAKE:
                    // No events occurred before the timeout or explicit wake. Stop checking for events.
                    done = true;
                    break;
                case ALOOPER_EVENT_ERROR:
                    aout << "ALooper_pollOnce returned an error" << std::endl;
                    break;
                case ALOOPER_POLL_CALLBACK:
                    break;
                default:
                    if (pSource) {
                        pSource->process(pApp, pSource);
                    }
            }
        }

        // Check if any user data is associated. This is assigned in handle_cmd
        if (pApp->userData) {
            resize_if_needed();
            handle_input(pApp);

            auto now = std::chrono::steady_clock::now();

            // Run emulator frames at ~60Hz
            if (now >= nextEmuTime) {
                app_run_frame();

                nextEmuTime = now + std::chrono::milliseconds((int)emulatorFrameTime);
            }

            // Draw at display rate (e.g., 120Hz)
            if (now >= nextDrawTime) {
                app_render();
                eglSwapBuffers(display_, surface_);
                nextDrawTime = now + std::chrono::milliseconds((int)displayFrameTime);
            }

            // TODO this is polling: not good --> sleep exact time needed
            //      try using blocking swap buffers to delay until next vsync, then
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    } while (!pApp->destroyRequested);
}
}
