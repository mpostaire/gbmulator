#pragma once

#include "gba.h"

#define REG_SP 13 // Stack Pointer
#define REG_LR 14 // Link Register
#define REG_PC 15 // Program Counter

#define IRQ_VBLANK 0   // LCD V-Blank
#define IRQ_HBLANK 1   // LCD H-Blank
#define IRQ_VCOUNT 2   // LCD V-Counter Match
#define IRQ_TIMER0 3   // Timer 0 Overflow
#define IRQ_TIMER1 4   // Timer 1 Overflow
#define IRQ_TIMER2 5   // Timer 2 Overflow
#define IRQ_TIMER3 6   // Timer 3 Overflow
#define IRQ_SERIAL 7   // Serial Communication
#define IRQ_DMA0 8     // DMA 0
#define IRQ_DMA1 9     // DMA 1
#define IRQ_DMA2 10    // DMA 2
#define IRQ_DMA3 11    // DMA 3
#define IRQ_KEYPAD 12  // Keypad
#define IRQ_GAMEPAK 13 // Game Pak (external IRQ source)

#define CPU_REQUEST_INTERRUPT(gba, irq) SET_BIT((gba)->bus->io_regs[IO_IF], irq)

typedef struct {
    uint32_t regs[16];

    uint32_t banked_regs_8_12[2][5];
    uint32_t banked_regs_13_14[6][2];

    uint32_t cpsr; // current program status register
    uint32_t spsr[6];

    uint32_t pipeline[2]; // array of instructions (because it is a 3 stage pipeline, we just need to remember 2 instructions)
    uint8_t pipeline_flush_cycles;
} gba_cpu_t;

void gba_cpu_step(gba_t *gba);

void gba_cpu_init(gba_t *gba);

void gba_cpu_quit(gba_t *gba);
