#include "../../core/core.h"

#include <adwaita.h>
#include <libmanette.h>
#include <linux/input-event-codes.h>
#include <netdb.h>
#include <string.h>

#include "camera.h"
#include "../common/app.h"
#include "../common/utils.h"

#ifndef VERSION
#   define VERSION ""
#endif

#define APP_NAME EMULATOR_NAME
#define APP_ICON "gbmulator"
#define APP_VERSION STRINGIFY(VERSION)
#define APP_COPYRIGHT_YEAR "2025"

// #define XPM_WHITE " "
// #define XPM_LIGHT_GRAY "."
// #define XPM_DARK_GRAY "+"
// #define XPM_BLACK "-"

static bool keycode_filter(unsigned int keyval);
static bool load_cartridge(char *path);
static gboolean loop_func(gpointer user_data);

// config struct initialized to defaults
static const config_t default_config = {
    .mode = GBMULATOR_MODE_GBA,
    .color_palette = PPU_COLOR_PALETTE_ORIG,
    .sound = 1.0f,
    .sound_drc = 1,
    .speed = 1.0f,
    .joypad_opacity = 1.0f,
    .enable_joypad = 0,
    .link_host = "127.0.0.1",
    .link_port = "7777",

    .gamepad_bindings = {
        BTN_A,
        BTN_B,
        BTN_SELECT,
        BTN_START,
        BTN_DPAD_RIGHT,
        BTN_DPAD_LEFT,
        BTN_DPAD_UP,
        BTN_DPAD_DOWN,
        BTN_TR,
        BTN_TL,
    },

    .keybindings = {
        [GBMULATOR_JOYPAD_A] = GDK_KEY_KP_0,
        [GBMULATOR_JOYPAD_B] = GDK_KEY_period,
        [GBMULATOR_JOYPAD_SELECT] = GDK_KEY_KP_2,
        [GBMULATOR_JOYPAD_START] = GDK_KEY_KP_1,
        [GBMULATOR_JOYPAD_RIGHT] = GDK_KEY_Right,
        [GBMULATOR_JOYPAD_LEFT] = GDK_KEY_Left,
        [GBMULATOR_JOYPAD_UP] = GDK_KEY_Up,
        [GBMULATOR_JOYPAD_DOWN] = GDK_KEY_Down,
        [GBMULATOR_JOYPAD_R] = GDK_KEY_KP_5,
        [GBMULATOR_JOYPAD_L] = GDK_KEY_KP_4,
    },
    .keycode_filter = keycode_filter
};

enum {
    GAMEPAD_DISABLED,
    GAMEPAD_PLAYING,
    GAMEPAD_BINDING
};

typedef struct {
    const char *name;
    const char *label_name;
    GtkWidget *widget;
} setter_handler_t;

static setter_handler_t key_handlers[] = {
    { "key_setter_a",      "key_setter_a_label",      NULL },
    { "key_setter_b",      "key_setter_b_label",      NULL },
    { "key_setter_select", "key_setter_select_label", NULL },
    { "key_setter_start",  "key_setter_start_label",  NULL },
    { "key_setter_right",  "key_setter_right_label",  NULL },
    { "key_setter_left",   "key_setter_left_label",   NULL },
    { "key_setter_up",     "key_setter_up_label",     NULL },
    { "key_setter_down",   "key_setter_down_label",   NULL },
    { "key_setter_r",      "key_setter_r_label",      NULL },
    { "key_setter_l",      "key_setter_l_label",      NULL }
};

static setter_handler_t gamepad_handlers[] = {
    { "gamepad_setter_a",      "gamepad_setter_a_label",      NULL },
    { "gamepad_setter_b",      "gamepad_setter_b_label",      NULL },
    { "gamepad_setter_select", "gamepad_setter_select_label", NULL },
    { "gamepad_setter_start",  "gamepad_setter_start_label",  NULL },
    { "gamepad_setter_right",  "gamepad_setter_right_label",  NULL },
    { "gamepad_setter_left",   "gamepad_setter_left_label",   NULL },
    { "gamepad_setter_up",     "gamepad_setter_up_label",     NULL },
    { "gamepad_setter_down",   "gamepad_setter_down_label",   NULL },
    { "gamepad_setter_r",      "gamepad_setter_r_label",      NULL },
    { "gamepad_setter_l",      "gamepad_setter_l_label",      NULL }
};

static const char *joypad_names[] = {
    "Right:",
    "Left:",
    "Up:",
    "Down:",
    "A:",
    "B:",
    "Select:",
    "Start:",
    "R:",
    "L:"
};

static gint argc;
static gchar **argv = NULL;

static gbmulator_joypad_t current_bind_setter;
static int binding_setter_handler = -1;
static int gamepad_state = GAMEPAD_DISABLED;

static AdwApplication *app;
static AdwToast *toast = NULL;
static GtkWidget *main_window, *window_title, *toast_overlay, *emu_gl_area, *printer_gl_area, *keybind_dialog;
static GtkWidget *joypad_name, *link_emu_dialog, *printer_window, *status, *bind_value, *mode_setter, *printer_save_btn;
static GtkWidget *printer_clear_btn, *speed_slider_container, *open_btn, *link_spinner_revealer, *link_spinner;
static AdwDialog *restart_dialog, *printer_dialog, *preferences_dialog;
static GtkAdjustment *printer_scroll_adj;
static GtkFileDialog *open_rom_dialog, *save_printer_image_dialog;
static guint loop_source = 0;

static gsize printer_gl_area_height = GB_SCREEN_HEIGHT;
static gboolean printer_window_allowed_to_close = FALSE;
static gboolean printer_save_dialog_resume_loop = FALSE;
static gboolean link_is_server = TRUE;
static double accel_x, accel_y;
static gbmulator_t *printer = NULL;

static GCancellable *link_task_cancellable;
static GTask *link_task;
static int sfd = -1;
static bool is_rewinding = false;
static bool is_mouse_pressed = false;

static void show_link_emu_dialog(GSimpleAction *action, GVariant *parameter, gpointer app);
static void show_printer_window(GSimpleAction *action, GVariant *parameter, gpointer app);
static void ask_restart_emulator(GSimpleAction *action, GVariant *parameter, gpointer app);
static void toggle_pause(GSimpleAction *action, GVariant *parameter, gpointer app);

static bool keycode_filter(unsigned int keyval) {
    switch (keyval) {
    case GDK_KEY_F1: case GDK_KEY_F2:
    case GDK_KEY_F3: case GDK_KEY_F4:
    case GDK_KEY_F5: case GDK_KEY_F6:
    case GDK_KEY_F7: case GDK_KEY_F8:
        return false;
    default:
        return true;
    }
}

static const char *gamepad_gamepad_button_parser(guint16 button) {
    switch (button) {
    case BTN_A:
        return "BTN_A";
    case BTN_B:
        return "BTN_B";
    case BTN_C:
        return "BTN_C";
    case BTN_X:
        return "BTN_X";
    case BTN_Y:
        return "BTN_Y";
    case BTN_Z:
        return "BTN_Z";
    case BTN_TL:
        return "BTN_TL";
    case BTN_TR:
        return "BTN_TR";
    case BTN_TL2:
        return "BTN_TL2";
    case BTN_TR2:
        return "BTN_TR2";
    case BTN_SELECT:
        return "BTN_SELECT";
    case BTN_START:
        return "BTN_START";
    case BTN_MODE:
        return "BTN_MODE";
    case BTN_THUMBL:
        return "BTN_THUMBL";
    case BTN_THUMBR:
        return "BTN_THUMBR";
    case BTN_DPAD_UP:
        return "BTN_DPAD_UP";
    case BTN_DPAD_DOWN:
        return "BTN_DPAD_DOWN";
    case BTN_DPAD_LEFT:
        return "BTN_DPAD_LEFT";
    case BTN_DPAD_RIGHT:
        return "BTN_DPAD_RIGHT";
    default:
        return NULL;
    }
}

