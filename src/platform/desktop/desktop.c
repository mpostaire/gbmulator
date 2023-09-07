#include "../../emulator/emulator.h"

#include <adwaita.h>
#include <libmanette.h>
#include <linux/input-event-codes.h>
#include <ctype.h>
#include <netdb.h>
#include <string.h>

#include "glrenderer.h"
#include "alrenderer.h"
#include "utils.h"
#include "link.h"
#include "../common/utils.h"
#include "../common/config.h"

#define APP_NAME EMULATOR_NAME
#define APP_ICON "gbmulator"
#define APP_VERSION "0.2"

#define CASE_THEN_STRING(x) case x: return #x
#define STRING(x) #x

static gboolean keycode_filter(guint keyval);
const char *gamepad_gamepad_button_parser(guint16 button);
int gamepad_button_name_parser(const char *button_name);

static gboolean loop(gpointer user_data);

// config struct initialized to defaults
config_t config = {
    .mode = CGB,
    .color_palette = PPU_COLOR_PALETTE_ORIG,
    .scale = 2,
    .sound = 1.0f,
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

setter_handler_t key_handlers[] = {
    { "key_setter_right", "key_setter_right_label", 0, NULL },
    { "key_setter_left", "key_setter_left_label", 1, NULL },
    { "key_setter_up", "key_setter_up_label", 2, NULL },
    { "key_setter_down", "key_setter_down_label", 3, NULL },
    { "key_setter_a", "key_setter_a_label", 4, NULL },
    { "key_setter_b", "key_setter_b_label", 5, NULL },
    { "key_setter_select", "key_setter_select_label", 6, NULL },
    { "key_setter_start", "key_setter_start_label", 7, NULL }
};

setter_handler_t gamepad_handlers[] = {
    { "gamepad_setter_right", "gamepad_setter_right_label", 0, NULL },
    { "gamepad_setter_left", "gamepad_setter_left_label", 1, NULL },
    { "gamepad_setter_up", "gamepad_setter_up_label", 2, NULL },
    { "gamepad_setter_down", "gamepad_setter_down_label", 3, NULL },
    { "gamepad_setter_a", "gamepad_setter_a_label", 4, NULL },
    { "gamepad_setter_b", "gamepad_setter_b_label", 5, NULL },
    { "gamepad_setter_select", "gamepad_setter_select_label", 6, NULL },
    { "gamepad_setter_start", "gamepad_setter_start_label", 7, NULL }
};

const char *joypad_names[] = {
    "Right:",
    "Left:",
    "Up:",
    "Down:",
    "A:",
    "B:",
    "Select:",
    "Start:"
};

gint argc;
gchar **argv = NULL;

int current_bind_setter;
int binding_setter_handler = -1;
int gamepad_state = GAMEPAD_DISABLED;

AdwApplication *app;
GtkWidget *main_window, *preferences_window, *window_title, *toast_overlay, *gl_area, *keybind_dialog, *bind_value, *mode_setter;
GtkWidget *joypad_name, *restart_dialog, *link_dialog, *status, *link_mode_setter_server, *link_host, *link_host_revealer;
GtkEventController *motion_event_controller;
glong motion_event_handler = 0;
GtkFileDialog *file_dialog;
guint loop_source;

GSocketService *socket_service = NULL;
gboolean is_connected = FALSE;

gboolean is_paused = TRUE, link_is_server = TRUE;
int steps_per_frame, sfd;
emulator_t *emu;
byte_t joypad_state = 0xFF;
double accel_x, accel_y;

char *rom_path, *forced_save_path, *config_path;

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

const char *gamepad_gamepad_button_parser(guint16 button) {
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

int gamepad_button_name_parser(const char *button_name) {
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
    loop_source = g_timeout_add_full(G_PRIORITY_HIGH, 1000 / 60, G_SOURCE_FUNC(loop), NULL, NULL);
    is_paused = FALSE;
}

void stop_loop(void) {
    g_source_remove(loop_source);
    is_paused = TRUE;
}

static void toggle_loop(void) {
    if (is_paused)
        start_loop();
    else
        stop_loop();
}

// TODO sdl - android link doesn't work
//      sld - adwaita link doesn't work
//      sdl - sdl link works
//      android - android link ????
//      adwaita - adwaita works if joypad exchange is done synchronously

static gboolean loop(gpointer user_data) {
    emulator_set_joypad_state(emu, joypad_state);

    // run the emulator for the approximate number of cycles it takes for the ppu to render a frame
    emulator_t *linked_emu = link_communicate();
    if (linked_emu)
        emulator_linked_run_frames(emu, linked_emu, 1);
    else
        emulator_run_steps(emu, steps_per_frame);

    gtk_gl_area_queue_render(GTK_GL_AREA(gl_area));

    return G_SOURCE_CONTINUE;
}

static void on_realize(GtkGLArea *area, gpointer user_data) {
    gtk_gl_area_make_current(area);
    if (gtk_gl_area_get_error(area) != NULL) {
        fprintf(stderr, "Unknown error\n");
        return;
    }

    glrenderer_init(160, 144, NULL);
}

static void on_unrealize(GtkGLArea *area, gpointer user_data) {
    if (!is_paused)
        stop_loop();
}

static gboolean on_render(GtkGLArea *area, GdkGLContext *context) {
    // inside this function it's safe to use GL; the given
    // GdkGLContext has been made current to the drawable
    // surface used by the `GtkGLArea` and the viewport has
    // already been set to be the size of the allocation

    glrenderer_render();

    // we completed our drawing; the draw commands will be
    // flushed at the end of the signal emission chain, and
    // the buffers will be drawn on the window
    return TRUE;
}

static void show_toast(const char *text) {
    AdwToast *toast = adw_toast_new(text);
    adw_toast_set_timeout(toast, 1);
    adw_toast_overlay_add_toast(ADW_TOAST_OVERLAY(toast_overlay), toast);
}

static void ppu_vblank_cb(const byte_t *pixels) {
    glrenderer_update_screen_texture(0, 0, GB_SCREEN_WIDTH, GB_SCREEN_HEIGHT, pixels);
}

static void set_accelerometer_data(double *x, double *y) {
    // TODO emulate this with numpad arrows
    *x = accel_x;
    *y = accel_y;
}

static byte_t *get_rom_data(const char *path, size_t *rom_size) {
    const char *dot = strrchr(path, '.');
    if (!dot || (strncmp(dot, ".gb", MAX(strlen(dot), sizeof(".gb"))) && strncmp(dot, ".gbc", MAX(strlen(dot), sizeof(".gbc"))))) {
        eprintf("%s: wrong file extension (expected .gb or .gbc)\n", path);
        return NULL;
    }

    FILE *f = fopen(path, "rb");
    if (!f) {
        errnoprintf("opening file %s", path);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    size_t len = ftell(f);
    fseek(f, 0, SEEK_SET);

    byte_t *buf = xmalloc(len);
    if (!fread(buf, len, 1, f)) {
        errnoprintf("reading %s", path);
        fclose(f);
        return NULL;
    }
    fclose(f);

    if (rom_size)
        *rom_size = len;
    return buf;
}

static void toggle_pause(GSimpleAction *action, GVariant *parameter, gpointer app) {
    show_toast(is_paused ? "Resumed" : "Paused");
    toggle_loop();
}

static void restart_emulator(AdwMessageDialog *self, gchar *response, gpointer user_data) {
    if (!strncmp(response, "restart", 8)) {
        // TODO what to do if there is an active link cable connection
        // if (linked_emu)
        //     on_link_disconnect();
        emulator_reset(emu, config.mode);
        emulator_print_status(emu);
    }
}

static void ask_restart_emulator(GSimpleAction *action, GVariant *parameter, gpointer app) {
    if (emu) {
        gamepad_state = GAMEPAD_DISABLED;
        gtk_window_present(GTK_WINDOW(restart_dialog));
    }
}

gboolean link_server_incoming(GSocketService *service, GSocketConnection *connection, GObject *source_object, gpointer user_data) {
    printf("Received Connection from client! Refusing new requests. %d %d %d\n", g_socket_service_is_active(service), g_socket_get_fd(g_socket_connection_get_socket(connection)), g_socket_connection_is_connected(connection));
    g_socket_service_stop(service); // TODO useless? this works stopping this signal but the print Hurray client still works and new file descriptor are still created...

    g_object_ref(connection); // don't close connection at the end of this handler
    link_setup_connection(connection, emu);
    show_toast("Link cable connected");
    is_connected = TRUE;

    return TRUE;
}

void link_client_connected(GObject *client, GAsyncResult *res, gpointer user_data) {
    GError *err = NULL;
    GSocketConnection *connection = g_socket_client_connect_finish(G_SOCKET_CLIENT(client), res, &err);

    // https://docs.gtk.org/gio/index.html
    // https://docs.gtk.org/gio/class.SocketService.html

    if (connection) {
        printf("Hurray! %d\n", g_socket_get_fd(g_socket_connection_get_socket(connection)));
        // g_object_ref(connection); // useless?
        link_setup_connection(connection, emu);
        is_connected = TRUE;
        show_toast("Link cable connected");
    } else {
        eprintf("%s\n", err->message);
        show_toast(err->message);
        g_error_free(err);
    }
}

static void start_link(void) {
    if (!emu) return;

    if (link_is_server) {
        struct addrinfo hints = {
            .ai_family = AF_UNSPEC,
            .ai_socktype = SOCK_STREAM,
            .ai_protocol = IPPROTO_TCP,
            .ai_flags = AI_PASSIVE // use my IP
        };
        struct addrinfo *res;
        int ret;
        if ((ret = getaddrinfo(NULL, config.link_port, &hints, &res)) != 0) {
            eprintf("getaddrinfo: %s\n", gai_strerror(ret));
            show_toast("Server connection setup error");
            return;
        }

        GSocketAddress *address = g_socket_address_new_from_native(res->ai_addr, res->ai_addrlen);
        if (!socket_service)
            socket_service = g_socket_service_new();
        g_signal_connect(socket_service, "incoming", G_CALLBACK(link_server_incoming), NULL);

        if (g_socket_listener_add_address(G_SOCKET_LISTENER(socket_service), address, G_SOCKET_TYPE_STREAM, G_SOCKET_PROTOCOL_TCP, NULL, NULL, NULL))
            show_toast("Link server listening...");
        else
            show_toast("Error listening to given address");
    } else {
        struct addrinfo hints = {
            .ai_family = AF_UNSPEC,
            .ai_socktype = SOCK_STREAM,
            .ai_protocol = IPPROTO_TCP
        };
        struct addrinfo *res;
        int ret;
        if ((ret = getaddrinfo(config.link_host, config.link_port, &hints, &res)) != 0) {
            eprintf("getaddrinfo: %s\n", gai_strerror(ret));
            show_toast("Client connection setup error");
            return;
        }

        GSocketAddress *address = g_socket_address_new_from_native(res->ai_addr, res->ai_addrlen);
        GSocketClient *client = g_socket_client_new();
        g_socket_client_connect_async(client, G_SOCKET_CONNECTABLE(address), NULL, link_client_connected, NULL);

        freeaddrinfo(res);
    }
}

static void link_dialog_response(GtkDialog *self, gint response_id, gpointer user_data) {
    gtk_window_close(GTK_WINDOW(self));
    if (response_id == GTK_RESPONSE_OK)
        start_link();
}

static void show_link_dialog(GSimpleAction *action, GVariant *parameter, gpointer app) {
    gamepad_state = GAMEPAD_DISABLED;
    gtk_window_present(GTK_WINDOW(link_dialog));
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
    if (!emu && !path) return 0;

    if (emu) {
        stop_loop();
        char *save_path = get_save_path(rom_path);
        save_battery_to_file(emu, forced_save_path ? forced_save_path : save_path);
        free(save_path);
    }

    size_t rom_size;
    byte_t *rom_data;
    if (path) {
        size_t len = strlen(path);
        rom_path = xrealloc(rom_path, len + 2);
        snprintf(rom_path, len + 1, "%s", path);

        rom_data = get_rom_data(rom_path, &rom_size);
        if (!rom_data) {
            free(rom_path);
            rom_path = NULL;
            return 0;
        }
    } else {
        byte_t *data = emulator_get_rom(emu, &rom_size);
        rom_data = xmalloc(rom_size);
        memcpy(rom_data, data, rom_size);
    }

    emulator_options_t opts = {
        .mode = config.mode,
        .on_apu_samples_ready = (on_apu_samples_ready_t) alrenderer_queue_audio,
        .on_new_frame = ppu_vblank_cb,
        .on_accelerometer_request = set_accelerometer_data,
        .apu_speed = config.speed,
        .apu_sound_level = config.sound,
        .apu_format = APU_FORMAT_U8,
        .palette = config.color_palette
    };
    emulator_t *new_emu = emulator_init(rom_data, rom_size, &opts);
    free(rom_data);
    if (!new_emu) return 0;

    char *save_path = get_save_path(rom_path);
    load_battery_from_file(new_emu, forced_save_path ? forced_save_path : save_path);
    free(save_path);

    if (emu) {
        // if (linked_emu)
        //     on_link_disconnect();
        emulator_quit(emu);
    } else {
        // init audio at the first successful call to load_cartridge to avoid opening audio device when there is no rom loaded
        alrenderer_init(GB_APU_SAMPLE_RATE);
    }
    emu = new_emu;
    emulator_print_status(emu);

    steps_per_frame = GB_CPU_STEPS_PER_FRAME * config.speed;

    const GActionEntry app_entries[] = {
        { "play_pause", toggle_pause, NULL, NULL, NULL },
        { "restart", ask_restart_emulator, NULL, NULL, NULL },
        { "link_cable", show_link_dialog, NULL, NULL, NULL },
    };
    g_action_map_add_action_entries(G_ACTION_MAP(app), app_entries, G_N_ELEMENTS(app_entries), app);

    adw_window_title_set_subtitle(ADW_WINDOW_TITLE(window_title), emulator_get_rom_title(emu));

    if (emulator_has_accelerometer(emu)) {
        motion_event_handler = g_signal_connect(motion_event_controller, "motion", G_CALLBACK(mouse_motion), NULL);
    } else if (motion_event_handler) {
        g_signal_handler_disconnect(motion_event_controller, motion_event_handler);
        motion_event_handler = 0;
    }

    gamepad_state = GAMEPAD_PLAYING;
    start_loop();

    return 1;
}

static void set_speed(GtkRange* self, gpointer user_data) {
    config.speed = gtk_range_get_value(self);
    steps_per_frame = GB_CPU_STEPS_PER_FRAME * config.speed;
    if (emu)
        emulator_set_apu_speed(emu, config.speed);
}

static void set_sound(GtkRange* self, gpointer user_data) {
    config.sound = gtk_range_get_value(self);
    if (emu)
        emulator_set_apu_sound_level(emu, config.sound);
}

static void set_palette(AdwComboRow *self, GParamSpec *pspec, gpointer user_data) {
    config.color_palette = adw_combo_row_get_selected(self);
    if (emu)
        emulator_set_palette(emu, config.color_palette);
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
        gtk_revealer_set_reveal_child(GTK_REVEALER(link_host_revealer), FALSE);
        link_is_server = TRUE;
    } else {
        gtk_revealer_set_reveal_child(GTK_REVEALER(link_host_revealer), TRUE);
        link_is_server = FALSE;
    }
}

static void set_mode(AdwComboRow *self, GParamSpec *pspec, gpointer user_data) {
    config.mode = adw_combo_row_get_selected(self) + 1;
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
                          "comments", "A Game Boy Color emulator with sound and link cable support over tcp.",
                          NULL);
}

void file_dialog_callback(GObject *dialog, GAsyncResult *res, gpointer user_data) {
    g_autoptr(GFile) file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(dialog), res, NULL);
    if (!file)
        return;

    char *file_path = g_file_get_path(file);
    if (load_cartridge(file_path)) {
        gtk_widget_set_visible(status, FALSE);
        gtk_widget_set_visible(gl_area, TRUE);
        gtk_widget_grab_focus(gl_area);
    } else {
        show_toast("Invalid ROM");
    }

    free(file_path);
}

