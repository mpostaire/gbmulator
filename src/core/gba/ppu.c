#include <assert.h>

#include "gba_priv.h"

#define VBLANK_HEIGHT 68

#define BG_FETCH_DELAY    31
#define OBJ_FETCH_DELAY   40
#define COMPOSITING_DELAY 46

#define PIXEL_DURATION  4
#define HDRAW_DURATION  1006
#define HBLANK_DURATION 226

#define FULL_HEIGHT (GBA_SCREEN_HEIGHT + VBLANK_HEIGHT)

#define SCANLINE_DURATION (HDRAW_DURATION + HBLANK_DURATION)
#define VDRAW_DURATION    (SCANLINE_DURATION * GBA_SCREEN_HEIGHT)
#define VBLANK_DURATION   (SCANLINE_DURATION * VBLANK_HEIGHT)
#define REFRESH_DURATION  (VDRAW_DURATION + VBLANK_DURATION)

#define PPU_GET_FRAME(gba) GET_BIT((gba)->bus.io[IO_DISPCNT], 4)

#define VRAM_OBJ_BASE_ADDR (4 * 0x4000)

#define DISPSTAT_W 0 // in VBLANK
#define DISPSTAT_G 1 // in HBLANK
#define DISPSTAT_Z 2 // VCOUNT == DISPCNT >> 8
#define DISPSTAT_V 3 // VBLANK IRQ enabled
#define DISPSTAT_H 4 // HBLANK IRQ enabled
#define DISPSTAT_Y 5 // VCOUNT == DISPCNT >> 8 IRQ enabled

static inline void set_pixel_rgb(gba_t *gba, uint32_t x, uint32_t y, uint8_t r, uint8_t g, uint8_t b) {
    if (!gba->ppu.pixels)
        return;

    uint32_t pixels_offset = (y * GBA_SCREEN_WIDTH * 4) + (x * 4);

    gba->ppu.pixels[pixels_offset]     = r;
    gba->ppu.pixels[pixels_offset + 1] = g;
    gba->ppu.pixels[pixels_offset + 2] = b;
    gba->ppu.pixels[pixels_offset + 3] = 0xFF;
}

static inline void set_pixel_color(gba_t *gba, uint32_t x, uint32_t y, uint16_t c) {
    uint8_t r = c & 0x001F;
    uint8_t g = (c >> 5) & 0x001F;
    uint8_t b = (c >> 10) & 0x001F;

    r = (r << 3) | (r >> 2);
    g = (g << 3) | (g >> 2);
    b = (b << 3) | (b >> 2);

    set_pixel_rgb(gba, x, y, r, g, b);
}

static inline uint8_t vram_read_u8(gba_t *gba, uint32_t address) {
    gba->bus.ppu_vram_accessed = gba->sched.cycle;
    address                    = address % sizeof(gba->bus.vram);

    assert(address < sizeof(gba->bus.vram));
    return gba->bus.vram[address];
}

static inline uint16_t pram_read_u16(gba_t *gba, uint32_t address) {
    address = ALIGN(address, 2);

    gba->bus.ppu_pram_accessed = gba->sched.cycle;
    address                    = address % sizeof(gba->bus.pram);

    assert(address < sizeof(gba->bus.pram));
    return (gba->bus.pram[address + 1] << 8) | gba->bus.pram[address];
}

static inline uint16_t vram_read_u16(gba_t *gba, uint32_t address) {
    address = ALIGN(address, 2);

    gba->bus.ppu_vram_accessed = gba->sched.cycle;
    address                    = address % sizeof(gba->bus.vram);

    assert(address < sizeof(gba->bus.vram));
    return (gba->bus.vram[address + 1] << 8) | gba->bus.vram[address];
}

static inline uint16_t oam_read_u16(gba_t *gba, uint32_t address) {
    address = ALIGN(address, 2);

    gba->bus.ppu_oam_accessed = gba->sched.cycle;
    address                   = address % sizeof(gba->bus.oam);

    assert(address < sizeof(gba->bus.oam));
    return (gba->bus.oam[address + 1] << 8) | gba->bus.oam[address];
}

