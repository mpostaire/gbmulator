#pragma once

#include "gb.h"
#include "mmu.h"
#include "cpu.h"
#include "serialize.h"

#define PPU_STAT_GET_MODE(gb)      ((gb)->mmu.io_registers[IO_STAT] & 0x03)
#define PPU_STAT_IS_MODE(gb, mode) (PPU_STAT_GET_MODE(gb) == (mode))

#define IS_LCD_ENABLED(gb) (CHECK_BIT((gb)->mmu.io_registers[IO_LCDC], 7))

#define IS_LY_LYC_IRQ_STAT_ENABLED(gb) (CHECK_BIT((gb)->mmu.io_registers[IO_STAT], 6))
#define IS_OAM_IRQ_STAT_ENABLED(gb)    (CHECK_BIT((gb)->mmu.io_registers[IO_STAT], 5))
#define IS_VBLANK_IRQ_STAT_ENABLED(gb) (CHECK_BIT((gb)->mmu.io_registers[IO_STAT], 4))
#define IS_HBLANK_IRQ_STAT_ENABLED(gb) (CHECK_BIT((gb)->mmu.io_registers[IO_STAT], 3))

#define UPDATE_STAT_LY_LYC_BIT(gb) CHANGE_BIT((gb)->mmu.io_registers[IO_STAT], 2, (gb)->mmu.io_registers[IO_LY] == (gb)->mmu.io_registers[IO_LYC])

typedef enum {
    PPU_MODE_HBLANK,
    PPU_MODE_VBLANK,
    PPU_MODE_OAM,
    PPU_MODE_DRAWING
} gb_ppu_mode_t;

typedef struct {
    uint8_t  color;
    uint16_t palette;    // the address of the palette in DMG mode, the palette id in CGB mode (and CGB compatibility mode)
    uint8_t  attributes; // obj attributes or of bg/win attributes (only in CGB mode) depending if pixel is from obj or bg/win
    uint8_t  oam_pos;    // position of the obj in the oam memory for oam priority (only if this is an obj pixel and in CGB mode)
} gb_pixel_t;

#define PIXEL_FIFO_SIZE 8
typedef struct {
    gb_pixel_t pixels[PIXEL_FIFO_SIZE];
    uint8_t    size;
    uint8_t    head;
    uint8_t    tail;
} gb_pixel_fifo_t;

typedef enum {
    GET_TILE_ID         = 1,
    GET_TILE_SLICE_LOW  = 3,
    GET_TILE_SLICE_HIGH = 5,
    PUSH
} gb_pixel_fetcher_step_t;

typedef enum {
    FETCH_BG,
    FETCH_WIN,
    FETCH_OBJ
} gb_pixel_fetcher_mode_t;

typedef struct {
    uint8_t y;
    uint8_t x;
    uint8_t tile_id;
    uint8_t attributes;
    // the order of the struct members is important!
} gb_obj_t;

typedef struct {
    uint8_t  mode;              // actual mode of the ppu (the mode reflected in the STAT io register is not always true)
    int8_t   pending_stat_mode; // when ppu changes mode, it has a 1 cycle delay before being seen in the STAT register (this is -1 if no pending stat mode)
    uint8_t  is_lcd_turning_on;
    uint16_t cycles;
    uint8_t  discarded_pixels;     // dicarded pixel counter at the start of a scanline due to either SCX scrolling or WX < 7
    uint8_t  lcd_x;                // x coordinate of the lcd pixel shifter
    uint8_t  is_wx_triggered;      // 1 whenever WX matches lcd_x (with some caveats during dummy fetch), 0 otherwise
    int16_t  wly;                  // window "LY" internal counter
    int8_t   saved_wly;            // the wly value at the moment when the window was disabled in the middle of a frame where it was already enabled
    uint8_t  win_actually_enabled; // window was enabled before the current line's drawing mode (3): if window enable (LCDC bit 5) is disabled during drawing, the window will still be drawn until the end of the scanline.
    uint8_t  is_last_vblank_line;
    uint8_t  stat_irq_line;

    struct {
        gb_obj_t objs[10];         // this is ordered on the x coord of the gb_obj_t, popping an element is just increasing the index
        uint8_t  objs_oam_pos[10]; // objs_oam_pos[i] is objs[i]'s pos in the oam memory (used in CGB mode for OAM priority)
        uint8_t  size;
        uint8_t  index; // used in oam mode to iterate over the oam memory, in drawing mode this is the first element of the objs array
        uint8_t  step;
    } oam_scan;

    gb_pixel_fifo_t bg_win_fifo;
    gb_pixel_fifo_t obj_fifo;

    struct {
        gb_pixel_t              pixels[8];
        gb_pixel_fetcher_mode_t mode;
        gb_pixel_fetcher_mode_t old_mode; // mode that the FETCH_OBJ has replaced
        gb_pixel_fetcher_step_t step;
        uint8_t                 curr_oam_index;      // only relevant when step is FETCH_OBJ, holds the index of the obj to fetch in the oam_scan.objs array
        uint8_t                 is_dummy_fetch_done; // the fetcher has a dummy fetch of 6 cycles when starting a scanline

        uint8_t current_tile_id;
        uint8_t x; // x position of the fetcher on the scanline
    } pixel_fetcher;

    uint8_t pixels[GB_SCREEN_WIDTH * GB_SCREEN_HEIGHT * 4];
} gb_ppu_t;

void ppu_enable_lcd(gb_t *gb);

void ppu_disable_lcd(gb_t *gb);

void ppu_update_stat_irq_line(gb_t *gb);

void ppu_step(gb_t *gb);

void ppu_reset(gb_t *gb);

SERIALIZE_FUNCTION_DECLS(ppu);