static int gamepad_button_name_parser(const char *button_name) {
    size_t max_len = sizeof(XSTRINGIFY(BTN_DPAD_RIGHT));
    if (!strncmp(button_name, XSTRINGIFY(BTN_A), max_len))
        return BTN_A;
    if (!strncmp(button_name, XSTRINGIFY(BTN_B), max_len))
        return BTN_B;
    if (!strncmp(button_name, XSTRINGIFY(BTN_C), max_len))
        return BTN_C;
    if (!strncmp(button_name, XSTRINGIFY(BTN_X), max_len))
        return BTN_X;
    if (!strncmp(button_name, XSTRINGIFY(BTN_Y), max_len))
        return BTN_Y;
    if (!strncmp(button_name, XSTRINGIFY(BTN_Z), max_len))
        return BTN_Z;
    if (!strncmp(button_name, XSTRINGIFY(BTN_TL), max_len))
        return BTN_TL;
    if (!strncmp(button_name, XSTRINGIFY(BTN_TR), max_len))
        return BTN_TR;
    if (!strncmp(button_name, XSTRINGIFY(BTN_TL2), max_len))
        return BTN_TL2;
    if (!strncmp(button_name, XSTRINGIFY(BTN_TR2), max_len))
        return BTN_TR2;
    if (!strncmp(button_name, XSTRINGIFY(BTN_SELECT), max_len))
        return BTN_SELECT;
    if (!strncmp(button_name, XSTRINGIFY(BTN_START), max_len))
        return BTN_START;
    if (!strncmp(button_name, XSTRINGIFY(BTN_MODE), max_len))
        return BTN_MODE;
    if (!strncmp(button_name, XSTRINGIFY(BTN_THUMBL), max_len))
        return BTN_THUMBL;
    if (!strncmp(button_name, XSTRINGIFY(BTN_THUMBR), max_len))
        return BTN_THUMBR;
    if (!strncmp(button_name, XSTRINGIFY(BTN_DPAD_UP), max_len))
        return BTN_DPAD_UP;
    if (!strncmp(button_name, XSTRINGIFY(BTN_DPAD_DOWN), max_len))
        return BTN_DPAD_DOWN;
    if (!strncmp(button_name, XSTRINGIFY(BTN_DPAD_LEFT), max_len))
        return BTN_DPAD_LEFT;
    if (!strncmp(button_name, XSTRINGIFY(BTN_DPAD_RIGHT), max_len))
        return BTN_DPAD_RIGHT;
    return 0;
}

void start_loop(void) {
    if (loop_source > 0 || link_task)
        return;
    app_set_pause(false);
    loop_source = g_timeout_add(1000 / 60, G_SOURCE_FUNC(loop_func), NULL);
}

void stop_loop(void) {
    // TODO why link_task cond?
    if (loop_source == 0 || link_task)
        return;
    app_set_pause(true);
    g_source_remove(loop_source);
    loop_source = 0;
}

static void toggle_loop(void) {
    if (app_is_paused())
        start_loop();
    else
        stop_loop();
}

static void disconnect_emu(GSimpleAction *action, GVariant *parameter, gpointer app) {
    if (link_task)
        g_cancellable_cancel(link_task_cancellable);
    else
        close(sfd);
}

static void set_link_gui_actions(gboolean enabled, gboolean link_is_gb) {
    gtk_widget_set_sensitive(open_btn, enabled);
    if (link_is_gb)
        gtk_widget_set_sensitive(speed_slider_container, enabled);

    if (enabled) {
        const GActionEntry app_entries[] = {
            { "connect_emulator", show_link_emu_dialog, NULL, NULL, NULL },
            { "connect_printer", show_printer_window, NULL, NULL, NULL },
            { "restart", ask_restart_emulator, NULL, NULL, NULL },
        };
        g_action_map_add_action_entries(G_ACTION_MAP(app), app_entries, G_N_ELEMENTS(app_entries), app);
        if (link_is_gb) {
            const GActionEntry other_app_entries[] = {{ "play_pause", toggle_pause, NULL, NULL, NULL }};
            g_action_map_add_action_entries(G_ACTION_MAP(app), other_app_entries, G_N_ELEMENTS(other_app_entries), app);
            g_action_map_remove_action(G_ACTION_MAP(app), "disconnect_emulator");
        }
    } else {
        g_action_map_remove_action(G_ACTION_MAP(app), "connect_emulator");
        g_action_map_remove_action(G_ACTION_MAP(app), "connect_printer");
        g_action_map_remove_action(G_ACTION_MAP(app), "restart");
        if (link_is_gb) {
            const GActionEntry app_entries[] = {{ "disconnect_emulator", disconnect_emu, NULL, NULL, NULL }};
            g_action_map_add_action_entries(G_ACTION_MAP(app), app_entries, G_N_ELEMENTS(app_entries), app);
            g_action_map_remove_action(G_ACTION_MAP(app), "play_pause");
        }
    }
}

static void dismissed_cb(GtkWidget *self) {
    toast = NULL;
}

static void show_toast(const char *text) {
    if (!toast) {
        toast = adw_toast_new(text);
        g_signal_connect(toast, "dismissed", G_CALLBACK(dismissed_cb), NULL);
        adw_toast_overlay_add_toast(ADW_TOAST_OVERLAY(toast_overlay), toast);
    } else {
        adw_toast_set_title(toast, text);
    }

    adw_toast_set_timeout(toast, 1);
}

static inline gboolean loop_func(gpointer user_data) {
    app_run_frame();

    gtk_gl_area_queue_render(GTK_GL_AREA(emu_gl_area));

    return G_SOURCE_CONTINUE;
}

static void on_emu_realize(GtkGLArea *area, gpointer user_data) {
    gtk_gl_area_make_current(area);
    if (gtk_gl_area_get_error(area) != NULL) {
        eprintf("Unknown error\n");
        return;
    }

    app_init();

    if (argc > 1) {
        if (!load_cartridge(argv[1])) {
            gtk_widget_set_visible(status, TRUE);
            gtk_widget_set_visible(emu_gl_area, FALSE);
            gtk_widget_grab_focus(emu_gl_area);
            show_toast("Invalid ROM");
        }

        g_strfreev(argv);
    }
}

static void on_emu_unrealize(GtkGLArea *area, gpointer user_data) {
    stop_loop(); // call stop_loop() on unrealize to avoid annoying warnings when app is quitting
}

static gboolean on_emu_render(GtkGLArea *area, GdkGLContext *context, gpointer user_data) {
    app_render();
    return TRUE;
}

void on_emu_resize(GtkGLArea* area, gint width, gint height, gpointer user_data) {
    app_set_size(width, height);
}

static void on_printer_realize(GtkGLArea *area, gpointer user_data) {
    // gtk_gl_area_make_current(area);
    // if (gtk_gl_area_get_error(area) != NULL) {
    //     eprintf("Unknown error\n");
    //     return;
    // }

    // int viewport_w = gtk_widget_get_width(GTK_WIDGET(area));
    // int viewport_h = gtk_widget_get_height(GTK_WIDGET(area));

    // printer_renderer = glrenderer_init(GBPRINTER_IMG_WIDTH, printer_gl_area_height, viewport_w, viewport_h, false);
}

static gboolean on_printer_render(GtkGLArea *area, GdkGLContext *context, gpointer user_data) {
    // glrenderer_render(printer_renderer);
    return TRUE;
}

static gboolean on_printer_resize(GtkGLArea *area, GdkGLContext *context) {
    // double adj_upper = gtk_adjustment_get_upper(GTK_ADJUSTMENT(printer_scroll_adj));
    // gtk_adjustment_set_value(GTK_ADJUSTMENT(printer_scroll_adj), adj_upper);
    return TRUE;
}