void gba_ppu_reset(gba_t *gba) {
    memset(&gba->ppu, 0, sizeof(gba->ppu));

    // at ppu reset we just entered VBLANK HDRAW at VCOUNT == 225

    gba->bus.io[IO_VCOUNT] = 225;
    SET_BIT(gba->bus.io[IO_DISPSTAT], DISPSTAT_W);
    SET_BIT(gba->bus.io[IO_DISPSTAT], DISPSTAT_G);

    gba_sched_add(gba, GBA_SCHED_EVENT_PPU_ENTER_VHBLANK, SCANLINE_DURATION + 1, SCANLINE_DURATION);
    gba_sched_add(gba, GBA_SCHED_EVENT_PPU_ENTER_VHDRAW, HBLANK_DURATION, SCANLINE_DURATION);

    gba->ppu.scanline_cycles = HDRAW_DURATION;
}

static inline uint8_t render_text_tile_8bpp(gba_t *gba, uint32_t tile_base_addr, uint16_t tile_id, uint32_t x, uint32_t y, bool flip_x, bool flip_y) {
    uint32_t tile_x = x % 8;
    uint32_t tile_y = y % 8;

    if (flip_x)
        tile_x = 7 - tile_x;
    if (flip_y)
        tile_y = 7 - tile_y;

    uint32_t char_addr_offset  = tile_id * 0x40;
    char_addr_offset          += (tile_y * 8) + tile_x;

    uint32_t char_data_addr = tile_base_addr + char_addr_offset;

    if (tile_base_addr < VRAM_OBJ_BASE_ADDR && char_data_addr >= VRAM_OBJ_BASE_ADDR)
        return 0;

    return vram_read_u8(gba, char_data_addr);
}

static inline uint8_t render_text_tile_4bpp(gba_t *gba, uint32_t tile_base_addr, uint16_t tile_id, uint32_t x, uint32_t y, bool flip_x, bool flip_y) {
    uint32_t tile_x = x % 8;
    uint32_t tile_y = y % 8;

    if (flip_x)
        tile_x = 7 - tile_x;
    if (flip_y)
        tile_y = 7 - tile_y;

    uint32_t char_addr_offset  = tile_id * 0x20;
    char_addr_offset          += (tile_y * 4) + tile_x / 2; // tile_y * 4 and tile_x / 2 because 4bpp

    uint32_t char_data_addr = tile_base_addr + char_addr_offset;

    if (tile_base_addr < VRAM_OBJ_BASE_ADDR && char_data_addr >= VRAM_OBJ_BASE_ADDR)
        return 0;

    uint8_t char_data = vram_read_u8(gba, char_data_addr);

    if (tile_x % 2)
        char_data >>= 4;
    else
        char_data &= 0x0F;

    uint8_t palette_index_lo = char_data & 0x0F;

    return palette_index_lo;
}

