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
    // TODO
    return 0xFFFFFFFF;
}

static uint16_t io_regs_read(gba_t *gba, uint16_t address) {
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

    return gba->bus->io_regs[address] & mask;
}

static void io_regs_write(gba_t *gba, uint16_t address, uint16_t data) {
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
        LOG_DEBUG("IO_BG0CNT 0x%04X\n", data);
        break;
    case IO_BG1CNT:
        LOG_DEBUG("IO_BG1CNT 0x%04X\n", data);
        break;
    case IO_BG2CNT:
        LOG_DEBUG("IO_BG2CNT 0x%04X\n", data);
        break;
    case IO_BG3CNT:
        LOG_DEBUG("IO_BG3CNT 0x%04X\n", data);
        break;
    case IO_BG0HOFS:
        LOG_DEBUG("IO_BG0HOFS 0x%04X\n", data);
        break;
    case IO_BG0VOFS:
        LOG_DEBUG("IO_BG0VOFS 0x%04X\n", data);
        break;
    case IO_BG1HOFS:
        LOG_DEBUG("IO_BG1HOFS 0x%04X\n", data);
        break;
    case IO_BG1VOFS:
        LOG_DEBUG("IO_BG1VOFS 0x%04X\n", data);
        break;
    case IO_BG2HOFS:
        LOG_DEBUG("IO_BG2HOFS 0x%04X\n", data);
        break;
    case IO_BG2VOFS:
        LOG_DEBUG("IO_BG2VOFS 0x%04X\n", data);
        break;
    case IO_BG3HOFS:
        LOG_DEBUG("IO_BG3HOFS 0x%04X\n", data);
        break;
    case IO_BG3VOFS:
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
        LOG_DEBUG("IO_WININ 0x%04X\n", data);
        break;
    case IO_WINOUT:
        LOG_DEBUG("IO_WINOUT 0x%04X\n", data);
        break;
    case IO_MOSAIC:
        LOG_DEBUG("IO_MOSAIC 0x%04X\n", data);
        break;
    case IO_BLDCNT:
        LOG_DEBUG("IO_BLDCNT 0x%04X\n", data);
        break;
    case IO_BLDALPHA:
        LOG_DEBUG("IO_BLDALPHA 0x%04X\n", data);
        break;
    case IO_BLDY:
        LOG_DEBUG("IO_BLDY 0x%04X\n", data);
        break;

    // Sound Registers
    case IO_SOUND1CNT_L:
        LOG_DEBUG("IO_SOUND1CNT_L 0x%04X\n", data);
        break;
    case IO_SOUND1CNT_H:
        LOG_DEBUG("IO_SOUND1CNT_H 0x%04X\n", data);
        break;
    case IO_SOUND1CNT_X:
        LOG_DEBUG("IO_SOUND1CNT_X 0x%04X\n", data);
        break;
    case IO_SOUND2CNT_L:
        LOG_DEBUG("IO_SOUND2CNT_L 0x%04X\n", data);
        break;
    case IO_SOUND2CNT_H:
        LOG_DEBUG("IO_SOUND2CNT_H 0x%04X\n", data);
        break;
    case IO_SOUND3CNT_L:
        LOG_DEBUG("IO_SOUND3CNT_L 0x%04X\n", data);
        break;
    case IO_SOUND3CNT_H:
        LOG_DEBUG("IO_SOUND3CNT_H 0x%04X\n", data);
        break;
    case IO_SOUND3CNT_X:
        LOG_DEBUG("IO_SOUND3CNT_X 0x%04X\n", data);
        break;
    case IO_SOUND4CNT_L:
        LOG_DEBUG("IO_SOUND4CNT_L 0x%04X\n", data);
        break;
    case IO_SOUND4CNT_H:
        LOG_DEBUG("IO_SOUND4CNT_H 0x%04X\n", data);
        break;
    case IO_SOUNDCNT_L:
        LOG_DEBUG("IO_SOUNDCNT_L 0x%04X\n", data);
        break;
    case IO_SOUNDCNT_H:
        LOG_DEBUG("IO_SOUNDCNT_H 0x%04X\n", data);
        break;
    case IO_SOUNDCNT_X:
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
        break;
    case IO_TM0CNT_H:
        LOG_DEBUG("IO_TM0CNT_H 0x%04X\n", data);
        break;
    case IO_TM1CNT_L:
        LOG_DEBUG("IO_TM1CNT_L 0x%04X\n", data);
        break;
    case IO_TM1CNT_H:
        LOG_DEBUG("IO_TM1CNT_H 0x%04X\n", data);
        break;
    case IO_TM2CNT_L:
        LOG_DEBUG("IO_TM2CNT_L 0x%04X\n", data);
        break;
    case IO_TM2CNT_H:
        LOG_DEBUG("IO_TM2CNT_H 0x%04X\n", data);
        break;
    case IO_TM3CNT_L:
        LOG_DEBUG("IO_TM3CNT_L 0x%04X\n", data);
        break;
    case IO_TM3CNT_H:
        LOG_DEBUG("IO_TM3CNT_H 0x%04X\n", data);
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
        data = gba->bus->io_regs[address] & ~data;
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

    CHANGE_BITS(gba->bus->io_regs[address], mask, data);
}

