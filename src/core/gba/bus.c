#include <stdlib.h>

#include "gba_priv.h"

// mGBA debug registers
#define MGBA_REG_DEBUG_ENABLE 0x4FFF780
#define MGBA_REG_DEBUG_FLAGS  0x4FFF700
#define MGBA_REG_DEBUG_STRING 0x4FFF600

#define MGBA_LOG_ENABLE_REQ_MAGIC 0xC0DE
#define MGBA_LOG_ENABLE_RES_MAGIC 0x1DEA

#define MGBA_LOG_FATAL 0
#define MGBA_LOG_ERROR 1
#define MGBA_LOG_WARN  2
#define MGBA_LOG_INFO  3
#define MGBA_LOG_DEBUG 4

typedef struct {
    uint32_t (*read)(gba_t *gba, uint8_t size, bus_access_type_t access, uint32_t address);
    void (*write)(gba_t *gba, uint8_t size, bus_access_type_t access, uint32_t address, uint32_t data);
} bus_accessors_t;

// clang-format off
static uint8_t gba_bios[] = {
    #embed "../../../build/bootroms/gba/gba_bios.bin"
};
// clang-format on

static void gba_parse_cartridge(gba_t *gba) {
    uint8_t *rom = gba->base->opts.rom;

    // uint8_t entrypoint = rom[0x00];

    memcpy(gba->rom_title, &rom[0xA0], sizeof(gba->rom_title));
    gba->rom_title[12] = '\0';

    // char short_title[3]; // short_title is 2 chars
    // memcpy(short_title, &rom[0xAD], sizeof(short_title));
    // short_title[2] = '\0';

    // TODO multiboot entries
}

static inline uint32_t read32(uint8_t *base) {
    uint32_t data;
    memcpy(&data, base, sizeof(data));
    return data;
}

static inline void write16(uint8_t *base, uint16_t data) {
    memcpy(base, &data, sizeof(data));
}

static inline void write32(uint8_t *base, uint32_t data) {
    memcpy(base, &data, sizeof(data));
}

