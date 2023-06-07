#include <gio/gio.h>
#include "../../emulator/emulator.h"
#include "../../emulator/emulator_priv.h"

typedef enum {
    PKT_INFO,
    PKT_ROM,
    PKT_STATE,
    PKT_JOYPAD
} pkt_type_t;

typedef enum {
    EXCHANGE_INFO = 0,
    EXCHANGE_INFO_DONE = 2,
    EXCHANGE_ROM = 3,
    EXCHANGE_ROM_DONE = 5,
    EXCHANGE_STATE = 6,
    RECEIVE_STATE_PAYLOAD = 8,
    EXCHANGE_STATE_DONE = 10,
    EXCHANGE_JOYPAD = 11,
    EXCHANGE_JOYPAD_DONE = 13,
} transfer_state_t;

GSocketConnection *connection = NULL;
GOutputStream *output_stream = NULL;
GInputStream *input_stream = NULL;

transfer_state_t link_state = EXCHANGE_INFO;
byte_t *in_pkt = NULL;
byte_t *out_pkt = NULL;

gboolean write_done = FALSE;
gboolean read_done = FALSE;

word_t checksum = 0;
emulator_mode_t emu_mode = DMG;
gboolean can_compress = FALSE;
byte_t *savestate_data = NULL;
size_t savestate_len = 0;
emulator_t *linked_emu = NULL;

static void write_cb(GObject *source, GAsyncResult *res, gpointer data) {
    GError *error = NULL;

    gssize written = g_output_stream_write_finish(G_OUTPUT_STREAM(source), res, &error);
    if (written == -1) {
        g_error("LINK WRITE ERROR: %s\n", error->message);
        // TODO handle error
        return;
    }

    write_done = TRUE;
    if (read_done) {
        write_done = FALSE;
        read_done = FALSE;
        link_state++;
    }
}

static void read_cb(GObject *source, GAsyncResult *res, gpointer data) {
    GError *error = NULL;

    gssize read = g_input_stream_read_finish(G_INPUT_STREAM(source), res, &error);
    if (read == -1) {
        g_error("LINK READ ERROR: %s\n", error->message);
        // TODO handle error
        return;
    }

    if ((size_t) read < (size_t) data) {
        size_t diff = (size_t) data - read;
        g_input_stream_read_async(input_stream, &in_pkt[read], diff, G_PRIORITY_DEFAULT, NULL, read_cb, (gpointer) diff);
        return;
    }

    read_done = TRUE;
    if (write_done) {
        write_done = FALSE;
        read_done = FALSE;
        link_state++;
    }
}

static void exchange_info(emulator_t *emu) {
    out_pkt = xrealloc(out_pkt, 4);
    memset(out_pkt, 0, 4);
    out_pkt[0] = PKT_INFO;
    out_pkt[1] = emulator_get_mode(emu);
    #ifdef __HAVE_ZLIB__
    // SET_BIT(out_pkt[1], 7); // TODO fix compression
    #endif

    checksum = emulator_get_cartridge_checksum(emu);
    memcpy(&out_pkt[2], &checksum, 2);

    g_output_stream_write_async(output_stream, out_pkt, 4, G_PRIORITY_DEFAULT, NULL, write_cb, NULL);

    in_pkt = xrealloc(in_pkt, 4);
    memset(in_pkt, 0, 4);
    g_input_stream_read_async(input_stream, in_pkt, 4, G_PRIORITY_DEFAULT, NULL, read_cb, (gpointer) 4);

    link_state++;
}

static gboolean exchange_info_done(void) {
    if (in_pkt[0] != PKT_INFO) {
        eprintf("received packet type %d but expected %d (ignored)\n", in_pkt[0], PKT_INFO);
        return FALSE; // TODO should return TRUE to resume emulation BUT need to handle this error first...
    }

    emu_mode = in_pkt[1] & 0x03;
    #ifdef __HAVE_ZLIB__
    can_compress = GET_BIT(in_pkt[1], 7);
    #endif

    word_t received_checksum = 0;
    memcpy(&received_checksum, &in_pkt[2], 2);
    if (received_checksum == checksum) {
        link_state = EXCHANGE_STATE;
    } else {
        eprintf("checksum mismatch ('%x' != '%x'): exchanging ROMs\n", checksum, received_checksum);
        link_state = EXCHANGE_ROM;
    }

    return TRUE;
}

static void exchange_state(emulator_t *emu) {
    savestate_data = emulator_get_savestate(emu, &savestate_len, can_compress);

    out_pkt = xrealloc(out_pkt, savestate_len + 9);
    memset(out_pkt, 0, savestate_len + 9);
    out_pkt[0] = PKT_STATE;
    memcpy(&out_pkt[1], &savestate_len, sizeof(size_t));
    memcpy(&out_pkt[9], savestate_data, savestate_len);

    g_output_stream_write_async(output_stream, out_pkt, savestate_len + 9, G_PRIORITY_DEFAULT, NULL, write_cb, NULL);

    // receive PKT_STATE header
    in_pkt = xrealloc(in_pkt, 9);
    memset(in_pkt, 0, 9);
    g_input_stream_read_async(input_stream, in_pkt, 9, G_PRIORITY_DEFAULT, NULL, read_cb, (gpointer) 9);

    link_state++;
}

