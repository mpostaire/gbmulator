#include <stdlib.h>

#include "gba_priv.h"

static bool gba_parse_cartridge(gba_t *gba) {
    // uint8_t entrypoint = gba->bus->game_rom[0x00];

    memcpy(gba->rom_title, &gba->bus->game_rom[0xA0], sizeof(gba->rom_title));
    gba->rom_title[12] = '\0';

    // uint8_t game_type = gba->bus->game_rom[0xAC];
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
    // memcpy(short_title, &gba->bus->game_rom[0xAD], sizeof(short_title));
    // short_title[2] = '\0';

    if (gba->bus->game_rom[0xB2] != 0x96 || gba->bus->game_rom[0xB3] != 0x00)
        return false;

    for (int i = 0xB5; i < 0xBC; i++)
        if (gba->bus->game_rom[i] != 0x00)
            return false;

    uint8_t checksum = 0;
    for (int i = 0xA0; i < 0xBC; i++)
        checksum -= gba->bus->game_rom[i];
    checksum -= 0x19;

    if (gba->bus->game_rom[0xBD] != checksum) {
        eprintf("Invalid cartridge header checksum");
        return false;
    }

    if (gba->bus->game_rom[0xBE] != 0x00 && gba->bus->game_rom[0xBF] != 0x00)
        return false;

    // TODO multiboot entries

    return true;
}

static uint32_t read_open_bus(gba_t *gba, uint32_t address) {
    return 0xFFFFFFFF;
}