static void printer_new_line_cb(const uint8_t *pixels, size_t current_height, size_t total_height) {
    // if (!printer_renderer)
    //     return; // segfault if printer gl area was not realized (printer window not shown at least once before printing)

    // int height_offset = printer_gl_area_height - current_height;
    // if (height_offset < 0) {
    //     printer_gl_area_height++;
    //     height_offset = printer_gl_area_height - current_height;

    //     glrenderer_resize_screen(printer_renderer, GB_SCREEN_WIDTH, printer_gl_area_height);
    //     gtk_widget_set_size_request(GTK_WIDGET(printer_gl_area), GB_SCREEN_WIDTH * 2, printer_gl_area_height * 2);
    // }

    // glrenderer_update_screen(printer_renderer, pixels);
    // gtk_gl_area_queue_render(GTK_GL_AREA(printer_gl_area));

    // if (current_height == 0 || current_height >= total_height) {
    //     gtk_widget_set_sensitive(GTK_WIDGET(printer_save_btn), current_height >= total_height);
    //     gtk_widget_set_sensitive(GTK_WIDGET(printer_clear_btn), current_height >= total_height);
    // }
}

static void set_accelerometer_data(double *x, double *y) {
    *x = accel_x;
    *y = accel_y;
}

static void toggle_pause(GSimpleAction *action, GVariant *parameter, gpointer app) {
    if (!link_task) { // TODO why link_task cond?
        show_toast(app_is_paused() ? "Resumed" : "Paused");
        toggle_loop();
    }
}

static void restart_emulator(AdwAlertDialog *self, gchar *response, gpointer user_data) {
    if (!strncmp(response, "restart", 8)) {
        app_reset();
        start_loop();
    }
}

static void ask_restart_emulator(GSimpleAction *action, GVariant *parameter, gpointer app) {
    if (!link_task) { // TODO why link_task cond?
        gamepad_state = GAMEPAD_DISABLED;
        adw_dialog_present(restart_dialog, main_window);
    }
}

void start_link_thread_cb(GObject *source_object, GAsyncResult *res, gpointer data) {
    // gtk_revealer_set_reveal_child(GTK_REVEALER(link_spinner_revealer), FALSE);

    // if (g_task_propagate_boolean(G_TASK(res), NULL)) {
    //     show_toast("Link Cable connected");
    // } else if (g_cancellable_is_cancelled(link_task_cancellable)) {
    //     link_cancel();
    //     close(sfd);
    //     sfd = -1;

    //     set_link_gui_actions(TRUE, TRUE);
    //     show_toast("Connection cancelled");
    // } else {
    //     set_link_gui_actions(TRUE, TRUE);
    //     show_toast("Connection error");
    // }

    // g_object_unref(link_task_cancellable);
    // g_object_unref(link_task);
    // link_task = NULL;
    // link_task_cancellable = NULL;
    // start_loop();
}

void start_link_thread(GTask *task, gpointer source_object, gpointer task_data, GCancellable *cancellable) {
    // if (link_is_server)
    //     sfd = link_start_server(config.link_port);
    // else
    //     sfd = link_connect_to_server(config.link_host, config.link_port);

    // gbmulator_t *new_linked_emu;
    // if (sfd > 0 && link_init_transfer(sfd, emu, &new_linked_emu)) {
    //     linked_emu = new_linked_emu;
    //     steps_per_frame = GB_CPU_STEPS_PER_FRAME;
    //     gbmulator_options_t opts;
    //     gbmulator_get_options(emu, &opts);
    //     opts.apu_speed = 1.0f;
    //     gbmulator_set_options(emu, &opts);
    //     g_task_return_boolean(link_task, TRUE);
    // } else {
    //     g_task_return_boolean(link_task, FALSE);
    //     sfd = -1; // closed by link_init_transfer in case of error
    // }
}

static void link_dialog_response(GtkDialog *self, gint response_id, gpointer user_data) {
    // gtk_window_close(GTK_WINDOW(self));
    // if (response_id != GTK_RESPONSE_OK || !emu)
    //     return;

    // stop_loop();

    // show_toast("Connecting Link Cable...");

    // set_link_gui_actions(FALSE, TRUE);
    // gtk_widget_set_visible(link_spinner_revealer, TRUE);
    // gtk_revealer_set_reveal_child(GTK_REVEALER(link_spinner_revealer), TRUE);
    // gtk_spinner_set_spinning(GTK_SPINNER(link_spinner), TRUE);

    // link_task_cancellable = g_cancellable_new();
    // link_task = g_task_new(NULL, link_task_cancellable, start_link_thread_cb, NULL);
    // g_task_set_return_on_cancel(link_task, TRUE);
    // g_task_run_in_thread(link_task, start_link_thread);
}

static void show_link_emu_dialog(GSimpleAction *action, GVariant *parameter, gpointer app) {
    if (!app_get_rom_title())
        return;

    gamepad_state = GAMEPAD_DISABLED;
    gtk_window_present(GTK_WINDOW(link_emu_dialog));
}

static void show_printer_window(GSimpleAction *action, GVariant *parameter, gpointer app) {
    if (!app_get_rom_title())
        return;

    // gbmulator_options_t opts = {
    //     .mode = GBMULATOR_MODE_GBPRINTER,
    //     .on_new_line = printer_new_line_cb
    // };
    // printer = gbmulator_init(&opts);

    // gbmulator_link_connect(emu, printer, GBMULATOR_LINK_CABLE);

    // set_link_gui_actions(FALSE, FALSE);
    // gamepad_state = GAMEPAD_DISABLED;
    // gtk_window_present(GTK_WINDOW(printer_window));
}

void on_mouse_pressed(GtkGestureClick* self, gint n_press, gdouble x, gdouble y, gpointer user_data) {
    is_mouse_pressed = true;
    app_touch_press(0, x, y);
}

void on_mouse_released(GtkGestureClick* self, gint n_press, gdouble x, gdouble y, gpointer user_data) {
    is_mouse_pressed = false;
    app_touch_release(0, x, y);
}

static gboolean on_mouse_motion(GtkEventControllerMotion *self, gdouble x, gdouble y, gpointer user_data) {
    // x = CLAMP(x / 2, 0, GB_SCREEN_WIDTH);
    // y = CLAMP(y / 2, 0, GB_SCREEN_HEIGHT);
    // accel_x = (x - 80) / -80.0;
    // accel_y = (y - 72) / -72.0;
    // printf("(%lf, %lf) accel_x=%lf accel_y=%lf\n", x, y, accel_x, accel_y);

    if (is_mouse_pressed)
        app_touch_move(0, x, y);

    return TRUE;
}

static bool load_cartridge(char *path) {
    if (!path)
        return false;

    size_t rom_size = 0;
    uint8_t *rom = NULL;

    rom = read_rom(path, &rom_size);
    if (!rom)
        return false;

    if (!app_load_cartridge(rom, rom_size))
        return false;

    const GActionEntry app_entries[] = {
        { "play_pause",       toggle_pause,         NULL, NULL, NULL },
        { "restart",          ask_restart_emulator, NULL, NULL, NULL },
        { "connect_emulator", show_link_emu_dialog, NULL, NULL, NULL },
        { "connect_printer",  show_printer_window,  NULL, NULL, NULL },
    };
    g_action_map_add_action_entries(G_ACTION_MAP(app), app_entries, G_N_ELEMENTS(app_entries), app);

    adw_window_title_set_subtitle(ADW_WINDOW_TITLE(window_title), app_get_rom_title());

    // if (gbmulator_has_peripheral(emu, GBMULATOR_PERIPHERAL_CAMERA))
    //     camera_play();
    // else
    //     camera_pause();

    gamepad_state = GAMEPAD_PLAYING;

    start_loop();

    return true;
}

static void set_speed(GtkRange *self, gpointer user_data) {
    app_set_speed(gtk_range_get_value(self));
}

static void set_sound(GtkRange *self, gpointer user_data) {
    app_set_sound(gtk_range_get_value(self));
}

static void set_joypad_opacity(GtkRange *self, gpointer user_data) {
    app_set_joypad_opacity(gtk_range_get_value(self));
}

