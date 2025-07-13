#include <stdlib.h>

#include "gba_priv.h"

#define BUS_ACCESS_READ (0 << 1)
#define BUS_ACCESS_WRITE (1 << 1)
#define BUS_ACCESS_SIZE(x) (((x - 1) & 0x03) << 2)

// mGBA debug registers
#define MGBA_REG_DEBUG_ENABLE 0x4FFF780
#define MGBA_REG_DEBUG_FLAGS 0x4FFF700
#define MGBA_REG_DEBUG_STRING 0x4FFF600

#define MGBA_LOG_ENABLE_REQ_MAGIC 0xC0DE
#define MGBA_LOG_ENABLE_RES_MAGIC 0x1DEA

#define MGBA_LOG_FATAL 0
#define MGBA_LOG_ERROR 1
#define MGBA_LOG_WARN 2
#define MGBA_LOG_INFO 3
#define MGBA_LOG_DEBUG 4

static const char *mgba_log_levels[] = {
    [MGBA_LOG_FATAL] = "FATAL",
    [MGBA_LOG_ERROR] = "ERROR",
    [MGBA_LOG_WARN] = "WARN",
    [MGBA_LOG_INFO] = "INFO",
    [MGBA_LOG_DEBUG] = "DEBUG"
};

static bool gba_parse_cartridge(gba_t *gba) {
    // uint8_t entrypoint = gba->bus->rom[0x00];

    memcpy(gba->rom_title, &gba->bus->rom[0xA0], sizeof(gba->rom_title));
    gba->rom_title[12] = '\0';

    // uint8_t game_type = gba->bus->rom[0xAC];
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
    //         eprintf("game type '%c' is not implemented yet", game_type);
    //         return false;
    //     default:
    //         eprintf("invalid game type: %c", game_type);
    //         return false;
    // }

    // char short_title[3]; // short_title is 2 chars
    // memcpy(short_title, &gba->bus->rom[0xAD], sizeof(short_title));
    // short_title[2] = '\0';

    if (gba->bus->rom[0xB2] != 0x96 || gba->bus->rom[0xB3] != 0x00)
        return false;

    for (int i = 0xB5; i < 0xBC; i++)
        if (gba->bus->rom[i] != 0x00)
            return false;

    uint8_t checksum = 0;
    for (int i = 0xA0; i < 0xBC; i++)
        checksum -= gba->bus->rom[i];
    checksum -= 0x19;

    // if (gba->bus->rom[0xBD] != checksum) {
    //     eprintf("Invalid cartridge header checksum");
    //     return false;
    // }

    if (gba->bus->rom[0xBE] != 0x00 && gba->bus->rom[0xBF] != 0x00)
        return false;

    // TODO multiboot entries

    return true;
}

