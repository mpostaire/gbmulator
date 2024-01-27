#include "../../core/gb.h"

#include <adwaita.h>
#include <libmanette.h>
#include <linux/input-event-codes.h>
#include <ctype.h>
#include <netdb.h>
#include <string.h>

#include "glrenderer.h"
#include "alrenderer.h"
#include "camera.h"
#include "../common/link.h"
#include "../common/utils.h"
#include "../common/config.h"

#define APP_NAME EMULATOR_NAME
#define APP_ICON "gbmulator"
#define APP_VERSION "0.2"

#define CASE_THEN_STRING(x) case x: return #x
#define STRING(x) #x

#define XPM_WHITE " "
#define XPM_LIGHT_GRAY "."
#define XPM_DARK_GRAY "+"
#define XPM_BLACK "-"

static gboolean keycode_filter(guint keyval);
static const char *gamepad_gamepad_button_parser(guint16 button);
static int gamepad_button_name_parser(const char *button_name);

static gboolean loop(gpointer user_data);

// config struct initialized to defaults
static config_t config = {
    .mode = GB_MODE_CGB,
    .color_palette = PPU_COLOR_PALETTE_ORIG,
    .scale = 2,
    .sound = 1.0f,
    .sound_drc = 1,
    .speed = 1.0f,
    .link_host = "127.0.0.1",
    .link_port = "7777",

    .gamepad_bindings = {
        BTN_DPAD_RIGHT,
        BTN_DPAD_LEFT,
        BTN_DPAD_UP,
        BTN_DPAD_DOWN,
        BTN_A,
        BTN_B,
        BTN_SELECT,
        BTN_START
    },
    .gamepad_button_parser = (keycode_parser_t) gamepad_gamepad_button_parser,
    .gamepad_button_name_parser = (keyname_parser_t) gamepad_button_name_parser,

    .keybindings = {
        GDK_KEY_Right,
        GDK_KEY_Left,
        GDK_KEY_Up,
        GDK_KEY_Down,
        GDK_KEY_KP_0,
        GDK_KEY_period,
        GDK_KEY_KP_2,
        GDK_KEY_KP_1
    },
    .keycode_filter = keycode_filter,
    .keycode_parser = gdk_keyval_name,
    .keyname_parser = gdk_keyval_from_name
};

enum {
    GAMEPAD_DISABLED,
    GAMEPAD_PLAYING,
    GAMEPAD_BINDING
};

typedef struct {
    const char *name;
    const char *label_name;
    int id;
    GtkWidget *widget;
} setter_handler_t;

static setter_handler_t key_handlers[] = {
    { "key_setter_right", "key_setter_right_label", 0, NULL },
    { "key_setter_left", "key_setter_left_label", 1, NULL },
    { "key_setter_up", "key_setter_up_label", 2, NULL },
    { "key_setter_down", "key_setter_down_label", 3, NULL },
    { "key_setter_a", "key_setter_a_label", 4, NULL },
    { "key_setter_b", "key_setter_b_label", 5, NULL },
    { "key_setter_select", "key_setter_select_label", 6, NULL },
    { "key_setter_start", "key_setter_start_label", 7, NULL }
};

static setter_handler_t gamepad_handlers[] = {
    { "gamepad_setter_right", "gamepad_setter_right_label", 0, NULL },
    { "gamepad_setter_left", "gamepad_setter_left_label", 1, NULL },
    { "gamepad_setter_up", "gamepad_setter_up_label", 2, NULL },
    { "gamepad_setter_down", "gamepad_setter_down_label", 3, NULL },
    { "gamepad_setter_a", "gamepad_setter_a_label", 4, NULL },
    { "gamepad_setter_b", "gamepad_setter_b_label", 5, NULL },
    { "gamepad_setter_select", "gamepad_setter_select_label", 6, NULL },
    { "gamepad_setter_start", "gamepad_setter_start_label", 7, NULL }
};

static const char *joypad_names[] = {
    "Right:",
    "Left:",
    "Up:",
    "Down:",
    "A:",
    "B:",
    "Select:",
    "Start:"
};

static gint argc;
static gchar **argv = NULL;

static int current_bind_setter;
static int binding_setter_handler = -1;
static int gamepad_state = GAMEPAD_DISABLED;

static AdwApplication *app;
static GtkWidget *main_window, *preferences_window, *window_title, *toast_overlay, *emu_gl_area, *printer_gl_area, *keybind_dialog;
static GtkWidget *joypad_name, *restart_dialog, *link_emu_dialog, *printer_window, *status, *bind_value, *mode_setter, *printer_save_btn;
static GtkWidget *printer_clear_btn, *speed_slider_container, *open_btn, *link_spinner_revealer, *link_spinner;
static GtkAdjustment *printer_scroll_adj;
static GtkEventController *motion_event_controller;
static glong motion_event_handler = 0;
static GtkFileDialog *open_rom_dialog, *save_printer_image_dialog;
static guint loop_source = 0;

static gsize printer_gl_area_height = GB_SCREEN_HEIGHT;
static gboolean printer_window_allowed_to_close = FALSE;
static gboolean printer_save_dialog_resume_loop = FALSE;
static gboolean is_paused = TRUE, link_is_server = TRUE;
static int steps_per_frame;
static gb_t *gb = NULL;
static byte_t joypad_state = 0xFF;
static double accel_x, accel_y;
static gb_printer_t *printer = NULL;

static GCancellable *link_task_cancellable;
static GTask *link_task;
static gb_t *linked_gb = NULL;
static int sfd = -1;

static char *rom_path, *forced_save_path, *config_path;

static glrenderer_t *emu_renderer, *printer_renderer;

static void show_link_emu_dialog(GSimpleAction *action, GVariant *parameter, gpointer app);
static void show_printer_window(GSimpleAction *action, GVariant *parameter, gpointer app);
static void ask_restart_emulator(GSimpleAction *action, GVariant *parameter, gpointer app);
static void toggle_pause(GSimpleAction *action, GVariant *parameter, gpointer app);