static uint16_t io_regs_read(gba_t *gba, uint16_t address) {
    uint32_t mask   = 0xFFFF;
    address       >>= 1;

    switch (address) {
    // LCD I/O Registers
    case IO_DISPCNT:
        LOG_DEBUG("IO_DISPCNT");
        break;
    case IO_GREENSWAP:
        LOG_DEBUG("IO_GREENSWAP");
        break;
    case IO_DISPSTAT:
        LOG_DEBUG("IO_DISPSTAT");
        break;
    case IO_VCOUNT:
        LOG_DEBUG("IO_VCOUNT");
        break;
    case IO_BG0CNT:
        LOG_DEBUG("IO_BG0CNT");
        break;
    case IO_BG1CNT:
        LOG_DEBUG("IO_BG1CNT");
        break;
    case IO_BG2CNT:
        LOG_DEBUG("IO_BG2CNT");
        break;
    case IO_BG3CNT:
        LOG_DEBUG("IO_BG3CNT");
        break;
    case IO_BG0HOFS:
        LOG_DEBUG("IO_BG0HOFS");
        return gba->bus.read_data_latch;
    case IO_BG0VOFS:
        LOG_DEBUG("IO_BG0VOFS");
        return gba->bus.read_data_latch;
    case IO_BG1HOFS:
        LOG_DEBUG("IO_BG1HOFS");
        return gba->bus.read_data_latch;
    case IO_BG1VOFS:
        LOG_DEBUG("IO_BG1VOFS");
        return gba->bus.read_data_latch;
    case IO_BG2HOFS:
        LOG_DEBUG("IO_BG2HOFS");
        return gba->bus.read_data_latch;
    case IO_BG2VOFS:
        LOG_DEBUG("IO_BG2VOFS");
        return gba->bus.read_data_latch;
    case IO_BG3HOFS:
        LOG_DEBUG("IO_BG3HOFS");
        return gba->bus.read_data_latch;
    case IO_BG3VOFS:
        LOG_DEBUG("IO_BG3VOFS");
        return gba->bus.read_data_latch;
    case IO_BG2PA:
        LOG_DEBUG("IO_BG2PA");
        return gba->bus.read_data_latch;
    case IO_BG2PB:
        LOG_DEBUG("IO_BG2PB");
        return gba->bus.read_data_latch;
    case IO_BG2PC:
        LOG_DEBUG("IO_BG2PC");
        return gba->bus.read_data_latch;
    case IO_BG2PD:
        LOG_DEBUG("IO_BG2PD");
        return gba->bus.read_data_latch;
    case IO_BG2X ... IO_BG2X + 1:
        LOG_DEBUG("IO_BG2X");
        return gba->bus.read_data_latch;
    case IO_BG2Y ... IO_BG2Y + 1:
        LOG_DEBUG("IO_BG2Y");
        return gba->bus.read_data_latch;
    case IO_BG3PA:
        LOG_DEBUG("IO_BG3PA");
        return gba->bus.read_data_latch;
    case IO_BG3PB:
        LOG_DEBUG("IO_BG3PB");
        return gba->bus.read_data_latch;
    case IO_BG3PC:
        LOG_DEBUG("IO_BG3PC");
        return gba->bus.read_data_latch;
    case IO_BG3PD:
        LOG_DEBUG("IO_BG3PD");
        return gba->bus.read_data_latch;
    case IO_BG3X ... IO_BG3X + 1:
        LOG_DEBUG("IO_BG3X");
        return gba->bus.read_data_latch;
    case IO_BG3Y ... IO_BG3Y + 1:
        LOG_DEBUG("IO_BG3Y");
        return gba->bus.read_data_latch;
    case IO_WIN0H:
        LOG_DEBUG("IO_WIN0H");
        return gba->bus.read_data_latch;
    case IO_WIN1H:
        LOG_DEBUG("IO_WIN1H");
        return gba->bus.read_data_latch;
    case IO_WIN0V:
        LOG_DEBUG("IO_WIN0V");
        return gba->bus.read_data_latch;
    case IO_WIN1V:
        LOG_DEBUG("IO_WIN1V");
        return gba->bus.read_data_latch;
    case IO_WININ:
        LOG_DEBUG("IO_WININ");
        break;
    case IO_WINOUT:
        LOG_DEBUG("IO_WINOUT");
        break;
    case IO_MOSAIC:
        LOG_DEBUG("IO_MOSAIC");
        return gba->bus.read_data_latch;
    case IO_BLDCNT:
        LOG_DEBUG("IO_BLDCNT");
        break;
    case IO_BLDALPHA:
        LOG_DEBUG("IO_BLDALPHA");
        break;
    case IO_BLDY:
        LOG_DEBUG("IO_BLDY");
        return gba->bus.read_data_latch;

    // Sound Registers
    case IO_SOUND1CNT_L:
        LOG_DEBUG("IO_SOUND1CNT_L");
        break;
    case IO_SOUND1CNT_H:
        LOG_DEBUG("IO_SOUND1CNT_H");
        break;
    case IO_SOUND1CNT_X:
        LOG_DEBUG("IO_SOUND1CNT_X");
        break;
    case IO_SOUND1CNT_X + 1:
        return 0;
    case IO_SOUND2CNT_L:
        LOG_DEBUG("IO_SOUND2CNT_L");
        break;
    case IO_SOUND2CNT_L + 1:
        return 0;
    case IO_SOUND2CNT_H:
        LOG_DEBUG("IO_SOUND2CNT_H");
        break;
    case IO_SOUND2CNT_H + 1:
        return 0;
    case IO_SOUND3CNT_L:
        LOG_DEBUG("IO_SOUND3CNT_L");
        break;
    case IO_SOUND3CNT_H:
        LOG_DEBUG("IO_SOUND3CNT_H");
        break;
    case IO_SOUND3CNT_X:
        LOG_DEBUG("IO_SOUND3CNT_X");
        break;
    case IO_SOUND3CNT_X + 1:
        return 0;
    case IO_SOUND4CNT_L:
        LOG_DEBUG("IO_SOUND4CNT_L");
        break;
    case IO_SOUND4CNT_L + 1:
        return 0;
    case IO_SOUND4CNT_H:
        LOG_DEBUG("IO_SOUND4CNT_H");
        break;
    case IO_SOUND4CNT_H + 1:
        return 0;
    case IO_SOUNDCNT_L:
        LOG_DEBUG("IO_SOUNDCNT_L");
        break;
    case IO_SOUNDCNT_H:
        LOG_DEBUG("IO_SOUNDCNT_H");
        break;
    case IO_SOUNDCNT_X:
        LOG_DEBUG("IO_SOUNDCNT_X");
        break;
    case IO_SOUNDCNT_X + 1:
        return 0;
    case IO_SOUNDBIAS:
        LOG_DEBUG("IO_SOUNDBIAS");
        break;
    case IO_SOUNDBIAS + 1:
        return 0;
    case IO_WAVE_RAM ... IO_WAVE_RAM + 7:
        LOG_DEBUG("IO_WAVE_RAM");
        break;
    case IO_FIFO_A ... IO_FIFO_A + 1:
        LOG_DEBUG("IO_FIFO_A");
        return gba->bus.read_data_latch;
    case IO_FIFO_B ... IO_FIFO_B + 1:
        LOG_DEBUG("IO_FIFO_B");
        return gba->bus.read_data_latch;

    // DMA Transfer Channels
    case IO_DMA0SAD ... IO_DMA0SAD + 1:
        LOG_DEBUG("IO_DMA0SAD");
        return gba->bus.read_data_latch;
    case IO_DMA0DAD ... IO_DMA0DAD + 1:
        LOG_DEBUG("IO_DMA0DAD");
        return gba->bus.read_data_latch;
    case IO_DMA0CNT_L:
        LOG_DEBUG("IO_DMA0CNT_L");
        return 0;
    case IO_DMA0CNT_H:
        LOG_DEBUG("IO_DMA0CNT_H");
        mask = 0xF7E0;
        break;
    case IO_DMA1SAD ... IO_DMA1SAD + 1:
        LOG_DEBUG("IO_DMA1SAD");
        return gba->bus.read_data_latch;
    case IO_DMA1DAD ... IO_DMA1DAD + 1:
        LOG_DEBUG("IO_DMA1DAD");
        return gba->bus.read_data_latch;
    case IO_DMA1CNT_L:
        LOG_DEBUG("IO_DMA1CNT_L");
        return 0;
    case IO_DMA1CNT_H:
        LOG_DEBUG("IO_DMA1CNT_H");
        mask = 0xF7E0;
        break;
    case IO_DMA2SAD ... IO_DMA2SAD + 1:
        LOG_DEBUG("IO_DMA2SAD");
        return gba->bus.read_data_latch;
    case IO_DMA2DAD ... IO_DMA2DAD + 1:
        LOG_DEBUG("IO_DMA2DAD");
        return gba->bus.read_data_latch;
    case IO_DMA2CNT_L:
        LOG_DEBUG("IO_DMA2CNT_L");
        return 0;
    case IO_DMA2CNT_H:
        LOG_DEBUG("IO_DMA2CNT_H");
        mask = 0xF7E0;
        break;
    case IO_DMA3SAD ... IO_DMA3SAD + 1:
        LOG_DEBUG("IO_DMA3SAD");
        return gba->bus.read_data_latch;
    case IO_DMA3DAD ... IO_DMA3DAD + 1:
        LOG_DEBUG("IO_DMA3DAD");
        return gba->bus.read_data_latch;
    case IO_DMA3CNT_L:
        LOG_DEBUG("IO_DMA3CNT_L");
        return 0;
    case IO_DMA3CNT_H:
        LOG_DEBUG("IO_DMA3CNT_H");
        mask = 0xFFE0;
        break;

    // Timer Registers
    case IO_TM0CNT_L:
        LOG_DEBUG("IO_TM0CNT_L");
        break;
    case IO_TM0CNT_H:
        LOG_DEBUG("IO_TM0CNT_H");
        break;
    case IO_TM1CNT_L:
        LOG_DEBUG("IO_TM1CNT_L");
        break;
    case IO_TM1CNT_H:
        LOG_DEBUG("IO_TM1CNT_H");
        break;
    case IO_TM2CNT_L:
        LOG_DEBUG("IO_TM2CNT_L");
        break;
    case IO_TM2CNT_H:
        LOG_DEBUG("IO_TM2CNT_H");
        break;
    case IO_TM3CNT_L:
        LOG_DEBUG("IO_TM3CNT_L");
        break;
    case IO_TM3CNT_H:
        LOG_DEBUG("IO_TM3CNT_H");
        break;

    // Serial Communication (1)
    // case IO_SIODATA32 ... IO_SIODATA32 + 1:
    //     LOG_DEBUG("IO_SIODATA32");
    //     break;
    case IO_SIOMULTI0:
        LOG_DEBUG("IO_SIOMULTI0");
        break;
    case IO_SIOMULTI1:
        LOG_DEBUG("IO_SIOMULTI1");
        break;
    case IO_SIOMULTI2:
        LOG_DEBUG("IO_SIOMULTI2");
        break;
    case IO_SIOMULTI3:
        LOG_DEBUG("IO_SIOMULTI3");
        break;
    case IO_SIOCNT:
        LOG_DEBUG("IO_SIOCNT");
        break;
    case IO_SIOMLT_SEND:
        LOG_DEBUG("IO_SIOMLT_SEND");
        break;
    // case IO_SIODATA8:
    //     LOG_DEBUG("IO_SIODATA8");
    //     break;

    // Keypad Input
    case IO_KEYINPUT:
        LOG_DEBUG("IO_KEYINPUT");
        break;
    case IO_KEYCNT:
        LOG_DEBUG("IO_KEYCNT");
        break;

    // Serial Communication (2)
    case IO_RCNT:
        LOG_DEBUG("IO_RCNT");
        break;
    case IO_IR:
        return 0;
    case IO_JOYCNT:
        LOG_DEBUG("IO_JOYCNT");
        break;
    case IO_JOYCNT + 1:
        return 0;
    case IO_JOY_RECV ... IO_JOY_RECV + 1:
        LOG_DEBUG("IO_JOY_RECV");
        break;
    case IO_JOY_TRANS ... IO_JOY_TRANS + 1:
        LOG_DEBUG("IO_JOY_TRANS");
        break;
    case IO_JOYSTAT:
        LOG_DEBUG("IO_JOYSTAT");
        break;
    case IO_JOYSTAT + 1:
        return 0;

    // Interrupt, Waitstate, and Power-Down Control
    case IO_IE:
        mask = 0x3FFF;
        LOG_DEBUG("IO_IE");
        break;
    case IO_IF:
        mask = 0x3FFF;
        LOG_DEBUG("IO_IF");
        break;
    case IO_WAITCNT:
        LOG_DEBUG("IO_WAITCNT");
        break;
    case IO_WAITCNT + 1:
        return 0;
    case IO_IME:
        mask = 0x0001;
        LOG_DEBUG("IO_IME");
        break;
    case IO_IME + 1:
        return 0;
    case IO_POSTFLG_HALTCNT:
        LOG_DEBUG("IO_POSTFLG_HALTCNT");
        break;
    case IO_POSTFLG_HALTCNT + 1:
        return 0;

    default:
        return gba->bus.read_data_latch;
    }

    return gba->bus.io[address] & mask;
}