static void set_enable_joypad(AdwExpanderRow *self, GParamSpec *pspec, gpointer user_data) {
    app_set_touchscreen_mode(adw_expander_row_get_enable_expansion(self));
}

static void set_sound_drc(AdwSwitchRow *self, gpointer user_data) {
    app_set_drc(adw_switch_row_get_active(self));
}

static void set_palette(AdwComboRow *self, GParamSpec *pspec, gpointer user_data) {
    app_set_palette(adw_combo_row_get_selected(self));
}

static void set_keybinding(GtkDialog *self, gint response_id, gpointer user_data) {
    gtk_window_close(GTK_WINDOW(self));

    if (response_id == GTK_RESPONSE_APPLY) {
        const char *keyname = gtk_label_get_label(GTK_LABEL(bind_value));
        if (!strncmp(keyname, "Press a key", 12)) return;
        unsigned int keyval = gdk_keyval_from_name(keyname);

        gbmulator_joypad_t swapped_button;
        unsigned int swapped_keyval;
        if (app_set_keybind(current_bind_setter, keyval, &swapped_button, &swapped_keyval)) {
            if (swapped_button != current_bind_setter)
                gtk_label_set_label(GTK_LABEL(key_handlers[swapped_button].widget), gdk_keyval_name(swapped_keyval));
            gtk_label_set_label(GTK_LABEL(key_handlers[current_bind_setter].widget), gdk_keyval_name(keyval));
        }
    }
}

static void set_gamepad_binding(GtkDialog *self, gint response_id, gpointer user_data) {
    // gtk_window_close(GTK_WINDOW(self));
    // if (response_id == GTK_RESPONSE_APPLY) {
    //     const char *button_name = gtk_label_get_label(GTK_LABEL(bind_value));
    //     if (!strncmp(button_name, "Press a key", 12)) return;
    //     unsigned int button = gamepad_button_name_parser(button_name);

    //     // detect if the key is already attributed, if yes, swap them
    //     for (int i = 0; i < 10; i++) {
    //         if (config.gamepad_bindings[i] == button && current_bind_setter != i) {
    //             config.gamepad_bindings[i] = config.gamepad_bindings[current_bind_setter];
    //             gtk_label_set_label(GTK_LABEL(gamepad_handlers[i].widget), gamepad_gamepad_button_parser(config.gamepad_bindings[i]));
    //             break;
    //         }
    //     }

    //     config.gamepad_bindings[current_bind_setter] = button;
    //     gtk_label_set_label(GTK_LABEL(gamepad_handlers[current_bind_setter].widget), gamepad_gamepad_button_parser(config.gamepad_bindings[current_bind_setter]));
    // }
}

static void host_insert_text_handler(GtkEditable *self, const char *text, int length, int *position, gpointer data) {
    g_signal_handlers_block_by_func(self, (gpointer) host_insert_text_handler, data);
    if (strlen(gtk_editable_get_text(self)) < INET6_ADDRSTRLEN)
        gtk_editable_insert_text(self, text, strlen(text), position);
    else
        gtk_editable_insert_text(self, "", 0, position);
    g_signal_handlers_unblock_by_func(self, (gpointer) host_insert_text_handler, data);
    g_signal_stop_emission_by_name(self, "insert_text");
}

static void set_link_host(GtkEditable *self, gpointer user_data) {
    // strncpy(config.link_host, gtk_editable_get_text(self), INET6_ADDRSTRLEN);
    // config.link_host[INET6_ADDRSTRLEN - 1] = '\0';
}

static void set_link_port(GtkSpinButton *self, gpointer user_data) {
    // snprintf(config.link_port, sizeof(config.link_port), "%d", (int) gtk_spin_button_get_value(self));
    // config.link_port[5] = '\0';
}

static void link_mode_setter_server_toggled(GtkToggleButton *self, gpointer user_data) {
    if (gtk_toggle_button_get_active(self)) {
        gtk_revealer_set_reveal_child(GTK_REVEALER(user_data), FALSE);
        link_is_server = TRUE;
    } else {
        gtk_revealer_set_reveal_child(GTK_REVEALER(user_data), TRUE);
        link_is_server = FALSE;
    }
}

static void set_mode(AdwComboRow *self, GParamSpec *pspec, gpointer user_data) {
    app_set_mode(adw_combo_row_get_selected(self));
}

static void set_camera(AdwComboRow *self, GParamSpec *pspec, gpointer user_data) {
    // guint selected_camera = adw_combo_row_get_selected(self);
    // gsize len;
    // gchar ***paths = camera_get_devices_paths(&len);
    // if (selected_camera < len) {
    //     camera_quit();
    //     camera_init((*paths)[selected_camera]);
    //     if (gbmulator_has_peripheral(emu, GBMULATOR_PERIPHERAL_CAMERA))
    //         camera_play();
    // }
}

static void key_setter_activated(AdwActionRow *self, gpointer user_data) {
    current_bind_setter = GPOINTER_TO_INT(user_data);
    gtk_label_set_label(GTK_LABEL(joypad_name), joypad_names[current_bind_setter]);
    gtk_label_set_label(GTK_LABEL(bind_value), "Press a key");
    if (binding_setter_handler > 0)
        g_signal_handler_disconnect(keybind_dialog, binding_setter_handler);
    binding_setter_handler = g_signal_connect(keybind_dialog, "response", G_CALLBACK(set_keybinding), NULL);
    gtk_window_present(GTK_WINDOW(keybind_dialog));
}

static void gamepad_setter_activated(AdwActionRow *self, gpointer user_data) {
    current_bind_setter = GPOINTER_TO_INT(user_data);
    gtk_label_set_label(GTK_LABEL(joypad_name), joypad_names[current_bind_setter]);
    gtk_label_set_label(GTK_LABEL(bind_value), "Press a button");
    if (binding_setter_handler > 0)
        g_signal_handler_disconnect(keybind_dialog, binding_setter_handler);
    binding_setter_handler = g_signal_connect(keybind_dialog, "response", G_CALLBACK(set_gamepad_binding), NULL);
    gamepad_state = GAMEPAD_BINDING;
    gtk_window_present(GTK_WINDOW(keybind_dialog));
}

static void show_preferences(GSimpleAction *action, GVariant *parameter, gpointer app) {
    gamepad_state = GAMEPAD_DISABLED;
    adw_dialog_present(preferences_dialog, main_window);
}

static void show_about(GSimpleAction *action, GVariant *parameter, gpointer app) {
    const char *developers[] = {
        "Maxime Postaire https://github.com/mpostaire",
        NULL
    };

    adw_show_about_dialog(GTK_WIDGET(gtk_application_get_active_window(GTK_APPLICATION(app))),
                          "application-name", APP_NAME,
                          "application-icon", APP_ICON,
                          "version", APP_VERSION,
                          "copyright", "© " APP_COPYRIGHT_YEAR " Maxime Postaire",
                          "issue-url", "https://github.com/mpostaire/gbmulator/issues/new",
                          "license-type", GTK_LICENSE_MIT_X11,
                          "developers", developers,
                          "website", "https://github.com/mpostaire/gbmulator",
                          "comments", "A Game Boy Color emulator with sound and Link Cable / IR sensor support over tcp.",
                          NULL);
}

static void open_rom_dialog_cb(GObject *dialog, GAsyncResult *res, gpointer user_data) {
    g_autoptr(GFile) file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(dialog), res, NULL);
    if (!file)
        return;

    char *file_path = g_file_get_path(file);
    if (load_cartridge(file_path)) {
        gtk_widget_set_visible(status, FALSE);
        gtk_widget_set_visible(emu_gl_area, TRUE);
        gtk_widget_grab_focus(emu_gl_area);
    } else {
        show_toast("Invalid ROM");
    }

    free(file_path);
}