static inline void draw_text_bg(gba_t *gba, uint8_t bg, uint32_t x, uint32_t y) {
    gba_ppu_t *ppu = &gba->ppu;

    uint16_t bgxcnt  = IO_BG0CNT + bg;
    uint16_t bgxvofs = IO_BG0VOFS + bg;
    uint16_t bgxhofs = IO_BG0HOFS + bg;

    uint8_t priority          = gba->bus.io[bgxcnt] & 0x03;
    uint8_t char_base_block   = (gba->bus.io[bgxcnt] >> 2) & 0x03;
    bool    mosaic            = CHECK_BIT(gba->bus.io[bgxcnt], 6);
    bool    is_8bpp           = CHECK_BIT(gba->bus.io[bgxcnt], 7);
    uint8_t screen_base_block = (gba->bus.io[bgxcnt] >> 8) & 0x1F;
    uint8_t screen_size       = (gba->bus.io[bgxcnt] >> 14) & 0x03;

    uint16_t n_tiles_x = CHECK_BIT(screen_size, 0) ? 64 : 32;
    uint16_t n_tiles_y = CHECK_BIT(screen_size, 1) ? 64 : 32;

    uint32_t voffset = gba->bus.io[bgxvofs] & 0x03FF;
    uint32_t hoffset = gba->bus.io[bgxhofs] & 0x03FF;

    uint32_t base_x  = x + hoffset;
    base_x          %= n_tiles_x * 8;
    uint32_t base_y  = y + voffset;
    base_y          %= n_tiles_y * 8;

    uint32_t screen_block = 0;
    switch (screen_size) {
    case 0b00:
        break;
    case 0b01:
        if (base_x >= 256) {
            screen_block  = 1;
            base_x       %= 256;
        }
        break;
    case 0b10:
        if (base_y >= 256) {
            screen_block  = 1;
            base_y       %= 256;
        }
        break;
    case 0b11:
        if (base_x >= 256 && base_y >= 256) {
            screen_block  = 3;
            base_x       %= 256;
            base_y       %= 256;
        } else if (base_x >= 256) {
            screen_block  = 1;
            base_x       %= 256;
        } else if (base_y >= 256) {
            screen_block  = 2;
            base_y       %= 256;
        }
        break;
    default:
        todo("this should never happen");
        break;
    }

    uint32_t sbe_base  = (screen_base_block + screen_block) * 0x0800;
    uint32_t char_base = char_base_block * 0x4000;

    uint32_t sbe_addr_offset  = (base_y / 8) * 32 + (base_x / 8); // y * 32 because a screenblock can fit 32x32 sbe
    sbe_addr_offset          *= 2;
    uint16_t sbe              = vram_read_u16(gba, sbe_base + sbe_addr_offset);

    uint16_t tile_id = sbe & 0x03FF;
    bool     flip_x  = CHECK_BIT(sbe, 10);
    bool     flip_y  = CHECK_BIT(sbe, 11);

    if (is_8bpp) {
        ppu->line_layers[bg][x] = render_text_tile_8bpp(gba, char_base, tile_id, base_x, base_y, flip_x, flip_y);
    } else { // 4bpp
        uint8_t palette_bank = (sbe >> 12) & 0x0F;
        // store palette bank in hi byte of line layer to be used by compositing step later
        ppu->line_layers[bg][x] = (palette_bank << 8) | render_text_tile_4bpp(gba, char_base, tile_id, base_x, base_y, flip_x, flip_y);
    }
}

static inline void draw_affine_bg(gba_t *gba, uint8_t bg, uint32_t x, uint32_t y) {
    // TODO
}