static gboolean keycode_filter(guint keyval) {
    switch (keyval) {
    case GDK_KEY_F1: case GDK_KEY_F2:
    case GDK_KEY_F3: case GDK_KEY_F4:
    case GDK_KEY_F5: case GDK_KEY_F6:
    case GDK_KEY_F7: case GDK_KEY_F8:
        return FALSE;
    default:
        return TRUE;
    }
}

static const char *gamepad_gamepad_button_parser(guint16 button) {
    switch (button) {
        CASE_THEN_STRING(BTN_A);
        CASE_THEN_STRING(BTN_B);
        CASE_THEN_STRING(BTN_C);
        CASE_THEN_STRING(BTN_X);
        CASE_THEN_STRING(BTN_Y);
        CASE_THEN_STRING(BTN_Z);
        CASE_THEN_STRING(BTN_TL);
        CASE_THEN_STRING(BTN_TR);
        CASE_THEN_STRING(BTN_TL2);
        CASE_THEN_STRING(BTN_TR2);
        CASE_THEN_STRING(BTN_SELECT);
        CASE_THEN_STRING(BTN_START);
        CASE_THEN_STRING(BTN_MODE);
        CASE_THEN_STRING(BTN_THUMBL);
        CASE_THEN_STRING(BTN_THUMBR);
        CASE_THEN_STRING(BTN_DPAD_UP);
        CASE_THEN_STRING(BTN_DPAD_DOWN);
        CASE_THEN_STRING(BTN_DPAD_LEFT);
        CASE_THEN_STRING(BTN_DPAD_RIGHT);
    default:
        return NULL;
    }
}

static int gamepad_button_name_parser(const char *button_name) {
    size_t max_len = sizeof(STRING(BTN_DPAD_RIGHT));
    if (!strncmp(button_name, STRING(BTN_A), max_len))
        return BTN_A;
    if (!strncmp(button_name, STRING(BTN_B), max_len))
        return BTN_B;
    if (!strncmp(button_name, STRING(BTN_C), max_len))
        return BTN_C;
    if (!strncmp(button_name, STRING(BTN_X), max_len))
        return BTN_X;
    if (!strncmp(button_name, STRING(BTN_Y), max_len))
        return BTN_Y;
    if (!strncmp(button_name, STRING(BTN_Z), max_len))
        return BTN_Z;
    if (!strncmp(button_name, STRING(BTN_TL), max_len))
        return BTN_TL;
    if (!strncmp(button_name, STRING(BTN_TR), max_len))
        return BTN_TR;
    if (!strncmp(button_name, STRING(BTN_TL2), max_len))
        return BTN_TL2;
    if (!strncmp(button_name, STRING(BTN_TR2), max_len))
        return BTN_TR2;
    if (!strncmp(button_name, STRING(BTN_SELECT), max_len))
        return BTN_SELECT;
    if (!strncmp(button_name, STRING(BTN_START), max_len))
        return BTN_START;
    if (!strncmp(button_name, STRING(BTN_MODE), max_len))
        return BTN_MODE;
    if (!strncmp(button_name, STRING(BTN_THUMBL), max_len))
        return BTN_THUMBL;
    if (!strncmp(button_name, STRING(BTN_THUMBR), max_len))
        return BTN_THUMBR;
    if (!strncmp(button_name, STRING(BTN_DPAD_UP), max_len))
        return BTN_DPAD_UP;
    if (!strncmp(button_name, STRING(BTN_DPAD_DOWN), max_len))
        return BTN_DPAD_DOWN;
    if (!strncmp(button_name, STRING(BTN_DPAD_LEFT), max_len))
        return BTN_DPAD_LEFT;
    if (!strncmp(button_name, STRING(BTN_DPAD_RIGHT), max_len))
        return BTN_DPAD_RIGHT;
    return 0;
}

void start_loop(void) {
    if (loop_source > 0 || link_task)
        return;
    loop_source = g_timeout_add(1000 / 60, G_SOURCE_FUNC(loop), NULL);
    is_paused = FALSE;
    alrenderer_play();
}

void stop_loop(void) {
    g_source_remove(loop_source);
    loop_source = 0;
    is_paused = TRUE;
    alrenderer_pause();
}