static void open_btn_clicked(AdwActionRow *self, gpointer user_data) {
    if (!open_rom_dialog) {
        open_rom_dialog = gtk_file_dialog_new();
        gtk_file_dialog_set_title(open_rom_dialog, "Pick a ROM file");

        g_autoptr(GtkFileFilter) filter = gtk_file_filter_new();
        // add basic extension pattern in case the mime type is not available
        gtk_file_filter_add_pattern(filter, "*.emu");
        gtk_file_filter_add_pattern(filter, "*.gbc");
        gtk_file_filter_add_pattern(filter, "*.gba");
        gtk_file_filter_add_mime_type(filter, "application/x-gameboy-rom");
        gtk_file_filter_add_mime_type(filter, "application/x-gameboy-color-rom");
        gtk_file_filter_add_mime_type(filter, "application/x-gba-rom");
        gtk_file_filter_set_name(filter, "GameBoy / GameBoy Color / GameBoy Advance ROM");

        g_autoptr(GListStore) list = g_list_store_new(GTK_TYPE_FILE_FILTER);
        g_list_store_append(list, filter);

        gtk_file_dialog_set_filters(open_rom_dialog, G_LIST_MODEL(list));
    }

    gtk_file_dialog_open(open_rom_dialog, GTK_WINDOW(main_window), NULL, open_rom_dialog_cb, NULL);
}

// TODO save as bmp instead
static void printer_save_as_xpm(gbmulator_t *printer, const char *file_path) {
    // FILE *f = fopen(file_path, "w+");
    // if (!f) {
    //     errnoprintf("fopen()");
    //     return;
    // }

    // size_t height;
    // uint8_t *image_data = gbmulator_get_save(printer, &height);

    // fprintf(f, "/* XPM */\nstatic char *image = {\n");
    // fprintf(f, "\"%d %lu 4 1\",\n", GBPRINTER_IMG_WIDTH, height);
    // fprintf(f, "\""XPM_WHITE" c #FFFFFF\",\n\""XPM_LIGHT_GRAY" c #AAAAAA\",\n\""XPM_DARK_GRAY" c #555555\",\n\""XPM_BLACK" c #000000\",\n");

    // for (size_t i = 0; i < height; i++) {
    //     fprintf(f, "\"");
    //     for (int j = 0; j < GBPRINTER_IMG_WIDTH; j++) {
    //         int image_data_index = (i * GBPRINTER_IMG_WIDTH * 4) + (j * 4);
    //         char c = *XPM_BLACK;
    //         switch (image_data[image_data_index]) {
    //         case 0xFF: c = *XPM_WHITE; break;
    //         case 0xAA: c = *XPM_LIGHT_GRAY; break;
    //         case 0x55: c = *XPM_DARK_GRAY; break;
    //         case 0x00: c = *XPM_BLACK; break;
    //         default: eprintf("invalid color data\n"); break;
    //         }
    //         fprintf(f, "%c", c);
    //     }
    //     fprintf(f, "\"%s", i == height - 1 ? "};\n" : ",\n");
    // }

    // free(image_data);
    // fclose(f);
}

static void printer_save_dialog_cb(GObject *dialog, GAsyncResult *res, gpointer user_data) {
    if (printer_save_dialog_resume_loop) {
        start_loop();
        printer_save_dialog_resume_loop = FALSE;
    }

    g_autoptr(GFile) file = gtk_file_dialog_save_finish(GTK_FILE_DIALOG(dialog), res, NULL);
    if (!file)
        return;

    char *file_path = g_file_get_path(file);
    printer_save_as_xpm(printer, file_path);
    free(file_path);
}

static void printer_save_btn_clicked(AdwActionRow *self, gpointer user_data) {
    if (!save_printer_image_dialog) {
        save_printer_image_dialog = gtk_file_dialog_new();
        gtk_file_dialog_set_title(save_printer_image_dialog, "Pick a ROM file");
        g_autoptr(GFile) file = g_file_new_for_path("image.xpm");
        gtk_file_dialog_set_initial_file(save_printer_image_dialog, file);

        g_autoptr(GtkFileFilter) filter = gtk_file_filter_new();
        // add basic extension pattern in case the mime type is not available
        gtk_file_filter_add_pattern(filter, "*.xpm");
        gtk_file_filter_add_mime_type(filter, "image/x-xpixmap");
        gtk_file_filter_set_name(filter, "Image XPM");

        g_autoptr(GListStore) list = g_list_store_new(GTK_TYPE_FILE_FILTER);
        g_list_store_append(list, filter);

        gtk_file_dialog_set_filters(save_printer_image_dialog, G_LIST_MODEL(list));
    }

    if (!app_is_paused()) {
        stop_loop();
        printer_save_dialog_resume_loop = TRUE;
    }
    gtk_file_dialog_save(save_printer_image_dialog, GTK_WINDOW(printer_window), NULL, printer_save_dialog_cb, NULL);
}

static void clear_printer_gl_area(void) {
    // glrenderer_resize_screen(printer_renderer, GB_SCREEN_WIDTH, GB_SCREEN_HEIGHT);
    // gtk_widget_set_size_request(GTK_WIDGET(printer_gl_area), GB_SCREEN_WIDTH * 2, GB_SCREEN_HEIGHT * 2);
    // printer_gl_area_height = GB_SCREEN_HEIGHT;

    // void *pixels = xcalloc(1, GB_SCREEN_WIDTH * GB_SCREEN_HEIGHT * 4);
    // glrenderer_update_screen(printer_renderer, pixels);
    // free(pixels);

    // gtk_gl_area_queue_render(GTK_GL_AREA(printer_gl_area));
}

static void printer_clear_btn_clicked(AdwActionRow *self, gpointer user_data) {
    // todo();
    // // gbprinter_clear_image(printer);
    // if (!printer_renderer)
    //     return; // segfault if printer gl area was not realized (printer window not shown at least once before printing)

    // clear_printer_gl_area();

    // gtk_widget_set_sensitive(GTK_WIDGET(printer_save_btn), FALSE);
    // gtk_widget_set_sensitive(GTK_WIDGET(printer_clear_btn), FALSE);
}

static gboolean key_pressed_main(GtkEventControllerKey *self, guint keyval, guint keycode, GdkModifierType state, gpointer user_data) {
    if (!app_get_rom_title() || app_is_paused()) return FALSE;

    if (state & GDK_CONTROL_MASK) {
        switch (keyval) {
        case GDK_KEY_p:
            toggle_pause(NULL, NULL, NULL);
            break;
        case GDK_KEY_r:
            ask_restart_emulator(NULL, NULL, NULL);
            break;
        }
        return TRUE;
    }

    // if (keyval == GDK_KEY_r && emu && !linked_emu) {
    //     is_rewinding = true;
    //     return TRUE;
    // }

    is_rewinding = false;

    switch (keyval) {
    case GDK_KEY_F11:
        if (!app_get_rom_title())
            return FALSE;

        if (gtk_window_is_fullscreen(GTK_WINDOW(main_window)))
            gtk_window_unfullscreen(GTK_WINDOW(main_window));
        else
            gtk_window_fullscreen(GTK_WINDOW(main_window));
        return TRUE;
    case GDK_KEY_F1: case GDK_KEY_F2:
    case GDK_KEY_F3: case GDK_KEY_F4:
    case GDK_KEY_F5: case GDK_KEY_F6:
    case GDK_KEY_F7: case GDK_KEY_F8:
        if (state & GDK_SHIFT_MASK) {
            app_save_state(keyval - GDK_KEY_F1);
        } else {
            app_load_state(keyval - GDK_KEY_F1);
            adw_combo_row_set_selected(ADW_COMBO_ROW(mode_setter), app_get_mode());
        }
        return TRUE;
    }

    app_keyboard_press(keyval);
    return TRUE;
}

static gboolean key_released_main(GtkEventControllerKey *self, guint keyval, guint keycode, GdkModifierType state, gpointer user_data) {
    // if (keyval == GDK_KEY_r && emu && !linked_emu) {
    //     is_rewinding = false;
    //     return TRUE;
    // }

    app_keyboard_release(keyval);
    return TRUE;
}