static uint8_t io_regs_read_byte(gba_t *gba, uint16_t address) {
    switch (address) {
    // LCD I/O Registers
    case IO_DISPCNT ... IO_DISPCNT + 1:
        // todo("IO_DISPCNT\n");
        break;
    case IO_GREENSWAP ... IO_GREENSWAP + 1:
        todo("IO_GREENSWAP\n");
        break;
    case IO_DISPSTAT ... IO_DISPSTAT + 1:
        LOG_DEBUG("IO_DISPSTAT\n");
        break;
    case IO_VCOUNT ... IO_VCOUNT + 1:

        if (gba->cpu->regs[12] == 0) {
            todo("SUCCESS");
        } else {
            todo("FAILURE on test %d", gba->cpu->regs[12]);
        }

        LOG_DEBUG("IO_VCOUNT\n");
        break;
    case IO_BG0CNT ... IO_BG0CNT + 1:
        todo("IO_BG0CNT\n");
        break;
    case IO_BG1CNT ... IO_BG1CNT + 1:
        todo("IO_BG1CNT\n");
        break;
    case IO_BG2CNT ... IO_BG2CNT + 1:
        todo("IO_BG2CNT\n");
        break;
    case IO_BG3CNT ... IO_BG3CNT + 1:
        todo("IO_BG3CNT\n");
        break;
    case IO_BG0HOFS ... IO_BG0HOFS + 1:
        todo("IO_BG0HOFS\n");
        break;
    case IO_BG0VOFS ... IO_BG0VOFS + 1:
        todo("IO_BG0VOFS\n");
        break;
    case IO_BG1HOFS ... IO_BG1HOFS + 1:
        todo("IO_BG1HOFS\n");
        break;
    case IO_BG1VOFS ... IO_BG1VOFS + 1:
        todo("IO_BG1VOFS\n");
        break;
    case IO_BG2HOFS ... IO_BG2HOFS + 1:
        todo("IO_BG2HOFS\n");
        break;
    case IO_BG2VOFS ... IO_BG2VOFS + 1:
        todo("IO_BG2VOFS\n");
        break;
    case IO_BG3HOFS ... IO_BG3HOFS + 1:
        todo("IO_BG3HOFS\n");
        break;
    case IO_BG3VOFS ... IO_BG3VOFS + 1:
        todo("IO_BG3VOFS\n");
        break;
    case IO_BG2PA ... IO_BG2PA + 1:
        todo("IO_BG2PA\n");
        break;
    case IO_BG2PB ... IO_BG2PB + 1:
        todo("IO_BG2PB\n");
        break;
    case IO_BG2PC ... IO_BG2PC + 1:
        todo("IO_BG2PC\n");
        break;
    case IO_BG2PD ... IO_BG2PD + 1:
        todo("IO_BG2PD\n");
        break;
    case IO_BG2X ... IO_BG2X + 3:
        todo("IO_BG2X\n");
        break;
    case IO_BG2Y ... IO_BG2Y + 3:
        todo("IO_BG2Y\n");
        break;
    case IO_BG3PA ... IO_BG3PA + 1:
        todo("IO_BG3PA\n");
        break;
    case IO_BG3PB ... IO_BG3PB + 1:
        todo("IO_BG3PB\n");
        break;
    case IO_BG3PC ... IO_BG3PC + 1:
        todo("IO_BG3PC\n");
        break;
    case IO_BG3PD ... IO_BG3PD + 1:
        todo("IO_BG3PD\n");
        break;
    case IO_BG3X ... IO_BG3X + 3:
        todo("IO_BG3X\n");
        break;
    case IO_BG3Y ... IO_BG3Y + 3:
        todo("IO_BG3Y\n");
        break;
    case IO_WIN0H ... IO_WIN0H + 1:
        todo("IO_WIN0H\n");
        break;
    case IO_WIN1H ... IO_WIN1H + 1:
        todo("IO_WIN1H\n");
        break;
    case IO_WIN0V ... IO_WIN0V + 1:
        todo("IO_WIN0V\n");
        break;
    case IO_WIN1V ... IO_WIN1V + 1:
        todo("IO_WIN1V\n");
        break;
    case IO_WININ ... IO_WININ + 1:
        todo("IO_WININ\n");
        break;
    case IO_WINOUT ... IO_WINOUT + 1:
        todo("IO_WINOUT\n");
        break;
    case IO_MOSAIC ... IO_MOSAIC + 1:
        todo("IO_MOSAIC\n");
        break;
    case IO_BLDCNT ... IO_BLDCNT + 1:
        todo("IO_BLDCNT\n");
        break;
    case IO_BLDALPHA ... IO_BLDALPHA + 1:
        todo("IO_BLDALPHA\n");
        break;
    case IO_BLDY ... IO_BLDY + 1:
        todo("IO_BLDY\n");
        break;

    // Sound Registers
    case IO_SOUND1CNT_L ...  IO_SOUND1CNT_L + 1:
        todo("IO_SOUND1CNT_L\n");
        break;
    case IO_SOUND1CNT_H ... IO_SOUND1CNT_H + 1:
        todo("IO_SOUND1CNT_H\n");
        break;
    case IO_SOUND1CNT_X ... IO_SOUND1CNT_X + 1:
        todo("IO_SOUND1CNT_X\n");
        break;
    case IO_SOUND2CNT_L ... IO_SOUND2CNT_L + 1:
        todo("IO_SOUND2CNT_L\n");
        break;
    case IO_SOUND2CNT_H ... IO_SOUND2CNT_H + 1:
        todo("IO_SOUND2CNT_H\n");
        break;
    case IO_SOUND3CNT_L ... IO_SOUND3CNT_L + 1:
        todo("IO_SOUND3CNT_L\n");
        break;
    case IO_SOUND3CNT_H ... IO_SOUND3CNT_H + 1:
        todo("IO_SOUND3CNT_H\n");
        break;
    case IO_SOUND3CNT_X ... IO_SOUND3CNT_X + 1:
        todo("IO_SOUND3CNT_X\n");
        break;
    case IO_SOUND4CNT_L ... IO_SOUND4CNT_L + 1:
        todo("IO_SOUND4CNT_L\n");
        break;
    case IO_SOUND4CNT_H ... IO_SOUND4CNT_H + 1:
        todo("IO_SOUND4CNT_H\n");
        break;
    case IO_SOUNDCNT_L ... IO_SOUNDCNT_L + 1:
        todo("IO_SOUNDCNT_L\n");
        break;
    case IO_SOUNDCNT_H ... IO_SOUNDCNT_H + 1:
        todo("IO_SOUNDCNT_H\n");
        break;
    case IO_SOUNDCNT_X ... IO_SOUNDCNT_X + 1:
        todo("IO_SOUNDCNT_X\n");
        break;
    case IO_SOUNDBIAS ... IO_SOUNDBIAS + 1:
        todo("IO_SOUNDBIAS\n");
        break;
    case IO_WAVE_RAM ... IO_WAVE_RAM + 15:
        todo("IO_WAVE_RAM\n");
        break;
    case IO_FIFO_A ... IO_FIFO_A + 3:
        todo("IO_FIFO_A\n");
        break;
    case IO_FIFO_B ... IO_FIFO_B + 3:
        todo("IO_FIFO_B\n");
        break;

    // DMA Transfer Channels
    case IO_DMA0SAD ... IO_DMA0SAD + 3:
        todo("IO_DMA0SAD\n");
        break;
    case IO_DMA0DAD ... IO_DMA0DAD + 3:
        todo("IO_DMA0DAD\n");
        break;
    case IO_DMA0CNT_L ... IO_DMA0CNT_L + 1:
        todo("IO_DMA0CNT_L\n");
        break;
    case IO_DMA0CNT_H ... IO_DMA0CNT_H + 1:
        todo("IO_DMA0CNT_H\n");
        break;
    case IO_DMA1SAD ... IO_DMA1SAD + 3:
        todo("IO_DMA1SAD\n");
        break;
    case IO_DMA1DAD ... IO_DMA1DAD + 3:
        todo("IO_DMA1DAD\n");
        break;
    case IO_DMA1CNT_L ... IO_DMA1CNT_L + 1:
        todo("IO_DMA1CNT_L\n");
        break;
    case IO_DMA1CNT_H ... IO_DMA1CNT_H + 1:
        todo("IO_DMA1CNT_H\n");
        break;
    case IO_DMA2SAD ... IO_DMA2SAD + 3:
        todo("IO_DMA2SAD\n");
        break;
    case IO_DMA2DAD ... IO_DMA2DAD + 3:
        todo("IO_DMA2DAD\n");
        break;
    case IO_DMA2CNT_L ... IO_DMA2CNT_L + 1:
        todo("IO_DMA2CNT_L\n");
        break;
    case IO_DMA2CNT_H ... IO_DMA2CNT_H + 1:
        todo("IO_DMA2CNT_H\n");
        break;
    case IO_DMA3SAD ... IO_DMA3SAD + 3:
        todo("IO_DMA3SAD\n");
        break;
    case IO_DMA3DAD ... IO_DMA3DAD + 3:
        todo("IO_DMA3DAD\n");
        break;
    case IO_DMA3CNT_L ... IO_DMA3CNT_L + 1:
        todo("IO_DMA3CNT_L\n");
        break;
    case IO_DMA3CNT_H ... IO_DMA3CNT_H + 1:
        todo("IO_DMA3CNT_H\n");
        break;

    // Timer Registers
    case IO_TM0CNT_L ... IO_TM0CNT_L + 1:
        todo("IO_TM0CNT_L\n");
        break;
    case IO_TM0CNT_H ... IO_TM0CNT_H + 1:
        todo("IO_TM0CNT_H\n");
        break;
    case IO_TM1CNT_L ... IO_TM1CNT_L + 1:
        todo("IO_TM1CNT_L\n");
        break;
    case IO_TM1CNT_H ... IO_TM1CNT_H + 1:
        todo("IO_TM1CNT_H\n");
        break;
    case IO_TM2CNT_L ... IO_TM2CNT_L + 1:
        todo("IO_TM2CNT_L\n");
        break;
    case IO_TM2CNT_H ... IO_TM2CNT_H + 1:
        todo("IO_TM2CNT_H\n");
        break;
    case IO_TM3CNT_L ... IO_TM3CNT_L + 1:
        todo("IO_TM3CNT_L\n");
        break;
    case IO_TM3CNT_H ... IO_TM3CNT_H + 1:
        todo("IO_TM3CNT_H\n");
        break;

    // Serial Communication (1)
    // case IO_SIODATA32 ... IO_SIODATA32 + 3:
    //     todo("IO_SIODATA32\n");
    //     break;
    case IO_SIOMULTI0 ... IO_SIOMULTI0 + 1:
        todo("IO_SIOMULTI0\n");
        break;
    case IO_SIOMULTI1 ... IO_SIOMULTI1 + 1:
        todo("IO_SIOMULTI1\n");
        break;
    case IO_SIOMULTI2 ... IO_SIOMULTI2 + 1:
        todo("IO_SIOMULTI2\n");
        break;
    case IO_SIOMULTI3 ... IO_SIOMULTI3 + 1:
        todo("IO_SIOMULTI3\n");
        break;
    case IO_SIOCNT ... IO_SIOCNT + 1:
        todo("IO_SIOCNT\n");
        break;
    case IO_SIOMLT_SEND ... IO_SIOMLT_SEND + 1:
        todo("IO_SIOMLT_SEND\n");
        break;
    // case IO_SIODATA8:
    //     todo("IO_SIODATA8\n");
    //     break;

    // Keypad Input
    case IO_KEYINPUT ... IO_KEYINPUT + 1:
        todo("IO_KEYINPUT\n");
        break;
    case IO_KEYCNT ... IO_KEYCNT + 1:
        todo("IO_KEYCNT\n");
        break;

    // Serial Communication (2)
    case IO_RCNT ... IO_RCNT + 1:
        todo("IO_RCNT\n");
        break;
    // case IO_IR:
    //     todo("IO_IR\n");
    //     break;
    case IO_JOYCNT ... IO_JOYCNT + 1:
        todo("IO_JOYCNT\n");
        break;
    case IO_JOY_RECV ... IO_JOY_RECV + 3:
        todo("IO_JOY_RECV\n");
        break;
    case IO_JOY_TRANS ... IO_JOY_TRANS + 3:
        todo("IO_JOY_TRANS\n");
        break;
    case IO_JOYSTAT ... IO_JOYSTAT + 1:
        todo("IO_JOYSTAT\n");
        break;

    // Interrupt, Waitstate, and Power-Down Control
    case IO_IE ... IO_IE + 1:
        todo("IO_IE\n");
        break;
    case IO_IF ... IO_IF + 1:
        todo("IO_IF\n");
        break;
    case IO_WAITCNT ... IO_WAITCNT + 1:
        todo("IO_WAITCNT\n");
        break;
    case IO_IME ... IO_IME + 1:
        LOG_DEBUG("IO_IME\n");
        break;
    case IO_POSTFLG:
        todo("IO_POSTFLG\n");
        break;
    case IO_HALTCNT:
        todo("IO_HALTCNT\n");
        break;

    default:
        todo();
        return 0x00;
    }

    return gba->bus->io_regs[address];
}

