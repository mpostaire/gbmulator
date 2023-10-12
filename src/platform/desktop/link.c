#include <gio/gio.h>

#include "../../core/gb.h"
#include "desktop.h"

typedef enum {
    PKT_INFO,
    PKT_ROM,
    PKT_STATE,
    PKT_JOYPAD
} pkt_type_t;

static GSocketConnection *connection = NULL;
static GOutputStream *output_stream = NULL;
static GInputStream *input_stream = NULL;

static byte_t *in_pkt = NULL;
static byte_t *out_pkt = NULL;

static word_t checksum = 0;
static gb_mode_t emu_mode = GB_MODE_DMG;
static gboolean is_cable_link = FALSE;
static gboolean is_ir_link = FALSE;
static gboolean can_compress = FALSE;
static byte_t *savestate_data = NULL;
static size_t savestate_len = 0;
static gb_t *local_gb = NULL;
static gb_t *linked_gb = NULL;

static gboolean write_done = FALSE;
static gboolean read_done = FALSE;
typedef void (*cb_t)(void);
static struct read_cb_arg {
    gssize pkt_size;
    gssize read;
    cb_t cb;
} read_arg;

static void write_cb(GObject *source, GAsyncResult *res, gpointer data) {
    GError *err = NULL;

    gssize written = g_output_stream_write_finish(G_OUTPUT_STREAM(source), res, &err);
    if (written == -1) {
        eprintf("%s\n", err->message);
        g_error_free(err);
        return; // TODO handle error
    }

    write_done = TRUE;
    if (read_done) {
        write_done = FALSE;
        read_done = FALSE;
        ((cb_t) data)();
    }
}

static void read_cb(GObject *source, GAsyncResult *res, gpointer data) {
    GError *err = NULL;

    gssize read = g_input_stream_read_finish(G_INPUT_STREAM(source), res, &err);
    if (read == -1) {
        eprintf("%s\n", err->message);
        g_error_free(err);
        return; // TODO handle error
    }

    gssize pkt_size = ((struct read_cb_arg *) data)->pkt_size;
    ((struct read_cb_arg *) data)->read += read;
    gssize read_so_far = ((struct read_cb_arg *) data)->read;
    if (read_so_far < pkt_size) {
        gssize diff = pkt_size - read_so_far;
        g_input_stream_read_async(input_stream, &in_pkt[read_so_far], diff, G_PRIORITY_HIGH - 1, NULL, read_cb, data);
        return;
    }

    read_done = TRUE;
    if (write_done) {
        write_done = FALSE;
        read_done = FALSE;
        ((struct read_cb_arg *) data)->cb();
    }
}

static void exchange_state_done(void) {
    gb_options_t opts = { .mode = emu_mode };

    // if (rom) { // TODO
    //     linked_gb = gb_init(rom, rom_len, &opts);
    //     if (!linked_gb) {
    //         eprintf("received invalid or corrupted PKT_ROM\n");
    //         free(rom);
    //         return FALSE;
    //     }
    //     free(rom);
    // } else {
        size_t rom_len;
        byte_t *rom = gb_get_rom(local_gb, &rom_len);
        linked_gb = gb_init(rom, rom_len, &opts);
    // }

    if (!gb_load_savestate(linked_gb, in_pkt, savestate_len)) {
        eprintf("received invalid or corrupted savestate\n");
        return; // TODO cancel exchange on error
    }

    if (is_cable_link)
        gb_link_connect_gb(local_gb, linked_gb);
    if (is_ir_link)
        gb_ir_connect(local_gb, linked_gb);

    // allocate joypad pkts once here so we don't have to do it at every joypad exchange.
    out_pkt = xrealloc(out_pkt, 2);
    memset(out_pkt, 0, 2);
    in_pkt = xrealloc(in_pkt, 2);
    memset(in_pkt, 0, 2);

    start_loop();
}

static void receive_state_payload(void) {
    free(savestate_data);

    // receive PKT_STATE payload
    if (in_pkt[0] != PKT_STATE) {
        eprintf("received packet type %d but expected %d (ignored)\n", in_pkt[0], PKT_STATE);
        return; // TODO cancel exchange on error
    }

    memcpy(&savestate_len, &in_pkt[1], sizeof(size_t));

    in_pkt = xrealloc(in_pkt, savestate_len);
    memset(in_pkt, 0, savestate_len);

    read_arg = (struct read_cb_arg) {
        .pkt_size = savestate_len,
        .read = 0,
        .cb = exchange_state_done
    };

    write_done = TRUE;
    g_input_stream_read_async(input_stream, in_pkt, savestate_len, G_PRIORITY_HIGH - 1, NULL, read_cb, &read_arg);
}