static uint16_t io_read(gba_t *gba, uint16_t address) {
    uint32_t mask = 0xFFFF;
    address >>= 1;

    switch (address) {
    // LCD I/O Registers
    case IO_DISPCNT:
        LOG_DEBUG("IO_DISPCNT\n");
        break;
    case IO_GREENSWAP:
        LOG_DEBUG("IO_GREENSWAP\n");
        break;
    case IO_DISPSTAT:
        LOG_DEBUG("IO_DISPSTAT\n");
        break;
    case IO_VCOUNT:
        LOG_DEBUG("IO_VCOUNT\n");
        break;
    case IO_BG0CNT:
        LOG_DEBUG("IO_BG0CNT\n");
        break;
    case IO_BG1CNT:
        LOG_DEBUG("IO_BG1CNT\n");
        break;
    case IO_BG2CNT:
        LOG_DEBUG("IO_BG2CNT\n");
        break;
    case IO_BG3CNT:
        LOG_DEBUG("IO_BG3CNT\n");
        break;
    case IO_BG0HOFS:
        LOG_DEBUG("IO_BG0HOFS\n");
        break;
    case IO_BG0VOFS:
        LOG_DEBUG("IO_BG0VOFS\n");
        break;
    case IO_BG1HOFS:
        LOG_DEBUG("IO_BG1HOFS\n");
        break;
    case IO_BG1VOFS:
        LOG_DEBUG("IO_BG1VOFS\n");
        break;
    case IO_BG2HOFS:
        LOG_DEBUG("IO_BG2HOFS\n");
        break;
    case IO_BG2VOFS:
        LOG_DEBUG("IO_BG2VOFS\n");
        break;
    case IO_BG3HOFS:
        LOG_DEBUG("IO_BG3HOFS\n");
        break;
    case IO_BG3VOFS:
        LOG_DEBUG("IO_BG3VOFS\n");
        break;
    case IO_BG2PA:
        LOG_DEBUG("IO_BG2PA\n");
        break;
    case IO_BG2PB:
        LOG_DEBUG("IO_BG2PB\n");
        break;
    case IO_BG2PC:
        LOG_DEBUG("IO_BG2PC\n");
        break;
    case IO_BG2PD:
        LOG_DEBUG("IO_BG2PD\n");
        break;
    case IO_BG2X ... IO_BG2X + 1:
        LOG_DEBUG("IO_BG2X\n");
        break;
    case IO_BG2Y ... IO_BG2Y + 1:
        LOG_DEBUG("IO_BG2Y\n");
        break;
    case IO_BG3PA:
        LOG_DEBUG("IO_BG3PA\n");
        break;
    case IO_BG3PB:
        LOG_DEBUG("IO_BG3PB\n");
        break;
    case IO_BG3PC:
        LOG_DEBUG("IO_BG3PC\n");
        break;
    case IO_BG3PD:
        LOG_DEBUG("IO_BG3PD\n");
        break;
    case IO_BG3X ... IO_BG3X + 1:
        LOG_DEBUG("IO_BG3X\n");
        break;
    case IO_BG3Y ... IO_BG3Y + 1:
        LOG_DEBUG("IO_BG3Y\n");
        break;
    case IO_WIN0H:
        LOG_DEBUG("IO_WIN0H\n");
        break;
    case IO_WIN1H:
        LOG_DEBUG("IO_WIN1H\n");
        break;
    case IO_WIN0V:
        LOG_DEBUG("IO_WIN0V\n");
        break;
    case IO_WIN1V:
        LOG_DEBUG("IO_WIN1V\n");
        break;
    case IO_WININ:
        LOG_DEBUG("IO_WININ\n");
        break;
    case IO_WINOUT:
        LOG_DEBUG("IO_WINOUT\n");
        break;
    case IO_MOSAIC:
        LOG_DEBUG("IO_MOSAIC\n");
        break;
    case IO_BLDCNT:
        LOG_DEBUG("IO_BLDCNT\n");
        break;
    case IO_BLDALPHA:
        LOG_DEBUG("IO_BLDALPHA\n");
        break;
    case IO_BLDY:
        LOG_DEBUG("IO_BLDY\n");
        break;

    // Sound Registers
    case IO_SOUND1CNT_L:
        LOG_DEBUG("IO_SOUND1CNT_L\n");
        break;
    case IO_SOUND1CNT_H:
        LOG_DEBUG("IO_SOUND1CNT_H\n");
        break;
    case IO_SOUND1CNT_X:
        LOG_DEBUG("IO_SOUND1CNT_X\n");
        break;
    case IO_SOUND2CNT_L:
        LOG_DEBUG("IO_SOUND2CNT_L\n");
        break;
    case IO_SOUND2CNT_H:
        LOG_DEBUG("IO_SOUND2CNT_H\n");
        break;
    case IO_SOUND3CNT_L:
        LOG_DEBUG("IO_SOUND3CNT_L\n");
        break;
    case IO_SOUND3CNT_H:
        LOG_DEBUG("IO_SOUND3CNT_H\n");
        break;
    case IO_SOUND3CNT_X:
        LOG_DEBUG("IO_SOUND3CNT_X\n");
        break;
    case IO_SOUND4CNT_L:
        LOG_DEBUG("IO_SOUND4CNT_L\n");
        break;
    case IO_SOUND4CNT_H:
        LOG_DEBUG("IO_SOUND4CNT_H\n");
        break;
    case IO_SOUNDCNT_L:
        LOG_DEBUG("IO_SOUNDCNT_L\n");
        break;
    case IO_SOUNDCNT_H:
        LOG_DEBUG("IO_SOUNDCNT_H\n");
        break;
    case IO_SOUNDCNT_X:
        LOG_DEBUG("IO_SOUNDCNT_X\n");
        break;
    case IO_SOUNDBIAS:
        LOG_DEBUG("IO_SOUNDBIAS\n");
        break;
    case IO_WAVE_RAM ... IO_WAVE_RAM + 7:
        LOG_DEBUG("IO_WAVE_RAM\n");
        break;
    case IO_FIFO_A ... IO_FIFO_A + 1:
        LOG_DEBUG("IO_FIFO_A\n");
        break;
    case IO_FIFO_B ... IO_FIFO_B + 1:
        LOG_DEBUG("IO_FIFO_B\n");
        break;

    // DMA Transfer Channels
    case IO_DMA0SAD ... IO_DMA0SAD + 1:
        LOG_DEBUG("IO_DMA0SAD\n");
        break;
    case IO_DMA0DAD ... IO_DMA0DAD + 1:
        LOG_DEBUG("IO_DMA0DAD\n");
        break;
    case IO_DMA0CNT_L:
        LOG_DEBUG("IO_DMA0CNT_L\n");
        break;
    case IO_DMA0CNT_H:
        LOG_DEBUG("IO_DMA0CNT_H\n");
        break;
    case IO_DMA1SAD ... IO_DMA1SAD + 1:
        LOG_DEBUG("IO_DMA1SAD\n");
        break;
    case IO_DMA1DAD ... IO_DMA1DAD + 1:
        LOG_DEBUG("IO_DMA1DAD\n");
        break;
    case IO_DMA1CNT_L:
        LOG_DEBUG("IO_DMA1CNT_L\n");
        break;
    case IO_DMA1CNT_H:
        LOG_DEBUG("IO_DMA1CNT_H\n");
        break;
    case IO_DMA2SAD ... IO_DMA2SAD + 1:
        LOG_DEBUG("IO_DMA2SAD\n");
        break;
    case IO_DMA2DAD ... IO_DMA2DAD + 1:
        LOG_DEBUG("IO_DMA2DAD\n");
        break;
    case IO_DMA2CNT_L:
        LOG_DEBUG("IO_DMA2CNT_L\n");
        break;
    case IO_DMA2CNT_H:
        LOG_DEBUG("IO_DMA2CNT_H\n");
        break;
    case IO_DMA3SAD ... IO_DMA3SAD + 1:
        LOG_DEBUG("IO_DMA3SAD\n");
        break;
    case IO_DMA3DAD ... IO_DMA3DAD + 1:
        LOG_DEBUG("IO_DMA3DAD\n");
        break;
    case IO_DMA3CNT_L:
        LOG_DEBUG("IO_DMA3CNT_L\n");
        break;
    case IO_DMA3CNT_H:
        LOG_DEBUG("IO_DMA3CNT_H\n");
        break;

    // Timer Registers
    case IO_TM0CNT_L:
        LOG_DEBUG("IO_TM0CNT_L\n");
        break;
    case IO_TM0CNT_H:
        LOG_DEBUG("IO_TM0CNT_H\n");
        break;
    case IO_TM1CNT_L:
        LOG_DEBUG("IO_TM1CNT_L\n");
        break;
    case IO_TM1CNT_H:
        LOG_DEBUG("IO_TM1CNT_H\n");
        break;
    case IO_TM2CNT_L:
        LOG_DEBUG("IO_TM2CNT_L\n");
        break;
    case IO_TM2CNT_H:
        LOG_DEBUG("IO_TM2CNT_H\n");
        break;
    case IO_TM3CNT_L:
        LOG_DEBUG("IO_TM3CNT_L\n");
        break;
    case IO_TM3CNT_H:
        LOG_DEBUG("IO_TM3CNT_H\n");
        break;

    // Serial Communication (1)
    // case IO_SIODATA32 ... IO_SIODATA32 + 1:
    //     LOG_DEBUG("IO_SIODATA32\n");
    //     break;
    case IO_SIOMULTI0:
        LOG_DEBUG("IO_SIOMULTI0\n");
        break;
    case IO_SIOMULTI1:
        LOG_DEBUG("IO_SIOMULTI1\n");
        break;
    case IO_SIOMULTI2:
        LOG_DEBUG("IO_SIOMULTI2\n");
        break;
    case IO_SIOMULTI3:
        LOG_DEBUG("IO_SIOMULTI3\n");
        break;
    case IO_SIOCNT:
        LOG_DEBUG("IO_SIOCNT\n");
        break;
    case IO_SIOMLT_SEND:
        LOG_DEBUG("IO_SIOMLT_SEND\n");
        break;
    // case IO_SIODATA8:
    //     LOG_DEBUG("IO_SIODATA8\n");
    //     break;

    // Keypad Input
    case IO_KEYINPUT:
        LOG_DEBUG("IO_KEYINPUT\n");
        break;
    case IO_KEYCNT:
        LOG_DEBUG("IO_KEYCNT\n");
        break;

    // Serial Communication (2)
    case IO_RCNT:
        LOG_DEBUG("IO_RCNT\n");
        break;
    // case IO_IR:
    //     LOG_DEBUG("IO_IR\n");
    //     break;
    case IO_JOYCNT:
        LOG_DEBUG("IO_JOYCNT\n");
        break;
    case IO_JOY_RECV ... IO_JOY_RECV + 1:
        LOG_DEBUG("IO_JOY_RECV\n");
        break;
    case IO_JOY_TRANS ... IO_JOY_TRANS + 1:
        LOG_DEBUG("IO_JOY_TRANS\n");
        break;
    case IO_JOYSTAT:
        LOG_DEBUG("IO_JOYSTAT\n");
        break;

    // Interrupt, Waitstate, and Power-Down Control
    case IO_IE:
        mask = 0x3FFF;
        LOG_DEBUG("IO_IE\n");
        break;
    case IO_IF:
        mask = 0x3FFF;
        LOG_DEBUG("IO_IF\n");
        break;
    case IO_WAITCNT:
        LOG_DEBUG("IO_WAITCNT\n");
        break;
    case IO_IME:
        mask = 0x0001;
        LOG_DEBUG("IO_IME\n");
        break;
    case IO_POSTFLG_HALTCNT:
        LOG_DEBUG("IO_POSTFLG_HALTCNT\n");
        break;

    default:
        return 0x00;
    }

    return gba->bus->io[address] & mask;
}

