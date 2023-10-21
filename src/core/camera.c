#include "gb_priv.h"

#define GB_CAMERA_WIDTH 128
#define GB_CAMERA_HEIGHT 112

// TODO every even picture is corrupted on the upper right corner

// --------------------------------------------------------------------

// The code delimited by the dash lines comments is adapted from SameBoy:
// https://github.com/LIJI32/SameBoy/blob/c06e320b9579a5b8c1177ca1218ed8c87d57798c/Core/camera.c

// MIT License

// Copyright (c) 2015-2023 Lior Halphon

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

static inline int64_t get_processed_color(gb_mbc_t *mbc, byte_t x, byte_t y) {
    if (x == GB_CAMERA_WIDTH)
        x--;
    else if (x > GB_CAMERA_WIDTH)
        x = 0;

    if (y == GB_CAMERA_HEIGHT)
        y--;
    else if (y > GB_CAMERA_HEIGHT)
        y = 0;

    static const float gain_values[] = {
        0.8809390f, 0.9149149f, 0.9457498f, 0.9739758f,
        1.0000000f, 1.0241412f, 1.0466537f, 1.0677433f,
        1.0875793f, 1.1240310f, 1.1568911f, 1.1868043f,
        1.2142561f, 1.2396208f, 1.2743837f, 1.3157323f,
        1.3525190f, 1.3856512f, 1.4157897f, 1.4434309f,
        1.4689574f, 1.4926697f, 1.5148087f, 1.5355703f,
        1.5551159f, 1.5735801f, 1.5910762f, 1.6077008f,
        1.6235366f, 1.6386550f, 1.6531183f, 1.6669808f
    };

    int64_t color = mbc->camera.sensor_image[y * GB_CAMERA_SENSOR_WIDTH + x];
    if (CHECK_BIT(mbc->camera.work_regs[4], 3))
        color = 255 - color; // Invert color
    // Multiply color by gain value
    color *= gain_values[mbc->camera.work_regs[1] & 0x1F];
    // Color is multiplied by the exposure register to simulate exposure.
    return CLAMP(128 + (color * ((mbc->camera.work_regs[2] << 8) + mbc->camera.work_regs[3]) / 0x2000), 0, 255);
}

byte_t gb_camera_read_image(gb_t *gb, word_t address) {
    gb_mbc_t *mbc = &gb->mmu->mbc;

    byte_t tile_x = address / 0x10 % 0x10;
    byte_t tile_y = address / 0x10 / 0x10;

    byte_t y = ((address >> 1) & 0x7) + tile_y * 8;
    byte_t bit = address & 1;

    byte_t ret = 0;
    for (byte_t x = tile_x * 8; x < tile_x * 8 + 8; x++) {
        static const float edge_enhancement_ratios[] = {0.5f, 0.75f, 1.0f, 1.25f, 2.0f, 3.0f, 4.0f, 5.0f};
        float edge_enhancement_ratio = edge_enhancement_ratios[(mbc->camera.work_regs[4] >> 4) & 0x7];

        int64_t color = get_processed_color(mbc, x, y);
        if ((mbc->camera.work_regs[1] & 0xE0) == 0xE0) {
            color += (color * 4) * edge_enhancement_ratio;
            color -= get_processed_color(mbc, x - 1, y) * edge_enhancement_ratio;
            color -= get_processed_color(mbc, x + 1, y) * edge_enhancement_ratio;
            color -= get_processed_color(mbc, x, y - 1) * edge_enhancement_ratio;
            color -= get_processed_color(mbc, x, y + 1) * edge_enhancement_ratio;
        }

        // The camera's registers are used as a threshold pattern, which defines the dithering
        byte_t pattern_base = ((x & 3) + (y & 3) * 4) * 3 + 6;
        if (color < mbc->camera.work_regs[pattern_base])
            color = 3;
        else if (color < mbc->camera.work_regs[pattern_base + 1])
            color = 2;
        else if (color < mbc->camera.work_regs[pattern_base + 2])
            color = 1;
        else
            color = 0;

        ret <<= 1;
        ret |= (color >> bit) & 1;
    }

    return ret;
}

// --------------------------------------------------------------------

/**
 * fast random number generator: https://stackoverflow.com/a/3747462
 * we only want to show some good enough noise so we don't care about rand_seed conficts due to mutiple instance of gb_t
 */
static unsigned int rand_seed = 1;
static inline unsigned int fastrand(void) {
    rand_seed = (214013 * rand_seed + 2531011);
    return (rand_seed >> 16) & 0xFF;
}

byte_t camera_read_reg(gb_t *gb, word_t address) {
    if ((address & 0x003F) == 0)
        return gb->mmu->mbc.camera.regs[0] & 0x07;
    return 0x00;
}

void camera_write_reg(gb_t *gb, word_t address, byte_t data) {
    gb_mmu_t *mmu = gb->mmu;

    byte_t reg = MIN(address & 0x003F, GB_CAMERA_N_REGS);
    if (reg == 0) {
        byte_t request_capture = !CHECK_BIT(mmu->mbc.camera.regs[reg], 0) && CHECK_BIT(data, 0);
        byte_t capture_in_progress = mmu->mbc.camera.capture_cycles_remaining > 0;

        if (request_capture && !capture_in_progress) {
            word_t exposure = (mmu->mbc.camera.regs[2] << 8) | mmu->mbc.camera.regs[3];
            mmu->mbc.camera.capture_cycles_remaining = 4 * (32448 + (CHECK_BIT(mmu->mbc.camera.regs[1], 7) ? 0 : 512) + (exposure * 16));
            memcpy(mmu->mbc.camera.work_regs, mmu->mbc.camera.regs, GB_CAMERA_N_REGS);

            byte_t generate_noise_image = gb->on_camera_capture_image == NULL;
            if (!generate_noise_image)
                generate_noise_image |= !gb->on_camera_capture_image(mmu->mbc.camera.sensor_image);
            if (generate_noise_image) {
                for (int i = 0; i < GB_CAMERA_SENSOR_HEIGHT * GB_CAMERA_SENSOR_WIDTH; i++)
                    mmu->mbc.camera.sensor_image[i] = fastrand();
            }
        }
        mmu->mbc.camera.regs[reg] = data & 0x07;
    } else {
        mmu->mbc.camera.regs[reg] = data;
    }
}

void camera_step(gb_t *gb) {
    if (!CHECK_BIT(gb->mmu->mbc.camera.regs[0], 0))
        return;

    gb->mmu->mbc.camera.capture_cycles_remaining -= 4;
    if (gb->mmu->mbc.camera.capture_cycles_remaining == 0)
        RESET_BIT(gb->mmu->mbc.camera.regs[0], 0);
}