static gboolean key_pressed_keybind(GtkEventControllerKey *self, guint keyval, guint keycode, GdkModifierType state, gpointer user_data) {
    if (keyval == GDK_KEY_Escape || gamepad_state == GAMEPAD_BINDING)
        return FALSE;
    if (keycode_filter(keyval))
        gtk_label_set_label(GTK_LABEL(bind_value), gdk_keyval_name(keyval));
    return TRUE;
}

static void popover_closed(GtkPopover *self, gpointer user_data) {
    gtk_widget_grab_focus(emu_gl_area);
}

static void link_spinner_revealer_done_cb(GtkRevealer *self, gpointer user_data) {
    if (!gtk_revealer_get_child_revealed(self)) {
        gtk_widget_set_visible(link_spinner_revealer, FALSE);
        gtk_spinner_set_spinning(GTK_SPINNER(link_spinner), FALSE);
    }
}

static gboolean on_drop(GtkDropTarget *target, const GValue *value, double x, double y, gpointer data) {
    // GdkFileList is a boxed value so we use the boxed API.
    GdkFileList *file_list = g_value_get_boxed(value);
    GSList *list = gdk_file_list_get_files(file_list);
    GFile *file = list->data;

    if (load_cartridge(g_file_get_path(file))) {
        gtk_widget_set_visible(status, FALSE);
        gtk_widget_set_visible(emu_gl_area, TRUE);
        gtk_widget_grab_focus(emu_gl_area);
    } else {
        show_toast("Invalid ROM");
    }

    return TRUE;
}

static gchar *slider_format_percent(GtkScale *self, gdouble value, gpointer user_data) {
    return g_strdup_printf("%d%%", (int) (value * 100));
}

static gchar *slider_format_scale(GtkScale *self, gdouble value, gpointer user_data) {
    return g_strdup_printf("x%0.1f", value);
}

static void gamepad_button_press_event_cb(ManetteDevice *emitter, ManetteEvent *event, gpointer user_data) {
    guint16 button;
    switch (gamepad_state) {
    case GAMEPAD_DISABLED:
        break;
    case GAMEPAD_PLAYING:;
        if (manette_event_get_button(event, &button))
            app_gamepad_press(button);
        break;
    case GAMEPAD_BINDING:
        if (manette_event_get_button(event, &button))
            gtk_label_set_label(GTK_LABEL(bind_value), gamepad_gamepad_button_parser(button));
        break;
    }
}

static void gamepad_button_release_event_cb(ManetteDevice *emitter, ManetteEvent *event, gpointer user_data) {
    guint16 button;
    switch (gamepad_state) {
    case GAMEPAD_DISABLED:
        break;
    case GAMEPAD_PLAYING:
        if (manette_event_get_button(event, &button))
            app_gamepad_release(button);
        break;
    case GAMEPAD_BINDING:
        break;
    }
}

static void gamepad_disconnected_cb(ManetteDevice *device, gpointer user_data) {
    printf("%s: disconnected\n", manette_device_get_name(device));
}

static void gamepad_connected_cb(ManetteMonitor *self, ManetteDevice *device, gpointer user_data) {
    printf("%s: connected\n", manette_device_get_name(device));

    g_signal_connect_object(G_OBJECT(device), "disconnected", (GCallback) gamepad_disconnected_cb, NULL, 0);
    g_signal_connect_object(G_OBJECT(device), "button-press-event", (GCallback) gamepad_button_press_event_cb, NULL, 0);
    g_signal_connect_object(G_OBJECT(device), "button-release-event", (GCallback) gamepad_button_release_event_cb, NULL, 0);
}

static void secondary_window_hide_cb(GtkWidget *self, gpointer user_data) {
    // gamepad_state = (!emu || app_is_paused()) ? GAMEPAD_DISABLED : GAMEPAD_PLAYING;
}

static gboolean printer_window_close_request_cb(GtkWindow *self, gpointer user_data) {
    if (printer_window_allowed_to_close) {
        printer_window_allowed_to_close = FALSE;
        return FALSE;
    }

    adw_dialog_present(printer_dialog, printer_window);
    return TRUE;
}

static void printer_quit_dialog_response_cb(AdwMessageDialog *self, gchar *response, gpointer user_data) {
    // if (!strncmp(response, "disconnect", 11)) {
    //     gbmulator_link_disconnect(emu, GBMULATOR_LINK_CABLE);
    //     gbmulator_quit(printer);
    //     printer = NULL;
    //     clear_printer_gl_area();

    //     gtk_widget_set_sensitive(GTK_WIDGET(printer_save_btn), FALSE);
    //     gtk_widget_set_sensitive(GTK_WIDGET(printer_clear_btn), FALSE);
    //     set_link_gui_actions(TRUE, FALSE);

    //     printer_window_allowed_to_close = TRUE;
    //     gtk_window_close(GTK_WINDOW(printer_window));
    // }
}

static void keybind_dialog_hide_cb(GtkWidget *self, gpointer user_data) {
    gamepad_state = GAMEPAD_DISABLED;
}

static void main_window_fullscreen_notify(GObject *self, GParamSpec *pspec, gpointer user_data) {
    gboolean is_fullscreen = gtk_window_is_fullscreen(GTK_WINDOW(self));
    adw_toolbar_view_set_extend_content_to_top_edge(ADW_TOOLBAR_VIEW(user_data), is_fullscreen);
    adw_toolbar_view_set_reveal_top_bars(ADW_TOOLBAR_VIEW(user_data), !is_fullscreen);
}