static void io_write(gba_t *gba, uint16_t address, uint16_t data) {
    uint16_t mask = 0xFFFF;
    address >>= 1;

    switch (address) {
    // LCD I/O Registers
    case IO_DISPCNT:
        LOG_DEBUG("IO_DISPCNT 0x%04X\n", data);
        break;
    case IO_GREENSWAP:
        LOG_DEBUG("IO_GREENSWAP 0x%04X\n", data);
        break;
    case IO_DISPSTAT:
        mask = 0xFFF8;
        LOG_DEBUG("IO_DISPSTAT 0x%04X\n", data);
        break;
    case IO_VCOUNT:
        mask = 0x0000;
        LOG_DEBUG("IO_VCOUNT 0x%04X\n", data);
        break;
    case IO_BG0CNT:
        mask = 0xDFFF;
        LOG_DEBUG("IO_BG0CNT 0x%04X\n", data);
        break;
    case IO_BG1CNT:
        mask = 0xDFFF;
        LOG_DEBUG("IO_BG1CNT 0x%04X\n", data);
        break;
    case IO_BG2CNT:
        LOG_DEBUG("IO_BG2CNT 0x%04X\n", data);
        break;
    case IO_BG3CNT:
        LOG_DEBUG("IO_BG3CNT 0x%04X\n", data);
        break;
    case IO_BG0HOFS:
        mask = 0x03FF;
        LOG_DEBUG("IO_BG0HOFS 0x%04X\n", data);
        break;
    case IO_BG0VOFS:
        mask = 0x03FF;
        LOG_DEBUG("IO_BG0VOFS 0x%04X\n", data);
        break;
    case IO_BG1HOFS:
        mask = 0x03FF;
        LOG_DEBUG("IO_BG1HOFS 0x%04X\n", data);
        break;
    case IO_BG1VOFS:
        mask = 0x03FF;
        LOG_DEBUG("IO_BG1VOFS 0x%04X\n", data);
        break;
    case IO_BG2HOFS:
        mask = 0x03FF;
        LOG_DEBUG("IO_BG2HOFS 0x%04X\n", data);
        break;
    case IO_BG2VOFS:
        mask = 0x03FF;
        LOG_DEBUG("IO_BG2VOFS 0x%04X\n", data);
        break;
    case IO_BG3HOFS:
        mask = 0x03FF;
        LOG_DEBUG("IO_BG3HOFS 0x%04X\n", data);
        break;
    case IO_BG3VOFS:
        mask = 0x03FF;
        LOG_DEBUG("IO_BG3VOFS 0x%04X\n", data);
        break;
    case IO_BG2PA:
        LOG_DEBUG("IO_BG2PA 0x%04X\n", data);
        break;
    case IO_BG2PB:
        LOG_DEBUG("IO_BG2PB 0x%04X\n", data);
        break;
    case IO_BG2PC:
        LOG_DEBUG("IO_BG2PC 0x%04X\n", data);
        break;
    case IO_BG2PD:
        LOG_DEBUG("IO_BG2PD 0x%04X\n", data);
        break;
    case IO_BG2X ... IO_BG2X + 1:
        LOG_DEBUG("IO_BG2X 0x%04X\n", data);
        break;
    case IO_BG2Y ... IO_BG2Y + 1:
        LOG_DEBUG("IO_BG2Y 0x%04X\n", data);
        break;
    case IO_BG3PA:
        LOG_DEBUG("IO_BG3PA 0x%04X\n", data);
        break;
    case IO_BG3PB:
        LOG_DEBUG("IO_BG3PB 0x%04X\n", data);
        break;
    case IO_BG3PC:
        LOG_DEBUG("IO_BG3PC 0x%04X\n", data);
        break;
    case IO_BG3PD:
        LOG_DEBUG("IO_BG3PD 0x%04X\n", data);
        break;
    case IO_BG3X ... IO_BG3X + 1:
        LOG_DEBUG("IO_BG3X 0x%04X\n", data);
        break;
    case IO_BG3Y ... IO_BG3Y + 1:
        LOG_DEBUG("IO_BG3Y 0x%04X\n", data);
        break;
    case IO_WIN0H:
        LOG_DEBUG("IO_WIN0H 0x%04X\n", data);
        break;
    case IO_WIN1H:
        LOG_DEBUG("IO_WIN1H 0x%04X\n", data);
        break;
    case IO_WIN0V:
        LOG_DEBUG("IO_WIN0V 0x%04X\n", data);
        break;
    case IO_WIN1V:
        LOG_DEBUG("IO_WIN1V 0x%04X\n", data);
        break;
    case IO_WININ:
        mask = 0x3F3F;
        LOG_DEBUG("IO_WININ 0x%04X\n", data);
        break;
    case IO_WINOUT:
        mask = 0x3F3F;
        LOG_DEBUG("IO_WINOUT 0x%04X\n", data);
        break;
    case IO_MOSAIC:
        LOG_DEBUG("IO_MOSAIC 0x%04X\n", data);
        break;
    case IO_BLDCNT:
        mask = 0x3FFF;
        LOG_DEBUG("IO_BLDCNT 0x%04X\n", data);
        break;
    case IO_BLDALPHA:
        mask = 0x1F1F;
        LOG_DEBUG("IO_BLDALPHA 0x%04X\n", data);
        break;
    case IO_BLDY:
        LOG_DEBUG("IO_BLDY 0x%04X\n", data);
        break;

    // Sound Registers
    case IO_SOUND1CNT_L:
        mask = 0x007F;
        LOG_DEBUG("IO_SOUND1CNT_L 0x%04X\n", data);
        break;
    case IO_SOUND1CNT_H:
        mask = 0xFFC0;
        LOG_DEBUG("IO_SOUND1CNT_H 0x%04X\n", data);
        break;
    case IO_SOUND1CNT_X:
        mask = 0x4000;
        LOG_DEBUG("IO_SOUND1CNT_X 0x%04X\n", data);
        break;
    case IO_SOUND2CNT_L:
        mask = 0xFFC0;
        LOG_DEBUG("IO_SOUND2CNT_L 0x%04X\n", data);
        break;
    case IO_SOUND2CNT_H:
        mask = 0x4000;
        LOG_DEBUG("IO_SOUND2CNT_H 0x%04X\n", data);
        break;
    case IO_SOUND3CNT_L:
        mask = 0x00E0;
        LOG_DEBUG("IO_SOUND3CNT_L 0x%04X\n", data);
        break;
    case IO_SOUND3CNT_H:
        mask = 0xE000;
        LOG_DEBUG("IO_SOUND3CNT_H 0x%04X\n", data);
        break;
    case IO_SOUND3CNT_X:
        mask = 0x4000;
        LOG_DEBUG("IO_SOUND3CNT_X 0x%04X\n", data);
        break;
    case IO_SOUND4CNT_L:
        mask = 0xFF00;
        LOG_DEBUG("IO_SOUND4CNT_L 0x%04X\n", data);
        break;
    case IO_SOUND4CNT_H:
        mask = 0x40FF;
        LOG_DEBUG("IO_SOUND4CNT_H 0x%04X\n", data);
        break;
    case IO_SOUNDCNT_L:
        mask = 0xFF77;
        LOG_DEBUG("IO_SOUNDCNT_L 0x%04X\n", data);
        break;
    case IO_SOUNDCNT_H:
        mask = 0x770F;
        LOG_DEBUG("IO_SOUNDCNT_H 0x%04X\n", data);
        break;
    case IO_SOUNDCNT_X:
        mask = 0x0080;
        LOG_DEBUG("IO_SOUNDCNT_X 0x%04X\n", data);
        break;
    case IO_SOUNDBIAS:
        LOG_DEBUG("IO_SOUNDBIAS 0x%04X\n", data);
        break;
    case IO_WAVE_RAM ... IO_WAVE_RAM + 7:
        LOG_DEBUG("IO_WAVE_RAM 0x%04X\n", data);
        break;
    case IO_FIFO_A ... IO_FIFO_A + 1:
        LOG_DEBUG("IO_FIFO_A 0x%04X\n", data);
        break;
    case IO_FIFO_B ... IO_FIFO_B + 1:
        LOG_DEBUG("IO_FIFO_B 0x%04X\n", data);
        break;

    // DMA Transfer Channels
    case IO_DMA0SAD ... IO_DMA0SAD + 1:
        LOG_DEBUG("IO_DMA0SAD 0x%04X\n", data);
        break;
    case IO_DMA0DAD ... IO_DMA0DAD + 1:
        LOG_DEBUG("IO_DMA0DAD 0x%04X\n", data);
        break;
    case IO_DMA0CNT_L:
        LOG_DEBUG("IO_DMA0CNT_L 0x%04X\n", data);
        break;
    case IO_DMA0CNT_H:
        LOG_DEBUG("IO_DMA0CNT_H 0x%04X\n", data);
        break;
    case IO_DMA1SAD ... IO_DMA1SAD + 1:
        LOG_DEBUG("IO_DMA1SAD 0x%04X\n", data);
        break;
    case IO_DMA1DAD ... IO_DMA1DAD + 1:
        LOG_DEBUG("IO_DMA1DAD 0x%04X\n", data);
        break;
    case IO_DMA1CNT_L:
        LOG_DEBUG("IO_DMA1CNT_L 0x%04X\n", data);
        break;
    case IO_DMA1CNT_H:
        LOG_DEBUG("IO_DMA1CNT_H 0x%04X\n", data);
        break;
    case IO_DMA2SAD ... IO_DMA2SAD + 1:
        LOG_DEBUG("IO_DMA2SAD 0x%04X\n", data);
        break;
    case IO_DMA2DAD ... IO_DMA2DAD + 1:
        LOG_DEBUG("IO_DMA2DAD 0x%04X\n", data);
        break;
    case IO_DMA2CNT_L:
        LOG_DEBUG("IO_DMA2CNT_L 0x%04X\n", data);
        break;
    case IO_DMA2CNT_H:
        LOG_DEBUG("IO_DMA2CNT_H 0x%04X\n", data);
        break;
    case IO_DMA3SAD ... IO_DMA3SAD + 1:
        LOG_DEBUG("IO_DMA3SAD 0x%04X\n", data);
        break;
    case IO_DMA3DAD ... IO_DMA3DAD + 1:
        LOG_DEBUG("IO_DMA3DAD 0x%04X\n", data);
        break;
    case IO_DMA3CNT_L:
        LOG_DEBUG("IO_DMA3CNT_L 0x%04X\n", data);
        break;
    case IO_DMA3CNT_H:
        LOG_DEBUG("IO_DMA3CNT_H 0x%04X\n", data);
        break;

    // Timer Registers
    case IO_TM0CNT_L:
        LOG_DEBUG("IO_TM0CNT_L 0x%04X\n", data);
        gba->tmr->instance[0].reload = data;
        break;
    case IO_TM0CNT_H:
        LOG_DEBUG("IO_TM0CNT_H 0x%04X\n", data);
        gba_tmr_set(gba, data, 0);
        break;
    case IO_TM1CNT_L:
        LOG_DEBUG("IO_TM1CNT_L 0x%04X\n", data);
        gba->tmr->instance[1].reload = data;
        break;
    case IO_TM1CNT_H:
        LOG_DEBUG("IO_TM1CNT_H 0x%04X\n", data);
        gba_tmr_set(gba, data, 1);
        break;
    case IO_TM2CNT_L:
        LOG_DEBUG("IO_TM2CNT_L 0x%04X\n", data);
        gba->tmr->instance[2].reload = data;
        break;
    case IO_TM2CNT_H:
        LOG_DEBUG("IO_TM2CNT_H 0x%04X\n", data);
        gba_tmr_set(gba, data, 2);
        break;
    case IO_TM3CNT_L:
        LOG_DEBUG("IO_TM3CNT_L 0x%04X\n", data);
        gba->tmr->instance[3].reload = data;
        break;
    case IO_TM3CNT_H:
        LOG_DEBUG("IO_TM3CNT_H 0x%04X\n", data);
        gba_tmr_set(gba, data, 3);
        break;

    // Serial Communication (1)
    // case IO_SIODATA32 ... IO_SIODATA32 + 1:
    //     LOG_DEBUG("IO_SIODATA32 0x%04X\n", data);
    //     break;
    case IO_SIOMULTI0:
        LOG_DEBUG("IO_SIOMULTI0 0x%04X\n", data);
        break;
    case IO_SIOMULTI1:
        LOG_DEBUG("IO_SIOMULTI1 0x%04X\n", data);
        break;
    case IO_SIOMULTI2:
        LOG_DEBUG("IO_SIOMULTI2 0x%04X\n", data);
        break;
    case IO_SIOMULTI3:
        LOG_DEBUG("IO_SIOMULTI3 0x%04X\n", data);
        break;
    case IO_SIOCNT:
        LOG_DEBUG("IO_SIOCNT 0x%04X\n", data);
        break;
    case IO_SIOMLT_SEND:
        LOG_DEBUG("IO_SIOMLT_SEND 0x%04X\n", data);
        break;
    // case IO_SIODATA8:
    //     LOG_DEBUG("IO_SIODATA8 0x%04X\n", data);
    //     break;

    // Keypad Input
    case IO_KEYINPUT:
        LOG_DEBUG("IO_KEYINPUT 0x%04X\n", data);
        break;
    case IO_KEYCNT:
        LOG_DEBUG("IO_KEYCNT 0x%04X\n", data);
        break;

    // Serial Communication (2)
    case IO_RCNT:
        LOG_DEBUG("IO_RCNT 0x%04X\n", data);
        break;
    // case IO_IR:
    //     LOG_DEBUG("IO_IR 0x%04X\n", data);
    //     break;
    case IO_JOYCNT:
        LOG_DEBUG("IO_JOYCNT 0x%04X\n", data);
        break;
    case IO_JOY_RECV ... IO_JOY_RECV + 1:
        LOG_DEBUG("IO_JOY_RECV 0x%04X\n", data);
        break;
    case IO_JOY_TRANS ... IO_JOY_TRANS + 1:
        LOG_DEBUG("IO_JOY_TRANS 0x%04X\n", data);
        break;
    case IO_JOYSTAT:
        LOG_DEBUG("IO_JOYSTAT 0x%04X\n", data);
        break;

    // Interrupt, Waitstate, and Power-Down Control
    case IO_IE:
        LOG_DEBUG("IO_IE 0x%04X\n", data);
        break;
    case IO_IF:
        LOG_DEBUG("IO_IF 0x%04X\n", data);
        data = gba->bus->io[address] & ~data;
        break;
    case IO_WAITCNT:
        LOG_DEBUG("IO_WAITCNT 0x%04X\n", data);
        break;
    case IO_IME:
        mask = 0x0001;
        LOG_DEBUG("IO_IME 0x%04X\n", data);
        break;
    case IO_POSTFLG_HALTCNT:
        LOG_DEBUG("IO_POSTFLG_HALTCNT 0x%04X\n", data);
        break;

    default:
        break;
    }

    CHANGE_BITS(gba->bus->io[address], mask, data);
}