static void open_btn_clicked(AdwActionRow *self, gpointer user_data) {
    if (!file_dialog) {
        file_dialog = gtk_file_dialog_new();
        gtk_file_dialog_set_title(file_dialog, "Pick a ROM file");

        g_autoptr(GtkFileFilter) filter = gtk_file_filter_new();
        // add basic extension pattern in case the mime type is not available
        gtk_file_filter_add_pattern(filter, "*.gb");
        gtk_file_filter_add_pattern(filter, "*.gbc");
        gtk_file_filter_add_mime_type(filter, "application/x-gameboy-rom");
        gtk_file_filter_add_mime_type(filter, "application/x-gameboy-color-rom");
        gtk_file_filter_set_name(filter, "GameBoy Color ROM");

        g_autoptr(GListStore) list = g_list_store_new(GTK_TYPE_FILE_FILTER);
        g_list_store_append(list, filter);

        gtk_file_dialog_set_filters(file_dialog, G_LIST_MODEL(list));
    }

    gtk_file_dialog_open(file_dialog, GTK_WINDOW(main_window), NULL, file_dialog_callback, NULL);
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
            show_link_dialog(NULL, NULL, NULL);
            break;
        }
        return TRUE;
    }

    if (!emu || is_paused) return FALSE;

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
        char *savestate_path = get_savestate_path(rom_path, keyval - GDK_KEY_F1);
        if (state & GDK_SHIFT_MASK) {
            save_state_to_file(emu, savestate_path, 1);
        } else {
            int ret = load_state_from_file(emu, savestate_path);
            if (ret > 0) {
                config.mode = ret;
                adw_combo_row_set_selected(ADW_COMBO_ROW(mode_setter), config.mode - 1);
            }
        }
        free(savestate_path);
        return TRUE;
    }

    // don't use emulator_joypad_press() here as we want to keep track of the joypad state and set it once per loop for link cable synchronization
    int joypad = keycode_to_joypad(&config, keyval);
    if (joypad < 0) return TRUE;
    RESET_BIT(joypad_state, joypad);
    return TRUE;
}

