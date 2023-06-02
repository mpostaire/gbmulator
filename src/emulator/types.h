#pragma once

#include <stdint.h>
#include <stddef.h>

typedef unsigned char byte_t;
typedef signed char s_byte_t;
typedef unsigned short word_t;
typedef signed short s_word_t;

typedef struct emulator_t emulator_t;

typedef enum {
    DMG_WHITE,
    DMG_LIGHT_GRAY,
    DMG_DARK_GRAY,
    DMG_BLACK
} dmg_color_t;

typedef enum {
    PPU_COLOR_PALETTE_GRAY,
    PPU_COLOR_PALETTE_ORIG,
    PPU_COLOR_PALETTE_MAX // keep at the end
} color_palette_t;

typedef enum {
    JOYPAD_RIGHT,
    JOYPAD_LEFT,
    JOYPAD_UP,
    JOYPAD_DOWN,
    JOYPAD_A,
    JOYPAD_B,
    JOYPAD_SELECT,
    JOYPAD_START
} joypad_button_t;

typedef struct {
    union {
        struct {
            byte_t f;
            byte_t a;
        };
        word_t af;
    };

    union {
        struct {
            byte_t c;
            byte_t b;
        };
        word_t bc;
    };

    union {
        struct {
            byte_t e;
            byte_t d;
        };
        word_t de;
    };

    union {
        struct {
            byte_t l;
            byte_t h;
        };
        word_t hl;
    };

    word_t sp;
    word_t pc;
} registers_t;

typedef struct {
    registers_t registers;
    byte_t ime; // interrupt master enable
    byte_t halt;
    byte_t halt_bug;
    byte_t exec_state; // determines if the cpu is pushing an interrupt execution, executiong a normal opcode or a cb opcode
    byte_t opcode; // current opcode
    s_word_t opcode_state; // current opcode or current microcode inside the opcode (< 0 --> request new instruction fetch)
    word_t operand; // operand for the current opcode
    word_t opcode_cache_variable; // storage used for an easier implementation of some opcodes
} cpu_t;

typedef enum {
    ROM_ONLY,
    MBC1,
    MBC1M,
    MBC2,
    MBC3,
    MBC30,
    MBC5,
    MBC6,
    MBC7,
    HuC1
} mbc_type_t;

typedef enum {
    GDMA,
    HDMA
} hdma_type_t;

typedef struct {
    byte_t latched_s;
    byte_t latched_m;
    byte_t latched_h;
    byte_t latched_dl;
    byte_t latched_dh;

    byte_t s;
    byte_t m;
    byte_t h;
    byte_t dl;
    byte_t dh;

    byte_t enabled;
    byte_t reg; // rtc register
    byte_t latch;
    uint32_t rtc_cycles;
} rtc_t;

typedef struct {
    byte_t cartridge[8400000];
    size_t cartridge_size;

    byte_t mem[0x10000];
    byte_t eram[0x20000]; // max 16 banks of size 0x2000
    byte_t wram_extra[0x7000]; // 7 extra banks of wram of size 0x1000 for a total of 8 banks
    byte_t vram_extra[0x2000]; // 1 extra bank of wram of size 0x1000 for a total of 2 banks
    byte_t cram_bg[0x40]; // color palette memory: 8 palettes * 4 colors per palette * 2 bytes per color = 64 bytes
    byte_t cram_obj[0x40]; // color palette memory: 8 palettes * 4 colors per palette * 2 bytes per color = 64 bytes

    struct {
        byte_t step;
        byte_t hdma_ly;
        byte_t lock_cpu;
        byte_t type;
        word_t src_address;
        word_t dest_address;
    } hdma;

    struct {
        byte_t starting_statuses[2]; // array of statuses of the initialization of the oam dma (this is an array to allow multiple initializations at the same time; the last will eventually overwrite the previous oam dma)
        byte_t starting_count; // holds the number of oam dma initializations
        s_word_t progress; // < 0 if oam dma not running, else [0, 159)
        word_t src_address;
    } oam_dma;

    byte_t mbc;
    word_t rom_banks;
    byte_t eram_banks;

    // MBC
    byte_t ramg_reg; // 1 if eram is enabled, else 0
    // MBC1/MBC1M: bits 7-5: unused, bits 4-0 (MBC1M: bits 3-0): lower 5 (MBC1M: 4) bits of the ROM_BANKN number
    // (also used as a convenience variable in other MBCs to store the ROM_BANKN number)
    byte_t bank1_reg;
    // bits 7-2: unused, bits 1-0: upper 2 bits (bits 6-5) of the ROM_BANKN number
    // (also used as a convenience variable in other MBCs to store the ERAM_BANK number)
    byte_t bank2_reg;
    byte_t mode_reg; // bits 7-1: unused, bit 0: BANK2 mode
    byte_t rtc_mapped; // MBC3
    byte_t romb0_reg; // MBC5: lower ROM BANK register
    byte_t romb1_reg; // MBC5: upper ROM BANK register
    // MBC5: RAMB register is stored in bank2_reg

    // pointer to the start of the ROM memory region when accessing the 0x0000-0x3FFF range.
    byte_t *rom_bank0_pointer;
    // pointer to the start of the ROM memory region  when accessing the 0x4000-0x7FFF range
    // (actually with an offset of -0x4000 to avoid converting the address passed to the mmu_read() function).
    byte_t *rom_bankn_pointer;
    // pointer to the start of the ERAM memory region when accessing the 0x8000-0x9FFF range.
    // (actually with an offset of -0xA000 to avoid converting the address passed to the mmu_read() and mmu_write() functions).
    byte_t *eram_bank_pointer;

    byte_t has_eram;
    byte_t has_battery;
    byte_t has_rumble;
    byte_t has_rtc;

    rtc_t rtc;
} mmu_t;