// TODO this function is too big and ugly
static void bus_access(gba_t *gba, uint8_t mode, uint32_t address, uint32_t data) {
    gba_bus_t *bus = gba->bus;
    bus_access_t access = mode & 0x01;
    bool is_write = mode & BUS_ACCESS_WRITE;
    uint8_t size = ((mode >> 2) & 0x03) + 1;

    address = ALIGN(address, size);

    void *ret = NULL;

    switch (address) {
    case BUS_BIOS ... BUS_BIOS_UNUSED - 1:
        if (is_write)
            break;
        if (gba->cpu->regs[REG_PC] >= BUS_BIOS_UNUSED) {
            ret = &bus->last_fetched_bios_instr;
            break;
        }
        ret = &bus->bios[address - BUS_BIOS];
        break;
    case BUS_EWRAM ... BUS_IWRAM - 1:
        ret = &bus->ewram[(address - BUS_EWRAM) % (BUS_EWRAM_UNUSED - BUS_EWRAM)];
        break;
    case BUS_IWRAM ... BUS_IO - 1:
        ret = &bus->iwram[(address - BUS_IWRAM) % (BUS_IWRAM_UNUSED - BUS_IWRAM)];
        break;
    case BUS_IO ... BUS_IO_UNUSED - 1:
        ret = bus->io;
        uint32_t io_addr = address - BUS_IO;

        if (is_write) {
            io_write(gba, io_addr, data);
            if (size == 4)
                io_write(gba, io_addr + 2, data >> 16);
        } else {
            static uint32_t io_data = 0;
            ret = &io_data;

            io_data = io_read(gba, io_addr);
            if (size == 4)
                io_data |= io_read(gba, io_addr + 2) << 16;
        }
        break;
    case MGBA_REG_DEBUG_STRING ... (MGBA_REG_DEBUG_STRING + 0x100) - 1:
        ret = &gba->bus->mgba_logstr[address - MGBA_REG_DEBUG_STRING];
        break;
    case MGBA_REG_DEBUG_FLAGS:
        if (is_write) {
            printf("[mGBA LOG] %.256s\n", gba->bus->mgba_logstr);
            ret = bus->io;
            break;
        }
        break;
    case MGBA_REG_DEBUG_ENABLE:
        // if (is_write) {
        //     if (data == MGBA_LOG_ENABLE_REQ_MAGIC)
        //         enable mgba logs...
        //     else
        //         disable mgba logs...
        // }
        if (!is_write /*&& mgba logs enabled*/) {
            static uint32_t magic = MGBA_LOG_ENABLE_RES_MAGIC;
            ret = &magic;
            break;
        }
        break;
    case BUS_PRAM ... BUS_VRAM - 1:
        ret = &bus->pram[(address - BUS_PRAM) % (BUS_PRAM_UNUSED - BUS_PRAM)];
        if (is_write && size == 1) {
            size = 2; // bus is 16 or 32 bits wide --> if size == 1, force to 2
            data |= data << 8;
        }
        break;
    case BUS_VRAM ... BUS_OAM - 1:
        uint32_t vram_upper_bound = PPU_GET_MODE(gba) < 3 ? 0x10000 : 0x14000;
        address = (address - (BUS_VRAM_UNUSED + 0x8000)) % 0x20000;

        if (address >= vram_upper_bound && address >= 0x18000) {
            ret = is_write ? NULL : &bus->vram[address % 0x8000];
        } else {
            ret = &bus->vram[address % 0x20000];
        }

        if (is_write && size == 1) {
            size = 2; // bus is 16 or 32 bits wide --> if size == 1, force to 2
            data |= data << 8;
        }
        break;
    case BUS_OAM ... BUS_ROM0 - 1:
        if (is_write && size == 1)
            break;
        ret = &bus->oam[(address - BUS_OAM_UNUSED) % (BUS_OAM_UNUSED - BUS_OAM)];
        break;
    case BUS_ROM0 ... BUS_ROM1 - 1:
    case BUS_ROM1 ... BUS_ROM2 - 1:
    case BUS_ROM2 ... BUS_SRAM - 1:
        if (access == BUS_ACCESS_N)
            bus->rom_address_latch = address;
        else if (access == BUS_ACCESS_S)
            bus->rom_address_latch += size;
        ret = is_write ? NULL : &bus->rom[bus->rom_address_latch & 0x00FFFFFF];
        break;
    case BUS_SRAM ... BUS_SRAM_UNUSED - 1:
        if (!is_write) {
            data = bus->sram[address - BUS_SRAM];
            if (size == 2)
                data *= 0x0101;
            else if (size == 4)
                data *= 0x01010101;
            ret = &data;
            break;
        }

        ret = &bus->sram[address - BUS_SRAM];
        break;
    default:
        break;
    }

    if (!ret) // open bus
        return;

    switch (size) {
    case 1: {
        uint8_t *tmp = is_write ? &data : ret;
        bus->data_latch = ((uint32_t) *tmp << 24) | ((uint32_t) *tmp << 16) | ((uint32_t) *tmp << 8) | (uint32_t) *tmp;
        if (is_write && ret != bus->io)
            *((uint8_t *) ret) = bus->data_latch;
        break;
    }
    case 2: {
        uint16_t *tmp = is_write ? &data : ret;
        bus->data_latch = ((uint32_t) *tmp << 16) | (uint32_t) *tmp;
        if (is_write && ret != bus->io)
            *((uint16_t *) ret) = bus->data_latch;
        break;
    }
    case 4: {
        uint32_t *tmp = is_write ? &data : ret;
        bus->data_latch = *tmp;
        if (is_write && ret != bus->io)
            *((uint32_t *) ret) = bus->data_latch;
        break;
    }
    default:
        todo();
        break;
    }
}