static void activate_cb(GtkApplication *app) {
    app_load_config(&default_config);

    const config_t config;
    app_get_config((config_t *) &config);

    // Main window
    GtkBuilder *builder = gtk_builder_new_from_resource("/io/github/mpostaire/gbmulator/src/platform/desktop/ui/main.ui");
    main_window = GTK_WIDGET(gtk_builder_get_object(builder, "window"));
    gtk_widget_set_size_request(main_window, GBA_SCREEN_WIDTH * 2, GBA_SCREEN_HEIGHT * 2);

    GtkWidget *main_window_view = GTK_WIDGET(gtk_builder_get_object(builder, "toolbarview"));
    g_signal_connect(main_window, "notify::fullscreened", G_CALLBACK(main_window_fullscreen_notify), main_window_view);

    status = GTK_WIDGET(gtk_builder_get_object(builder, "status"));

    emu_gl_area = GTK_WIDGET(gtk_builder_get_object(builder, "emu_gl_area"));
    g_signal_connect(emu_gl_area, "realize", G_CALLBACK(on_emu_realize), NULL);
    g_signal_connect(emu_gl_area, "unrealize", G_CALLBACK(on_emu_unrealize), NULL);
    g_signal_connect(emu_gl_area, "render", G_CALLBACK(on_emu_render), NULL);
    g_signal_connect(emu_gl_area, "resize", G_CALLBACK(on_emu_resize), NULL);

    open_btn = GTK_WIDGET(gtk_builder_get_object(builder, "open_btn"));
    g_signal_connect(open_btn, "clicked", G_CALLBACK(open_btn_clicked), NULL);

    toast_overlay = GTK_WIDGET(gtk_builder_get_object(builder, "overlay"));

    GtkWidget *menu_btn = GTK_WIDGET(gtk_builder_get_object(builder, "menu_btn"));
    GtkPopover *popover = gtk_menu_button_get_popover(GTK_MENU_BUTTON(menu_btn));
    g_signal_connect(popover, "closed", G_CALLBACK(popover_closed), NULL);

    window_title = GTK_WIDGET(gtk_builder_get_object(builder, "window_title"));

    link_spinner_revealer = GTK_WIDGET(gtk_builder_get_object(builder, "link_spinner_revealer"));
    link_spinner = GTK_WIDGET(gtk_builder_get_object(builder, "link_spinner"));
    g_signal_connect(link_spinner_revealer, "notify::child-revealed", G_CALLBACK(link_spinner_revealer_done_cb), NULL);

    g_object_unref(builder);

    const GActionEntry app_entries[] = {
        { "preferences", show_preferences, NULL, NULL, NULL },
        { "about", show_about, NULL, NULL, NULL },
    };
    g_action_map_add_action_entries(G_ACTION_MAP(app), app_entries, G_N_ELEMENTS(app_entries), app);

    gtk_window_set_application(GTK_WINDOW(main_window), GTK_APPLICATION(app));

    // Preferences window
    builder = gtk_builder_new_from_resource("/io/github/mpostaire/gbmulator/src/platform/desktop/ui/preferences.ui");
    preferences_dialog = ADW_DIALOG(gtk_builder_get_object(builder, "dialog"));
    g_object_ref(preferences_dialog);
    g_signal_connect(preferences_dialog, "hide", G_CALLBACK(secondary_window_hide_cb), NULL);

    GtkWidget *widget = GTK_WIDGET(gtk_builder_get_object(builder, "pref_sound"));
    GtkAdjustment *sound_adjustment = gtk_adjustment_new(config.sound, 0.0, 1.0, 0.05, 0.25, 0.0);
    gtk_scale_set_format_value_func(GTK_SCALE(widget), slider_format_percent, NULL, NULL);
    gtk_range_set_adjustment(GTK_RANGE(widget), sound_adjustment);
    g_signal_connect(widget, "value-changed", G_CALLBACK(set_sound), NULL);

    widget = GTK_WIDGET(gtk_builder_get_object(builder, "pref_sound_drc"));
    adw_switch_row_set_active(ADW_SWITCH_ROW(widget), config.sound_drc);
    g_signal_connect(widget, "notify::active", G_CALLBACK(set_sound_drc), NULL);

    speed_slider_container = GTK_WIDGET(gtk_builder_get_object(builder, "pref_speed_container"));
    widget = GTK_WIDGET(gtk_builder_get_object(builder, "pref_speed"));
    GtkAdjustment *speed_adjustment = gtk_adjustment_new(config.speed, 1.0, 8.0, 0.5, 1, 0.0);
    gtk_scale_set_format_value_func(GTK_SCALE(widget), slider_format_scale, NULL, NULL);
    gtk_range_set_adjustment(GTK_RANGE(widget), speed_adjustment);
    g_signal_connect(widget, "value-changed", G_CALLBACK(set_speed), NULL);
    g_signal_connect(widget, "notify::selected", G_CALLBACK(set_speed), NULL);

    widget = GTK_WIDGET(gtk_builder_get_object(builder, "pref_palette"));
    g_signal_connect(widget, "notify::selected", G_CALLBACK(set_palette), NULL);
    adw_combo_row_set_selected(ADW_COMBO_ROW(widget), config.color_palette);

    mode_setter = GTK_WIDGET(gtk_builder_get_object(builder, "pref_mode"));
    g_signal_connect(mode_setter, "notify::selected", G_CALLBACK(set_mode), NULL);
    adw_combo_row_set_selected(ADW_COMBO_ROW(mode_setter), config.mode);

    for (size_t i = 0; i < G_N_ELEMENTS(key_handlers); i++) {
        widget = GTK_WIDGET(gtk_builder_get_object(builder, key_handlers[i].name));
        g_signal_connect(widget, "activated", G_CALLBACK(key_setter_activated), (gpointer) i);
        key_handlers[i].widget = GTK_WIDGET(gtk_builder_get_object(builder, key_handlers[i].label_name));
        gtk_label_set_label(GTK_LABEL(key_handlers[i].widget), gdk_keyval_name(config.keybindings[i]));
    }

    for (size_t i = 0; i < G_N_ELEMENTS(gamepad_handlers); i++) {
        widget = GTK_WIDGET(gtk_builder_get_object(builder, gamepad_handlers[i].name));
        g_signal_connect(widget, "activated", G_CALLBACK(gamepad_setter_activated), (gpointer) i);
        gamepad_handlers[i].widget = GTK_WIDGET(gtk_builder_get_object(builder, gamepad_handlers[i].label_name));
        gtk_label_set_label(GTK_LABEL(gamepad_handlers[i].widget), gamepad_gamepad_button_parser(config.gamepad_bindings[i]));
    }

    if (camera_find_devices()) {
        AdwComboRow *pref_camera_device = ADW_COMBO_ROW(gtk_builder_get_object(builder, "pref_camera_device"));
        gsize len;

        adw_combo_row_set_model(pref_camera_device, G_LIST_MODEL(camera_get_devices_names(&len)));
        guint selected_camera = adw_combo_row_get_selected(pref_camera_device);
        gchar ***camera_paths = camera_get_devices_paths(&len);
        if (selected_camera < len) {
            g_signal_connect(pref_camera_device, "notify::selected", G_CALLBACK(set_camera), NULL);
            camera_init((*camera_paths)[selected_camera]);
        }
    }

    widget = GTK_WIDGET(gtk_builder_get_object(builder, "pref_enable_jyp"));
    g_signal_connect(widget, "notify::enable-expansion", G_CALLBACK(set_enable_joypad), NULL);
    adw_expander_row_set_enable_expansion(ADW_EXPANDER_ROW(widget), config.enable_joypad);
    adw_expander_row_set_expanded(ADW_EXPANDER_ROW(widget), config.enable_joypad);

    widget = GTK_WIDGET(gtk_builder_get_object(builder, "pref_jyp_opacity"));
    GtkAdjustment *jyp_opacity_adjustment = gtk_adjustment_new(config.joypad_opacity, 0.0, 1.0, 0.05, 0.25, 0.0);
    gtk_scale_set_format_value_func(GTK_SCALE(widget), slider_format_percent, NULL, NULL);
    gtk_range_set_adjustment(GTK_RANGE(widget), jyp_opacity_adjustment);
    g_signal_connect(widget, "value-changed", G_CALLBACK(set_joypad_opacity), NULL);
    g_signal_connect(widget, "notify::selected", G_CALLBACK(set_joypad_opacity), NULL);

    g_object_unref(builder);

    // Keybind dialog
    builder = gtk_builder_new_from_resource("/io/github/mpostaire/gbmulator/src/platform/desktop/ui/bind_setter.ui");
    keybind_dialog = GTK_WIDGET(gtk_builder_get_object(builder, "dialog"));
    gtk_window_set_transient_for(GTK_WINDOW(keybind_dialog), GTK_WINDOW(main_window));
    g_signal_connect(keybind_dialog, "hide", G_CALLBACK(keybind_dialog_hide_cb), NULL);
    bind_value = GTK_WIDGET(gtk_builder_get_object(builder, "bind_value"));
    joypad_name = GTK_WIDGET(gtk_builder_get_object(builder, "joypad_name"));

    g_object_unref(builder);

    // Restart dialog
    builder = gtk_builder_new_from_resource("/io/github/mpostaire/gbmulator/src/platform/desktop/ui/restart_dialog.ui");
    restart_dialog = ADW_DIALOG(gtk_builder_get_object(builder, "dialog"));
    g_object_ref(restart_dialog);
    g_signal_connect(restart_dialog, "hide", G_CALLBACK(secondary_window_hide_cb), NULL);
    g_signal_connect(restart_dialog, "response", G_CALLBACK(restart_emulator), NULL);

    g_object_unref(builder);

    // Link with another emulator dialog
    builder = gtk_builder_new_from_resource("/io/github/mpostaire/gbmulator/src/platform/desktop/ui/link.ui");
    link_emu_dialog = GTK_WIDGET(gtk_builder_get_object(builder, "window"));
    gtk_window_set_transient_for(GTK_WINDOW(link_emu_dialog), GTK_WINDOW(main_window));
    g_signal_connect(link_emu_dialog, "hide", G_CALLBACK(secondary_window_hide_cb), NULL);
    g_signal_connect(link_emu_dialog, "response", G_CALLBACK(link_dialog_response), NULL);

    GtkWidget *link_host_revealer = GTK_WIDGET(gtk_builder_get_object(builder, "link_host_revealer"));
    widget = GTK_WIDGET(gtk_builder_get_object(builder, "link_mode_setter_server"));
    g_signal_connect(GTK_TOGGLE_BUTTON(widget), "toggled", G_CALLBACK(link_mode_setter_server_toggled), link_host_revealer);

    widget = GTK_WIDGET(gtk_builder_get_object(builder, "link_host"));
    g_signal_connect(gtk_editable_get_delegate(GTK_EDITABLE(widget)), "changed", G_CALLBACK(set_link_host), NULL);
	g_signal_connect(gtk_editable_get_delegate(GTK_EDITABLE(widget)), "insert-text", G_CALLBACK(host_insert_text_handler), NULL);
    gtk_editable_set_text(GTK_EDITABLE(widget), config.link_host);

    widget = GTK_WIDGET(gtk_builder_get_object(builder, "link_port"));
    GtkAdjustment *link_port_adjustment = gtk_adjustment_new(7777.0, 0.0, 65535.0, 1.0, 5.0, 0.0);
    gtk_spin_button_set_adjustment(GTK_SPIN_BUTTON(adw_action_row_get_activatable_widget(ADW_ACTION_ROW(widget))), link_port_adjustment);
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(adw_action_row_get_activatable_widget(ADW_ACTION_ROW(widget))), 0.0, 65535.0);
    gtk_spin_button_set_climb_rate(GTK_SPIN_BUTTON(adw_action_row_get_activatable_widget(ADW_ACTION_ROW(widget))), 1.0);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(adw_action_row_get_activatable_widget(ADW_ACTION_ROW(widget))), 0);
    g_signal_connect(adw_action_row_get_activatable_widget(ADW_ACTION_ROW(widget)), "value-changed", G_CALLBACK(set_link_port), NULL);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(adw_action_row_get_activatable_widget(ADW_ACTION_ROW(widget))), (gdouble) atoi(config.link_port));

    g_object_unref(builder);

    // Printer window
    builder = gtk_builder_new_from_resource("/io/github/mpostaire/gbmulator/src/platform/desktop/ui/printer.ui");
    printer_window = GTK_WIDGET(gtk_builder_get_object(builder, "window"));
    g_object_ref(printer_window);
    g_signal_connect(printer_window, "hide", G_CALLBACK(secondary_window_hide_cb), NULL);

    printer_gl_area = GTK_WIDGET(gtk_builder_get_object(builder, "printer_gl_area"));
    g_signal_connect(printer_gl_area, "realize", G_CALLBACK(on_printer_realize), NULL);
    g_signal_connect(printer_gl_area, "render", G_CALLBACK(on_printer_render), NULL);
    g_signal_connect(printer_gl_area, "resize", G_CALLBACK(on_printer_resize), NULL);

    printer_save_btn = GTK_WIDGET(gtk_builder_get_object(builder, "save_btn"));
    g_signal_connect(printer_save_btn, "clicked", G_CALLBACK(printer_save_btn_clicked), NULL);

    printer_clear_btn = GTK_WIDGET(gtk_builder_get_object(builder, "clear_btn"));
    g_signal_connect(printer_clear_btn, "clicked", G_CALLBACK(printer_clear_btn_clicked), NULL);

    GtkScrolledWindow *printer_scroll = GTK_SCROLLED_WINDOW(gtk_builder_get_object(builder, "printer_scroll"));
    printer_scroll_adj = gtk_scrolled_window_get_vadjustment(printer_scroll);

    g_object_unref(builder);

    // Printer quit dialog
    builder = gtk_builder_new_from_resource("/io/github/mpostaire/gbmulator/src/platform/desktop/ui/printer_dialog.ui");
    printer_dialog = ADW_DIALOG(gtk_builder_get_object(builder, "dialog"));
    g_object_ref(printer_dialog);
    g_signal_connect(printer_dialog, "hide", G_CALLBACK(secondary_window_hide_cb), NULL);
    g_signal_connect(printer_dialog, "response", G_CALLBACK(printer_quit_dialog_response_cb), NULL);

    g_signal_connect(printer_window, "close-request", G_CALLBACK(printer_window_close_request_cb), NULL);

    g_object_unref(builder);

    // Mouse click
    GtkGesture *gesture_click = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(gesture_click), 0);
    gtk_widget_add_controller(GTK_WIDGET(emu_gl_area), GTK_EVENT_CONTROLLER(gesture_click));
    g_signal_connect(gesture_click, "pressed", G_CALLBACK(on_mouse_pressed), NULL);
    g_signal_connect(gesture_click, "released", G_CALLBACK(on_mouse_released), NULL);

    // Mouse motion
    GtkEventController *motion_event_controller = gtk_event_controller_motion_new();
    gtk_widget_add_controller(GTK_WIDGET(emu_gl_area), GTK_EVENT_CONTROLLER(motion_event_controller));
    g_signal_connect(motion_event_controller, "motion", G_CALLBACK(on_mouse_motion), NULL);

    // Keyboard input (main)
    GtkEventController *key_event_controller = gtk_event_controller_key_new();
    gtk_widget_add_controller(GTK_WIDGET(main_window), GTK_EVENT_CONTROLLER(key_event_controller));
    g_signal_connect(key_event_controller, "key-pressed", G_CALLBACK(key_pressed_main), NULL);
    g_signal_connect(key_event_controller, "key-released", G_CALLBACK(key_released_main), NULL);

    // Keyboard input (keybind)
    key_event_controller = gtk_event_controller_key_new();
    gtk_widget_add_controller(GTK_WIDGET(keybind_dialog), GTK_EVENT_CONTROLLER(key_event_controller));
    g_signal_connect(key_event_controller, "key-pressed", G_CALLBACK(key_pressed_keybind), NULL);

    // Drag and drop
    GtkDropTarget *target = gtk_drop_target_new(G_TYPE_INVALID, GDK_ACTION_COPY);
    gtk_drop_target_set_gtypes(target, (GType[1]) { GDK_TYPE_FILE_LIST }, 1);
    g_signal_connect(target, "drop", G_CALLBACK(on_drop), NULL);
    gtk_widget_add_controller(GTK_WIDGET(main_window), GTK_EVENT_CONTROLLER(target));

    if (argc > 1) {
        gtk_widget_set_visible(status, FALSE);
        gtk_widget_set_visible(emu_gl_area, TRUE);
        gtk_widget_grab_focus(emu_gl_area);
    }

    gtk_window_present(GTK_WINDOW(main_window));
}

