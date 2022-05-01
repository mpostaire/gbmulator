#pragma once

#include "apu.h"
#include "cpu.h"
#include "joypad.h"
#include "link.h"
#include "mmu.h"
#include "ppu.h"
#include "timer.h"
#include "types.h"
#include "utils.h"

/**
 * Runs the emulator for an ammount of cycles.
 * @param cycles_limit the ammount of cycles the emulator will run for.
 */
void emulator_run_cycles(int cycles_limit);