static gboolean key_released_main(GtkEventControllerKey *self, guint keyval, guint keycode, GdkModifierType state, gpointer user_data) {
    if (!emu || is_paused) return FALSE;

    // don't use emulator_joypad_release() here as we want to keep track of the joypad state and set it once per loop for link cable synchronization
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
    gtk_widget_grab_focus(gl_area);
}

static gboolean on_drop(GtkDropTarget *target, const GValue *value, double x, double y, gpointer data) {
    // GdkFileList is a boxed value so we use the boxed API.
    GdkFileList *file_list = g_value_get_boxed(value);
    GSList *list = gdk_file_list_get_files(file_list);
    GFile *file = list->data;

    if (load_cartridge(g_file_get_path(file))) {
        gtk_widget_set_visible(status, FALSE);
        gtk_widget_set_visible(gl_area, TRUE);
        gtk_widget_grab_focus(gl_area);
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
        if (!emu || is_paused) return;
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
        if (!emu || is_paused) return;
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

static void preferences_window_hide_cb(GtkWidget *self, gpointer user_data) {
    gamepad_state = (!emu || is_paused) ? GAMEPAD_DISABLED : GAMEPAD_PLAYING;
}

static void keybind_dialog_hide_cb(GtkWidget *self, gpointer user_data) {
    gamepad_state = GAMEPAD_DISABLED;
}

static void restart_dialog_hide_cb(GtkWidget *self, gpointer user_data) {
    gamepad_state = (!emu || is_paused) ? GAMEPAD_DISABLED : GAMEPAD_PLAYING;
}

static void link_dialog_hide_cb(GtkWidget *self, gpointer user_data) {
    gamepad_state = (!emu || is_paused) ? GAMEPAD_DISABLED : GAMEPAD_PLAYING;
}

static void activate_cb(GtkApplication *app) {
    // Load config
    config_path = get_config_path();
    config_load_from_file(&config, config_path);

    // Main window
    GtkBuilder *builder = gtk_builder_new_from_resource("/io/github/mpostaire/gbmulator/src/platform/desktop/ui/main.ui");
    main_window = GTK_WIDGET(gtk_builder_get_object(builder, "window"));

    status = GTK_WIDGET(gtk_builder_get_object(builder, "status"));

    gl_area = GTK_WIDGET(gtk_builder_get_object(builder, "gl_area"));
    g_signal_connect(gl_area, "realize", G_CALLBACK(on_realize), NULL);
    g_signal_connect(gl_area, "unrealize", G_CALLBACK(on_unrealize), NULL); // call stop_loop() on unrealize to avoid annoying warnings when app is quitting
    g_signal_connect(gl_area, "render", G_CALLBACK(on_render), NULL);

    GtkWidget *open_btn = GTK_WIDGET(gtk_builder_get_object(builder, "open_btn"));
    g_signal_connect(open_btn, "clicked", G_CALLBACK(open_btn_clicked), NULL);

    toast_overlay = GTK_WIDGET(gtk_builder_get_object(builder, "overlay"));

    GtkWidget *menu_btn = GTK_WIDGET(gtk_builder_get_object(builder, "menu_btn"));
    GtkPopover *popover = gtk_menu_button_get_popover(GTK_MENU_BUTTON(menu_btn));
    g_signal_connect(popover, "closed", G_CALLBACK(popover_closed), NULL);

    window_title = GTK_WIDGET(gtk_builder_get_object(builder, "window_title"));

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
    g_signal_connect(preferences_window, "hide", G_CALLBACK(preferences_window_hide_cb), NULL);

    gtk_window_set_application(GTK_WINDOW(preferences_window), GTK_APPLICATION(app));
    gtk_window_set_transient_for(GTK_WINDOW(preferences_window), GTK_WINDOW(main_window));

    GtkWidget *widget = GTK_WIDGET(gtk_builder_get_object(builder, "pref_sound"));
    GtkAdjustment *sound_adjustment = gtk_adjustment_new(config.sound, 0.0, 1.0, 0.05, 0.25, 0.0);
    gtk_scale_set_format_value_func(GTK_SCALE(widget), sound_slider_format, NULL, NULL);
    gtk_range_set_adjustment(GTK_RANGE(widget), sound_adjustment);
    g_signal_connect(widget, "value-changed", G_CALLBACK(set_sound), NULL);

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
    g_signal_connect(preferences_window, "hide", G_CALLBACK(restart_dialog_hide_cb), NULL);
    adw_message_dialog_format_body(ADW_MESSAGE_DIALOG(restart_dialog), "This will restart the emulator and any unsaved progress will be lost.");
    adw_message_dialog_add_responses(ADW_MESSAGE_DIALOG(restart_dialog), "cancel", "Cancel", "restart", "Restart", NULL);
    adw_message_dialog_set_response_appearance(ADW_MESSAGE_DIALOG(restart_dialog), "restart", ADW_RESPONSE_DESTRUCTIVE);
    adw_message_dialog_set_default_response(ADW_MESSAGE_DIALOG(restart_dialog), "cancel");
    adw_message_dialog_set_close_response(ADW_MESSAGE_DIALOG(restart_dialog), "cancel");
    gtk_window_set_hide_on_close(GTK_WINDOW(restart_dialog), TRUE);
    g_signal_connect(restart_dialog, "response", G_CALLBACK(restart_emulator), NULL);

    // Link dialog
    builder = gtk_builder_new_from_resource("/io/github/mpostaire/gbmulator/src/platform/desktop/ui/link.ui");
    g_signal_connect(preferences_window, "hide", G_CALLBACK(link_dialog_hide_cb), NULL);
    link_dialog = GTK_WIDGET(gtk_builder_get_object(builder, "window"));
    gtk_window_set_transient_for(GTK_WINDOW(link_dialog), GTK_WINDOW(main_window));
    g_signal_connect(link_dialog, "response", G_CALLBACK(link_dialog_response), NULL);

    link_host_revealer = GTK_WIDGET(gtk_builder_get_object(builder, "link_host_revealer"));

    link_mode_setter_server = GTK_WIDGET(gtk_builder_get_object(builder, "link_mode_setter_server"));
    g_signal_connect(GTK_TOGGLE_BUTTON(link_mode_setter_server), "toggled", G_CALLBACK(link_mode_setter_server_toggled), NULL);

    link_host = GTK_WIDGET(gtk_builder_get_object(builder, "link_host"));
    g_signal_connect(gtk_editable_get_delegate(GTK_EDITABLE(link_host)), "changed", G_CALLBACK(set_link_host), NULL);
	g_signal_connect(gtk_editable_get_delegate(GTK_EDITABLE(link_host)), "insert-text", G_CALLBACK(host_insert_text_handler), NULL);
    gtk_editable_set_text(GTK_EDITABLE(link_host), config.link_host);

    widget = GTK_WIDGET(gtk_builder_get_object(builder, "link_port"));
    GtkAdjustment *link_port_adjustment = gtk_adjustment_new(7777.0, 0.0, 65535.0, 1.0, 5.0, 0.0);
    gtk_spin_button_set_adjustment(GTK_SPIN_BUTTON(adw_action_row_get_activatable_widget(ADW_ACTION_ROW(widget))), link_port_adjustment);
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(adw_action_row_get_activatable_widget(ADW_ACTION_ROW(widget))), 0.0, 65535.0);
    gtk_spin_button_set_climb_rate(GTK_SPIN_BUTTON(adw_action_row_get_activatable_widget(ADW_ACTION_ROW(widget))), 1.0);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(adw_action_row_get_activatable_widget(ADW_ACTION_ROW(widget))), 0);
    g_signal_connect(adw_action_row_get_activatable_widget(ADW_ACTION_ROW(widget)), "value-changed", G_CALLBACK(set_link_port), NULL);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(adw_action_row_get_activatable_widget(ADW_ACTION_ROW(widget))), (gdouble) atoi(config.link_port));

    g_object_unref(builder);

    // Mouse motion
    motion_event_controller = gtk_event_controller_motion_new();
    gtk_widget_add_controller(GTK_WIDGET(gl_area), GTK_EVENT_CONTROLLER(motion_event_controller));

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

    // Parse command line arguments
    if (argc > 1) {
        if (argc > 2)
            forced_save_path = argv[2];
        if (load_cartridge(argv[1])) {
            gtk_widget_set_visible(status, FALSE);
            gtk_widget_set_visible(gl_area, TRUE);
            gtk_widget_grab_focus(gl_area);
        } else {
            show_toast("Invalid ROM");
        }
    }
    if (argv)
        g_strfreev(argv);

    // show main_window
    gtk_window_present(GTK_WINDOW(main_window));
    // set window min size to scale == 2x
    gtk_widget_set_size_request(gl_area, GB_SCREEN_WIDTH * 2, GB_SCREEN_HEIGHT * 2);
}

static void shutdown_cb(GtkApplication *app) {
    if (emu) {
        char *save_path = get_save_path(rom_path);
        save_battery_to_file(emu, forced_save_path ? forced_save_path : save_path);
        free(save_path);
    }
    config_save_to_file(&config, config_path);
    free(config_path);
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

    g_signal_connect_object(G_OBJECT(monitor), "device-connected", (GCallback) gamepad_connected_cb, NULL, 0);

    ManetteDevice *device;
    while (manette_monitor_iter_next(iter, &device))
        gamepad_connected_cb(NULL, device, NULL);

    return g_application_run(G_APPLICATION(app), argc, argv);
}
