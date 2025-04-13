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

// this function can edit *data to be written
static void *io_access(gba_t *gba, uint16_t address, uint32_t *data) {
    switch (address) {
    // LCD I/O Registers
    case IO_DISPCNT:
        // todo("IO_DISPCNT%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_GREENSWAP:
        todo("IO_GREENSWAP%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_DISPSTAT:
        todo("IO_DISPSTAT%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_VCOUNT:
        todo("IO_VCOUNT%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_BG0CNT:
        todo("IO_BG0CNT%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_BG1CNT:
        todo("IO_BG1CNT%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_BG2CNT:
        todo("IO_BG2CNT%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_BG3CNT:
        todo("IO_BG3CNT%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_BG0HOFS:
        todo("IO_BG0HOFS%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_BG0VOFS:
        todo("IO_BG0VOFS%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_BG1HOFS:
        todo("IO_BG1HOFS%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_BG1VOFS:
        todo("IO_BG1VOFS%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_BG2HOFS:
        todo("IO_BG2HOFS%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_BG2VOFS:
        todo("IO_BG2VOFS%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_BG3HOFS:
        todo("IO_BG3HOFS%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_BG3VOFS:
        todo("IO_BG3VOFS%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_BG2PA:
        todo("IO_BG2PA%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_BG2PB:
        todo("IO_BG2PB%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_BG2PC:
        todo("IO_BG2PC%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_BG2PD:
        todo("IO_BG2PD%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_BG2X:
        todo("IO_BG2X%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_BG2Y:
        todo("IO_BG2Y%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_BG3PA:
        todo("IO_BG3PA%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_BG3PB:
        todo("IO_BG3PB%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_BG3PC:
        todo("IO_BG3PC%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_BG3PD:
        todo("IO_BG3PD%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_BG3X:
        todo("IO_BG3X%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_BG3Y:
        todo("IO_BG3Y%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_WIN0H:
        todo("IO_WIN0H%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_WIN1H:
        todo("IO_WIN1H%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_WIN0V:
        todo("IO_WIN0V%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_WIN1V:
        todo("IO_WIN1V%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_WININ:
        todo("IO_WININ%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_WINOUT:
        todo("IO_WINOUT%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_MOSAIC:
        todo("IO_MOSAIC%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_BLDCNT:
        todo("IO_BLDCNT%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_BLDALPHA:
        todo("IO_BLDALPHA%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_BLDY:
        todo("IO_BLDY%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;

    // Sound Registers
    case IO_SOUND1CNT_L:
        todo("IO_SOUND1CNT_L%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_SOUND1CNT_H:
        todo("IO_SOUND1CNT_H%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_SOUND1CNT_X:
        todo("IO_SOUND1CNT_X%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_SOUND2CNT_L:
        todo("IO_SOUND2CNT_L%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_SOUND2CNT_H:
        todo("IO_SOUND2CNT_H%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_SOUND3CNT_L:
        todo("IO_SOUND3CNT_L%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_SOUND3CNT_H:
        todo("IO_SOUND3CNT_H%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_SOUND3CNT_X:
        todo("IO_SOUND3CNT_X%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_SOUND4CNT_L:
        todo("IO_SOUND4CNT_L%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_SOUND4CNT_H:
        todo("IO_SOUND4CNT_H%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_SOUNDCNT_L:
        todo("IO_SOUNDCNT_L%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_SOUNDCNT_H:
        todo("IO_SOUNDCNT_H%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_SOUNDCNT_X:
        todo("IO_SOUNDCNT_X%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_SOUNDBIAS:
        todo("IO_SOUNDBIAS%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_WAVE_RAM:
        todo("IO_WAVE_RAM%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_FIFO_A:
        todo("IO_FIFO_A%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_FIFO_B:
        todo("IO_FIFO_B%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;

    // DMA Transfer Channels
    case IO_DMA0SAD:
        todo("IO_DMA0SAD%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_DMA0DAD:
        todo("IO_DMA0DAD%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_DMA0CNT_L:
        todo("IO_DMA0CNT_L%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_DMA0CNT_H:
        todo("IO_DMA0CNT_H%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_DMA1SAD:
        todo("IO_DMA1SAD%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_DMA1DAD:
        todo("IO_DMA1DAD%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_DMA1CNT_L:
        todo("IO_DMA1CNT_L%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_DMA1CNT_H:
        todo("IO_DMA1CNT_H%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_DMA2SAD:
        todo("IO_DMA2SAD%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_DMA2DAD:
        todo("IO_DMA2DAD%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_DMA2CNT_L:
        todo("IO_DMA2CNT_L%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_DMA2CNT_H:
        todo("IO_DMA2CNT_H%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_DMA3SAD:
        todo("IO_DMA3SAD%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_DMA3DAD:
        todo("IO_DMA3DAD%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_DMA3CNT_L:
        todo("IO_DMA3CNT_L%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_DMA3CNT_H:
        todo("IO_DMA3CNT_H%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;

    // Timer Registers
    case IO_TM0CNT_L:
        todo("IO_TM0CNT_L%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_TM0CNT_H:
        todo("IO_TM0CNT_H%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_TM1CNT_L:
        todo("IO_TM1CNT_L%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_TM1CNT_H:
        todo("IO_TM1CNT_H%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_TM2CNT_L:
        todo("IO_TM2CNT_L%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_TM2CNT_H:
        todo("IO_TM2CNT_H%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_TM3CNT_L:
        todo("IO_TM3CNT_L%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_TM3CNT_H:
        todo("IO_TM3CNT_H%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;

    // Serial Communication (1)
    case IO_SIODATA32:
        todo("IO_SIODATA32%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    // case IO_SIOMULTI0:
    //     todo("IO_SIOMULTI0%s (0x%08X)", data ? " write" : "", data ? *data : 0);
    //     break;
    case IO_SIOMULTI1:
        todo("IO_SIOMULTI1%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_SIOMULTI2:
        todo("IO_SIOMULTI2%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_SIOMULTI3:
        todo("IO_SIOMULTI3%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_SIOCNT:
        todo("IO_SIOCNT%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_SIOMLT_SEND:
        todo("IO_SIOMLT_SEND%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    // case IO_SIODATA8:
    //     todo("IO_SIODATA8%s (0x%08X)", data ? " write" : "", data ? *data : 0);
    //     break;

    // Keypad Input
    case IO_KEYINPUT:
        todo("IO_KEYINPUT%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_KEYCNT:
        todo("IO_KEYCNT%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;

    // Serial Communication (2)
    case IO_RCNT:
        todo("IO_RCNT%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_IR:
        todo("IO_IR%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_JOYCNT:
        todo("IO_JOYCNT%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_JOY_RECV:
        todo("IO_JOY_RECV%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_JOY_TRANS:
        todo("IO_JOY_TRANS%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_JOYSTAT:
        todo("IO_JOYSTAT%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;

    // Interrupt, Waitstate, and Power-Down Control
    case IO_IE:
        todo("IO_IE%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_IF:
        todo("IO_IF%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_WAITCNT:
        todo("IO_WAITCNT%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_IME:
        if (data)
            *data &= 0x01;
        LOG_DEBUG("IO_IME%s (0x%08X)\n", data ? " write" : "", data ? *data : 0);
        break;
    case IO_POSTFLG:
        todo("IO_POSTFLG%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;
    case IO_HALTCNT:
        todo("IO_HALTCNT%s (0x%08X)", data ? " write" : "", data ? *data : 0);
        break;

    default:
        todo();
        return NULL;
    }

    // return NULL in case of write to prevent writing again
    return &gba->bus->io_regs[address];
}

static void *bus_access(gba_t *gba, uint32_t address, uint32_t *data) {
    LOG_DEBUG("bus addr select: 0x%08X\n", address);

    gba_bus_t *bus = gba->bus;

    switch (address) {
    case BUS_BIOS_ROM ... BUS_BIOS_ROM_UNUSED - 1:
        return data ? NULL : &bus->bios_rom[address - BUS_BIOS_ROM];
    case BUS_WRAM_BOARD ... BUS_WRAM_BOARD_UNUSED - 1:
        return &bus->wram_board[address - BUS_WRAM_BOARD];
    case BUS_WRAM_CHIP ... BUS_WRAM_CHIP_UNUSED - 1:
        return &bus->wram_chip[address - BUS_WRAM_CHIP];
    case BUS_IO_REGS ... BUS_IO_REGS_UNUSED - 1:
        return io_access(gba, address - BUS_IO_REGS, data);
    case BUS_PALETTE_RAM ... BUS_PALETTE_RAM_UNUSED - 1:
        todo();
        return &bus->palette_ram[address - BUS_PALETTE_RAM];
    case BUS_VRAM ... BUS_VRAM_UNUSED - 1:
        if (data)
            todo("ok"); // ---> VRAM is never written! why? maybe we are stuck in an infinite loop before? are we waiting for an interrupt?
        return &bus->vram[address - BUS_VRAM];
    case BUS_OAM ... BUS_OAM_UNUSED - 1:
        todo();
        return &bus->oam[address - BUS_OAM];
    case BUS_GAME_ROM0 ... BUS_GAME_ROM1 - 1:
        return data ? NULL : &bus->game_rom[address - BUS_GAME_ROM0];
    case BUS_GAME_ROM1 ... BUS_GAME_ROM2 - 1:
        return data ? NULL : &bus->game_rom[address - BUS_GAME_ROM1];
    case BUS_GAME_ROM2 ... BUS_GAME_SRAM - 1:
        return data ? NULL : &bus->game_rom[address - BUS_GAME_ROM2];
    case BUS_GAME_SRAM ... BUS_GAME_UNUSED - 1:
        todo();
        return &bus->game_sram[address - BUS_GAME_SRAM];
    default:
        return NULL;
    }
}

uint8_t gba_bus_read_byte(gba_t *gba, uint32_t address) {
    uint8_t *ret = bus_access(gba, ALIGN(address, sizeof(*ret)), NULL);
    return ret ? *ret : read_open_bus(gba, address);
}

uint16_t gba_bus_read_half(gba_t *gba, uint32_t address) {
    uint16_t *ret = bus_access(gba, ALIGN(address, sizeof(*ret)), NULL);
    return ret ? *ret : read_open_bus(gba, address);
}

uint32_t gba_bus_read_word(gba_t *gba, uint32_t address) {
    uint32_t *ret = bus_access(gba, ALIGN(address, sizeof(*ret)), NULL);
    return ret ? *ret : read_open_bus(gba, address);
}

void gba_bus_write_byte(gba_t *gba, uint32_t address, uint8_t data) {
    uint32_t tmp = data;
    uint8_t *ret = bus_access(gba, ALIGN(address, sizeof(*ret)), &tmp);
    if (ret)
        *ret = data;
}

void gba_bus_write_half(gba_t *gba, uint32_t address, uint16_t data) {
    uint32_t tmp = data;
    uint16_t *ret = bus_access(gba, ALIGN(address, sizeof(*ret)), &tmp);
    if (ret)
        *ret = data;
}

void gba_bus_write_word(gba_t *gba, uint32_t address, uint32_t data) {
    uint32_t tmp = data;
    uint32_t *ret = bus_access(gba, ALIGN(address, sizeof(*ret)), &tmp);
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
