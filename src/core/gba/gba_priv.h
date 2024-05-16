#pragma once

#include "gba.h"
#include "cpu.h"
#include "bus.h"
#include "../utils.h"

struct gba_t {
    gba_cpu_t *cpu;
    gba_bus_t *bus;
};
