#pragma once

#include "types.h"
#include "mmu.h"
#include "cpu.h"
#include "serialize.h"

#define PPU_GET_MODE(emu_ptr) ((emu_ptr)->mmu->mem[STAT] & 0x03)
#define PPU_IS_MODE(emu_ptr, mode) (PPU_GET_MODE(emu_ptr) == (mode))

#define IS_LCD_ENABLED(emu_ptr) (CHECK_BIT((emu_ptr)->mmu->mem[LCDC], 7))

#define IS_LY_LYC_IRQ_STAT_ENABLED(emu_ptr) (CHECK_BIT((emu_ptr)->mmu->mem[STAT], 6))
#define IS_OAM_IRQ_STAT_ENABLED(emu_ptr) (CHECK_BIT((emu_ptr)->mmu->mem[STAT], 5))
#define IS_VBLANK_IRQ_STAT_ENABLED(emu_ptr) (CHECK_BIT((emu_ptr)->mmu->mem[STAT], 4))
#define IS_HBLANK_IRQ_STAT_ENABLED(emu_ptr) (CHECK_BIT((emu_ptr)->mmu->mem[STAT], 3))

typedef enum {
    PPU_MODE_HBLANK,
    PPU_MODE_VBLANK,
    PPU_MODE_OAM,
    PPU_MODE_DRAWING
} ppu_mode_t;

typedef struct {
    byte_t color;
    word_t palette; // the address of the palette in DMG mode, the palette id in CGB mode (and CGB compatibility mode)
    byte_t attributes; // obj attributes or of bg/win attributes (only in CGB mode) depending if pixel is from obj or bg/win
    byte_t oam_pos; // position of the obj in the oam memory for oam priority (only if this is an obj pixel and in CGB mode)
} pixel_t;

#define PIXEL_FIFO_SIZE 8
typedef struct {
    pixel_t pixels[PIXEL_FIFO_SIZE];
    byte_t size;
    byte_t head;
    byte_t tail;
} pixel_fifo_t;

typedef enum {
    GET_TILE_ID = 1,
    GET_TILE_SLICE_LOW = 3,
    GET_TILE_SLICE_HIGH = 5,
    PUSH
} pixel_fetcher_step_t;

typedef enum {
    FETCH_BG,
    FETCH_WIN,
    FETCH_OBJ
} pixel_fetcher_mode_t;

typedef struct {
    byte_t y;
    byte_t x;
    byte_t tile_id;
    byte_t attributes;
    // the order of the struct members is important!
} obj_t;

typedef struct {
    byte_t is_lcd_turning_on;
    word_t cycles;
    byte_t discarded_pixels; // dicarded pixel counter at the start of a scanline due to either SCX scrolling or WX < 7
    byte_t lcd_x; // x coordinate of the lcd pixel shifter
    s_word_t wly; // window "LY" internal counter
    byte_t is_last_vblank_line;

    struct {
        obj_t objs[10]; // this is ordered on the x coord of the obj_t, popping an element is just increasing the index
        byte_t objs_oam_pos[10]; // objs_oam_pos[i] is objs[i]'s pos in the oam memory (used in CGB mode for OAM priority)
        byte_t size;
        byte_t index; // used in oam mode to iterate over the oam memory, in drawing mode this is the first element of the objs array
        byte_t step;
    } oam_scan;

    pixel_fifo_t bg_win_fifo;
    pixel_fifo_t obj_fifo;

    struct {
        pixel_t pixels[8];
        pixel_fetcher_mode_t mode;
        pixel_fetcher_mode_t old_mode; // mode that the FETCH_OBJ has replaced
        pixel_fetcher_step_t step;
        byte_t curr_oam_index; // only relevant when step is FETCH_OBJ, holds the index of the obj to fetch in the oam_scan.objs array
        byte_t init_delay_done; // the fetcher has a dummy fetch of 6 cycles when starting a scanline

        byte_t current_tile_id;
        byte_t x; // x position of the fetcher on the scanline
    } pixel_fetcher;

    byte_t *pixels;
} ppu_t;

extern byte_t dmg_palettes[PPU_COLOR_PALETTE_MAX][4][3];

void ppu_ly_lyc_compare(emulator_t *emu);

void ppu_step(emulator_t *emu);

void ppu_init(emulator_t *emu);

void ppu_quit(emulator_t *emu);

SERIALIZE_FUNCTION_DECLS(ppu);