static inline void draw_obj(gba_t *gba, int32_t y) {
    gba_ppu_t *ppu = &gba->ppu;

    if (y >= GBA_SCREEN_HEIGHT)
        return;

#define OBJ_ATTR_FETCH_CYCLES 2
#define OAM_ENTRY_SIZE        8

    if (ppu->scanline_cycles < OBJ_FETCH_DELAY || (ppu->scanline_cycles % OBJ_ATTR_FETCH_CYCLES) != OBJ_ATTR_FETCH_CYCLES - 1)
        return;

    // TODO obj are fetched the previous scanline. unlike bg, they do not follow the current ppu.x coord, instead they insert at their attribute coord

    // TODO 32 bit read takes 2 cycles
    uint32_t address  = ALIGN(ppu->obj_id * OAM_ENTRY_SIZE, 4);
    uint32_t attrs01  = oam_read_u16(gba, address);
    attrs01          |= oam_read_u16(gba, address + 2) << 16;

    int32_t obj_y = attrs01 & 0xFF;
    if (obj_y >= GBA_SCREEN_HEIGHT)
        obj_y -= 256;
    uint8_t om = (attrs01 >> 8) & 0x03;

    // TODO
    // For regular sprites width / 2 16-bit VRAM accesses are performed (one access every two cycles). With each access
    // two pixels are rendered (even for 4BPP tile data).

    uint8_t gm      = (attrs01 >> 10) & 0x03;
    bool    mos     = CHECK_BIT(attrs01, 12);
    bool    is_8bpp = CHECK_BIT(attrs01, 13);
    uint8_t sh      = (attrs01 >> 14) & 0x03;

    int32_t obj_x = (attrs01 >> 16) & 0x01FF;
    if (obj_x >= GBA_SCREEN_WIDTH)
        obj_x -= 512;
    bool    flip_x = CHECK_BIT(attrs01, 28);
    bool    flip_y = CHECK_BIT(attrs01, 29);
    uint8_t sz     = (attrs01 >> 30) & 0x03;

    static const uint8_t obj_dims[4][4][2] = {
        { { 8, 8 },  { 16, 16 }, { 32, 32 }, { 64, 64 } },
        { { 16, 8 }, { 32, 8 },  { 32, 16 }, { 64, 32 } },
        { { 8, 16 }, { 8, 32 },  { 16, 32 }, { 32, 64 } },
        { { 8, 8 },  { 8, 8 },   { 8, 8 },   { 8, 8 }   }, // undefined behaviour if sh is 0b11
    };

    uint8_t obj_w = obj_dims[sh][sz][0];
    uint8_t obj_h = obj_dims[sh][sz][1];

    if (om == 0b10 || y < obj_y || y >= obj_y + obj_h) {
        ppu->obj_id = (ppu->obj_id + 1) % 128;
        return;
    }

    uint16_t attr2 = oam_read_u16(gba, (ppu->obj_id * OAM_ENTRY_SIZE) + 4);

    uint16_t base_tile_id = attr2 & 0x03FF;
    uint8_t  priority     = (attr2 >> 10) & 0x03;

    bool     is_1d_mapping = CHECK_BIT(gba->bus.io[IO_DISPCNT], 6);
    uint16_t mapping_width = is_1d_mapping ? 8 : 32;

    for (int32_t x = MAX(obj_x, 0); x < obj_x + obj_w && x < GBA_SCREEN_WIDTH; x++) { // TODO x/y oob
        uint32_t tile_x = x - obj_x;
        uint32_t tile_y = y - obj_y;

        // TODO Sprite rendering for the current scanline starts at cycle #40 of the previous scanline and continues
        // either until the horizontal blanking period of that previous scanline (if DISPCNT.bit5 = 1) or until cycle
        // #40 of the current scanline (if DISPCNT.bit5 = 0).

        if (flip_x)
            tile_x = (obj_w - 1) - tile_x;
        if (flip_y)
            tile_y = (obj_h - 1) - tile_y;

        uint16_t tile_id = base_tile_id + ((tile_y / 8) * mapping_width) + tile_x / 8;

        if (is_8bpp) {
            ppu->obj_layers[y & 1][x] = render_text_tile_8bpp(gba, VRAM_OBJ_BASE_ADDR, tile_id, x - obj_x, y - obj_y, flip_x, flip_y);
        } else { // 4bpp
            uint8_t palette_bank = (attr2 >> 12) & 0x0F;
            // store palette bank in hi byte of line layer to be used by compositing step later
            ppu->obj_layers[y & 1][x] = (palette_bank << 8) | render_text_tile_4bpp(gba, VRAM_OBJ_BASE_ADDR, tile_id, x - obj_x, y - obj_y, flip_x, flip_y);
        }
    }

    ppu->obj_id = (ppu->obj_id + 1) % 128;
}

static inline void draw_bg_mode0(gba_t *gba) {
    gba_ppu_t *ppu = &gba->ppu;

    if (ppu->scanline_cycles < BG_FETCH_DELAY || (ppu->scanline_cycles % PIXEL_DURATION) != PIXEL_DURATION - 1)
        return;

    uint32_t x = (ppu->scanline_cycles - BG_FETCH_DELAY) / PIXEL_DURATION;
    uint32_t y = gba->bus.io[IO_VCOUNT];

    if (x >= GBA_SCREEN_WIDTH)
        return;

    for (uint8_t bg = 0; bg < 4; bg++)
        if (CHECK_BIT(gba->bus.io[IO_DISPCNT], bg + 8))
            draw_text_bg(gba, bg, x, y);
}

static inline void draw_bg_mode1(gba_t *gba) {
    gba_ppu_t *ppu = &gba->ppu;

    if (ppu->scanline_cycles < BG_FETCH_DELAY || (ppu->scanline_cycles % PIXEL_DURATION) != PIXEL_DURATION - 1)
        return;

    uint32_t x = (ppu->scanline_cycles - BG_FETCH_DELAY) / PIXEL_DURATION;
    uint32_t y = gba->bus.io[IO_VCOUNT];

    if (x >= GBA_SCREEN_WIDTH)
        return;

    for (uint8_t bg = 0; bg < 2; bg++)
        if (CHECK_BIT(gba->bus.io[IO_DISPCNT], bg + 8))
            draw_text_bg(gba, bg, x, y);

    if (CHECK_BIT(gba->bus.io[IO_DISPCNT], 10))
        draw_affine_bg(gba, 2, x, y);
}