static void io_regs_write(gba_t *gba, uint16_t address, uint16_t data) {
    uint16_t mask   = 0xFFFF;
    address       >>= 1;

    switch (address) {
    // LCD I/O Registers
    case IO_DISPCNT:
        LOG_DEBUG("IO_DISPCNT 0x%04X", data);
        break;
    case IO_GREENSWAP:
        LOG_DEBUG("IO_GREENSWAP 0x%04X", data);
        break;
    case IO_DISPSTAT:
        mask = 0xFFF8;
        LOG_DEBUG("IO_DISPSTAT 0x%04X", data);
        break;
    case IO_VCOUNT:
        mask = 0x0000;
        LOG_DEBUG("IO_VCOUNT 0x%04X", data);
        break;
    case IO_BG0CNT:
        mask = 0xDFFF;
        LOG_DEBUG("IO_BG0CNT 0x%04X", data);
        break;
    case IO_BG1CNT:
        mask = 0xDFFF;
        LOG_DEBUG("IO_BG1CNT 0x%04X", data);
        break;
    case IO_BG2CNT:
        LOG_DEBUG("IO_BG2CNT 0x%04X", data);
        break;
    case IO_BG3CNT:
        LOG_DEBUG("IO_BG3CNT 0x%04X", data);
        break;
    case IO_BG0HOFS:
        mask = 0x03FF;
        LOG_DEBUG("IO_BG0HOFS 0x%04X", data);
        break;
    case IO_BG0VOFS:
        mask = 0x03FF;
        LOG_DEBUG("IO_BG0VOFS 0x%04X", data);
        break;
    case IO_BG1HOFS:
        mask = 0x03FF;
        LOG_DEBUG("IO_BG1HOFS 0x%04X", data);
        break;
    case IO_BG1VOFS:
        mask = 0x03FF;
        LOG_DEBUG("IO_BG1VOFS 0x%04X", data);
        break;
    case IO_BG2HOFS:
        mask = 0x03FF;
        LOG_DEBUG("IO_BG2HOFS 0x%04X", data);
        break;
    case IO_BG2VOFS:
        mask = 0x03FF;
        LOG_DEBUG("IO_BG2VOFS 0x%04X", data);
        break;
    case IO_BG3HOFS:
        mask = 0x03FF;
        LOG_DEBUG("IO_BG3HOFS 0x%04X", data);
        break;
    case IO_BG3VOFS:
        mask = 0x03FF;
        LOG_DEBUG("IO_BG3VOFS 0x%04X", data);
        break;
    case IO_BG2PA:
        LOG_DEBUG("IO_BG2PA 0x%04X", data);
        break;
    case IO_BG2PB:
        LOG_DEBUG("IO_BG2PB 0x%04X", data);
        break;
    case IO_BG2PC:
        LOG_DEBUG("IO_BG2PC 0x%04X", data);
        break;
    case IO_BG2PD:
        LOG_DEBUG("IO_BG2PD 0x%04X", data);
        break;
    case IO_BG2X ... IO_BG2X + 1:
        LOG_DEBUG("IO_BG2X 0x%04X", data);
        break;
    case IO_BG2Y ... IO_BG2Y + 1:
        LOG_DEBUG("IO_BG2Y 0x%04X", data);
        break;
    case IO_BG3PA:
        LOG_DEBUG("IO_BG3PA 0x%04X", data);
        break;
    case IO_BG3PB:
        LOG_DEBUG("IO_BG3PB 0x%04X", data);
        break;
    case IO_BG3PC:
        LOG_DEBUG("IO_BG3PC 0x%04X", data);
        break;
    case IO_BG3PD:
        LOG_DEBUG("IO_BG3PD 0x%04X", data);
        break;
    case IO_BG3X ... IO_BG3X + 1:
        LOG_DEBUG("IO_BG3X 0x%04X", data);
        break;
    case IO_BG3Y ... IO_BG3Y + 1:
        LOG_DEBUG("IO_BG3Y 0x%04X", data);
        break;
    case IO_WIN0H:
        LOG_DEBUG("IO_WIN0H 0x%04X", data);
        break;
    case IO_WIN1H:
        LOG_DEBUG("IO_WIN1H 0x%04X", data);
        break;
    case IO_WIN0V:
        LOG_DEBUG("IO_WIN0V 0x%04X", data);
        break;
    case IO_WIN1V:
        LOG_DEBUG("IO_WIN1V 0x%04X", data);
        break;
    case IO_WININ:
        mask = 0x3F3F;
        LOG_DEBUG("IO_WININ 0x%04X", data);
        break;
    case IO_WINOUT:
        mask = 0x3F3F;
        LOG_DEBUG("IO_WINOUT 0x%04X", data);
        break;
    case IO_MOSAIC:
        LOG_DEBUG("IO_MOSAIC 0x%04X", data);
        break;
    case IO_BLDCNT:
        mask = 0x3FFF;
        LOG_DEBUG("IO_BLDCNT 0x%04X", data);
        break;
    case IO_BLDALPHA:
        mask = 0x1F1F;
        LOG_DEBUG("IO_BLDALPHA 0x%04X", data);
        break;
    case IO_BLDY:
        LOG_DEBUG("IO_BLDY 0x%04X", data);
        break;

    // Sound Registers
    case IO_SOUND1CNT_L:
        mask = 0x007F;
        LOG_DEBUG("IO_SOUND1CNT_L 0x%04X", data);
        break;
    case IO_SOUND1CNT_H:
        mask = 0xFFC0;
        LOG_DEBUG("IO_SOUND1CNT_H 0x%04X", data);
        break;
    case IO_SOUND1CNT_X:
        mask = 0x4000;
        LOG_DEBUG("IO_SOUND1CNT_X 0x%04X", data);
        break;
    case IO_SOUND2CNT_L:
        mask = 0xFFC0;
        LOG_DEBUG("IO_SOUND2CNT_L 0x%04X", data);
        break;
    case IO_SOUND2CNT_H:
        mask = 0x4000;
        LOG_DEBUG("IO_SOUND2CNT_H 0x%04X", data);
        break;
    case IO_SOUND3CNT_L:
        mask = 0x00E0;
        LOG_DEBUG("IO_SOUND3CNT_L 0x%04X", data);
        break;
    case IO_SOUND3CNT_H:
        mask = 0xE000;
        LOG_DEBUG("IO_SOUND3CNT_H 0x%04X", data);
        break;
    case IO_SOUND3CNT_X:
        mask = 0x4000;
        LOG_DEBUG("IO_SOUND3CNT_X 0x%04X", data);
        break;
    case IO_SOUND4CNT_L:
        mask = 0xFF00;
        LOG_DEBUG("IO_SOUND4CNT_L 0x%04X", data);
        break;
    case IO_SOUND4CNT_H:
        mask = 0x40FF;
        LOG_DEBUG("IO_SOUND4CNT_H 0x%04X", data);
        break;
    case IO_SOUNDCNT_L:
        mask = 0xFF77;
        LOG_DEBUG("IO_SOUNDCNT_L 0x%04X", data);
        break;
    case IO_SOUNDCNT_H:
        mask = 0x770F;
        LOG_DEBUG("IO_SOUNDCNT_H 0x%04X", data);
        break;
    case IO_SOUNDCNT_X:
        mask = 0x0080;
        LOG_DEBUG("IO_SOUNDCNT_X 0x%04X", data);
        break;
    case IO_SOUNDBIAS:
        LOG_DEBUG("IO_SOUNDBIAS 0x%04X", data);
        break;
    case IO_WAVE_RAM ... IO_WAVE_RAM + 7:
        LOG_DEBUG("IO_WAVE_RAM 0x%04X", data);
        break;
    case IO_FIFO_A ... IO_FIFO_A + 1:
        LOG_DEBUG("IO_FIFO_A 0x%04X", data);
        break;
    case IO_FIFO_B ... IO_FIFO_B + 1:
        LOG_DEBUG("IO_FIFO_B 0x%04X", data);
        break;

    // DMA Transfer Channels
    case IO_DMA0SAD ... IO_DMA0SAD + 1:
        LOG_DEBUG("IO_DMA0SAD 0x%04X", data);
        break;
    case IO_DMA0DAD ... IO_DMA0DAD + 1:
        LOG_DEBUG("IO_DMA0DAD 0x%04X", data);
        break;
    case IO_DMA0CNT_L:
        LOG_DEBUG("IO_DMA0CNT_L 0x%04X", data);
        break;
    case IO_DMA0CNT_H:
        LOG_DEBUG("IO_DMA0CNT_H 0x%04X", data);
        break;
    case IO_DMA1SAD ... IO_DMA1SAD + 1:
        LOG_DEBUG("IO_DMA1SAD 0x%04X", data);
        break;
    case IO_DMA1DAD ... IO_DMA1DAD + 1:
        LOG_DEBUG("IO_DMA1DAD 0x%04X", data);
        break;
    case IO_DMA1CNT_L:
        LOG_DEBUG("IO_DMA1CNT_L 0x%04X", data);
        break;
    case IO_DMA1CNT_H:
        LOG_DEBUG("IO_DMA1CNT_H 0x%04X", data);
        break;
    case IO_DMA2SAD ... IO_DMA2SAD + 1:
        LOG_DEBUG("IO_DMA2SAD 0x%04X", data);
        break;
    case IO_DMA2DAD ... IO_DMA2DAD + 1:
        LOG_DEBUG("IO_DMA2DAD 0x%04X", data);
        break;
    case IO_DMA2CNT_L:
        LOG_DEBUG("IO_DMA2CNT_L 0x%04X", data);
        break;
    case IO_DMA2CNT_H:
        LOG_DEBUG("IO_DMA2CNT_H 0x%04X", data);
        break;
    case IO_DMA3SAD ... IO_DMA3SAD + 1:
        LOG_DEBUG("IO_DMA3SAD 0x%04X", data);
        break;
    case IO_DMA3DAD ... IO_DMA3DAD + 1:
        LOG_DEBUG("IO_DMA3DAD 0x%04X", data);
        break;
    case IO_DMA3CNT_L:
        LOG_DEBUG("IO_DMA3CNT_L 0x%04X", data);
        break;
    case IO_DMA3CNT_H:
        LOG_DEBUG("IO_DMA3CNT_H 0x%04X", data);
        break;

    // Timer Registers
    case IO_TM0CNT_L:
        LOG_DEBUG("IO_TM0CNT_L 0x%04X", data);
        gba->tmr.instance[0].reload = data;
        break;
    case IO_TM0CNT_H:
        LOG_DEBUG("IO_TM0CNT_H 0x%04X", data);
        gba_tmr_set(gba, data, 0);
        break;
    case IO_TM1CNT_L:
        LOG_DEBUG("IO_TM1CNT_L 0x%04X", data);
        gba->tmr.instance[1].reload = data;
        break;
    case IO_TM1CNT_H:
        LOG_DEBUG("IO_TM1CNT_H 0x%04X", data);
        gba_tmr_set(gba, data, 1);
        break;
    case IO_TM2CNT_L:
        LOG_DEBUG("IO_TM2CNT_L 0x%04X", data);
        gba->tmr.instance[2].reload = data;
        break;
    case IO_TM2CNT_H:
        LOG_DEBUG("IO_TM2CNT_H 0x%04X", data);
        gba_tmr_set(gba, data, 2);
        break;
    case IO_TM3CNT_L:
        LOG_DEBUG("IO_TM3CNT_L 0x%04X", data);
        gba->tmr.instance[3].reload = data;
        break;
    case IO_TM3CNT_H:
        LOG_DEBUG("IO_TM3CNT_H 0x%04X", data);
        gba_tmr_set(gba, data, 3);
        break;

    // Serial Communication (1)
    // case IO_SIODATA32 ... IO_SIODATA32 + 1:
    //     LOG_DEBUG("IO_SIODATA32 0x%04X", data);
    //     break;
    case IO_SIOMULTI0:
        LOG_DEBUG("IO_SIOMULTI0 0x%04X", data);
        break;
    case IO_SIOMULTI1:
        LOG_DEBUG("IO_SIOMULTI1 0x%04X", data);
        break;
    case IO_SIOMULTI2:
        LOG_DEBUG("IO_SIOMULTI2 0x%04X", data);
        break;
    case IO_SIOMULTI3:
        LOG_DEBUG("IO_SIOMULTI3 0x%04X", data);
        break;
    case IO_SIOCNT:
        LOG_DEBUG("IO_SIOCNT 0x%04X", data);
        break;
    case IO_SIOMLT_SEND:
        LOG_DEBUG("IO_SIOMLT_SEND 0x%04X", data);
        break;
    // case IO_SIODATA8:
    //     LOG_DEBUG("IO_SIODATA8 0x%04X", data);
    //     break;

    // Keypad Input
    case IO_KEYINPUT:
        LOG_DEBUG("IO_KEYINPUT 0x%04X", data);
        break;
    case IO_KEYCNT:
        LOG_DEBUG("IO_KEYCNT 0x%04X", data);
        break;

    // Serial Communication (2)
    case IO_RCNT:
        LOG_DEBUG("IO_RCNT 0x%04X", data);
        break;
    // case IO_IR:
    //     LOG_DEBUG("IO_IR 0x%04X", data);
    //     break;
    case IO_JOYCNT:
        LOG_DEBUG("IO_JOYCNT 0x%04X", data);
        break;
    case IO_JOY_RECV ... IO_JOY_RECV + 1:
        LOG_DEBUG("IO_JOY_RECV 0x%04X", data);
        break;
    case IO_JOY_TRANS ... IO_JOY_TRANS + 1:
        LOG_DEBUG("IO_JOY_TRANS 0x%04X", data);
        break;
    case IO_JOYSTAT:
        LOG_DEBUG("IO_JOYSTAT 0x%04X", data);
        break;

    // Interrupt, Waitstate, and Power-Down Control
    case IO_IE:
        LOG_DEBUG("IO_IE 0x%04X", data);
        break;
    case IO_IF:
        LOG_DEBUG("IO_IF 0x%04X", data);
        data = gba->bus.io[address] & ~data;
        break;
    case IO_WAITCNT:
        LOG_DEBUG("IO_WAITCNT 0x%04X", data);
        break;
    case IO_IME:
        mask = 0x0001;
        LOG_DEBUG("IO_IME 0x%04X", data);
        break;
    case IO_POSTFLG_HALTCNT:
        LOG_DEBUG("IO_POSTFLG_HALTCNT 0x%04X", data);
        break;

    default:
        break;
    }

    if (address >= sizeof(gba->bus.io) / sizeof(*gba->bus.io))
        return; // TODO IO address mirror?
    CHANGE_BITS(gba->bus.io[address], mask, data);
}

