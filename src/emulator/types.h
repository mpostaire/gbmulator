#pragma once

#include <time.h>

typedef unsigned char byte_t;
typedef signed char s_byte_t;
typedef unsigned short word_t;
typedef signed short s_word_t;

typedef struct emulator_t emulator_t;

// TODO? LCD off special bright white color
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
    int opcode_state; // current opcode or current microcode inside the opcode (< 0 --> request new instruction fetch)
    word_t operand; // operand for the current opcode
    word_t opcode_compute_storage; // storage used for an easier implementation of some opcodes
} cpu_t;

typedef enum {
    ROM_ONLY,
    MBC1,
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
    int cpu_cycles_counter;
    word_t rtc_cycles_counter;

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

    // do not move the 'value_in_seconds' member (mmu.c and emulator.c use offsetof value_in_seconds on this struct)
    // everything that is below this line will be saved in the save file
    time_t value_in_seconds; // current rtc time in seconds
    time_t timestamp; // real unix time at the moment the rtc was clocked
} rtc_t;

typedef struct {
    byte_t cartridge[8400000];
    size_t cartridge_size;
    // do not move the 'mbc' member (emulator.c uses offsetof mbc on this struct)
    // everything that is below this line will be saved in the savestates
    mbc_type_t mbc;
    byte_t rom_banks;
    byte_t eram_banks;
    word_t current_rom_bank;
    s_word_t current_eram_bank;
    byte_t mbc1_mode;
    byte_t eram_enabled;

    byte_t has_eram;
    byte_t has_battery;
    byte_t has_rumble;
    byte_t has_rtc;

    byte_t mem[0x10000];
    // do not move the 'eram' member for the same reason as the 'mbc' member
    byte_t eram[0x20000]; // max 16 banks of size 0x2000
    byte_t wram_extra[0x7000]; // 7 extra banks of wram of size 0x1000 for a total of 8 banks
    byte_t vram_extra[0x2000]; // 1 extra bank of wram of size 0x1000 for a total of 2 banks
    byte_t cram_bg[0x40]; // color palette memory: 8 palettes * 4 colors per palette * 2 bytes per color = 64 bytes
    byte_t cram_obj[0x40]; // color palette memory: 8 palettes * 4 colors per palette * 2 bytes per color = 64 bytes

    struct {
        byte_t is_active;
        word_t progress;
        word_t src_address;
    } oam_dma;

    struct {
        byte_t step;
        byte_t hdma_ly;
        byte_t lock_cpu;
        hdma_type_t type;
        word_t src_address;
        word_t dest_address;
    } hdma;

    rtc_t rtc;
} mmu_t;

typedef struct {
    byte_t *pixels;
    byte_t *scanline_cache_color_data;
    // stores the x position of each pixel drawn by the highest priority object
    byte_t *obj_pixel_priority;

    byte_t is_lcd_turning_on;

    struct {
        word_t objs_addresses[10];
        byte_t size;
    } oam_scan;

    byte_t wly; // window "LY" internal counter
    int cycles;
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
    float *audio_buffer;

    byte_t frame_sequencer;
    int frame_sequencer_cycles_count;

    channel_t channel1;
    channel_t channel2;
    channel_t channel3;
    channel_t channel4;
} apu_t;

typedef struct {
    int max_tima_cycles;
    int tima_counter;
} gbtimer_t;

typedef struct {
    byte_t action;
    byte_t direction;
} joypad_t;

typedef struct {
    int cycles_counter;
    int clock_cycles;
    int bit_counter;
    emulator_t *other_emu;
} link_t;

typedef enum {
    DMG = 1,
    CGB
} emulator_mode_t;

struct emulator_t {
    emulator_mode_t mode;
    float apu_sound_level;
    float apu_speed;
    int apu_sample_count;
    byte_t ppu_color_palette;
    void (*on_apu_samples_ready)(float *audio_buffer, int audio_buffer_size);
    void (*on_new_frame)(byte_t *pixels);

    char rom_title[17];

    cpu_t *cpu;
    mmu_t *mmu;
    ppu_t *ppu;
    apu_t *apu;
    gbtimer_t *timer;
    joypad_t *joypad;
    link_t *link;
};

typedef struct {
    emulator_mode_t mode; // either `DMG` for original game boy emulation or `CGB` for game boy color emulation
    color_palette_t palette;
    float apu_speed;
    float apu_sound_level;
    int apu_sample_count;
    void (*on_new_frame)(byte_t *pixels); // the function called whenever the ppu has finished rendering a new frame
    void (*on_apu_samples_ready)(float *audio_buffer, int audio_buffer_size); // the function called whenever the samples buffer of the apu is full
} emulator_options_t;