static inline void draw_bg_mode2(gba_t *gba) {
    gba_ppu_t *ppu = &gba->ppu;

    if (ppu->scanline_cycles < BG_FETCH_DELAY || (ppu->scanline_cycles % PIXEL_DURATION) != PIXEL_DURATION - 1)
        return;

    uint32_t x = (ppu->scanline_cycles - BG_FETCH_DELAY) / PIXEL_DURATION;
    uint32_t y = gba->bus.io[IO_VCOUNT];

    if (x >= GBA_SCREEN_WIDTH)
        return;

    for (uint8_t bg = 2; bg < 4; bg++)
        if (CHECK_BIT(gba->bus.io[IO_DISPCNT], bg + 8))
            draw_affine_bg(gba, bg, x, y);
}

static inline void draw_bg_mode3(gba_t *gba) {
    // TODO BG rendering seems to start only if it is enabled before HDRAW starts: can't do it in middle

    gba_ppu_t *ppu = &gba->ppu;

    if (ppu->scanline_cycles < BG_FETCH_DELAY || (ppu->scanline_cycles % PIXEL_DURATION) != PIXEL_DURATION - 1)
        return;

    uint32_t x = (ppu->scanline_cycles - BG_FETCH_DELAY) / PIXEL_DURATION;
    uint32_t y = gba->bus.io[IO_VCOUNT];

    if (x >= GBA_SCREEN_WIDTH)
        return;

    bool display_bg2 = CHECK_BIT(gba->bus.io[IO_DISPCNT], 10);
    if (display_bg2) {
        uint32_t pixel_base_addr   = 0;
        uint32_t pixel_addr_offset = (y << 1) * GBA_SCREEN_WIDTH + (x << 1);

        ppu->line_layers[2][x] = vram_read_u16(gba, pixel_base_addr + pixel_addr_offset);
    } else {
        ppu->line_layers[2][x] = pram_read_u16(gba, 0);
    }
}

static inline void draw_bg_mode4(gba_t *gba) {
    gba_ppu_t *ppu = &gba->ppu;

    if (ppu->scanline_cycles < BG_FETCH_DELAY || (ppu->scanline_cycles % PIXEL_DURATION) != PIXEL_DURATION - 1)
        return;

    uint32_t x = (ppu->scanline_cycles - BG_FETCH_DELAY) / PIXEL_DURATION;
    uint32_t y = gba->bus.io[IO_VCOUNT];

    if (x >= GBA_SCREEN_WIDTH)
        return;

    bool display_bg2 = CHECK_BIT(gba->bus.io[IO_DISPCNT], 10);
    if (display_bg2) {
        uint32_t pixel_base_addr   = BUS_VRAM + (PPU_GET_FRAME(gba) * 0xA000);
        uint32_t pixel_addr_offset = y * GBA_SCREEN_WIDTH + x;

        ppu->line_layers[2][x] = vram_read_u8(gba, pixel_base_addr + pixel_addr_offset);
    } else {
        ppu->line_layers[2][x] = 0;
    }
}

static inline void draw_bg_mode5(gba_t *gba) {
    gba_ppu_t *ppu = &gba->ppu;

    if (ppu->scanline_cycles < BG_FETCH_DELAY || (ppu->scanline_cycles % PIXEL_DURATION) != PIXEL_DURATION - 1)
        return;

    uint32_t x = (ppu->scanline_cycles - BG_FETCH_DELAY) / PIXEL_DURATION;
    uint32_t y = gba->bus.io[IO_VCOUNT];

    if (x >= GBA_SCREEN_WIDTH)
        return;

    bool display_bg2 = CHECK_BIT(gba->bus.io[IO_DISPCNT], 10);
    if (display_bg2 || x >= 160 || y >= 128) {
        uint32_t pixel_base_addr   = PPU_GET_FRAME(gba) * 0xA000;
        uint32_t pixel_addr_offset = (y << 1) * 160 + (x << 1);

        ppu->line_layers[2][x] = vram_read_u16(gba, pixel_base_addr + pixel_addr_offset);
    } else {
        ppu->line_layers[2][x] = pram_read_u16(gba, 0);
    }
}