static uint32_t unused_read(gba_t *gba, uint8_t size, bus_access_type_t access, uint32_t address) {
    return gba->bus.read_data_latch; // TODO
}

static uint32_t bios_read(gba_t *gba, uint8_t size, bus_access_type_t access, uint32_t address) {
    if (gba->cpu.regs[REG_PC] >= BUS_BIOS_UNUSED)
        return gba->bus.last_fetched_bios_instr;
    return read32(&gba->bus.bios[address - BUS_BIOS]);
}

static uint32_t ewram_read(gba_t *gba, uint8_t size, bus_access_type_t access, uint32_t address) {
    return read32(&gba->bus.ewram[(address - BUS_EWRAM) % (BUS_EWRAM_UNUSED - BUS_EWRAM)]);
}

static uint32_t iwram_read(gba_t *gba, uint8_t size, bus_access_type_t access, uint32_t address) {
    return read32(&gba->bus.iwram[(address - BUS_IWRAM) % (BUS_IWRAM_UNUSED - BUS_IWRAM)]);
}

static uint32_t io_read(gba_t *gba, uint8_t size, bus_access_type_t access, uint32_t address) {
    address -= BUS_IO;

    uint32_t data = io_regs_read(gba, address);
    if (size == 4)
        data |= ((uint32_t) io_regs_read(gba, address + 2)) << 16;

    switch (address + BUS_IO) {
    case MGBA_REG_DEBUG_ENABLE:
        data = MGBA_LOG_ENABLE_RES_MAGIC;
        break;
    default:
        break;
    }

    return data;
}