uint8_t _gba_bus_read_byte(gba_t *gba, bus_access_t access, uint32_t address) {
    bus_access(gba, BUS_ACCESS_SIZE(1) | BUS_ACCESS_READ | access, address, 0);
    return gba->bus->data_latch;
}

uint16_t _gba_bus_read_half(gba_t *gba, bus_access_t access, uint32_t address) {
    bus_access(gba, BUS_ACCESS_SIZE(2) | BUS_ACCESS_READ | access, address, 0);
    return gba->bus->data_latch;
}

uint32_t _gba_bus_read_word(gba_t *gba, bus_access_t access, uint32_t address) {
    bus_access(gba, BUS_ACCESS_SIZE(4) | BUS_ACCESS_READ | access, address, 0);
    return gba->bus->data_latch;
}

void _gba_bus_write_byte(gba_t *gba, bus_access_t access, uint32_t address, uint8_t data) {
    bus_access(gba, BUS_ACCESS_SIZE(1) | BUS_ACCESS_WRITE | access, address, data);
}

void _gba_bus_write_half(gba_t *gba, bus_access_t access, uint32_t address, uint16_t data) {
    bus_access(gba, BUS_ACCESS_SIZE(2) | BUS_ACCESS_WRITE | access, address, data);
}

void _gba_bus_write_word(gba_t *gba, bus_access_t access, uint32_t address, uint32_t data) {
    bus_access(gba, BUS_ACCESS_SIZE(4) | BUS_ACCESS_WRITE | access, address, data);
}

// TODO this shouldn't be responsible for cartridge loading and parsing (same for gb_mmu_t)
bool gba_bus_init(gba_t *gba, const uint8_t *rom, size_t rom_size) {
    if (!rom || rom_size < 0xBF)
        return false;

    gba->bus = xcalloc(1, sizeof(*gba->bus));
    memcpy(gba->bus->rom, rom, rom_size);

    if (!gba_parse_cartridge(gba)) {
        gba_bus_quit(gba);
        return false;
    }

    gba->bus->rom_size = rom_size;

    gba->bus->io[IO_KEYINPUT] = 0x03FF;

    // TODO do not load bios from hardcoded file path
    FILE *f = fopen("/home/maxime/dev/gbmulator/src/bootroms/gba/gba_bios.bin", "r");
    if (!f) {
        gba_bus_quit(gba);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    fread(gba->bus->bios, sz, 1, f);
    fclose(f);

    return true;
}

void gba_bus_quit(gba_t *gba) {
    free(gba->bus);
}