static void *bus_access(gba_t *gba, uint32_t address, uint8_t size, bool is_write) {
    gba_bus_t *bus = gba->bus;

    switch (address) {
    case BUS_BIOS_ROM ... BUS_BIOS_ROM_UNUSED - 1:
        if (is_write)
            return NULL;
        if (gba->cpu->regs[REG_PC] >= BUS_BIOS_ROM_UNUSED)
            return &gba->bus->last_fetched_bios_instr;
        return &bus->bios_rom[address - BUS_BIOS_ROM];
    case BUS_EWRAM ... BUS_IWRAM - 1:
        return &bus->ewram[(address - BUS_EWRAM) % (BUS_EWRAM_UNUSED - BUS_EWRAM)];
    case BUS_IWRAM ... BUS_IO_REGS - 1:
        return &bus->iwram[(address - BUS_IWRAM) % (BUS_IWRAM_UNUSED - BUS_IWRAM)];
    case BUS_IO_REGS ... BUS_IO_REGS_UNUSED - 1:
        return bus->io_regs;
    case BUS_PRAM ... BUS_VRAM - 1:
        return &bus->palette_ram[(address - BUS_PRAM) % (BUS_PRAM_UNUSED - BUS_PRAM)];
    case BUS_VRAM ... BUS_OAM - 1:
        uint32_t vram_upper_bound = PPU_GET_MODE(gba) < 3 ? 0x10000 : 0x14000;
        address = (address - (BUS_VRAM_UNUSED + 0x8000)) % 0x20000;
        if (address >= vram_upper_bound && address >= 0x18000)
            return is_write ? NULL : &bus->vram[address % 0x8000];
        return &bus->vram[address % 0x20000];
    case BUS_OAM ... BUS_GAME_ROM0 - 1:
        if (is_write && size == 1)
            return NULL;
        return &bus->oam[(address - BUS_OAM_UNUSED) % (BUS_OAM_UNUSED - BUS_OAM)];
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
    uint8_t *ret = bus_access(gba, address, 1, false);

    if ((uint16_t *) ret == gba->bus->io_regs)
        return io_regs_read(gba, address - BUS_IO_REGS);

    return ret ? *ret : read_open_bus(gba, address);
}

uint16_t gba_bus_read_half(gba_t *gba, uint32_t address) {
    address = ALIGN(address, 2);

    uint16_t *ret = bus_access(gba, address, 2, false);

    if (ret == gba->bus->io_regs)
        return io_regs_read(gba, address - BUS_IO_REGS);

    return ret ? *ret : read_open_bus(gba, address);
}

uint32_t gba_bus_read_word(gba_t *gba, uint32_t address) {
    address = ALIGN(address, 4);

    uint32_t *ret = bus_access(gba, address, 4, false);

    if ((uint16_t *) ret == gba->bus->io_regs) {
        address -= BUS_IO_REGS;
        return (io_regs_read(gba, address + 2)) << 16 | io_regs_read(gba, address);
    }

    return ret ? *ret : read_open_bus(gba, address);
}

void gba_bus_write_byte(gba_t *gba, uint32_t address, uint8_t data) {
    uint8_t *ret = bus_access(gba, address, 1, true);

    if ((uint16_t *) ret == gba->bus->io_regs) {
        io_regs_write(gba, address - BUS_IO_REGS, data);
        return;
    }

    if (ret) {
        if (address >= BUS_PRAM && address < BUS_OAM)
            *((uint16_t *) ret) = (data << 8) | data;
        else
            *ret = data;
    }
}

void gba_bus_write_half(gba_t *gba, uint32_t address, uint16_t data) {
    address = ALIGN(address, 2);

    uint16_t *ret = bus_access(gba, address, 2, true);

    if (ret == gba->bus->io_regs) {
        io_regs_write(gba, address - BUS_IO_REGS, data);
        return;
    }

    if (ret)
        *ret = data;
}

void gba_bus_write_word(gba_t *gba, uint32_t address, uint32_t data) {
    address = ALIGN(address, 4);

    uint32_t *ret = bus_access(gba, address, 4, true);

    if ((uint16_t *) ret == gba->bus->io_regs) {
        address -= BUS_IO_REGS;
        io_regs_write(gba, address + 2, data >> 16);
        io_regs_write(gba, address, data);
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
        gba_bus_quit(gba);
        return false;
    }

    gba->bus->rom_size = rom_size;

    gba->bus->io_regs[IO_KEYINPUT] = 0x03FF;

    // TODO do not load bios from hardcoded file path
    FILE *f = fopen("src/bootroms/gba/gba_bios.bin", "r");
    if (!f) {
        gba_bus_quit(gba);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    fread(gba->bus->bios_rom, sz, 1, f);
    fclose(f);

    return true;
}

void gba_bus_quit(gba_t *gba) {
    free(gba->bus);
}