static uint32_t pram_read(gba_t *gba, uint8_t size, bus_access_type_t access, uint32_t address) {
    return read32(&gba->bus.pram[(address - BUS_PRAM) % (BUS_PRAM_UNUSED - BUS_PRAM)]);
}

static uint32_t vram_read(gba_t *gba, uint8_t size, bus_access_type_t access, uint32_t address) {
    gba_bus_t *bus = &gba->bus;

    uint32_t vram_upper_bound = PPU_GET_MODE(gba) < 3 ? 0x10000 : 0x14000;
    address                   = (address - (BUS_VRAM_UNUSED + 0x8000)) % 0x20000;

    uint32_t data;
    if (address >= vram_upper_bound && address >= 0x18000)
        data = read32(&bus->vram[address % 0x8000]);
    else
        data = read32(&bus->vram[address % 0x20000]);

    return data;
}

static uint32_t oam_read(gba_t *gba, uint8_t size, bus_access_type_t access, uint32_t address) {
    return read32(&gba->bus.oam[(address - BUS_OAM_UNUSED) % (BUS_OAM_UNUSED - BUS_OAM)]);
}

static uint32_t rom_read(gba_t *gba, uint8_t size, bus_access_type_t access, uint32_t address) {
    gba_bus_t        *bus         = &gba->bus;
    bus_access_type_t access_type = access & 0x01;

    if (access_type == BUS_ACCESS_TYPE_N)
        bus->rom_address_latch = address & 0x01FFFFFF;

    uint32_t data;

    if (bus->rom_address_latch >= bus->rom_size) {
        // Reading from GamePak ROM when no Cartridge is inserted (or address beyond cartridge capacity)
        // Because Gamepak uses the same signal-lines for both 16bit data and for lower 16bit halfword address, the
        // entire gamepak ROM area is effectively filled by incrementing 16bit values (Address/2 AND FFFFh).
        data  = (bus->rom_address_latch >> 1) & 0xFFFF;
        data |= (data + 1) << 16;
    } else {
        data = read32(&bus->rom[bus->rom_address_latch]);
    }

    // TODO do writes update the rom_address_latch?
    bus->rom_address_latch = (bus->rom_address_latch + size) & 0x01FFFFFF;

    return data;
}