static void exchange_state(void) {
    savestate_data = gb_get_savestate(local_gb, &savestate_len, can_compress);

    out_pkt = xrealloc(out_pkt, savestate_len + 9);
    memset(out_pkt, 0, savestate_len + 9);
    out_pkt[0] = PKT_STATE;
    memcpy(&out_pkt[1], &savestate_len, sizeof(size_t));
    memcpy(&out_pkt[9], savestate_data, savestate_len);

    read_arg = (struct read_cb_arg) {
        .pkt_size = 9,
        .read = 0,
        .cb = receive_state_payload
    };

    g_output_stream_write_async(output_stream, out_pkt, savestate_len + 9, G_PRIORITY_HIGH - 1, NULL, write_cb, read_arg.cb);

    // receive PKT_STATE header
    in_pkt = xrealloc(in_pkt, 9);
    memset(in_pkt, 0, 9);
    g_input_stream_read_async(input_stream, in_pkt, 9, G_PRIORITY_HIGH - 1, NULL, read_cb, &read_arg);
}

static void exchange_info_done(void) {
    if (in_pkt[0] != PKT_INFO) {
        eprintf("received packet type %d but expected %d (ignored)\n", in_pkt[0], PKT_INFO);
        return; // TODO cancel exchange on error
    }

    emu_mode = CHECK_BIT(out_pkt[1], 0) ? GB_MODE_CGB : GB_MODE_DMG;
    is_cable_link = CHECK_BIT(out_pkt[1], 1); // cable-link
    is_ir_link = CHECK_BIT(out_pkt[1], 2); // ir-link
    #ifdef __HAVE_ZLIB__
    can_compress = GET_BIT(in_pkt[1], 7);
    #endif

    word_t received_checksum = 0;
    memcpy(&received_checksum, &in_pkt[2], 2);
    if (received_checksum == checksum) {
        exchange_state();
    } else {
        eprintf("checksum mismatch ('%x' != '%x'): exchanging ROMs\n", checksum, received_checksum);
        // exchange_rom(); // TODO
    }
}

static void exchange_info(void) {
    out_pkt = xrealloc(out_pkt, 4);
    memset(out_pkt, 0, 4);
    out_pkt[0] = PKT_INFO;
    out_pkt[1] = gb_is_cgb(local_gb);
    SET_BIT(out_pkt[1], 1); // cable-link
    SET_BIT(out_pkt[1], 2); // ir-link
    #ifdef __HAVE_ZLIB__
    SET_BIT(out_pkt[1], 7);
    #endif

    checksum = gb_get_cartridge_checksum(local_gb);
    memcpy(&out_pkt[2], &checksum, 2);

    read_arg = (struct read_cb_arg) {
        .pkt_size = 4,
        .read = 0,
        .cb = exchange_info_done
    };

    g_output_stream_write_async(output_stream, out_pkt, 4, G_PRIORITY_HIGH - 1, NULL, write_cb, read_arg.cb);

    in_pkt = xrealloc(in_pkt, 4);
    memset(in_pkt, 0, 4);
    g_input_stream_read_async(input_stream, in_pkt, 4, G_PRIORITY_HIGH - 1, NULL, read_cb, &read_arg);
}

static void exchange_joypad_done(void) {
    if (in_pkt[0] != PKT_JOYPAD) {
        eprintf("received packet type %d but expected %d (ignored)\n", in_pkt[0], PKT_JOYPAD);
        return; // TODO cancel exchange on error
    }

    gb_set_joypad_state(linked_gb, in_pkt[1]);
}

// TODO make it work asynchronously OR implement the separate emulation thread
//      --> making this asynchronous while keeping both emulators in sync is very difficult and
//          I didn't manage to find a way.
gb_t *link_communicate(void) {
    if (!linked_gb || !local_gb)
        return NULL;

    out_pkt[0] = PKT_JOYPAD;
    out_pkt[1] = gb_get_joypad_state(local_gb);

    read_arg = (struct read_cb_arg) {
        .pkt_size = 2,
        .read = 0,
        .cb = exchange_joypad_done
    };

    write_done = FALSE;
    read_done = FALSE;

    in_pkt[0] = 0;
    in_pkt[1] = 0;

    // TODO handle error/disconnect
    g_output_stream_write(output_stream, out_pkt, 2, NULL, NULL);

    g_input_stream_read(input_stream, in_pkt, 2, NULL, NULL);

    exchange_joypad_done();

    return linked_gb;
}

void link_setup_connection(GSocketConnection *conn, gb_t *gb) {
    if (!gb || !conn)
        return;

    stop_loop();

    if (connection)
        g_io_stream_close(G_IO_STREAM(conn), NULL, NULL);
    connection = conn;
    local_gb = gb;

    // TODO reset everything if a connection was previously established

    output_stream = g_io_stream_get_output_stream(G_IO_STREAM(conn));
    input_stream = g_io_stream_get_input_stream(G_IO_STREAM(conn));

    exchange_info();
}
