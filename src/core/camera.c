#include "gb_priv.h"

// TODO serialize

// --------------------------------------------------------------------
// Taken from https://gbdev.io/pandocs/Gameboy_Camera.html#sample-code-for-emulators

// The actual sensor image is 128x126 or so.
#define GBCAM_SENSOR_EXTRA_LINES (8)
#define GBCAM_SENSOR_W (128)
#define GBCAM_SENSOR_H (112 + GBCAM_SENSOR_EXTRA_LINES)

#define GBCAM_W (128)
#define GBCAM_H (112)

#define BIT(n) (1 << (n))

// Webcam image
static int webcam_image[GBCAM_SENSOR_W][GBCAM_SENSOR_H]; // TODO add these into the gb_t struct or one of its children
// Image processed by sensor chip
static int gb_camera_image[GBCAM_SENSOR_W][GBCAM_SENSOR_H]; // TODO add these into the gb_t struct or one of its children

/**
 * fast random number generator: https://stackoverflow.com/a/3747462
 * we only want to show some good enough noise so we don't care about rand_seed conficts due to mutiple instance of gb_t
 */
static unsigned int rand_seed = 1;
static inline unsigned int fastrand(void) {
    rand_seed = (214013 * rand_seed + 2531011);
    return (rand_seed >> 16) & 0xFF;
}

static inline uint32_t gb_cam_matrix_process(byte_t *regs, uint32_t value, uint32_t x, uint32_t y) {
    x = x & 3;
    y = y & 3;

    int base = 6 + (y * 4 + x) * 3;

    uint32_t r0 = regs[base + 0];
    uint32_t r1 = regs[base + 1];
    uint32_t r2 = regs[base + 2];

    if (value < r0)
        return 0x00;
    else if (value < r1)
        return 0x40;
    else if (value < r2)
        return 0x80;
    return 0xC0;
}