static uint32_t sram_read(gba_t *gba, uint8_t size, bus_access_type_t access, uint32_t address) {
    gba_bus_t *bus = &gba->bus;

    uint32_t data = bus->sram[(address - BUS_SRAM) % (BUS_SRAM_UNUSED - BUS_SRAM)];
    if (size == 2)
        data *= 0x0101;
    else if (size == 4)
        data *= 0x01010101;

    return data;
}

static void unused_write(gba_t *gba, uint8_t size, bus_access_type_t access, uint32_t address, uint32_t data) {
    // do nothing
}

static void bios_write(gba_t *gba, uint8_t size, bus_access_type_t access, uint32_t address, uint32_t data) {
    // do nothing
}

static void ewram_write(gba_t *gba, uint8_t size, bus_access_type_t access, uint32_t address, uint32_t data) {
    gba_bus_t *bus = &gba->bus;

    switch (size) {
    case 1:
        bus->ewram[(address - BUS_EWRAM) % (BUS_EWRAM_UNUSED - BUS_EWRAM)] = data;
        break;
    case 2:
        write16(&bus->ewram[(address - BUS_EWRAM) % (BUS_EWRAM_UNUSED - BUS_EWRAM)], data);
        break;
    case 4:
        write32(&bus->ewram[(address - BUS_EWRAM) % (BUS_EWRAM_UNUSED - BUS_EWRAM)], data);
        break;
    }
}