static void io_regs_write_byte(gba_t *gba, uint16_t address, uint8_t data) {
    switch (address) {
    // LCD I/O Registers
    case IO_DISPCNT ... IO_DISPCNT + 1:
        // todo("IO_DISPCNT\n");
        break;
    case IO_GREENSWAP ... IO_GREENSWAP + 1:
        todo("IO_GREENSWAP\n");
        break;
    case IO_DISPSTAT ... IO_DISPSTAT + 1:
        LOG_DEBUG("IO_DISPSTAT\n");
        break;
    case IO_VCOUNT ... IO_VCOUNT + 1:
        todo("IO_VCOUNT\n");
        break;
    case IO_BG0CNT ... IO_BG0CNT + 1:
        todo("IO_BG0CNT\n");
        break;
    case IO_BG1CNT ... IO_BG1CNT + 1:
        todo("IO_BG1CNT\n");
        break;
    case IO_BG2CNT ... IO_BG2CNT + 1:
        todo("IO_BG2CNT\n");
        break;
    case IO_BG3CNT ... IO_BG3CNT + 1:
        todo("IO_BG3CNT\n");
        break;
    case IO_BG0HOFS ... IO_BG0HOFS + 1:
        todo("IO_BG0HOFS\n");
        break;
    case IO_BG0VOFS ... IO_BG0VOFS + 1:
        todo("IO_BG0VOFS\n");
        break;
    case IO_BG1HOFS ... IO_BG1HOFS + 1:
        todo("IO_BG1HOFS\n");
        break;
    case IO_BG1VOFS ... IO_BG1VOFS + 1:
        todo("IO_BG1VOFS\n");
        break;
    case IO_BG2HOFS ... IO_BG2HOFS + 1:
        todo("IO_BG2HOFS\n");
        break;
    case IO_BG2VOFS ... IO_BG2VOFS + 1:
        todo("IO_BG2VOFS\n");
        break;
    case IO_BG3HOFS ... IO_BG3HOFS + 1:
        todo("IO_BG3HOFS\n");
        break;
    case IO_BG3VOFS ... IO_BG3VOFS + 1:
        todo("IO_BG3VOFS\n");
        break;
    case IO_BG2PA ... IO_BG2PA + 1:
        todo("IO_BG2PA\n");
        break;
    case IO_BG2PB ... IO_BG2PB + 1:
        todo("IO_BG2PB\n");
        break;
    case IO_BG2PC ... IO_BG2PC + 1:
        todo("IO_BG2PC\n");
        break;
    case IO_BG2PD ... IO_BG2PD + 1:
        todo("IO_BG2PD\n");
        break;
    case IO_BG2X ... IO_BG2X + 3:
        todo("IO_BG2X\n");
        break;
    case IO_BG2Y ... IO_BG2Y + 3:
        todo("IO_BG2Y\n");
        break;
    case IO_BG3PA ... IO_BG3PA + 1:
        todo("IO_BG3PA\n");
        break;
    case IO_BG3PB ... IO_BG3PB + 1:
        todo("IO_BG3PB\n");
        break;
    case IO_BG3PC ... IO_BG3PC + 1:
        todo("IO_BG3PC\n");
        break;
    case IO_BG3PD ... IO_BG3PD + 1:
        todo("IO_BG3PD\n");
        break;
    case IO_BG3X ... IO_BG3X + 3:
        todo("IO_BG3X\n");
        break;
    case IO_BG3Y ... IO_BG3Y + 3:
        todo("IO_BG3Y\n");
        break;
    case IO_WIN0H ... IO_WIN0H + 1:
        todo("IO_WIN0H\n");
        break;
    case IO_WIN1H ... IO_WIN1H + 1:
        todo("IO_WIN1H\n");
        break;
    case IO_WIN0V ... IO_WIN0V + 1:
        todo("IO_WIN0V\n");
        break;
    case IO_WIN1V ... IO_WIN1V + 1:
        todo("IO_WIN1V\n");
        break;
    case IO_WININ ... IO_WININ + 1:
        todo("IO_WININ\n");
        break;
    case IO_WINOUT ... IO_WINOUT + 1:
        todo("IO_WINOUT\n");
        break;
    case IO_MOSAIC ... IO_MOSAIC + 1:
        todo("IO_MOSAIC\n");
        break;
    case IO_BLDCNT ... IO_BLDCNT + 1:
        todo("IO_BLDCNT\n");
        break;
    case IO_BLDALPHA ... IO_BLDALPHA + 1:
        todo("IO_BLDALPHA\n");
        break;
    case IO_BLDY ... IO_BLDY + 1:
        todo("IO_BLDY\n");
        break;

    // Sound Registers
    case IO_SOUND1CNT_L ...  IO_SOUND1CNT_L + 1:
        todo("IO_SOUND1CNT_L\n");
        break;
    case IO_SOUND1CNT_H ... IO_SOUND1CNT_H + 1:
        todo("IO_SOUND1CNT_H\n");
        break;
    case IO_SOUND1CNT_X ... IO_SOUND1CNT_X + 1:
        todo("IO_SOUND1CNT_X\n");
        break;
    case IO_SOUND2CNT_L ... IO_SOUND2CNT_L + 1:
        todo("IO_SOUND2CNT_L\n");
        break;
    case IO_SOUND2CNT_H ... IO_SOUND2CNT_H + 1:
        todo("IO_SOUND2CNT_H\n");
        break;
    case IO_SOUND3CNT_L ... IO_SOUND3CNT_L + 1:
        todo("IO_SOUND3CNT_L\n");
        break;
    case IO_SOUND3CNT_H ... IO_SOUND3CNT_H + 1:
        todo("IO_SOUND3CNT_H\n");
        break;
    case IO_SOUND3CNT_X ... IO_SOUND3CNT_X + 1:
        todo("IO_SOUND3CNT_X\n");
        break;
    case IO_SOUND4CNT_L ... IO_SOUND4CNT_L + 1:
        todo("IO_SOUND4CNT_L\n");
        break;
    case IO_SOUND4CNT_H ... IO_SOUND4CNT_H + 1:
        todo("IO_SOUND4CNT_H\n");
        break;
    case IO_SOUNDCNT_L ... IO_SOUNDCNT_L + 1:
        todo("IO_SOUNDCNT_L\n");
        break;
    case IO_SOUNDCNT_H ... IO_SOUNDCNT_H + 1:
        todo("IO_SOUNDCNT_H\n");
        break;
    case IO_SOUNDCNT_X ... IO_SOUNDCNT_X + 1:
        todo("IO_SOUNDCNT_X\n");
        break;
    case IO_SOUNDBIAS ... IO_SOUNDBIAS + 1:
        todo("IO_SOUNDBIAS\n");
        break;
    case IO_WAVE_RAM ... IO_WAVE_RAM + 15:
        todo("IO_WAVE_RAM\n");
        break;
    case IO_FIFO_A ... IO_FIFO_A + 3:
        todo("IO_FIFO_A\n");
        break;
    case IO_FIFO_B ... IO_FIFO_B + 3:
        todo("IO_FIFO_B\n");
        break;

    // DMA Transfer Channels
    case IO_DMA0SAD ... IO_DMA0SAD + 3:
        todo("IO_DMA0SAD\n");
        break;
    case IO_DMA0DAD ... IO_DMA0DAD + 3:
        todo("IO_DMA0DAD\n");
        break;
    case IO_DMA0CNT_L ... IO_DMA0CNT_L + 1:
        todo("IO_DMA0CNT_L\n");
        break;
    case IO_DMA0CNT_H ... IO_DMA0CNT_H + 1:
        todo("IO_DMA0CNT_H\n");
        break;
    case IO_DMA1SAD ... IO_DMA1SAD + 3:
        todo("IO_DMA1SAD\n");
        break;
    case IO_DMA1DAD ... IO_DMA1DAD + 3:
        todo("IO_DMA1DAD\n");
        break;
    case IO_DMA1CNT_L ... IO_DMA1CNT_L + 1:
        todo("IO_DMA1CNT_L\n");
        break;
    case IO_DMA1CNT_H ... IO_DMA1CNT_H + 1:
        todo("IO_DMA1CNT_H\n");
        break;
    case IO_DMA2SAD ... IO_DMA2SAD + 3:
        todo("IO_DMA2SAD\n");
        break;
    case IO_DMA2DAD ... IO_DMA2DAD + 3:
        todo("IO_DMA2DAD\n");
        break;
    case IO_DMA2CNT_L ... IO_DMA2CNT_L + 1:
        todo("IO_DMA2CNT_L\n");
        break;
    case IO_DMA2CNT_H ... IO_DMA2CNT_H + 1:
        todo("IO_DMA2CNT_H\n");
        break;
    case IO_DMA3SAD ... IO_DMA3SAD + 3:
        todo("IO_DMA3SAD\n");
        break;
    case IO_DMA3DAD ... IO_DMA3DAD + 3:
        todo("IO_DMA3DAD\n");
        break;
    case IO_DMA3CNT_L ... IO_DMA3CNT_L + 1:
        todo("IO_DMA3CNT_L\n");
        break;
    case IO_DMA3CNT_H ... IO_DMA3CNT_H + 1:
        todo("IO_DMA3CNT_H\n");
        break;

    // Timer Registers
    case IO_TM0CNT_L ... IO_TM0CNT_L + 1:
        todo("IO_TM0CNT_L\n");
        break;
    case IO_TM0CNT_H ... IO_TM0CNT_H + 1:
        todo("IO_TM0CNT_H\n");
        break;
    case IO_TM1CNT_L ... IO_TM1CNT_L + 1:
        todo("IO_TM1CNT_L\n");
        break;
    case IO_TM1CNT_H ... IO_TM1CNT_H + 1:
        todo("IO_TM1CNT_H\n");
        break;
    case IO_TM2CNT_L ... IO_TM2CNT_L + 1:
        todo("IO_TM2CNT_L\n");
        break;
    case IO_TM2CNT_H ... IO_TM2CNT_H + 1:
        todo("IO_TM2CNT_H\n");
        break;
    case IO_TM3CNT_L ... IO_TM3CNT_L + 1:
        todo("IO_TM3CNT_L\n");
        break;
    case IO_TM3CNT_H ... IO_TM3CNT_H + 1:
        todo("IO_TM3CNT_H\n");
        break;

    // Serial Communication (1)
    // case IO_SIODATA32 ... IO_SIODATA32 + 3:
    //     todo("IO_SIODATA32\n");
    //     break;
    case IO_SIOMULTI0 ... IO_SIOMULTI0 + 1:
        todo("IO_SIOMULTI0\n");
        break;
    case IO_SIOMULTI1 ... IO_SIOMULTI1 + 1:
        todo("IO_SIOMULTI1\n");
        break;
    case IO_SIOMULTI2 ... IO_SIOMULTI2 + 1:
        todo("IO_SIOMULTI2\n");
        break;
    case IO_SIOMULTI3 ... IO_SIOMULTI3 + 1:
        todo("IO_SIOMULTI3\n");
        break;
    case IO_SIOCNT ... IO_SIOCNT + 1:
        todo("IO_SIOCNT\n");
        break;
    case IO_SIOMLT_SEND ... IO_SIOMLT_SEND + 1:
        todo("IO_SIOMLT_SEND\n");
        break;
    // case IO_SIODATA8:
    //     todo("IO_SIODATA8\n");
    //     break;

    // Keypad Input
    case IO_KEYINPUT ... IO_KEYINPUT + 1:
        todo("IO_KEYINPUT\n");
        break;
    case IO_KEYCNT ... IO_KEYCNT + 1:
        todo("IO_KEYCNT\n");
        break;

    // Serial Communication (2)
    case IO_RCNT ... IO_RCNT + 1:
        todo("IO_RCNT\n");
        break;
    // case IO_IR:
    //     todo("IO_IR\n");
    //     break;
    case IO_JOYCNT ... IO_JOYCNT + 1:
        todo("IO_JOYCNT\n");
        break;
    case IO_JOY_RECV ... IO_JOY_RECV + 3:
        todo("IO_JOY_RECV\n");
        break;
    case IO_JOY_TRANS ... IO_JOY_TRANS + 3:
        todo("IO_JOY_TRANS\n");
        break;
    case IO_JOYSTAT ... IO_JOYSTAT + 1:
        todo("IO_JOYSTAT\n");
        break;

    // Interrupt, Waitstate, and Power-Down Control
    case IO_IE ... IO_IE + 1:
        todo("IO_IE\n");
        break;
    case IO_IF ... IO_IF + 1:
        todo("IO_IF\n");
        break;
    case IO_WAITCNT ... IO_WAITCNT + 1:
        todo("IO_WAITCNT\n");
        break;
    case IO_IME ... IO_IME + 1:
        LOG_DEBUG("IO_IME\n");
        break;
    case IO_POSTFLG:
        todo("IO_POSTFLG\n");
        break;
    case IO_HALTCNT:
        todo("IO_HALTCNT\n");
        break;

    default:
        todo();
        break;
    }

    gba->bus->io_regs[address] = data;
}