static gboolean receive_state_payload(void) {
    free(savestate_data);

    // receive PKT_STATE payload
    if (in_pkt[0] != PKT_STATE) {
        eprintf("received packet type %d but expected %d (ignored)\n", in_pkt[0], PKT_STATE);
        return FALSE;
    }

    memcpy(&savestate_len, &in_pkt[1], sizeof(size_t));

    in_pkt = xrealloc(in_pkt, savestate_len);
    memset(in_pkt, 0, savestate_len);

    write_done = TRUE;
    g_input_stream_read_async(input_stream, in_pkt, savestate_len, G_PRIORITY_DEFAULT, NULL, read_cb, (gpointer) savestate_len);

    link_state++;

    return TRUE;
}

static gboolean exchange_state_done(emulator_t *emu) {
    emulator_options_t opts = { .mode = emu_mode };

    // if (rom_data) { // TODO
    //     linked_emu = emulator_init(rom_data, rom_len, &opts);
    //     if (!linked_emu) {
    //         eprintf("received invalid or corrupted PKT_ROM\n");
    //         free(rom_data);
    //         return FALSE;
    //     }
    //     free(rom_data);
    // } else {
        size_t rom_len;
        byte_t *rom_data = emulator_get_rom(emu, &rom_len);
        linked_emu = emulator_init(rom_data, rom_len, &opts);
    // }

    // FIXME this function fails (received invalid or corrupted savestate)
    if (!emulator_load_savestate(linked_emu, in_pkt, savestate_len)) {
        eprintf("received invalid or corrupted savestate\n");
        return FALSE;
    }

    emulator_link_connect(emu, linked_emu);

    // allocate joypad pkts once here so we don't have to do it at every joypad exchange.
    out_pkt = xrealloc(out_pkt, 2);
    memset(out_pkt, 0, 2);
    in_pkt = xrealloc(in_pkt, 2);
    memset(in_pkt, 0, 2);

    link_state++;

    return TRUE;
}

// TODO make it work asynchronously OR implement the separate emulation thread, making this obsolete
static void exchange_joypad(byte_t joypad_state, emulator_t *emu) {
    out_pkt[0] = PKT_JOYPAD;
    out_pkt[1] = joypad_state;

    // g_output_stream_write_async(output_stream, out_pkt, 2, G_PRIORITY_DEFAULT, NULL, write_cb, NULL);

    // g_input_stream_read_async(input_stream, in_pkt, 2, G_PRIORITY_DEFAULT, NULL, read_cb, (gpointer) 2);

    // link_state++;

    g_output_stream_write(output_stream, out_pkt, 2, NULL, NULL);

    gssize read = 0;
    gssize target = 2;
    do {
        target -= read;
        read = g_input_stream_read(input_stream, &in_pkt[2 - target], target, NULL, NULL);
    } while (target > 0);

    emulator_set_joypad_state(linked_emu, in_pkt[1]);
}

// static gboolean exchange_joypad_done(void) {
//     if (in_pkt[0] != PKT_JOYPAD) {
//         eprintf("received packet type %d but expected %d (ignored)\n", in_pkt[0], PKT_JOYPAD);
//         return FALSE;
//     }

//     emulator_set_joypad_state(linked_emu, in_pkt[1]);

//     link_state = EXCHANGE_JOYPAD;

//     return TRUE;
// }

emulator_t *link_cable(emulator_t *emu, byte_t joypad_state) {
    // TODO allow fallthrough in case we have the results directly (don't wait for another 16ms loop)
    //  ---> don't really know how to do this... because there are callbacks involved...
    // ---> maybe don't allow fallthroug but in the callbacks, allow logic to continue
    //      this will complicate the control flow so maybe don't do this and let the 16ms lag...
    //      for INFO, ROM and STATE states this is ok but the joypad exchange needs to have maximum
    //      reduced lag.... Maybe only do this for joypad then...

    switch (link_state) {
    case EXCHANGE_INFO:
        exchange_info(emu);
        return NULL;
    case EXCHANGE_INFO_DONE:
        if (!exchange_info_done()) {
            eprintf("ERROR EXCHANGE_INFO_DONE\n"); // TODO
            exit(42);
        }
        return NULL;
    case EXCHANGE_ROM:
        puts("TODO EXCHANGE_ROM"); // TODO
        return NULL;
    case EXCHANGE_ROM_DONE:
        puts("TODO EXCHANGE_ROM_DONE"); // TODO
        return NULL;
    case EXCHANGE_STATE:
        exchange_state(emu);
        return NULL;
    case RECEIVE_STATE_PAYLOAD:
        receive_state_payload();
        return NULL;
    case EXCHANGE_STATE_DONE:
        if (!exchange_state_done(emu)) {
            eprintf("ERROR EXCHANGE_STATE_DONE\n");
            exit(42);
        }
        return NULL;
    case EXCHANGE_JOYPAD:
        exchange_joypad(joypad_state, emu);
        // return NULL;
        return linked_emu;
    case EXCHANGE_JOYPAD_DONE:
        // if (!exchange_joypad_done()) {
        //     eprintf("ERROR EXCHANGE_JOYPAD_DONE");
        //     exit(42);
        // }
        // return linked_emu;
        return NULL;
    default:
        eprintf("ERROR UNDEFINED LINK BEHAVIOR %d\n", link_state);
        return NULL;
    }
}

void link_setup_connection(GSocketConnection *conn) {
    if (connection)
        g_io_stream_close(G_IO_STREAM(conn), NULL, NULL);
    connection = conn;

    // TODO reset everything if a connection was previously established

    output_stream = g_io_stream_get_output_stream(G_IO_STREAM(conn));
    input_stream = g_io_stream_get_input_stream(G_IO_STREAM(conn));
}