static inline void compositing(gba_t *gba) {
    // TODO priorities, alpha blending, greenswap, palette ram access timings, other bgs than bg2, objs

    gba_ppu_t *ppu = &gba->ppu;

    if (ppu->scanline_cycles < COMPOSITING_DELAY || (ppu->scanline_cycles % PIXEL_DURATION) != PIXEL_DURATION - 1)
        return;

    uint32_t x = (ppu->scanline_cycles - COMPOSITING_DELAY) / PIXEL_DURATION;
    uint32_t y = gba->bus.io[IO_VCOUNT];

    if (x >= GBA_SCREEN_WIDTH)
        return;

    uint8_t mode = PPU_GET_MODE(gba);

    uint16_t color = pram_read_u16(gba, 0); // backdrop color

    for (uint8_t i = 0; i < 4; i++) {
        bool bg_enabled = CHECK_BIT(gba->bus.io[IO_DISPCNT], i + 8);
        if (!bg_enabled)
            continue;

        if (mode == 3 || mode == 5) {
            color = ppu->line_layers[i][x];
        } else {
            uint16_t palette_bank  = ppu->line_layers[i][x] >> 8;
            uint16_t palette_index = ppu->line_layers[i][x] & 0x0F;

            if (palette_index != 0) {
                if (palette_bank)
                    palette_index |= palette_bank << 4;
                color = pram_read_u16(gba, palette_index << 1);
            }
        }
    }

    bool obj_enabled = CHECK_BIT(gba->bus.io[IO_DISPCNT], 12);
    if (obj_enabled && (mode == 0 || mode == 2)) { // TODO is this mode check accurate?
        uint8_t current_obj_layer = y & 1;

        uint16_t palette_bank  = ppu->obj_layers[current_obj_layer][x] >> 8;
        uint16_t palette_index = ppu->obj_layers[current_obj_layer][x] & 0x0F;

        ppu->obj_layers[current_obj_layer][x] = 0;

        if (palette_index != 0) {
            if (palette_bank)
                palette_index |= palette_bank << 4;
            color = pram_read_u16(gba, 0x0200 + (palette_index << 1));
        }
    }

    set_pixel_color(gba, x, y, color);
}

void gba_ppu_sync(gba_t *gba) {
    gba_ppu_t *ppu = &gba->ppu;

    // LOG_WARN("ppu sync %" PRIu64, gba->sched.cycle - gba->ppu.last_sync_cycle);

    assert(gba->sched.cycle - gba->ppu.last_sync_cycle < SCANLINE_DURATION);

    for (; gba->ppu.last_sync_cycle < gba->sched.cycle; gba->ppu.last_sync_cycle++) {
        ppu->scanline_cycles++; // TODO at the start or end of this func? --> move into gba_ppu_enter_vhdraw (start of scanline)
        if (ppu->scanline_cycles >= SCANLINE_DURATION)
            ppu->scanline_cycles = 0;

        // switch (PPU_GET_MODE(gba)) {
        // case 0:
        // case 2:
        //     if (gba->bus.io[IO_VCOUNT] == GBA_SCREEN_HEIGHT + VBLANK_HEIGHT - 1)
        //         draw_obj(gba, 0);
        //     else
        //         draw_obj(gba, gba->bus.io[IO_VCOUNT] + 1);
        //     break;
        // default:
        //     break;
        // }

        bool is_hdraw = (gba->bus.io[IO_DISPSTAT] & 0x03) == 0x00;
        if (!is_hdraw)
            continue;

        switch (PPU_GET_MODE(gba)) {
        case 0:
            draw_bg_mode0(gba);
            break;
        case 1:
            draw_bg_mode1(gba);
            break;
        case 2:
            draw_bg_mode2(gba);
            break;
        case 3:
            draw_bg_mode3(gba);
            break;
        case 4:
            draw_bg_mode4(gba);
            break;
        case 5:
            draw_bg_mode5(gba);
            break;
        default:
            break;
        }

        if (CHECK_BIT(gba->bus.io[IO_GREENSWAP], 0))
            todo("green swap");

        // TODO composite step
        compositing(gba);

        // TODO  Although the drawing time is only 960 cycles (240*4), the H-Blank flag is "0" for a total of 1006 cycles.
        // --> 1006 - 960 == 46 --> this 46 offset is the composite offset?
        // so we enter hblank really at 1006 cycles not 960

        // if (gba->ppu.scanline_cycles >= HDRAW_DURATION) {
        //     // gba->ppu.period = GBA_PPU_PERIOD_HBLANK;
        //     SET_BIT(gba->bus.io[IO_DISPSTAT], DISPSTAT_G);

        //     if (CHECK_BIT(gba->bus.io[IO_DISPSTAT], DISPSTAT_H))
        //         CPU_REQUEST_INTERRUPT(gba, GBA_IRQ_HBLANK);
        // }
    }
}