static void *bus_access(gba_t *gba, uint32_t address, bool is_write) {
    gba_bus_t *bus = gba->bus;

    switch (address) {
    case BUS_BIOS_ROM ... BUS_BIOS_ROM_UNUSED - 1:
        return is_write ? NULL : &bus->bios_rom[address - BUS_BIOS_ROM];
    case BUS_WRAM_BOARD ... BUS_WRAM_BOARD_UNUSED - 1:
        return &bus->wram_board[address - BUS_WRAM_BOARD];
    case BUS_WRAM_CHIP ... BUS_WRAM_CHIP_UNUSED - 1:
        return &bus->wram_chip[address - BUS_WRAM_CHIP];
    case BUS_IO_REGS ... BUS_IO_REGS_UNUSED - 1:
        return bus->io_regs;
    case BUS_PALETTE_RAM ... BUS_PALETTE_RAM_UNUSED - 1:
        return &bus->palette_ram[address - BUS_PALETTE_RAM];
    case BUS_VRAM ... BUS_VRAM_UNUSED - 1:
        return &bus->vram[address - BUS_VRAM];
    case BUS_OAM ... BUS_OAM_UNUSED - 1:
        return &bus->oam[address - BUS_OAM];
    case BUS_GAME_ROM0 ... BUS_GAME_ROM1 - 1:
        return is_write ? NULL : &bus->game_rom[address - BUS_GAME_ROM0];
    case BUS_GAME_ROM1 ... BUS_GAME_ROM2 - 1:
        return is_write ? NULL : &bus->game_rom[address - BUS_GAME_ROM1];
    case BUS_GAME_ROM2 ... BUS_GAME_SRAM - 1:
        return is_write ? NULL : &bus->game_rom[address - BUS_GAME_ROM2];
    case BUS_GAME_SRAM ... BUS_GAME_UNUSED - 1:
        return &bus->game_sram[address - BUS_GAME_SRAM];
    default:
        return NULL;
    }
}