typedef struct {
    byte_t color;
    word_t palette;
    byte_t obj_priority; // only on CGB mode
    byte_t bg_priority;
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

typedef enum {
    APU_CHANNEL_1,
    APU_CHANNEL_2,
    APU_CHANNEL_3,
    APU_CHANNEL_4
} channel_id_t;

typedef struct {
    byte_t wave_position;
    byte_t duty_position;
    byte_t duty;
    int freq_timer;
    int length_counter; // length module
    int envelope_period; // envelope module
    byte_t envelope_volume; // envelope module
    int sweep_timer; // sweep module
    int sweep_freq; // sweep module
    byte_t sweep_enabled; // sweep module

    // registers
    byte_t *NRx0;
    byte_t *NRx1;
    byte_t *NRx2;
    byte_t *NRx3;
    byte_t *NRx4;
    word_t LFSR;

    channel_id_t id;
} channel_t;

typedef struct {
    int take_sample_cycles_count;

    int audio_buffer_index;
    size_t audio_buffer_sample_size;
    void *audio_buffer;

    byte_t frame_sequencer;
    int frame_sequencer_cycles_count;

    channel_t channel1;
    channel_t channel2;
    channel_t channel3;
    channel_t channel4;
} apu_t;

typedef struct {
    byte_t tima_increase_div_bit; // bit of DIV that causes an increase of TIMA when set to 1 and TAC timer is enabled
    byte_t falling_edge_detector_delay;
    s_word_t tima_loading_value;
    s_word_t old_tma; // holds the value of the old tma for one cpu step if it has been overwritten, -1 otherwise
} gbtimer_t;

typedef struct {
    byte_t action;
    byte_t direction;
} joypad_t;

typedef struct {
    word_t cycles;
    word_t max_clock_cycles;
    byte_t bit_counter;
    emulator_t *other_emu;
} link_t;

typedef enum {
    DMG = 1,
    CGB
} emulator_mode_t;

typedef enum {
    APU_FORMAT_F32 = 1,
    APU_FORMAT_U8,
} apu_audio_format_t;

typedef void (*on_new_frame_t)(const byte_t *pixels);
typedef void (*on_apu_samples_ready_t)(const void *audio_buffer, int audio_buffer_size);

typedef struct {
    emulator_mode_t mode; // either `DMG` for original game boy emulation or `CGB` for game boy color emulation
    byte_t disable_cgb_color_correction;
    color_palette_t palette;
    float apu_speed;
    float apu_sound_level;
    int apu_sample_count;
    apu_audio_format_t apu_format;
    on_new_frame_t on_new_frame; // the function called whenever the ppu has finished rendering a new frame
    on_apu_samples_ready_t on_apu_samples_ready; // the function called whenever the samples buffer of the apu is full
} emulator_options_t;

struct emulator_t {
    emulator_mode_t mode;
    byte_t disable_cgb_color_correction;
    byte_t exit_on_invalid_opcode;
    float apu_sound_level;
    float apu_speed;
    int apu_sample_count;
    apu_audio_format_t apu_format;
    byte_t ppu_color_palette;
    on_new_frame_t on_new_frame;
    on_apu_samples_ready_t on_apu_samples_ready;

    char rom_title[17];

    cpu_t *cpu;
    mmu_t *mmu;
    ppu_t *ppu;
    apu_t *apu;
    gbtimer_t *timer;
    joypad_t *joypad;
    link_t *link;
};