static void shutdown_cb(GtkApplication *app) {
    app_quit();
}

static gint command_line_cb(GtkApplication *app, GApplicationCommandLine *command_line, gpointer user_data) {
    argv = g_application_command_line_get_arguments(command_line, &argc);

    // prevent app to close immediately after parsing command line arguments
    g_application_activate(G_APPLICATION(app));
    return 0;
}

int main(int argc, char **argv) {
    app = adw_application_new(NULL, G_APPLICATION_HANDLES_COMMAND_LINE);
    g_signal_connect(app, "activate", G_CALLBACK(activate_cb), NULL);
    g_signal_connect(app, "shutdown", G_CALLBACK(shutdown_cb), NULL);
    g_signal_connect(app, "command-line", G_CALLBACK(command_line_cb), NULL);

    g_autoptr(ManetteMonitor) monitor = manette_monitor_new();
    g_autoptr(ManetteMonitorIter) iter = manette_monitor_iterate(monitor);

    g_signal_connect_object(G_OBJECT(monitor), "device-connected", G_CALLBACK(gamepad_connected_cb), NULL, 0);

    ManetteDevice *device;
    while (manette_monitor_iter_next(iter, &device))
        gamepad_connected_cb(monitor, device, NULL);

    gst_init(&argc, &argv);
    return g_application_run(G_APPLICATION(app), argc, argv);
}