uint8_t gba_bus_read_byte(gba_t *gba, uint32_t address) {
    address = ALIGN(address, 1);

    uint8_t *ret = bus_access(gba, address, false);

    if (ret == gba->bus->io_regs)
        return io_regs_read_byte(gba, address - BUS_IO_REGS);

    return ret ? *ret : read_open_bus(gba, address);
}

uint16_t gba_bus_read_half(gba_t *gba, uint32_t address) {
    address = ALIGN(address, 2);

    uint16_t *ret = bus_access(gba, address, false);

    if ((uint8_t *) ret == gba->bus->io_regs) {
        address -= BUS_IO_REGS;
        return (io_regs_read_byte(gba, address + 1)) << 8
            | io_regs_read_byte(gba, address);
    }

    return ret ? *ret : read_open_bus(gba, address);
}

uint32_t gba_bus_read_word(gba_t *gba, uint32_t address) {
    address = ALIGN(address, 4);

    uint32_t *ret = bus_access(gba, address, false);

    if ((uint8_t *) ret == gba->bus->io_regs) {
        address -= BUS_IO_REGS;
        return (io_regs_read_byte(gba, address + 3)) << 24
            | (io_regs_read_byte(gba, address + 2)) << 16
            | (io_regs_read_byte(gba, address + 1)) << 8
            | io_regs_read_byte(gba, address);
    }

    return ret ? *ret : read_open_bus(gba, address);
}