static void toggle_loop(void) {
    if (is_paused)
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

static void show_toast(const char *text) {
    AdwToast *toast = adw_toast_new(text);
    adw_toast_set_timeout(toast, 1);
    adw_toast_overlay_add_toast(ADW_TOAST_OVERLAY(toast_overlay), toast);
}

static inline gboolean loop(gpointer user_data) {
    gb_set_joypad_state(gb, joypad_state);

    // TODO async or timeout link_exchange_joypad
    if (linked_gb) {
        if (!link_exchange_joypad(sfd, gb, linked_gb)) {
            linked_gb = NULL;
            sfd = -1;
            set_link_gui_actions(TRUE, TRUE);
            steps_per_frame = GB_CPU_STEPS_PER_FRAME * config.speed;
            gb_set_apu_speed(gb, config.speed);
            show_toast("Link Cable disconnected");
        }
    }
    gb_run_steps(gb, steps_per_frame);

    gtk_gl_area_queue_render(GTK_GL_AREA(emu_gl_area));

    return G_SOURCE_CONTINUE;
}

static void on_emu_realize(GtkGLArea *area, gpointer user_data) {
    gtk_gl_area_make_current(area);
    if (gtk_gl_area_get_error(area) != NULL) {
        fprintf(stderr, "Unknown error\n");
        return;
    }

    emu_renderer = glrenderer_init(GB_SCREEN_WIDTH, GB_SCREEN_HEIGHT, NULL);
}

static void on_emu_unrealize(GtkGLArea *area, gpointer user_data) {
    if (!is_paused)
        stop_loop();
}

static gboolean on_emu_render(GtkGLArea *area, GdkGLContext *context, gpointer user_data) {
    glrenderer_render(emu_renderer);
    return TRUE;
}

static void on_printer_realize(GtkGLArea *area, gpointer user_data) {
    gtk_gl_area_make_current(area);
    if (gtk_gl_area_get_error(area) != NULL) {
        fprintf(stderr, "Unknown error\n");
        return;
    }

    printer_renderer = glrenderer_init(GB_PRINTER_IMG_WIDTH, printer_gl_area_height, NULL);
}

static gboolean on_printer_render(GtkGLArea *area, GdkGLContext *context, gpointer user_data) {
    glrenderer_render(printer_renderer);
    return TRUE;
}

static gboolean on_printer_resize(GtkGLArea *area, GdkGLContext *context) {
    double adj_upper = gtk_adjustment_get_upper(GTK_ADJUSTMENT(printer_scroll_adj));
    gtk_adjustment_set_value(GTK_ADJUSTMENT(printer_scroll_adj), adj_upper);
    return TRUE;
}

static void ppu_vblank_cb(const byte_t *pixels) {
    glrenderer_update_texture(emu_renderer, 0, 0, GB_SCREEN_WIDTH, GB_SCREEN_HEIGHT, pixels);
}

static void printer_new_line_cb(const byte_t *pixels, size_t height) {
    if (!printer_renderer)
        return; // segfault if printer gl area was not realized (printer window not shown at least once before printing)

    int height_offset = printer_gl_area_height - height;
    if (height_offset < 0) {
        printer_gl_area_height++;
        height_offset = printer_gl_area_height - height;

        glrenderer_resize_texture(printer_renderer, GB_SCREEN_WIDTH, printer_gl_area_height);
        gtk_widget_set_size_request(GTK_WIDGET(printer_gl_area), GB_SCREEN_WIDTH * 2, printer_gl_area_height * 2);
    }

    glrenderer_update_texture(printer_renderer, 0, height_offset, GB_SCREEN_WIDTH, height, pixels);
    gtk_gl_area_queue_render(GTK_GL_AREA(printer_gl_area));
}

static void printer_start_cb(const byte_t *pixels, size_t height) {
    gtk_widget_set_sensitive(GTK_WIDGET(printer_save_btn), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(printer_clear_btn), FALSE);
}

static void printer_finish_cb(const byte_t *pixels, size_t height) {
    gtk_widget_set_sensitive(GTK_WIDGET(printer_save_btn), TRUE);
    gtk_widget_set_sensitive(GTK_WIDGET(printer_clear_btn), TRUE);
}

static void set_accelerometer_data(double *x, double *y) {
    *x = accel_x;
    *y = accel_y;
}

static void toggle_pause(GSimpleAction *action, GVariant *parameter, gpointer app) {
    if (!linked_gb && !link_task) {
        show_toast(is_paused ? "Resumed" : "Paused");
        toggle_loop();
    }
}

static void restart_emulator(AdwMessageDialog *self, gchar *response, gpointer user_data) {
    if (!strncmp(response, "restart", 8)) {
        start_loop();
        gb_reset(gb, config.mode);
        gb_print_status(gb);
        alrenderer_clear_queue();
    }
}

static void ask_restart_emulator(GSimpleAction *action, GVariant *parameter, gpointer app) {
    if (gb && !linked_gb && !link_task) {
        gamepad_state = GAMEPAD_DISABLED;
        gtk_window_present(GTK_WINDOW(restart_dialog));
    }
}

void start_link_thread_cb(GObject *source_object, GAsyncResult *res, gpointer data) {
    gtk_revealer_set_reveal_child(GTK_REVEALER(link_spinner_revealer), FALSE);

    if (g_task_propagate_boolean(G_TASK(res), NULL)) {
        show_toast("Link Cable connected");
    } else if (g_cancellable_is_cancelled(link_task_cancellable)) {
        link_cancel();
        close(sfd);
        sfd = -1;

        set_link_gui_actions(TRUE, TRUE);
        show_toast("Connection cancelled");
    } else {
        set_link_gui_actions(TRUE, TRUE);
        show_toast("Connection error");
    }

    g_object_unref(link_task_cancellable);
    g_object_unref(link_task);
    link_task = NULL;
    link_task_cancellable = NULL;
    start_loop();
}

void start_link_thread(GTask *task, gpointer source_object, gpointer task_data, GCancellable *cancellable) {
    if (link_is_server)
        sfd = link_start_server(config.link_port);
    else
        sfd = link_connect_to_server(config.link_host, config.link_port);

    gb_t *new_linked_gb;
    if (sfd > 0 && link_init_transfer(sfd, gb, &new_linked_gb)) {
        linked_gb = new_linked_gb;
        steps_per_frame = GB_CPU_STEPS_PER_FRAME;
        gb_set_apu_speed(gb, 1.0f);
        g_task_return_boolean(link_task, TRUE);
    } else {
        g_task_return_boolean(link_task, FALSE);
        sfd = -1; // closed by link_init_transfer in case of error
    }
}

static void link_dialog_response(GtkDialog *self, gint response_id, gpointer user_data) {
    gtk_window_close(GTK_WINDOW(self));
    if (response_id != GTK_RESPONSE_OK || !gb)
        return;

    stop_loop();

    show_toast("Connecting Link Cable...");

    set_link_gui_actions(FALSE, TRUE);
    gtk_widget_set_visible(link_spinner_revealer, TRUE);
    gtk_revealer_set_reveal_child(GTK_REVEALER(link_spinner_revealer), TRUE);
    gtk_spinner_set_spinning(GTK_SPINNER(link_spinner), TRUE);

    link_task_cancellable = g_cancellable_new();
    link_task = g_task_new(NULL, link_task_cancellable, start_link_thread_cb, NULL);
    g_task_set_return_on_cancel(link_task, TRUE);
    g_task_run_in_thread(link_task, start_link_thread);
}

static void show_link_emu_dialog(GSimpleAction *action, GVariant *parameter, gpointer app) {
    gamepad_state = GAMEPAD_DISABLED;
    gtk_window_present(GTK_WINDOW(link_emu_dialog));
}

static void show_printer_window(GSimpleAction *action, GVariant *parameter, gpointer app) {
    printer = gb_printer_init(printer_new_line_cb, printer_start_cb, printer_finish_cb);
    gb_link_connect_printer(gb, printer);
    set_link_gui_actions(FALSE, FALSE);
    gamepad_state = GAMEPAD_DISABLED;
    gtk_window_present(GTK_WINDOW(printer_window));
}

static gboolean mouse_motion(GtkEventControllerMotion *self, gdouble x, gdouble y, gpointer user_data) {
    x = CLAMP(x / 2, 0, GB_SCREEN_WIDTH);
    y = CLAMP(y / 2, 0, GB_SCREEN_HEIGHT);
    accel_x = (x - 80) / -80.0;
    accel_y = (y - 72) / -72.0;
    printf("(%lf, %lf) accel_x=%lf accel_y=%lf\n", x, y, accel_x, accel_y);
    return TRUE;
}

static int load_cartridge(char *path) {
    if (!gb && !path) return 0;

    if (gb) {
        stop_loop();
        char *save_path = get_save_path(rom_path);
        save_battery_to_file(gb, forced_save_path ? forced_save_path : save_path);
        free(save_path);
    }

    size_t rom_size;
    byte_t *rom;
    if (path) {
        size_t len = strlen(path);
        rom_path = xrealloc(rom_path, len + 2);
        snprintf(rom_path, len + 1, "%s", path);

        rom = get_rom(rom_path, &rom_size);
        if (!rom) {
            free(rom_path);
            rom_path = NULL;
            return 0;
        }
    } else {
        byte_t *data = gb_get_rom(gb, &rom_size);
        rom = xmalloc(rom_size);
        memcpy(rom, data, rom_size);
    }

    gb_options_t opts = {
        .mode = config.mode,
        .on_new_sample = alrenderer_queue_sample,
        .on_new_frame = ppu_vblank_cb,
        .on_accelerometer_request = set_accelerometer_data,
        .apu_speed = config.speed,
        .apu_sampling_rate = alrenderer_get_sampling_rate(),
        .apu_sound_level = config.sound,
        .palette = config.color_palette,
        .on_camera_capture_image = gb_camera_capture_image_cb
    };
    gb_t *new_emu = gb_init(rom, rom_size, &opts);
    free(rom);
    if (!new_emu) return 0;

    char *save_path = get_save_path(rom_path);
    load_battery_from_file(new_emu, forced_save_path ? forced_save_path : save_path);
    free(save_path);

    if (gb) {
        // if (linked_gb)
        //     on_link_disconnect();
        gb_quit(gb);
    }
    gb = new_emu;
    gb_print_status(gb);
    alrenderer_clear_queue();

    steps_per_frame = GB_CPU_STEPS_PER_FRAME * config.speed;

    const GActionEntry app_entries[] = {
        { "play_pause", toggle_pause, NULL, NULL, NULL },
        { "restart", ask_restart_emulator, NULL, NULL, NULL },
        { "connect_emulator", show_link_emu_dialog, NULL, NULL, NULL },
        { "connect_printer", show_printer_window, NULL, NULL, NULL },
    };
    g_action_map_add_action_entries(G_ACTION_MAP(app), app_entries, G_N_ELEMENTS(app_entries), app);

    adw_window_title_set_subtitle(ADW_WINDOW_TITLE(window_title), gb_get_rom_title(gb));

    if (gb_has_accelerometer(gb)) {
        motion_event_handler = g_signal_connect(motion_event_controller, "motion", G_CALLBACK(mouse_motion), NULL);
    } else if (motion_event_handler) {
        g_signal_handler_disconnect(motion_event_controller, motion_event_handler);
        motion_event_handler = 0;
    }

    if (gb_has_camera(gb))
        camera_play();
    else
        camera_pause();

    gamepad_state = GAMEPAD_PLAYING;
    start_loop();

    return 1;
}

static void set_speed(GtkRange *self, gpointer user_data) {
    config.speed = gtk_range_get_value(self);
    steps_per_frame = GB_CPU_STEPS_PER_FRAME * config.speed;
    if (gb)
        gb_set_apu_speed(gb, config.speed);
}

static void set_sound(GtkRange *self, gpointer user_data) {
    config.sound = gtk_range_get_value(self);
    if (gb)
        gb_set_apu_sound_level(gb, config.sound);
}

static void set_sound_drc(AdwSwitchRow *self, gpointer user_data) {
    config.sound_drc = adw_switch_row_get_active(self);
    alrenderer_enable_dynamic_rate_control(config.sound_drc);
}

static void set_palette(AdwComboRow *self, GParamSpec *pspec, gpointer user_data) {
    config.color_palette = adw_combo_row_get_selected(self);
    if (gb)
        gb_set_palette(gb, config.color_palette);
}

static void set_keybinding(GtkDialog *self, gint response_id, gpointer user_data) {
    gtk_window_close(GTK_WINDOW(self));
    if (response_id == GTK_RESPONSE_APPLY) {
        const char *keyname = gtk_label_get_label(GTK_LABEL(bind_value));
        if (!strncmp(keyname, "Press a key", 12)) return;
        unsigned int keyval = gdk_keyval_from_name(keyname);

        // detect if the key is already attributed, if yes, swap them
        for (int i = JOYPAD_RIGHT; i < JOYPAD_START; i++) {
            if (config.keybindings[i] == keyval && current_bind_setter != i) {
                config.keybindings[i] = config.keybindings[current_bind_setter];
                gtk_label_set_label(GTK_LABEL(key_handlers[i].widget), gdk_keyval_name(config.keybindings[i]));
                break;
            }
        }

        config.keybindings[current_bind_setter] = keyval;
        gtk_label_set_label(GTK_LABEL(key_handlers[current_bind_setter].widget), gdk_keyval_name(config.keybindings[current_bind_setter]));
    }
}

static void set_gamepad_binding(GtkDialog *self, gint response_id, gpointer user_data) {
    gtk_window_close(GTK_WINDOW(self));
    if (response_id == GTK_RESPONSE_APPLY) {
        const char *button_name = gtk_label_get_label(GTK_LABEL(bind_value));
        if (!strncmp(button_name, "Press a key", 12)) return;
        unsigned int button = gamepad_button_name_parser(button_name);

        // detect if the key is already attributed, if yes, swap them
        for (int i = JOYPAD_RIGHT; i < JOYPAD_START; i++) {
            if (config.gamepad_bindings[i] == button && current_bind_setter != i) {
                config.gamepad_bindings[i] = config.gamepad_bindings[current_bind_setter];
                gtk_label_set_label(GTK_LABEL(gamepad_handlers[i].widget), gamepad_gamepad_button_parser(config.gamepad_bindings[i]));
                break;
            }
        }

        config.gamepad_bindings[current_bind_setter] = button;
        gtk_label_set_label(GTK_LABEL(gamepad_handlers[current_bind_setter].widget), gamepad_gamepad_button_parser(config.gamepad_bindings[current_bind_setter]));
    }
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
    strncpy(config.link_host, gtk_editable_get_text(self), INET6_ADDRSTRLEN);
    config.link_host[INET6_ADDRSTRLEN - 1] = '\0';
}

static void set_link_port(GtkSpinButton *self, gpointer user_data) {
    snprintf(config.link_port, sizeof(config.link_port), "%d", (int) gtk_spin_button_get_value(self));
    config.link_port[5] = '\0';
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
    config.mode = adw_combo_row_get_selected(self) + 1;
}

static void set_camera(AdwComboRow *self, GParamSpec *pspec, gpointer user_data) {
    guint selected_camera = adw_combo_row_get_selected(self);
    gsize len;
    gchar ***paths = camera_get_devices_paths(&len);
    if (selected_camera < len) {
        camera_quit();
        camera_init((*paths)[selected_camera]);
        if (gb_has_camera(gb))
            camera_play();
    }
}

static void key_setter_activated(AdwActionRow *self, gpointer user_data) {
    current_bind_setter = *((int *) user_data);
    gtk_label_set_label(GTK_LABEL(joypad_name), joypad_names[current_bind_setter]);
    gtk_label_set_label(GTK_LABEL(bind_value), "Press a key");
    if (binding_setter_handler > 0)
        g_signal_handler_disconnect(keybind_dialog, binding_setter_handler);
    binding_setter_handler = g_signal_connect(keybind_dialog, "response", G_CALLBACK(set_keybinding), NULL);
    gtk_window_present(GTK_WINDOW(keybind_dialog));
}

static void gamepad_setter_activated(AdwActionRow *self, gpointer user_data) {
    current_bind_setter = *((int *) user_data);
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
    gtk_window_present(GTK_WINDOW(preferences_window));
}

static void show_about(GSimpleAction *action, GVariant *parameter, gpointer app) {
    const char *developers[] = {
        "Maxime Postaire https://github.com/mpostaire",
        NULL
    };

    adw_show_about_window(gtk_application_get_active_window(GTK_APPLICATION(app)),
                          "application-name", APP_NAME,
                          "application-icon", APP_ICON,
                          "version", APP_VERSION,
                          "copyright", "© 2022 Maxime Postaire",
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
        gtk_file_filter_add_pattern(filter, "*.gb");
        gtk_file_filter_add_pattern(filter, "*.gbc");
        gtk_file_filter_add_mime_type(filter, "application/x-gameboy-rom");
        gtk_file_filter_add_mime_type(filter, "application/x-gameboy-color-rom");
        gtk_file_filter_set_name(filter, "GameBoy Color ROM");

        g_autoptr(GListStore) list = g_list_store_new(GTK_TYPE_FILE_FILTER);
        g_list_store_append(list, filter);

        gtk_file_dialog_set_filters(open_rom_dialog, G_LIST_MODEL(list));
    }

    gtk_file_dialog_open(open_rom_dialog, GTK_WINDOW(main_window), NULL, open_rom_dialog_cb, NULL);
}

static void printer_save_as_xpm(gb_printer_t *printer, char *file_path) {
    size_t height;
    byte_t *image_data = gb_printer_get_image(printer, &height);

    FILE *f = fopen(file_path, "w+");

    fprintf(f, "/* XPM */\nstatic char *image = {\n");
    fprintf(f, "\"%d %lu 4 1\",\n", GB_PRINTER_IMG_WIDTH, height);
    fprintf(f, "\""XPM_WHITE" c #FFFFFF\",\n\""XPM_LIGHT_GRAY" c #AAAAAA\",\n\""XPM_DARK_GRAY" c #555555\",\n\""XPM_BLACK" c #000000\",\n");

    for (size_t i = 0; i < height; i++) {
        fprintf(f, "\"");
        for (int j = 0; j < GB_PRINTER_IMG_WIDTH; j++) {
            int image_data_index = (i * GB_PRINTER_IMG_WIDTH * 4) + (j * 4);
            char c = *XPM_BLACK;
            switch (image_data[image_data_index]) {
            case 0xFF: c = *XPM_WHITE; break;
            case 0xAA: c = *XPM_LIGHT_GRAY; break;
            case 0x55: c = *XPM_DARK_GRAY; break;
            case 0x00: c = *XPM_BLACK; break;
            default: eprintf("invalid color data\n"); break;
            }
            fprintf(f, "%c", c);
        }
        fprintf(f, "\"%s", i == height - 1 ? "};\n" : ",\n");
    }

    fclose(f);
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

    if (!is_paused) {
        stop_loop();
        printer_save_dialog_resume_loop = TRUE;
    }
    gtk_file_dialog_save(save_printer_image_dialog, GTK_WINDOW(printer_window), NULL, printer_save_dialog_cb, NULL);
}

static void clear_printer_gl_area(void) {
    glrenderer_resize_texture(printer_renderer, GB_SCREEN_WIDTH, GB_SCREEN_HEIGHT);
    gtk_widget_set_size_request(GTK_WIDGET(printer_gl_area), GB_SCREEN_WIDTH * 2, GB_SCREEN_HEIGHT * 2);
    printer_gl_area_height = GB_SCREEN_HEIGHT;

    void *pixels = xcalloc(1, GB_SCREEN_WIDTH * GB_SCREEN_HEIGHT * 4);
    glrenderer_update_texture(printer_renderer, 0, 0, GB_SCREEN_WIDTH, GB_SCREEN_HEIGHT, pixels);
    free(pixels);

    gtk_gl_area_queue_render(GTK_GL_AREA(printer_gl_area));
}

static void printer_clear_btn_clicked(AdwActionRow *self, gpointer user_data) {
    gb_printer_clear_image(printer);
    if (!printer_renderer)
        return; // segfault if printer gl area was not realized (printer window not shown at least once before printing)

    clear_printer_gl_area();

    gtk_widget_set_sensitive(GTK_WIDGET(printer_save_btn), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(printer_clear_btn), FALSE);
}

static gboolean key_pressed_main(GtkEventControllerKey *self, guint keyval, guint keycode, GdkModifierType state, gpointer user_data) {
    if (state & GDK_CONTROL_MASK) {
        switch (keyval) {
        case GDK_KEY_p:
            toggle_pause(NULL, NULL, NULL);
            break;
        case GDK_KEY_r:
            ask_restart_emulator(NULL, NULL, NULL);
            break;
        case GDK_KEY_l:
            show_link_emu_dialog(NULL, NULL, NULL);
            break;
        }
        return TRUE;
    }

    if (!gb || is_paused) return FALSE;

    switch (keyval) {
    case GDK_KEY_F11:
        if (gtk_window_is_fullscreen(GTK_WINDOW(main_window)))
            gtk_window_unfullscreen(GTK_WINDOW(main_window));
        else
            gtk_window_fullscreen(GTK_WINDOW(main_window));
        return TRUE;
    case GDK_KEY_F1: case GDK_KEY_F2:
    case GDK_KEY_F3: case GDK_KEY_F4:
    case GDK_KEY_F5: case GDK_KEY_F6:
    case GDK_KEY_F7: case GDK_KEY_F8:
        if (linked_gb)
            return TRUE;

        char *savestate_path = get_savestate_path(rom_path, keyval - GDK_KEY_F1);
        if (state & GDK_SHIFT_MASK) {
            save_state_to_file(gb, savestate_path, 1);
        } else {
            int ret = load_state_from_file(gb, savestate_path);
            if (ret > 0) {
                config.mode = ret;
                adw_combo_row_set_selected(ADW_COMBO_ROW(mode_setter), config.mode - 1);
            }
        }
        free(savestate_path);
        return TRUE;
    }

    // don't use gb_joypad_press() here as we want to keep track of the joypad state and set it once per loop for link cable synchronization
    int joypad = keycode_to_joypad(&config, keyval);
    if (joypad < 0) return TRUE;
    RESET_BIT(joypad_state, joypad);
    return TRUE;
}

static gboolean key_released_main(GtkEventControllerKey *self, guint keyval, guint keycode, GdkModifierType state, gpointer user_data) {
    if (!gb || is_paused) return FALSE;

    // don't use gb_joypad_release() here as we want to keep track of the joypad state and set it once per loop for link cable synchronization
    int joypad = keycode_to_joypad(&config, keyval);
    if (joypad < 0) return TRUE;
    SET_BIT(joypad_state, joypad);
    return TRUE;
}

static gboolean key_pressed_keybind(GtkEventControllerKey *self, guint keyval, guint keycode, GdkModifierType state, gpointer user_data) {
    if (keyval == GDK_KEY_Escape || gamepad_state == GAMEPAD_BINDING)
        return FALSE;
    if (config.keycode_filter(keyval))
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

static gchar *sound_slider_format(GtkScale *self, gdouble value, gpointer user_data) {
    return g_strdup_printf("%d%%", (int) (value * 100));
}

static gchar *speed_slider_format(GtkScale *self, gdouble value, gpointer user_data) {
    return g_strdup_printf("x%0.1f", value);
}

static void gamepad_button_press_event_cb(ManetteDevice *emitter, ManetteEvent *event, gpointer user_data) {
    guint16 button;
    switch (gamepad_state) {
    case GAMEPAD_DISABLED:
        break;
    case GAMEPAD_PLAYING:;
        if (!gb || is_paused) return;
        if (manette_event_get_button(event, &button)) {
            int joypad = button_to_joypad(&config, button);
            if (joypad < 0) return;
            RESET_BIT(joypad_state, joypad);
        }
        break;
    case GAMEPAD_BINDING:
        if (manette_event_get_button(event, &button))
            gtk_label_set_label(GTK_LABEL(bind_value), gamepad_gamepad_button_parser(button));
        break;
    }
}

static void gamepad_button_release_event_cb(ManetteDevice *emitter, ManetteEvent *event, gpointer user_data) {
    switch (gamepad_state) {
    case GAMEPAD_DISABLED:
        break;
    case GAMEPAD_PLAYING:
        if (!gb || is_paused) return;
        guint16 button;
        if (manette_event_get_button(event, &button)) {
            int joypad = button_to_joypad(&config, button);
            if (joypad < 0) return;
            SET_BIT(joypad_state, joypad);
        }
        break;
    case GAMEPAD_BINDING:
        break;
    }
}

static void gamepad_disconnected_cb(ManetteDevice *emitter, gpointer user_data) {
    printf("%s: disconnected\n", manette_device_get_name(emitter));
    g_object_unref(G_OBJECT(emitter));
}

static void gamepad_connected_cb(ManetteMonitor *emitter, ManetteDevice *device, gpointer user_data) {
    printf("%s: connected\n", manette_device_get_name(device));
    g_signal_connect_object(G_OBJECT(device), "disconnected", (GCallback) gamepad_disconnected_cb, NULL, 0);
    g_signal_connect_object(G_OBJECT(device), "button-press-event", (GCallback) gamepad_button_press_event_cb, NULL, 0);
    g_signal_connect_object(G_OBJECT(device), "button-release-event", (GCallback) gamepad_button_release_event_cb, NULL, 0);
}

static void secondary_window_hide_cb(GtkWidget *self, gpointer user_data) {
    gamepad_state = (!gb || is_paused) ? GAMEPAD_DISABLED : GAMEPAD_PLAYING;
}

static gboolean printer_window_close_request_cb(GtkWidget *self, gpointer user_data) {
    if (printer_window_allowed_to_close) {
        printer_window_allowed_to_close = FALSE;
        return FALSE;
    }
    gtk_window_present(GTK_WINDOW(user_data));
    return TRUE;
}

static void printer_quit_dialog_response_cb(AdwMessageDialog *self, gchar *response, gpointer user_data) {
    if (!strncmp(response, "disconnect", 11)) {
        gb_link_disconnect(gb);
        gb_printer_quit(printer);
        printer = NULL;
        clear_printer_gl_area();

        gtk_widget_set_sensitive(GTK_WIDGET(printer_save_btn), FALSE);
        gtk_widget_set_sensitive(GTK_WIDGET(printer_clear_btn), FALSE);
        set_link_gui_actions(TRUE, FALSE);

        printer_window_allowed_to_close = TRUE;
        gtk_window_close(GTK_WINDOW(printer_window));
    }
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
    // Load config
    config_path = get_config_path();
    config_load_from_file(&config, config_path);

    // Main window
    GtkBuilder *builder = gtk_builder_new_from_resource("/io/github/mpostaire/gbmulator/src/platform/desktop/ui/main.ui");
    main_window = GTK_WIDGET(gtk_builder_get_object(builder, "window"));
    GtkWidget *main_window_view = GTK_WIDGET(gtk_builder_get_object(builder, "toolbarview"));
    g_signal_connect(main_window, "notify::fullscreened", G_CALLBACK(main_window_fullscreen_notify), main_window_view);

    status = GTK_WIDGET(gtk_builder_get_object(builder, "status"));

    emu_gl_area = GTK_WIDGET(gtk_builder_get_object(builder, "emu_gl_area"));
    g_signal_connect(emu_gl_area, "realize", G_CALLBACK(on_emu_realize), NULL);
    g_signal_connect(emu_gl_area, "unrealize", G_CALLBACK(on_emu_unrealize), NULL); // call stop_loop() on unrealize to avoid annoying warnings when app is quitting
    g_signal_connect(emu_gl_area, "render", G_CALLBACK(on_emu_render), NULL);

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
    preferences_window = GTK_WIDGET(gtk_builder_get_object(builder, "window"));
    g_signal_connect(preferences_window, "hide", G_CALLBACK(secondary_window_hide_cb), NULL);

    gtk_window_set_application(GTK_WINDOW(preferences_window), GTK_APPLICATION(app));
    gtk_window_set_transient_for(GTK_WINDOW(preferences_window), GTK_WINDOW(main_window));

    GtkWidget *widget = GTK_WIDGET(gtk_builder_get_object(builder, "pref_sound"));
    GtkAdjustment *sound_adjustment = gtk_adjustment_new(config.sound, 0.0, 1.0, 0.05, 0.25, 0.0);
    gtk_scale_set_format_value_func(GTK_SCALE(widget), sound_slider_format, NULL, NULL);
    gtk_range_set_adjustment(GTK_RANGE(widget), sound_adjustment);
    g_signal_connect(widget, "value-changed", G_CALLBACK(set_sound), NULL);

    widget = GTK_WIDGET(gtk_builder_get_object(builder, "pref_sound_drc"));
    adw_switch_row_set_active(ADW_SWITCH_ROW(widget), config.sound_drc);
    g_signal_connect(widget, "notify::active", G_CALLBACK(set_sound_drc), NULL);

    speed_slider_container = GTK_WIDGET(gtk_builder_get_object(builder, "pref_speed_container"));
    widget = GTK_WIDGET(gtk_builder_get_object(builder, "pref_speed"));
    GtkAdjustment *speed_adjustment = gtk_adjustment_new(config.speed, 1.0, 8.0, 0.5, 1, 0.0);
    gtk_scale_set_format_value_func(GTK_SCALE(widget), speed_slider_format, NULL, NULL);
    gtk_range_set_adjustment(GTK_RANGE(widget), speed_adjustment);
    g_signal_connect(widget, "value-changed", G_CALLBACK(set_speed), NULL);
    g_signal_connect(widget, "notify::selected", G_CALLBACK(set_speed), NULL);

    widget = GTK_WIDGET(gtk_builder_get_object(builder, "pref_palette"));
    g_signal_connect(widget, "notify::selected", G_CALLBACK(set_palette), NULL);
    adw_combo_row_set_selected(ADW_COMBO_ROW(widget), config.color_palette);

    mode_setter = GTK_WIDGET(gtk_builder_get_object(builder, "pref_mode"));
    g_signal_connect(mode_setter, "notify::selected", G_CALLBACK(set_mode), NULL);
    adw_combo_row_set_selected(ADW_COMBO_ROW(mode_setter), config.mode - 1);

    for (size_t i = 0; i < G_N_ELEMENTS(key_handlers); i++) {
        widget = GTK_WIDGET(gtk_builder_get_object(builder, key_handlers[i].name));
        g_signal_connect(widget, "activated", G_CALLBACK(key_setter_activated), (gpointer) &key_handlers[i].id);
        key_handlers[i].widget = GTK_WIDGET(gtk_builder_get_object(builder, key_handlers[i].label_name));
        gtk_label_set_label(GTK_LABEL(key_handlers[i].widget), gdk_keyval_name(config.keybindings[i]));
    }

    for (size_t i = 0; i < G_N_ELEMENTS(gamepad_handlers); i++) {
        widget = GTK_WIDGET(gtk_builder_get_object(builder, gamepad_handlers[i].name));
        g_signal_connect(widget, "activated", G_CALLBACK(gamepad_setter_activated), (gpointer) &gamepad_handlers[i].id);
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

    g_object_unref(builder);

    // Keybind dialog
    builder = gtk_builder_new_from_resource("/io/github/mpostaire/gbmulator/src/platform/desktop/ui/bind_setter.ui");
    keybind_dialog = GTK_WIDGET(gtk_builder_get_object(builder, "window"));
    gtk_window_set_transient_for(GTK_WINDOW(keybind_dialog), GTK_WINDOW(preferences_window));
    g_signal_connect(keybind_dialog, "hide", G_CALLBACK(keybind_dialog_hide_cb), NULL);
    bind_value = GTK_WIDGET(gtk_builder_get_object(builder, "bind_value"));
    joypad_name = GTK_WIDGET(gtk_builder_get_object(builder, "joypad_name"));

    g_object_unref(builder);

    // Restart dialog
    restart_dialog = adw_message_dialog_new(GTK_WINDOW(main_window), "Restart emulator?", NULL);
    adw_message_dialog_format_body(ADW_MESSAGE_DIALOG(restart_dialog), "This will restart the emulator and any unsaved progress will be lost.");
    adw_message_dialog_add_responses(ADW_MESSAGE_DIALOG(restart_dialog), "cancel", "Cancel", "restart", "Restart", NULL);
    adw_message_dialog_set_response_appearance(ADW_MESSAGE_DIALOG(restart_dialog), "restart", ADW_RESPONSE_DESTRUCTIVE);
    adw_message_dialog_set_default_response(ADW_MESSAGE_DIALOG(restart_dialog), "cancel");
    adw_message_dialog_set_close_response(ADW_MESSAGE_DIALOG(restart_dialog), "cancel");
    gtk_window_set_hide_on_close(GTK_WINDOW(restart_dialog), TRUE);
    g_signal_connect(restart_dialog, "hide", G_CALLBACK(secondary_window_hide_cb), NULL);
    g_signal_connect(restart_dialog, "response", G_CALLBACK(restart_emulator), NULL);

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

    // Printer quit dialog
    widget = adw_message_dialog_new(GTK_WINDOW(printer_window), "Disconnect printer?", NULL);
    g_signal_connect(widget, "response", G_CALLBACK(printer_quit_dialog_response_cb), NULL);
    adw_message_dialog_format_body(ADW_MESSAGE_DIALOG(widget), "This will disconnect the Game Boy Printer from the emulator.");
    adw_message_dialog_add_responses(ADW_MESSAGE_DIALOG(widget), "cancel", "Cancel", "disconnect", "Disconnect", NULL);
    adw_message_dialog_set_response_appearance(ADW_MESSAGE_DIALOG(widget), "disconnect", ADW_RESPONSE_DESTRUCTIVE);
    adw_message_dialog_set_default_response(ADW_MESSAGE_DIALOG(widget), "cancel");
    adw_message_dialog_set_close_response(ADW_MESSAGE_DIALOG(widget), "cancel");
    gtk_window_set_hide_on_close(GTK_WINDOW(widget), TRUE);

    g_signal_connect(printer_window, "close-request", G_CALLBACK(printer_window_close_request_cb), widget);

    g_object_unref(builder);

    // Mouse motion
    motion_event_controller = gtk_event_controller_motion_new();
    gtk_widget_add_controller(GTK_WIDGET(emu_gl_area), GTK_EVENT_CONTROLLER(motion_event_controller));

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
        if (argc > 2)
            forced_save_path = argv[2];
        if (load_cartridge(argv[1])) {
            gtk_widget_set_visible(status, FALSE);
            gtk_widget_set_visible(emu_gl_area, TRUE);
            gtk_widget_grab_focus(emu_gl_area);
        } else {
            show_toast("Invalid ROM");
        }
    }
    if (argv)
        g_strfreev(argv);

    gtk_window_present(GTK_WINDOW(main_window));
    gtk_widget_set_size_request(emu_gl_area, GB_SCREEN_WIDTH * 2, GB_SCREEN_HEIGHT * 2);
}

static void shutdown_cb(GtkApplication *app) {
    if (gb) {
        char *save_path = get_save_path(rom_path);
        save_battery_to_file(gb, forced_save_path ? forced_save_path : save_path);
        free(save_path);
        gb_quit(gb);
    }
    config_save_to_file(&config, config_path);
    free(config_path);

    if (emu_renderer)
        glrenderer_quit(emu_renderer);
    if (printer_renderer)
        glrenderer_quit(printer_renderer);
    alrenderer_quit();
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
        gamepad_connected_cb(NULL, device, NULL);

    alrenderer_init(0);
    gst_init(&argc, &argv);
    return g_application_run(G_APPLICATION(app), argc, argv);
}
