#pragma once

#include "gba.h"
#include "bus.h"

#define PIPELINE_FETCHING 0
#define PIPELINE_DECODING 1

#define REG_SP 13 // Stack Pointer
#define REG_LR 14 // Link Register
#define REG_PC 15 // Program Counter

#define IRQ_VBLANK  0  // LCD V-Blank
#define IRQ_HBLANK  1  // LCD H-Blank
#define IRQ_VCOUNT  2  // LCD V-Counter Match
#define IRQ_TIMER0  3  // Timer 0 Overflow
#define IRQ_TIMER1  4  // Timer 1 Overflow
#define IRQ_TIMER2  5  // Timer 2 Overflow
#define IRQ_TIMER3  6  // Timer 3 Overflow
#define IRQ_SERIAL  7  // Serial Communication
#define IRQ_DMA0    8  // DMA 0
#define IRQ_DMA1    9  // DMA 1
#define IRQ_DMA2    10 // DMA 2
#define IRQ_DMA3    11 // DMA 3
#define IRQ_KEYPAD  12 // Keypad
#define IRQ_GAMEPAK 13 // Game Pak (external IRQ source)

#define CPU_REQUEST_INTERRUPT(gba, irq) SET_BIT((gba)->bus.io[IO_IF], irq)

#define CPSR_N (((uint32_t) 1) << 31) // Negative or less than
#define CPSR_Z (((uint32_t) 1) << 30) // Zero
#define CPSR_C (((uint32_t) 1) << 29) // Carry or borrow or extend
#define CPSR_V (((uint32_t) 1) << 28) // Overflow
#define CPSR_I (((uint32_t) 1) << 7)  // IRQ disable
#define CPSR_F (((uint32_t) 1) << 6)  // FIQ disable
#define CPSR_T (((uint32_t) 1) << 5)  // State bit

#define CPSR_CHECK_FLAG(cpu, flag) ((cpu)->cpsr & (flag))

typedef struct {
    uint32_t regs[16];

    uint32_t banked_regs_8_12[2][5];
    uint32_t banked_regs_13_14[7][2];

    uint32_t cpsr; // current program status register
    uint32_t spsr[7];

    uint32_t          pipeline[2]; // array of instructions (because it is a 3 stage pipeline, we just need to remember 2 instructions)
    bus_access_type_t pipeline_access_type;
} gba_cpu_t;

void gba_cpu_step(gba_t *gba);

void gba_cpu_reset(gba_t *gba);

// TODO make this private
void bank_registers(gba_cpu_t *cpu, uint8_t old_mode, uint8_t new_mode);