void gba_bus_write_byte(gba_t *gba, uint32_t address, uint8_t data) {
    address = ALIGN(address, 1);

    uint8_t *ret = bus_access(gba, address, true);

    if ((uint8_t *) ret == gba->bus->io_regs) {
        io_regs_write_byte(gba, address - BUS_IO_REGS, data);
        return;
    }

    if (ret)
        *ret = data;
}

void gba_bus_write_half(gba_t *gba, uint32_t address, uint16_t data) {
    address = ALIGN(address, 2);

    uint16_t *ret = bus_access(gba, address, true);

    if ((uint8_t *) ret == gba->bus->io_regs) {
        address -= BUS_IO_REGS;
        io_regs_write_byte(gba, address + 1, data >> 8);
        io_regs_write_byte(gba, address, data);
        return;
    }

    if (ret)
        *ret = data;
}

void gba_bus_write_word(gba_t *gba, uint32_t address, uint32_t data) {
    address = ALIGN(address, 4);

    uint32_t *ret = bus_access(gba, address, true);

    if ((uint8_t *) ret == gba->bus->io_regs) {
        address -= BUS_IO_REGS;
        io_regs_write_byte(gba, address + 3, data >> 24);
        io_regs_write_byte(gba, address + 2, data >> 16);
        io_regs_write_byte(gba, address + 1, data >> 8);
        io_regs_write_byte(gba, address, data);
        return;
    }

    if (ret)
        *ret = data;
}

// TODO this shouldn't be responsible for cartridge loading and parsing (same for gb_mmu_t)
bool gba_bus_init(gba_t *gba, const uint8_t *rom, size_t rom_size) {
    if (!rom || rom_size < 0xBF)
        return false;

    gba->bus = xcalloc(1, sizeof(*gba->bus));
    memcpy(gba->bus->game_rom, rom, rom_size);

    if (!gba_parse_cartridge(gba)) {
        gba_bus_quit(gba->bus);
        return false;
    }

    // TODO do not load bios from hardcoded file path
    FILE *f = fopen("src/bootroms/gba/gba_bios.bin", "r");
    if (!f) {
        gba_bus_quit(gba->bus);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    fread(gba->bus->bios_rom, sz, 1, f);
    fclose(f);

    return true;
}

void gba_bus_quit(gba_bus_t *bus) {
    free(bus);
}