static void camera_capture_image(gb_t *gb) {
    byte_t *regs = gb->mmu->mbc.camera.regs;

    //------------------------------------------------

    // Get webcam image
    // ----------------

    if (gb->on_camera_capture_image) {
        gb->on_camera_capture_image((int *) webcam_image, GBCAM_SENSOR_W, GBCAM_SENSOR_H);
    } else { // no camera callback, generate noise
        for (int i = 0; i < GBCAM_SENSOR_W; i++)
            for (int j = 0; j < GBCAM_SENSOR_H; j++)
                webcam_image[i][j] = fastrand();
    }

    //------------------------------------------------

    // Get configuration
    // -----------------

    // Register 0
    uint32_t P_bits = 0;
    uint32_t M_bits = 0;

    switch ((regs[0] >> 1) & 3) {
    case 0:
        P_bits = 0x00;
        M_bits = 0x01;
        break;
    case 1:
        P_bits = 0x01;
        M_bits = 0x00;
        break;
    case 2:
    case 3:
        P_bits = 0x01;
        M_bits = 0x02;
        break;
    default:
        break;
    }

    // Register 1
    uint32_t N_bit = (regs[1] & BIT(7)) >> 7;
    uint32_t VH_bits = (regs[1] & (BIT(6) | BIT(5))) >> 5;

    // Registers 2 and 3
    uint32_t EXPOSURE_bits = regs[3] | (regs[2] << 8);

    // Register 4
    const float edge_ratio_lut[8] = {0.50, 0.75, 1.00, 1.25, 2.00, 3.00, 4.00, 5.00};

    float EDGE_alpha = edge_ratio_lut[(regs[4] & 0x70) >> 4];

    uint32_t E3_bit = (regs[4] & BIT(7)) >> 7;
    uint32_t I_bit = (regs[4] & BIT(3)) >> 3;

    //------------------------------------------------

    // Calculate timings
    // -----------------

    gb->mmu->mbc.camera.cam_cycles_left = 4 * (32446 + (N_bit ? 0 : 512) + 16 * EXPOSURE_bits);

    //------------------------------------------------

    // Sensor handling
    // ---------------

    // Copy webcam buffer to sensor buffer applying color correction and exposure time
    for (int i = 0; i < GBCAM_SENSOR_W; i++) {
        for (int j = 0; j < GBCAM_SENSOR_H; j++) {
            int value = webcam_image[i][j];
            value = ((value * EXPOSURE_bits) / 0x0300); // 0x0300 could be other values
            value = 128 + (((value - 128) * 1) / 8);    // "adapt" to "3.1"/5.0 V
            gb_camera_image[i][j] = CLAMP(value, 0, 255);
        }
    }

    if (I_bit) // Invert image
        for (int i = 0; i < GBCAM_SENSOR_W; i++)
            for (int j = 0; j < GBCAM_SENSOR_H; j++)
                gb_camera_image[i][j] = 255 - gb_camera_image[i][j];

    // Make signed
    for (int i = 0; i < GBCAM_SENSOR_W; i++)
        for (int j = 0; j < GBCAM_SENSOR_H; j++)
            gb_camera_image[i][j] = gb_camera_image[i][j] - 128;

    int temp_buf[GBCAM_SENSOR_W][GBCAM_SENSOR_H];

    uint32_t filtering_mode = (N_bit << 3) | (VH_bits << 1) | E3_bit;
    switch (filtering_mode) {
    case 0x0: // 1-D filtering
        for (int i = 0; i < GBCAM_SENSOR_W; i++)
            for (int j = 0; j < GBCAM_SENSOR_H; j++)
                temp_buf[i][j] = gb_camera_image[i][j];

        for (int i = 0; i < GBCAM_SENSOR_W; i++) {
            for (int j = 0; j < GBCAM_SENSOR_H; j++) {
                int ms = temp_buf[i][MIN(j + 1, GBCAM_SENSOR_H - 1)];
                int px = temp_buf[i][j];

                int value = 0;
                if (P_bits & BIT(0))
                    value += px;
                if (P_bits & BIT(1))
                    value += ms;
                if (M_bits & BIT(0))
                    value -= px;
                if (M_bits & BIT(1))
                    value -= ms;
                gb_camera_image[i][j] = CLAMP(value, -128, 127);
            }
        }
        break;
    case 0x2: // 1-D filtering + Horiz. enhancement : P + {2P-(MW+ME)} * alpha
        for (int i = 0; i < GBCAM_SENSOR_W; i++) {
            for (int j = 0; j < GBCAM_SENSOR_H; j++) {
                int mw = gb_camera_image[MAX(0, i - 1)][j];
                int me = gb_camera_image[MIN(i + 1, GBCAM_SENSOR_W - 1)][j];
                int px = gb_camera_image[i][j];

                temp_buf[i][j] = CLAMP(px + ((2 * px - mw - me) * EDGE_alpha), 0, 255);
            }
        }

        for (int i = 0; i < GBCAM_SENSOR_W; i++) {
            for (int j = 0; j < GBCAM_SENSOR_H; j++) {
                int ms = temp_buf[i][MIN(j + 1, GBCAM_SENSOR_H - 1)];
                int px = temp_buf[i][j];

                int value = 0;
                if (P_bits & BIT(0))
                    value += px;
                if (P_bits & BIT(1))
                    value += ms;
                if (M_bits & BIT(0))
                    value -= px;
                if (M_bits & BIT(1))
                    value -= ms;
                gb_camera_image[i][j] = CLAMP(value, -128, 127);
            }
        }
        break;
    case 0xE: // 2D enhancement : P + {4P-(MN+MS+ME+MW)} * alpha
        for (int i = 0; i < GBCAM_SENSOR_W; i++) {
            for (int j = 0; j < GBCAM_SENSOR_H; j++) {
                int ms = gb_camera_image[i][MIN(j + 1, GBCAM_SENSOR_H - 1)];
                int mn = gb_camera_image[i][MAX(0, j - 1)];
                int mw = gb_camera_image[MAX(0, i - 1)][j];
                int me = gb_camera_image[MIN(i + 1, GBCAM_SENSOR_W - 1)][j];
                int px = gb_camera_image[i][j];

                temp_buf[i][j] = CLAMP(px + ((4 * px - mw - me - mn - ms) * EDGE_alpha), -128, 127);
            }
        }

        for (int i = 0; i < GBCAM_SENSOR_W; i++)
            for (int j = 0; j < GBCAM_SENSOR_H; j++)
                gb_camera_image[i][j] = temp_buf[i][j];
        break;
    case 0x1:
        // In my GB Camera cartridge this is always the same color. The datasheet of the
        // sensor doesn't have this configuration documented. Maybe this is a bug?
        for (int i = 0; i < GBCAM_SENSOR_W; i++)
            for (int j = 0; j < GBCAM_SENSOR_H; j++)
                gb_camera_image[i][j] = 0;
        break;
    default:
        // Ignore filtering
        eprintf("Unsupported GB Cam mode: 0x%X\n%02X %02X %02X %02X %02X %02X", filtering_mode, regs[0], regs[1], regs[2], regs[3], regs[4], regs[5]);
        break;
    }

    // Make unsigned
    for (int i = 0; i < GBCAM_SENSOR_W; i++) {
        for (int j = 0; j < GBCAM_SENSOR_H; j++) {
            gb_camera_image[i][j] = gb_camera_image[i][j] + 128;
        }
    }

    //------------------------------------------------

    // Controller handling
    // -------------------

    int fourcolorsbuffer[GBCAM_W][GBCAM_H]; // buffer after controller matrix

    // Convert to Game Boy colors using the controller matrix
    for (int i = 0; i < GBCAM_W; i++)
        for (int j = 0; j < GBCAM_H; j++)
            fourcolorsbuffer[i][j] = gb_cam_matrix_process(regs, gb_camera_image[i][j + (GBCAM_SENSOR_EXTRA_LINES / 2)], i, j);

    // Convert to tiles
    uint8_t finalbuffer[14][16][16]; // final buffer
    memset(finalbuffer, 0, sizeof(finalbuffer));
    for (int i = 0; i < GBCAM_W; i++) {
        for (int j = 0; j < GBCAM_H; j++) {
            uint8_t outcolor = 3 - (fourcolorsbuffer[i][j] >> 6);

            uint8_t *tile_base = finalbuffer[j >> 3][i >> 3];
            tile_base = &tile_base[(j & 7) * 2];

            if (outcolor & 1)
                tile_base[0] |= 1 << (7 - (7 & i));
            if (outcolor & 2)
                tile_base[1] |= 1 << (7 - (7 & i));
        }
    }

    // Copy to cart ram...
    memcpy(&gb->mmu->eram[0xA100 - ERAM], finalbuffer, sizeof(finalbuffer));
}

//--------------------------------------------------------------------


byte_t camera_read_reg(gb_t *gb, word_t address) {
    if ((address & 0x003F) == 0)
        return gb->mmu->mbc.camera.regs[0] & 0x07;
    return 0x00;
}

void camera_write_reg(gb_t *gb, word_t address, byte_t data) {
    gb_mmu_t *mmu = gb->mmu;

    byte_t reg = MIN(address & 0x003F, sizeof(mmu->mbc.camera.regs));
    if (reg == 0) {
        mmu->mbc.camera.regs[reg] = data & 0x07;
        if (CHECK_BIT(data, 0))
            camera_capture_image(gb);
        return;
    }

    mmu->mbc.camera.regs[reg] = data;
}

void camera_step(gb_t *gb) {
    if (!CHECK_BIT(gb->mmu->mbc.camera.regs[0], 0))
        return;

    // TODO if the capture process was stopped then resumed, it is resumed with its old capture parameter even if
    //      the registers have changed while it was stopped

    gb->mmu->mbc.camera.cam_cycles_left -= 4;
    if (gb->mmu->mbc.camera.cam_cycles_left == 0)
        RESET_BIT(gb->mmu->mbc.camera.regs[0], 0);
}