static void iwram_write(gba_t *gba, uint8_t size, bus_access_type_t access, uint32_t address, uint32_t data) {
    gba_bus_t *bus = &gba->bus;

    switch (size) {
    case 1:
        bus->iwram[(address - BUS_IWRAM) % (BUS_IWRAM_UNUSED - BUS_IWRAM)] = data;
        break;
    case 2:
        write16(&bus->iwram[(address - BUS_IWRAM) % (BUS_IWRAM_UNUSED - BUS_IWRAM)], data);
        break;
    case 4:
        write32(&bus->iwram[(address - BUS_IWRAM) % (BUS_IWRAM_UNUSED - BUS_IWRAM)], data);
        break;
    }
}

static void io_write(gba_t *gba, uint8_t size, bus_access_type_t access, uint32_t address, uint32_t data) {
    address -= BUS_IO;

    io_regs_write(gba, address, data);
    if (size == 4)
        io_regs_write(gba, address + 2, data >> 16);

    switch (address + BUS_IO) {
    case MGBA_REG_DEBUG_STRING ...(MGBA_REG_DEBUG_STRING + 0x100) - 1:
        memcpy(&gba->bus.mgba_logstr[address + BUS_IO - MGBA_REG_DEBUG_STRING], &data, size);
        break;
    case MGBA_REG_DEBUG_FLAGS:
        if (!gba->bus.mgba_logs_enabled)
            break;

        uint8_t offset = 0;
        for (uint32_t i = 0; i < sizeof(gba->bus.mgba_logstr); i++) {
            char c = gba->bus.mgba_logstr[i];
            if (c == '\n' || c == '\0') {
                LOG_INFO("[mGBA LOG] %.*s", i - offset, &gba->bus.mgba_logstr[offset]);
                offset = i + 1;

                if (c == '\0')
                    break;
            }
        }

        break;
    case MGBA_REG_DEBUG_ENABLE:
        gba->bus.mgba_logs_enabled = (data & 0xFFFF) == MGBA_LOG_ENABLE_REQ_MAGIC;
        break;
    default:
        break;
    }
}

static void pram_write(gba_t *gba, uint8_t size, bus_access_type_t access, uint32_t address, uint32_t data) {
    gba_bus_t *bus = &gba->bus;

    switch (size) {
    case 1:
    case 2:
        // PRAM bus for writes is 16/32 bits wide --> when writing a byte, we actually write a half with hi nibble
        // mirrored from lo nibble. The caller function has already mirrored the data so we don't have to do it here.
        write16(&bus->pram[(address - BUS_PRAM) % (BUS_PRAM_UNUSED - BUS_PRAM)], data);
        break;
    case 4:
        write32(&bus->pram[(address - BUS_PRAM) % (BUS_PRAM_UNUSED - BUS_PRAM)], data);
        break;
    }
}

static void vram_write(gba_t *gba, uint8_t size, bus_access_type_t access, uint32_t address, uint32_t data) {
    gba_bus_t *bus = &gba->bus;

    uint32_t vram_upper_bound = PPU_GET_MODE(gba) < 3 ? 0x10000 : 0x14000;
    address                   = (address - (BUS_VRAM_UNUSED + 0x8000)) % 0x20000;

    if (address >= vram_upper_bound && address >= 0x18000)
        return;

    switch (size) {
    case 1:
    case 2:
        // VRAM bus for writes is 16/32 bits wide --> when writing a byte, we actually write a half with hi nibble
        // mirrored from lo nibble. The caller function has already mirrored the data so we don't have to do it here.
        write16(&bus->vram[address % 0x20000], data);
        break;
    case 4:
        write32(&bus->vram[address % 0x20000], data);
        break;
    }
}