static inline void set_vcount(gba_t *gba, uint16_t value) {
    bool prev_dispstat_z   = gba->bus.io[IO_VCOUNT] == gba->bus.io[IO_DISPSTAT] >> 8;
    gba->bus.io[IO_VCOUNT] = value;
    bool new_dispstat_z    = gba->bus.io[IO_VCOUNT] == gba->bus.io[IO_DISPSTAT] >> 8;

    CHANGE_BIT(gba->bus.io[IO_DISPSTAT], DISPSTAT_Z, new_dispstat_z);

    if (CHECK_BIT(gba->bus.io[IO_DISPSTAT], DISPSTAT_Y) && prev_dispstat_z && !new_dispstat_z)
        CPU_REQUEST_INTERRUPT(gba, GBA_IRQ_VCOUNT);

    if (gba->bus.io[IO_VCOUNT] == GBA_SCREEN_HEIGHT)
        SET_BIT(gba->bus.io[IO_DISPSTAT], DISPSTAT_W);
    else if (gba->bus.io[IO_VCOUNT] == 0)
        RESET_BIT(gba->bus.io[IO_DISPSTAT], DISPSTAT_W);
}

void gba_ppu_enter_vhdraw(gba_t *gba) {
    gba_ppu_sync(gba);

    LOG_DEBUG("gba_ppu_enter_vhdraw\t%" PRIu64 " (vcount=%" PRIu16 ")", gba->sched.cycle, gba->bus.io[IO_VCOUNT]);

    RESET_BIT(gba->bus.io[IO_DISPSTAT], DISPSTAT_G);

    if (gba->bus.io[IO_VCOUNT] < GBA_SCREEN_HEIGHT)
        set_vcount(gba, gba->bus.io[IO_VCOUNT] + 1);
    else if (gba->bus.io[IO_VCOUNT] == GBA_SCREEN_HEIGHT + VBLANK_HEIGHT - 1)
        set_vcount(gba, 0);
    else
        set_vcount(gba, gba->bus.io[IO_VCOUNT] + 1);
}

void gba_ppu_enter_vhblank(gba_t *gba) {
    gba_ppu_sync(gba);

    LOG_DEBUG("gba_ppu_enter_vhblank\t%" PRIu64 " (vcount=%" PRIu16 ")", gba->sched.cycle, gba->bus.io[IO_VCOUNT]);

    SET_BIT(gba->bus.io[IO_DISPSTAT], DISPSTAT_G);

    if (gba->bus.io[IO_VCOUNT] < GBA_SCREEN_HEIGHT) {
        // this is VDRAW HBLANK
        if (CHECK_BIT(gba->bus.io[IO_DISPSTAT], DISPSTAT_H))
            CPU_REQUEST_INTERRUPT(gba, GBA_IRQ_HBLANK);
    } else if (gba->bus.io[IO_VCOUNT] == GBA_SCREEN_HEIGHT) {
        // this is last VDRAW HBLANK before VBLANK
        if (CHECK_BIT(gba->bus.io[IO_DISPSTAT], DISPSTAT_V))
            CPU_REQUEST_INTERRUPT(gba, GBA_IRQ_VBLANK);
    } else if (gba->bus.io[IO_VCOUNT] >= GBA_SCREEN_HEIGHT + VBLANK_HEIGHT - 1) {
        // this is last VBLANK HBLANK before HDRAW
        if (gba->base->opts.on_pixbuf_request)
            gba->ppu.pixels = gba->base->opts.on_pixbuf_request(GBA_SCREEN_WIDTH, GBA_SCREEN_HEIGHT);
    }
}
