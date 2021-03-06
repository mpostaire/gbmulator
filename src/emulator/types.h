#pragma once

#include <time.h>
#include <arpa/inet.h>

typedef unsigned char byte_t;
typedef char s_byte_t;
typedef unsigned short word_t;
typedef short s_word_t;

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
    JOYPAD_LEFT,
    JOYPAD_RIGHT,
    JOYPAD_UP,
    JOYPAD_DOWN,
    JOYPAD_A,
    JOYPAD_B,
    JOYPAD_START,
    JOYPAD_SELECT
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
    // do not move the 'mem' member (savestate.c uses offsetof mem on this struct)
    // everything that is below this line will be saved in the savestates
    byte_t mem[0x10000];
    byte_t eram[0x20000]; // max 16 banks of size 0x2000
    byte_t wram_extra[0x7000]; // 7 extra banks of wram of size 0x1000 for a total of 8 banks
    byte_t vram_extra[0x2000]; // 1 extra bank of wram of size 0x1000 for a total of 2 banks
    byte_t cram_bg[0x40]; // color palette memory: 8 palettes * 4 colors per palette * 2 bytes per color = 64 bytes
    byte_t cram_obj[0x40]; // color palette memory: 8 palettes * 4 colors per palette * 2 bytes per color = 64 bytes

    struct {
        byte_t step;
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

    rtc_t rtc;
} mmu_t;

typedef struct {
    void (*new_frame_cb)(byte_t *pixels);
    byte_t current_color_palette;

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
    float global_sound_level;

    int take_sample_cycles_count;
    float speed;
    void (*samples_ready_cb)(float *audio_buffer, int audio_buffer_size);

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
    int div_counter;
    int tima_counter;
} gbtimer_t;

typedef struct {
    byte_t action;
    byte_t direction;
} joypad_t;

typedef struct {
    int cycles_counter;
    int bit_counter;
    int sfd; // server's socket used only by the server
    int is_server;
    int other_sfd;
    int connected;
    struct sockaddr *other_addr;
} link_t;

typedef enum {
    DMG,
    CGB
} emulator_mode_t;

typedef struct {
    emulator_mode_t mode;

    char *rom_filepath;
    char rom_title[17];

    cpu_t *cpu;
    mmu_t *mmu;
    ppu_t *ppu;
    apu_t *apu;
    gbtimer_t *timer;
    joypad_t *joypad;
    link_t *link;
} emulator_t;