static void oam_write(gba_t *gba, uint8_t size, bus_access_type_t access, uint32_t address, uint32_t data) {
    gba_bus_t *bus = &gba->bus;

    switch (size) {
    case 1:
        // OAM bus for writes is 16/32 bits wide --> byte writes are ignored.
        break;
    case 2:
        write16(&bus->oam[(address - BUS_OAM_UNUSED) % (BUS_OAM_UNUSED - BUS_OAM)], data);
        break;
    case 4:
        write32(&bus->oam[(address - BUS_OAM_UNUSED) % (BUS_OAM_UNUSED - BUS_OAM)], data);
        break;
    }
}

static void rom_write(gba_t *gba, uint8_t size, bus_access_type_t access, uint32_t address, uint32_t data) {
    // do nothing
}

static void sram_write(gba_t *gba, uint8_t size, bus_access_type_t access, uint32_t address, uint32_t data) {
    gba->bus.sram[(address - BUS_SRAM) % (BUS_SRAM_UNUSED - BUS_SRAM)] = data;
}

static bus_accessors_t accessors[16] = {
    [0x00] = { .read = bios_read,   .write = bios_write   },
    [0x01] = { .read = unused_read, .write = unused_write },
    [0x02] = { .read = ewram_read,  .write = ewram_write  },
    [0x03] = { .read = iwram_read,  .write = iwram_write  },
    [0x04] = { .read = io_read,     .write = io_write     },
    [0x05] = { .read = pram_read,   .write = pram_write   },
    [0x06] = { .read = vram_read,   .write = vram_write   },
    [0x07] = { .read = oam_read,    .write = oam_write    },
    [0x08] = { .read = rom_read,    .write = rom_write    },
    [0x09] = { .read = rom_read,    .write = rom_write    },
    [0x0A] = { .read = rom_read,    .write = rom_write    },
    [0x0B] = { .read = rom_read,    .write = rom_write    },
    [0x0C] = { .read = rom_read,    .write = rom_write    },
    [0x0D] = { .read = rom_read,    .write = rom_write    },
    [0x0E] = { .read = sram_read,   .write = sram_write   },
    [0x0F] = { .read = unused_read, .write = unused_write },
};

uint32_t gba_bus_read(gba_t *gba, uint8_t size, bus_access_type_t access, uint32_t address) {
    uint32_t address_hi = address >> 24;
    if (address_hi > 0xF)
        address_hi = 0xF;

    uint32_t data = accessors[address_hi].read(gba, size, access, address);

    switch (size) {
    case 1:
        data &= 0xFF;
        data  = (data << 24) | (data << 16) | (data << 8) | data;
        break;
    case 2:
        data &= 0xFFFF;
        data  = (data << 16) | data;
        break;
    case 4:
        break;
    default:
        todo();
        break;
    }

    gba->bus.read_data_latch = data;

    return data;
}

void gba_bus_write(gba_t *gba, uint8_t size, bus_access_type_t access, uint32_t address, uint32_t data) {
    uint32_t address_hi = address >> 24;
    if (address_hi > 0xF)
        address_hi = 0xF;

    switch (size) {
    case 1:
        data                      &= 0xFF;
        gba->bus.write_data_latch  = (data << 24) | (data << 16) | (data << 8) | data;
        break;
    case 2:
        data                      &= 0xFFFF;
        gba->bus.write_data_latch  = (data << 16) | data;
        break;
    case 4:
        gba->bus.write_data_latch = data;
        break;
    default:
        todo();
        break;
    }

    accessors[address_hi].write(gba, size, access, address, gba->bus.write_data_latch);
}

bool gba_bus_validate_rom(const uint8_t *rom, size_t size) {
    if (!rom || size < 0xBF || size > (BUS_ROM1 - BUS_ROM0))
        return false;

    // uint8_t game_type = rom[0xAC];
    // switch (game_type) {
    //     case 'A':
    //     case 'B':
    //     case 'C':
    //         break;
    //     case 'F':
    //     case 'K':
    //     case 'P':
    //     case 'R':
    //     case 'U':
    //     case 'V':
    //         LOG_ERROR("game type '%c' is not implemented yet", game_type);
    //         return false;
    //     default:
    //         LOG_ERROR("invalid game type: %c", game_type);
    //         return false;
    // }

    // char short_title[3]; // short_title is 2 chars
    // memcpy(short_title, &rom[0xAD], sizeof(short_title));
    // short_title[2] = '\0';

    if (rom[0xB2] != 0x96 || rom[0xB3] != 0x00)
        return false;

    for (int i = 0xB5; i < 0xBC; i++)
        if (rom[i] != 0x00)
            return false;

    uint8_t checksum = 0;
    for (int i = 0xA0; i < 0xBC; i++)
        checksum -= rom[i];
    checksum -= 0x19;

    // if (rom[0xBD] != checksum) {
    //     LOG_ERROR("Invalid cartridge header checksum");
    //     return false;
    // }

    if (rom[0xBE] != 0x00 && rom[0xBF] != 0x00)
        return false;

    return true;
}

void gba_bus_reset(gba_t *gba) {
    memset(&gba->bus, 0, sizeof(gba->bus));

    // TODO do not memcpy rom, use pointer from gba.base.opts.rom as in gb_mmu_t (and add bound checks in bus reads/writes)
    memcpy(gba->bus.rom, gba->base->opts.rom, gba->base->opts.rom_size);

    gba_parse_cartridge(gba);

    gba->bus.rom_size        = gba->base->opts.rom_size;
    gba->bus.bios            = gba_bios;
    gba->bus.io[IO_KEYINPUT] = 0x03FF;
}
