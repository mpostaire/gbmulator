#include <stdlib.h>
#include <stdbit.h>

#include "gba_priv.h"

// // links I used while implementing this
// https://emudev.org/system_resources
// https://mgba.io/2015/06/27/cycle-counting-prefetch/
// https://problemkaputt.de/gbatek.htm#armcpureference
// https://github.com/nba-emu/NanoBoyAdvance/blob/master/src/nba/src/arm/handlers/arithmetic.inl#L84
// https://vision.gel.ulaval.ca/~jflalonde/cours/1001/h17/docs/arm-instructionset.pdf
// https://github.com/Normmatt/gba_bios/blob/master/asm/bios.s

#define CYCLE_I 1
#define CYCLE_N 1
#define CYCLE_S 1

#define REG_SP 13 // Stack Pointer
#define REG_LR 14 // Link Register
#define REG_PC 15 // Program Counter

#define PIPELINE_FETCHING 0
#define PIPELINE_DECODING 1

#define CPSR_N (1 << 31) // Negative or less than
#define CPSR_Z (1 << 30) // Zero
#define CPSR_C (1 << 29) // Carry or borrow or extend
#define CPSR_V (1 << 28) // Overflow
#define CPSR_I (1 << 7) // IRQ disable
#define CPSR_F (1 << 6) // FIQ disable
#define CPSR_T (1 << 5) // State bit

#define CPSR_MODE_USR 0b10000 // User (usr): The normal ARM program execution state
#define CPSR_MODE_FIQ 0b10001 // FIQ (fiq): Designed to support a data transfer or channel process
#define CPSR_MODE_IRQ 0b10010 // IRQ (irq): Used for general-purpose interrupt handling
#define CPSR_MODE_SVC 0b10011 // Supervisor (svc): Protected mode for the operating system
#define CPSR_MODE_ABT 0b10111 // Abort mode (abt): Entered after a data or instruction prefetch abort
#define CPSR_MODE_UND 0b11011 // Undefined (und): Entered when an undefined instruction is executed
#define CPSR_MODE_SYS 0b11111 // System (sys): A privileged user mode for the operating system

#define CPSR_CHECK_FLAG(cpu, flag) ((cpu)->cpsr & (flag))
#define CPSR_CHANGE_FLAG(cpu, flag, value) ((cpu)->cpsr ^= (-(value) ^ (cpu)->cpsr) & (flag))

#define CPSR_MODE_MASK 0x0000001F // Mode bits
#define CPSR_GET_MODE(cpu) ((cpu)->cpsr & CPSR_MODE_MASK)
#define CPSR_SET_MODE(cpu, mode) CHANGE_BITS((cpu)->cpsr, CPSR_MODE_MASK, mode)

#define VECTOR_RESET 0x00000000 // Reset --> mode on entry: CPSR_MODE_SVC
#define VECTOR_UNDEFINED_INSTR 0x00000004 // Undefined instruction --> mode on entry: CPSR_MODE_UND
#define VECTOR_SOFTWARE_INTERRUPT 0x00000008 // Software interrupt --> mode on entry: CPSR_MODE_SVC
#define VECTOR_ABORT_PREFETCH 0x0000000C // Abort (prefetch) --> mode on entry: CPSR_MODE_ABT
#define VECTOR_ABORT_DATA 0x00000010 // Abort (data) --> mode on entry: CPSR_MODE_ABT
#define VECTOR_RESERVED 0x00000014 // Reserved Reserved
#define VECTOR_IRQ 0x00000018 // IRQ --> mode on entry: CPSR_MODE_IRQ
#define VECTOR_FIQ 0x0000001C // FIQ --> mode on entry: CPSR_MODE_FIQ

#define ARM_INSTR_GET_COND(instr) ((instr) >> 28)

// shift left
#define LSL(op, amount) ((op) << (amount))
// shift right
#define LSR(op, amount) ((op) >> (amount))
// arithmetic shift right (https://stackoverflow.com/a/53766752)
#define ASR(op, amount) ((int64_t) (int32_t) (op) >> (amount))
// rotate right
#define ROR(op, amount) (((op) >> (amount)) | ((op) << (32 - (amount))))

#define _STM_LDM_STM_BUS_ACCESS gba_bus_write_word((gba), dest_addr, (gba)->cpu->regs[i])
#define _STM_LDM_LDM_BUS_ACCESS (gba)->cpu->regs[i] = gba_bus_read_word((gba), dest_addr)
#define _STM_LDM_NEXT_REG(rlist) (stdc_first_trailing_one((rlist)) - 1)
#define _STM_LDM(op, gba, rb, rlist, p, u, w)                                                  \
    do {                                                                                       \
        typeof((rlist)) rlist_tmp = (rlist);                                                   \
        int8_t transfer_size = stdc_count_ones((rlist)) * 4;                                   \
        uint32_t dest_addr = (gba)->cpu->regs[(rb)];                                           \
        bool p_tmp = p;                                                                        \
        if (!(u)) {                                                                            \
            dest_addr -= transfer_size;                                                        \
            p_tmp = !p_tmp;                                                                    \
        }                                                                                      \
        if ((w))                                                                               \
            (gba)->cpu->regs[(rb)] = dest_addr;                                                \
        for (int i = _STM_LDM_NEXT_REG(rlist_tmp); i >= 0; i = _STM_LDM_NEXT_REG(rlist_tmp)) { \
            if ((p_tmp))                                                                       \
                dest_addr += 4;                                                                \
            (op);                                                                              \
            if (!(p_tmp))                                                                      \
                dest_addr += 4;                                                                \
            RESET_BIT(rlist_tmp, i);                                                           \
        }                                                                                      \
    } while (0)
#define STM(gba, rb, rlist, p, u, w) _STM_LDM(_STM_LDM_STM_BUS_ACCESS, gba, rb, rlist, p, u, w)
#define LDM(gba, rb, rlist, p, u, w) _STM_LDM(_STM_LDM_LDM_BUS_ACCESS, gba, rb, rlist, p, u, w)

#define FOREACH_COND(X) \
    X(EQ)               \
    X(NE)               \
    X(CS)               \
    X(CC)               \
    X(MI)               \
    X(PL)               \
    X(VS)               \
    X(VC)               \
    X(HI)               \
    X(LS)               \
    X(GE)               \
    X(LT)               \
    X(GT)               \
    X(LE)               \
    Y(AL)
#define COND_GENERATOR(name) COND_##name,
#define COND_NAME_GENERATOR(name) STRINGIFY(name),

#define Y COND_GENERATOR
typedef enum {
    FOREACH_COND(COND_GENERATOR)
} cond_t;
#undef Y
#ifdef DEBUG
#define Y(name) ""
static const char *cond_names[] = {
    FOREACH_COND(COND_NAME_GENERATOR)
};
#undef Y
#endif

#ifdef DEBUG
static const char *reg_names[] = {
    "R0",
    "R1",
    "R2",
    "R3",
    "R4",
    "R5",
    "R6",
    "R7",
    "R8",
    "R9",
    "R10",
    "R11",
    "R12",
    "SP",
    "LR",
    "PC"
};

static const char *stm_ldm_addr_mode_names[2][4] = {
    {
        "ED",
        "EA",
        "FD",
        "FA"
    },
    {
        "DA",
        "DB",
        "IA",
        "IB"
    }
};
#endif

typedef enum {
    REG_IDX_USR_SYS = 0,
    REG_IDX_FIQ,
    REG_IDX_IRQ,
    REG_IDX_SVC,
    REG_IDX_ABT,
    REG_IDX_UND,
    REG_IDX_INVALID_MODE
} cpu_mode_reg_indexes_t;

static uint8_t regs_mode_hashes[] = {
    REG_IDX_USR_SYS,
    REG_IDX_FIQ,
    REG_IDX_IRQ,
    REG_IDX_SVC,
    REG_IDX_INVALID_MODE,
    REG_IDX_INVALID_MODE,
    REG_IDX_INVALID_MODE,
    REG_IDX_ABT,
    REG_IDX_INVALID_MODE,
    REG_IDX_INVALID_MODE,
    REG_IDX_INVALID_MODE,
    REG_IDX_UND,
    REG_IDX_INVALID_MODE,
    REG_IDX_INVALID_MODE,
    REG_IDX_INVALID_MODE,
    REG_IDX_USR_SYS
};

typedef bool (*handler_t)(gba_t *gba, uint32_t instr);
handler_t handlers[];

// using double lookup tables to reduce impact on host CPU cache
uint8_t arm_handlers[1 << 12];
uint8_t thumb_handlers[1 << 8]; // TODO not sure of this size

static inline void compute_flags_nz(gba_cpu_t *cpu, uint32_t res) {
    CPSR_CHANGE_FLAG(cpu, CPSR_N, res >> 31);
    CPSR_CHANGE_FLAG(cpu, CPSR_Z, res == 0);
}

#define ADD_SET_FLAGS(cpu, res, op1, op2)                                            \
    do {                                                                             \
        compute_flags_nz((cpu), (res));                                              \
        CPSR_CHANGE_FLAG((cpu), CPSR_C, (res) < (op1));                              \
        CPSR_CHANGE_FLAG((cpu), CPSR_V, (~((op1) ^ (op2)) & ((op2) ^ (res))) >> 31); \
    } while (0)

#define ADC_SET_FLAGS(cpu, res, op1, op2)                                            \
    do {                                                                             \
        compute_flags_nz((cpu), (res));                                              \
        CPSR_CHANGE_FLAG((cpu), CPSR_C, (res) < (op1));                              \
        CPSR_CHANGE_FLAG((cpu), CPSR_V, (~((op1) ^ (op2)) & ((op2) ^ (res))) >> 31); \
    } while (0)

#define SUB_SET_FLAGS(cpu, res, op1, op2)                                           \
    do {                                                                            \
        compute_flags_nz((cpu), (res));                                             \
        CPSR_CHANGE_FLAG((cpu), CPSR_C, (op2) <= (op1));                            \
        CPSR_CHANGE_FLAG((cpu), CPSR_V, (((op1) ^ (op2)) & ((op1) ^ (res))) >> 31); \
    } while (0)

#define SBC_SET_FLAGS(cpu, res, op1, op2)                                           \
    do {                                                                            \
        compute_flags_nz((cpu), (res));                                             \
        CPSR_CHANGE_FLAG((cpu), CPSR_C, (op2) <= (op1));                            \
        CPSR_CHANGE_FLAG((cpu), CPSR_V, (((op1) ^ (op2)) & ((op1) ^ (res))) >> 31); \
    } while (0)

#ifdef DEBUG
static char *_rlist_to_str(uint16_t rlist, char *buf, size_t buf_size) {
    size_t buf_offset = 0;

    int first = -1;
    int last = -1;
    for (int i = 0; i < 16; i++) {
        if (CHECK_BIT(rlist, i)) {
            if (first < 0) {
                first = i;
                buf_offset += snprintf(buf + buf_offset, buf_size - buf_offset, "%s%s", last >= 0 ? "," : "", reg_names[i]);
            } else if (buf_offset > 0 && buf[buf_offset - 1] != '-') {
                buf_offset += snprintf(buf + buf_offset, buf_size - buf_offset, "-");
            }
            last = i;
        } else {
            if (first >= 0) {
                if (last != first && last >= 0)
                    buf_offset += snprintf(buf + buf_offset, buf_size - buf_offset, "%s", reg_names[last]);
                first = -1;
            }
        }
    }

    if (buf_offset > 0 && buf[buf_offset - 1] == '-' && last >= 0)
        buf_offset += snprintf(buf + buf_offset, buf_size - buf_offset, "%s", reg_names[last]);

    return buf;
}

static char *_strh_addr_str(uint8_t rn, int32_t offset, bool offset_is_reg, bool p, bool u, bool w, char *buf, size_t buf_size) {
    size_t buf_offset = 0;
    buf_offset += snprintf(buf + buf_offset, buf_size - buf_offset, "[%s", reg_names[rn]);

    if (p) {
        if (offset_is_reg)
            buf_offset += snprintf(buf + buf_offset, buf_size - buf_offset, ",%s%s]%s", u ? "" : "-", reg_names[offset], w ? "!" : "");
        else
            buf_offset += snprintf(buf + buf_offset, buf_size - buf_offset, ",%s0x%04X]%s", u ? "" : "-", offset < 0 ? -offset : offset, w ? "!" : "");
    } else {
        if (offset_is_reg)
            buf_offset += snprintf(buf + buf_offset, buf_size - buf_offset, "],%s%s", u ? "" : "-", reg_names[offset], w ? "!" : "");
        else
            buf_offset += snprintf(buf + buf_offset, buf_size - buf_offset, "],%s0x%04X", u ? "" : "-", offset < 0 ? -offset : offset, w ? "!" : "");
    }

    return buf;
}

static char *_msr_op_to_str(uint32_t instr, uint8_t op, bool i, char *buf, size_t buf_size) {
    size_t buf_offset = 0;
    if (i)
        buf_offset += snprintf(buf + buf_offset, buf_size - buf_offset, "#0x%08X", op);
    else
        buf_offset += snprintf(buf + buf_offset, buf_size - buf_offset, "%s", reg_names[instr & 0x0F]);
    return buf;
}

#define strh_addr_str(rn, offset, offset_is_reg, p, u, w) _strh_addr_str((rn), (offset), (offset_is_reg), (p), (u), (w), (char[32]) {}, 32)

#define rlist_to_str(rlist) _rlist_to_str((rlist), (char[32]) {}, 32)

#define msr_op_to_str(instr, op, i) _msr_op_to_str((instr), (op), (i), (char[32]) {}, 32)
#endif

static bool not_implemented_handler(UNUSED gba_t *gba, uint32_t instr) {
    int instr_size = CPSR_CHECK_FLAG(gba->cpu, CPSR_T) ? 2 : 4;
    todo("not implemented instruction: 0x%0*X (0b%0.*b)", instr_size * 2, instr, instr_size * 8, instr);
    return true;
}

static uint32_t shift_offset(gba_cpu_t *cpu, uint8_t type, uint32_t offset, uint8_t amount, bool i, bool *c) {
    *c = CPSR_CHECK_FLAG(cpu, CPSR_C);
    bool old_c = *c;

    uint64_t offset_64 = offset;

    switch (type) {
    case 0b00: // shift left
        if (amount > 0) {
            amount = MIN(amount, 33);
            offset_64 = LSL(offset_64, amount);
            *c = GET_BIT(offset_64, 32);
        }
        break;
    case 0b01: // shift right
        amount = amount == 0 && i ? 32 : amount;
        if (amount > 0) {
            amount = MIN(amount, 33);
            offset_64 = LSR(offset_64, amount - 1);
            *c = GET_BIT(offset_64, 0);
            offset_64 = LSR(offset_64, 1);
        }
        break;
    case 0b10: // arithmetic shift right
        amount = amount == 0 && i ? 32 : amount;
        if (amount > 0) {
            amount = MIN(amount, 33);
            offset_64 = ASR(offset_64, amount - 1);
            *c = GET_BIT(offset_64, 0);
            offset_64 = ASR(offset_64, 1);
        }
        break;
    case 0b11: // rotate right
        if (amount == 0 && i) {
            // RRX
            *c = GET_BIT(offset_64, 0);
            offset_64 = (offset_64 >> 1) | (old_c << 31);
            amount = 1;
        } else {
            amount &= 31;
            if (amount > 0) {
                offset_64 = ROR(offset_64, amount);
                *c = GET_BIT(offset_64, 31);
            }
        }
        break;
    }

    if (amount == 0)
        *c = old_c;

    return offset_64;
}

static bool data_processing_begin(gba_t *gba, uint32_t instr, uint8_t *rd, uint32_t *op1, uint32_t *op2, bool *c) {
    bool i = CHECK_BIT(instr, 25);
    bool s = CHECK_BIT(instr, 20);
    uint8_t rn = (instr & 0x000F0000) >> 16;

    *rd = (instr & 0x0000F000) >> 12;
    *op1 = gba->cpu->regs[rn];
    *op2 = instr & 0x00000FFF;

    // TODO replace this function

    if (i) {
        *c = CPSR_CHECK_FLAG(gba->cpu, CPSR_C);

        uint32_t amount = (*op2 & 0xF00) >> 7;
        uint32_t imm = (*op2 & 0x0FF);
        *op2 = ROR(imm, amount);
        *c = amount == 0 ? *c : GET_BIT(*op2, 31);
    } else {
        uint8_t rm = *op2 & 0x00F;
        uint16_t shift = (*op2 & 0xFF0) >> 4;
        uint8_t shift_type = (shift & 0b00000110) >> 1;

        uint64_t offset = gba->cpu->regs[rm];

        uint8_t amount;
        bool i = !CHECK_BIT(shift, 0);
        if (i) {
            amount = (shift & 0xF8) >> 3;
        } else {
            amount = gba->cpu->regs[(shift & 0xF0) >> 4];
            if (rm == REG_PC)
                offset += 4;
            else if (rn == REG_PC)
                *op1 += 4;
        }

        *op2 = shift_offset(gba->cpu, shift_type, offset, amount, i, c);
    }

    return s && rd != REG_PC;
}

static bool mul_handler(gba_t *gba, uint32_t instr) {
    uint8_t rd = (instr >> 16) & 0x0F;
    uint8_t rs = (instr >> 8) & 0x0F;
    uint8_t rm = instr & 0x0F;
    bool s = CHECK_BIT(instr, 20);

    if (s)
        todo("s");

    gba->cpu->regs[rd] = gba->cpu->regs[rm] * gba->cpu->regs[rs];

    LOG_DEBUG("(0x%08X) MUL%s%s %s,%s,%s\n", instr, cond_names[ARM_INSTR_GET_COND(instr)], s ? "S" : "", reg_names[rd], reg_names[rm], reg_names[rs]);

    if (rd == REG_PC) {
        todo("flush pipeline");
        return false;
    }

    return true;
}

static bool mla_handler(gba_t *gba, uint32_t instr) {
    uint8_t rd = (instr >> 16) & 0x0F;
    uint8_t rn = (instr >> 12) & 0x0F;
    uint8_t rs = (instr >> 8) & 0x0F;
    uint8_t rm = instr & 0x0F;
    bool s = CHECK_BIT(instr, 20);

    if (s)
        todo("s");

    gba->cpu->regs[rd] = (gba->cpu->regs[rm] * gba->cpu->regs[rs]) + gba->cpu->regs[rn];

    LOG_DEBUG("(0x%08X) MLA%s%s %s,%s,%s,%s\n", instr, cond_names[ARM_INSTR_GET_COND(instr)], s ? "S" : "", reg_names[rd], reg_names[rm], reg_names[rs], reg_names[rn]);

    if (rd == REG_PC) {
        todo("flush pipeline");
        return false;
    }

    return true;
}

static bool mull_handler(gba_t *gba, uint32_t instr) {
    uint8_t rdhi = (instr >> 16) & 0x0F;
    uint8_t rdlo = (instr >> 12) & 0x0F;
    uint8_t rs = (instr >> 8) & 0x0F;
    uint8_t rm = instr & 0x0F;
    bool u = CHECK_BIT(instr, 22);
    bool s = CHECK_BIT(instr, 20);

    if (s)
        todo("s");

    uint64_t res;
    if (u)
        res = (uint64_t) gba->cpu->regs[rm] * (uint64_t) gba->cpu->regs[rs];
    else
        res = ((int64_t) (int32_t) gba->cpu->regs[rm]) * ((int64_t) (int32_t) gba->cpu->regs[rs]);

    gba->cpu->regs[rdhi] = res >> 32;
    gba->cpu->regs[rdlo] = res;

    LOG_DEBUG("(0x%08X) %cMULL%s%s %s,%s,%s,%s\n", instr, u ? 'S' : 'U', cond_names[ARM_INSTR_GET_COND(instr)], s ? "S" : "", reg_names[rdlo], reg_names[rdhi], reg_names[rm], reg_names[rs]);

    return true;
}

static bool bx_handler(gba_t *gba, uint32_t instr) {
    uint8_t rn = instr & 0x0F;
    uint32_t pc_dest = gba->cpu->regs[rn];

    if (rn == REG_PC)
        todo("undefined behaviour");

    // change cpu state to THUMB/ARM
    CPSR_CHANGE_FLAG(gba->cpu, CPSR_T, GET_BIT(pc_dest, 0));

    // TODO 2S + 1N cycles --> does this take into account the pipeline flush (1 cycle to execute, 2 cycles to fill pipeline until next instruction execution)?
    //                       ------> NO BUT IT COINCIDES BECAUSE IT THE SAME TIME

    if (CPSR_CHECK_FLAG(gba->cpu, CPSR_T))
        pc_dest -= 1; // TODO really -1 here? not align 2?

    LOG_DEBUG("(0x%08X) BX%s %s\n", instr, cond_names[ARM_INSTR_GET_COND(instr)], reg_names[rn]);

    // TODO is this sure?
    gba->cpu->regs[REG_PC] = pc_dest;

    gba->cpu->pipeline[PIPELINE_FETCHING] = 0x00000000;
    gba->cpu->pipeline[PIPELINE_DECODING] = 0x00000000;

    return false;
}

static bool and_handler(gba_t *gba, uint32_t instr) {
    uint8_t rn = (instr & 0x000F0000) >> 16;
    bool s = CHECK_BIT(instr, 20);

    uint32_t op1;
    uint32_t op2;
    uint8_t rd;
    bool c;
    bool set_flags = data_processing_begin(gba, instr, &rd, &op1, &op2, &c);

    LOG_DEBUG("(0x%08X) AND%s%s %s, %s, 0x%X\n", instr, cond_names[ARM_INSTR_GET_COND(instr)], s ? "S" : "", reg_names[rd], reg_names[rn], op2);

    gba->cpu->regs[rd] = op1 & op2;

    if (set_flags) {
        compute_flags_nz(gba->cpu, gba->cpu->regs[rd]);
        CPSR_CHANGE_FLAG(gba->cpu, CPSR_C, c);
    }

    return true;
}

static inline void bank_registers(gba_cpu_t *cpu, uint8_t new_mode) {
    uint8_t old_mode = CPSR_GET_MODE(cpu);

    if (old_mode == new_mode)
        return;

    // TODO if old_mode == CPSR_MODE_USR, only cond flags of cpsr reg can be changed

    uint8_t old_mode_reg_index = regs_mode_hashes[old_mode & 0x0F];
    uint8_t new_mode_reg_index = regs_mode_hashes[new_mode & 0x0F];

    if (new_mode_reg_index == REG_IDX_INVALID_MODE)
        todo("undefined behaviour: switching to invalid CPSR mode");

    if (old_mode == CPSR_MODE_FIQ || new_mode == CPSR_MODE_FIQ) {
        // backup old regs
        cpu->banked_regs_8_12[old_mode_reg_index][0] = cpu->regs[8];
        cpu->banked_regs_8_12[old_mode_reg_index][1] = cpu->regs[9];
        cpu->banked_regs_8_12[old_mode_reg_index][2] = cpu->regs[10];
        cpu->banked_regs_8_12[old_mode_reg_index][3] = cpu->regs[11];
        cpu->banked_regs_8_12[old_mode_reg_index][4] = cpu->regs[12];

        // use new regs
        cpu->regs[8] = cpu->banked_regs_8_12[new_mode_reg_index][0];
        cpu->regs[9] = cpu->banked_regs_8_12[new_mode_reg_index][1];
        cpu->regs[10] = cpu->banked_regs_8_12[new_mode_reg_index][2];
        cpu->regs[11] = cpu->banked_regs_8_12[new_mode_reg_index][3];
        cpu->regs[12] = cpu->banked_regs_8_12[new_mode_reg_index][4];
    }

    // backup old regs
    cpu->banked_regs_13_14[old_mode_reg_index][0] = cpu->regs[13];
    cpu->banked_regs_13_14[old_mode_reg_index][1] = cpu->regs[14];

    // use new regs
    cpu->regs[13] = cpu->banked_regs_13_14[new_mode_reg_index][0];
    cpu->regs[14] = cpu->banked_regs_13_14[new_mode_reg_index][1];
}

static bool msr_handler(gba_t *gba, uint32_t instr) {
    bool pd = CHECK_BIT(instr, 22);
    bool i = CHECK_BIT(instr, 25);
    bool c = CHECK_BIT(instr, 16);

    // TODO handle R15
    // TODO add undefined behaviour if CPSR_T bit is changed
    // TODO what happens when reserved bits are changed/user mode tries to change mode bits?

    uint32_t op;
    if (i) {
        uint8_t rotate = ((instr >> 8) & 0x0F) << 1;
        op = ROR(instr & 0xFF, rotate);
    } else {
        uint8_t rm = instr & 0x0F;
        op = gba->cpu->regs[rm];
    }

    uint32_t mask = c ? 0xFFFFFFFF : 0xF0000000;

    LOG_DEBUG("(0x%08X) MSR%s %cPSR_%s,%s\n", instr, cond_names[ARM_INSTR_GET_COND(instr)], pd ? 'S' : 'C', c ? "fc" : "flg", msr_op_to_str(instr, op, i));

    if (pd) {
        if (CPSR_GET_MODE(gba->cpu) == CPSR_MODE_USR)
            todo("undefined behaviour");

        CHANGE_BITS(gba->cpu->spsr[regs_mode_hashes[CPSR_GET_MODE(gba->cpu) & 0x0F]], mask, op);
    } else {
        bank_registers(gba->cpu, op & CPSR_MODE_MASK);
        CHANGE_BITS(gba->cpu->cpsr, mask, op);
    }

    return true;
}

static bool mrs_handler(gba_t *gba, uint32_t instr) {
    bool ps = CHECK_BIT(instr, 22);
    uint8_t rd = (instr >> 12) & 0x0F;

    gba->cpu->regs[rd] = ps ? gba->cpu->spsr[regs_mode_hashes[CPSR_GET_MODE(gba->cpu) & 0x0F]] : gba->cpu->cpsr;

    LOG_DEBUG("(0x%08X) MRS%s %s,%cPSR_fc\n", instr, cond_names[ARM_INSTR_GET_COND(instr)], reg_names[rd], ps ? 'S' : 'C');

    return true;
}

static bool eor_handler(gba_t *gba, uint32_t instr) {
    uint8_t rn = (instr & 0x000F0000) >> 16;
    bool s = CHECK_BIT(instr, 20);

    uint32_t op1;
    uint32_t op2;
    uint8_t rd;
    bool c;
    bool set_flags = data_processing_begin(gba, instr, &rd, &op1, &op2, &c);

    gba->cpu->regs[rd] = op1 ^ op2;

    LOG_DEBUG("(0x%08X) EOR%s %s, #0x%X\n", instr, cond_names[ARM_INSTR_GET_COND(instr)], reg_names[rn], op2);

    if (set_flags) {
        compute_flags_nz(gba->cpu, gba->cpu->regs[rd]);
        CPSR_CHANGE_FLAG(gba->cpu, CPSR_C, c);
    }

    return true;
}

static bool sub_handler(gba_t *gba, uint32_t instr) {
    uint8_t rn = (instr & 0x000F0000) >> 16;
    bool s = CHECK_BIT(instr, 20);

    uint32_t op1;
    uint32_t op2;
    uint8_t rd;
    bool c;
    bool set_flags = data_processing_begin(gba, instr, &rd, &op1, &op2, &c);

    LOG_DEBUG("(0x%08X) SUB%s%s %s, %s, 0x%X\n", instr, cond_names[ARM_INSTR_GET_COND(instr)], s ? "S" : "", reg_names[rd], reg_names[rn], op2);

    gba->cpu->regs[rd] = op1 - op2;

    if (set_flags)
        SUB_SET_FLAGS(gba->cpu, gba->cpu->regs[rd], op1, op2);

    return true;
}

static bool rsb_handler(gba_t *gba, uint32_t instr) {
    uint8_t rn = (instr & 0x000F0000) >> 16;
    bool s = CHECK_BIT(instr, 20);

    uint32_t op1;
    uint32_t op2;
    uint8_t rd;
    bool c;
    bool set_flags = data_processing_begin(gba, instr, &rd, &op1, &op2, &c);

    LOG_DEBUG("(0x%08X) RSB%s%s %s, %s, 0x%X\n", instr, cond_names[ARM_INSTR_GET_COND(instr)], s ? "S" : "", reg_names[rd], reg_names[rn], op2);

    gba->cpu->regs[rd] = op2 - op1;

    if (set_flags)
        SUB_SET_FLAGS(gba->cpu, gba->cpu->regs[rd], op2, op1);

    return true;
}

static bool add_handler(gba_t *gba, uint32_t instr) {
    uint8_t rn = (instr & 0x000F0000) >> 16;
    bool s = CHECK_BIT(instr, 20);

    uint32_t op1;
    uint32_t op2;
    uint8_t rd;
    bool c;
    bool set_flags = data_processing_begin(gba, instr, &rd, &op1, &op2, &c);

    LOG_DEBUG("(0x%08X) ADD%s%s %s, %s, 0x%X\n", instr, cond_names[ARM_INSTR_GET_COND(instr)], s ? "S" : "", reg_names[rd], reg_names[rn], op2);

    gba->cpu->regs[rd] = op1 + op2;

    if (set_flags)
        ADD_SET_FLAGS(gba->cpu, gba->cpu->regs[rd], op1, op2);

    return true;
}

static bool adc_handler(gba_t *gba, uint32_t instr) {
    uint8_t rn = (instr & 0x000F0000) >> 16;
    bool s = CHECK_BIT(instr, 20);

    uint32_t op1;
    uint32_t op2;
    uint8_t rd;
    bool c;
    bool set_flags = data_processing_begin(gba, instr, &rd, &op1, &op2, &c);

    LOG_DEBUG("(0x%08X) ADC%s%s %s, %s, 0x%X\n", instr, cond_names[ARM_INSTR_GET_COND(instr)], s ? "S" : "", reg_names[rd], reg_names[rn], op2);

    gba->cpu->regs[rd] = op1 + op2 + ((bool) CPSR_CHECK_FLAG(gba->cpu, CPSR_C));

    if (set_flags)
        ADC_SET_FLAGS(gba->cpu, gba->cpu->regs[rd], op1, op2);

    return true;
}

static bool sbc_handler(gba_t *gba, uint32_t instr) {
    uint8_t rn = (instr & 0x000F0000) >> 16;
    bool s = CHECK_BIT(instr, 20);

    uint32_t op1;
    uint32_t op2;
    uint8_t rd;
    bool c;
    bool set_flags = data_processing_begin(gba, instr, &rd, &op1, &op2, &c);

    LOG_DEBUG("(0x%08X) SBC%s%s %s, %s, 0x%X\n", instr, cond_names[ARM_INSTR_GET_COND(instr)], s ? "S" : "", reg_names[rd], reg_names[rn], op2);

    bool tmp = !((bool) CPSR_CHECK_FLAG(gba->cpu, CPSR_C));
    uint64_t res = op1 - op2 - tmp;
    gba->cpu->regs[rd] = res;

    if (set_flags)
        SBC_SET_FLAGS(gba->cpu, res, op1, op2 + tmp);

    return true;
}

static bool rsc_handler(gba_t *gba, uint32_t instr) {
    uint8_t rn = (instr & 0x000F0000) >> 16;
    bool s = CHECK_BIT(instr, 20);

    uint32_t op1;
    uint32_t op2;
    uint8_t rd;
    bool c;
    bool set_flags = data_processing_begin(gba, instr, &rd, &op1, &op2, &c);

    LOG_DEBUG("(0x%08X) RSC%s%s %s, %s, 0x%X\n", instr, cond_names[ARM_INSTR_GET_COND(instr)], s ? "S" : "", reg_names[rd], reg_names[rn], op2);

    bool tmp = !((bool) CPSR_CHECK_FLAG(gba->cpu, CPSR_C));
    uint64_t res = op2 - op1 - tmp;
    gba->cpu->regs[rd] = res;

    if (set_flags)
        SBC_SET_FLAGS(gba->cpu, res, op2, op1 + tmp);

    return true;
}

static bool tst_handler(gba_t *gba, uint32_t instr) {
    uint8_t rn = (instr & 0x000F0000) >> 16;

    uint32_t op1;
    uint32_t op2;
    uint8_t rd;
    bool c;
    bool set_flags = data_processing_begin(gba, instr, &rd, &op1, &op2, &c);

    LOG_DEBUG("(0x%08X) TST%s %s, #0x%X\n", instr, cond_names[ARM_INSTR_GET_COND(instr)], reg_names[rn], op2);

    uint32_t res = op1 & op2;

    if (set_flags) {
        compute_flags_nz(gba->cpu, res);
        CPSR_CHANGE_FLAG(gba->cpu, CPSR_C, c);
    }

    return true;
}

static bool teq_handler(gba_t *gba, uint32_t instr) {
    uint8_t rn = (instr & 0x000F0000) >> 16;

    uint32_t op1;
    uint32_t op2;
    uint8_t rd;
    bool c;
    bool set_flags = data_processing_begin(gba, instr, &rd, &op1, &op2, &c);

    LOG_DEBUG("(0x%08X) TEQ%s %s, #0x%X\n", instr, cond_names[ARM_INSTR_GET_COND(instr)], reg_names[rn], op2);

    uint32_t res = op1 ^ op2;

    if (set_flags) {
        compute_flags_nz(gba->cpu, res);
        CPSR_CHANGE_FLAG(gba->cpu, CPSR_C, c);
    }

    return true;
}

static bool cmp_handler(gba_t *gba, uint32_t instr) {
    uint8_t rn = (instr & 0x000F0000) >> 16;

    uint32_t op1;
    uint32_t op2;
    uint8_t rd;
    bool c;
    bool set_flags = data_processing_begin(gba, instr, &rd, &op1, &op2, &c);

    LOG_DEBUG("(0x%08X) CMP%s %s, #0x%X\n", instr, cond_names[ARM_INSTR_GET_COND(instr)], reg_names[rn], op2);

    uint32_t res = op1 - op2;

    if (set_flags)
        SUB_SET_FLAGS(gba->cpu, res, op1, op2);

    return true;
}

static bool cmn_handler(gba_t *gba, uint32_t instr) {
    uint8_t rn = (instr & 0x000F0000) >> 16;

    uint32_t op1;
    uint32_t op2;
    uint8_t rd;
    bool c;
    bool set_flags = data_processing_begin(gba, instr, &rd, &op1, &op2, &c);

    LOG_DEBUG("(0x%08X) CMN%s %s, #0x%X\n", instr, cond_names[ARM_INSTR_GET_COND(instr)], reg_names[rn], op2);

    uint32_t res = op1 + op2;

    if (set_flags)
        ADD_SET_FLAGS(gba->cpu, res, op1, op2);

    return true;
}

static bool orr_handler(gba_t *gba, uint32_t instr) {
    uint8_t rn = (instr & 0x000F0000) >> 16;
    bool s = CHECK_BIT(instr, 20);

    uint32_t op1;
    uint32_t op2;
    uint8_t rd;
    bool c;
    bool set_flags = data_processing_begin(gba, instr, &rd, &op1, &op2, &c);

    gba->cpu->regs[rd] = op1 | op2;

    LOG_DEBUG("(0x%08X) ORR%s %s, #0x%X\n", instr, cond_names[ARM_INSTR_GET_COND(instr)], reg_names[rn], op2);

    if (set_flags) {
        compute_flags_nz(gba->cpu, gba->cpu->regs[rd]);
        CPSR_CHANGE_FLAG(gba->cpu, CPSR_C, c);
    }

    return true;
}

static bool mov_handler(gba_t *gba, uint32_t instr) {
    bool s = CHECK_BIT(instr, 20);

    uint32_t op1;
    uint32_t op2;
    uint8_t rd;
    bool c;
    bool set_flags = data_processing_begin(gba, instr, &rd, &op1, &op2, &c);

    LOG_DEBUG("(0x%08X) MOV%s%s %s, #0x%X\n", instr, cond_names[ARM_INSTR_GET_COND(instr)], s ? "S" : "", reg_names[rd], op2);

    gba->cpu->regs[rd] = op2;

    if (set_flags) {
        compute_flags_nz(gba->cpu, gba->cpu->regs[rd]);
        CPSR_CHANGE_FLAG(gba->cpu, CPSR_C, c);
    }

    if (rd == REG_PC) {
        gba->cpu->pipeline[PIPELINE_FETCHING] = 0x00000000;
        gba->cpu->pipeline[PIPELINE_DECODING] = 0x00000000;
        return false;
    }

    return true;
}

static bool bic_handler(gba_t *gba, uint32_t instr) {
    bool s = CHECK_BIT(instr, 20);

    uint32_t op1;
    uint32_t op2;
    uint8_t rd;
    bool c;
    bool set_flags = data_processing_begin(gba, instr, &rd, &op1, &op2, &c);

    LOG_DEBUG("(0x%08X) BIC%s%s %s, #0x%X\n", instr, cond_names[ARM_INSTR_GET_COND(instr)], s ? "S" : "", reg_names[rd], op2);

    gba->cpu->regs[rd] = op1 & ~op2;

    if (set_flags) {
        compute_flags_nz(gba->cpu, gba->cpu->regs[rd]);
        CPSR_CHANGE_FLAG(gba->cpu, CPSR_C, c);
    }

    if (rd == REG_PC) {
        gba->cpu->pipeline[PIPELINE_FETCHING] = 0x00000000;
        gba->cpu->pipeline[PIPELINE_DECODING] = 0x00000000;
        return false;
    }

    return true;
}

static bool mvn_handler(gba_t *gba, uint32_t instr) {
    bool s = CHECK_BIT(instr, 20);

    uint32_t op1;
    uint32_t op2;
    uint8_t rd;
    bool c;
    bool set_flags = data_processing_begin(gba, instr, &rd, &op1, &op2, &c);

    LOG_DEBUG("(0x%08X) MVN%s%s %s, #0x%X\n", instr, cond_names[ARM_INSTR_GET_COND(instr)], s ? "S" : "", reg_names[rd], op2);

    gba->cpu->regs[rd] = ~op2;

    if (set_flags) {
        compute_flags_nz(gba->cpu, gba->cpu->regs[rd]);
        CPSR_CHANGE_FLAG(gba->cpu, CPSR_C, c);
    }

    if (rd == REG_PC) {
        gba->cpu->pipeline[PIPELINE_FETCHING] = 0x00000000;
        gba->cpu->pipeline[PIPELINE_DECODING] = 0x00000000;
        return false;
    }

    return true;
}

static bool strh_reg_handler(gba_t *gba, uint32_t instr) {
    bool p = CHECK_BIT(instr, 24);
    bool u = CHECK_BIT(instr, 23);
    bool w = CHECK_BIT(instr, 21);
    bool l = CHECK_BIT(instr, 20);
    bool s = CHECK_BIT(instr, 6);
    bool h = CHECK_BIT(instr, 5);

    uint8_t rn = (instr >> 16) & 0x07;
    uint8_t rd = (instr >> 12) & 0x07;
    uint8_t rm = instr & 0x0F;

    int32_t offset = gba->cpu->regs[rm];
    uint32_t base_addr = gba->cpu->regs[rn];

    if (!u)
        offset = -offset;

    if (p)
        base_addr += offset;

    if (w)
        todo("w");

    switch ((s << 1) | h) {
    case 0b01:
        gba_bus_write_half(gba, base_addr, gba->cpu->regs[rd]);
        break;
    case 0b10:
        todo("0b10");
        break;
    case 0b11:
        todo("0b11");
        break;
    case 0b00:
    default:
        todo();
    }

    if (!p)
        base_addr += offset;

    LOG_DEBUG("(0x%08X) STR%s%s%s %s, %s\n", instr, cond_names[ARM_INSTR_GET_COND(instr)], s ? "S" : "", h ? "H" : "", reg_names[rd], strh_addr_str(rn, rm, true, p, u, w));

    return true;
}

static bool strh_imm_handler(gba_t *gba, uint32_t instr) {
    bool p = CHECK_BIT(instr, 24);
    bool u = CHECK_BIT(instr, 23);
    bool w = CHECK_BIT(instr, 21);
    bool l = CHECK_BIT(instr, 20);
    bool s = CHECK_BIT(instr, 6);
    bool h = CHECK_BIT(instr, 5);

    uint8_t rn = (instr >> 16) & 0x07;
    uint8_t rd = (instr >> 12) & 0x07;
    int32_t offset = ((instr & 0x0F00) >> 4) | (instr & 0x0F);

    uint32_t base_addr = gba->cpu->regs[rn];

    if (!u)
        offset = -offset;

    if (p)
        base_addr += offset;

    if (w)
        todo("w");

    switch ((s << 1) | h) {
        case 0b01:
            gba_bus_write_half(gba, base_addr, gba->cpu->regs[rd]);
            break;
        case 0b10:
            todo("0b10");
            break;
        case 0b11:
            todo("0b11");
            break;
        case 0b00:
        default:
            todo();
        }

    if (!p)
        base_addr += offset;

    LOG_DEBUG("(0x%08X) STR%s%s%s %s, %s\n", instr, cond_names[ARM_INSTR_GET_COND(instr)], s ? "S" : "", h ? "H" : "", reg_names[rd], strh_addr_str(rn, offset, false, p, u, w));

    return true;
}

// TODO ldm and str handlers are VERY similar --> break into subfunctions
static bool str_handler(gba_t *gba, uint32_t instr) {
    bool i = CHECK_BIT(instr, 25);
    bool p = CHECK_BIT(instr, 24);
    bool u = CHECK_BIT(instr, 23);
    bool b = CHECK_BIT(instr, 22);
    bool w = CHECK_BIT(instr, 21);
    uint8_t rn = (instr >> 16) & 0x0F;
    uint8_t rd = (instr >> 12) & 0x0F;

    uint32_t addr = gba->cpu->regs[rn];
    uint32_t offset;

    if (i) {
        uint8_t rm = instr & 0x0F;
        offset = gba->cpu->regs[rm];

        uint8_t shift = (instr >> 4) & 0xFF;
        uint8_t shift_type = (shift & 0b00000110) >> 1;
        uint8_t amount = (shift & 0xF8) >> 3;

        bool c;
        offset = shift_offset(gba->cpu, shift_type, offset, amount, true, &c);
    } else {
        offset = instr & 0x0FFF;
    }

    if (!u)
        offset = -offset;

    if (p)
        addr += offset;

    LOG_DEBUG("(0x%08X) STR%s%s%s %s,[%s%s,#%s0x%X%s\n", instr, cond_names[ARM_INSTR_GET_COND(instr)], b ? "B" : "", w ? "T" : "", reg_names[rd], reg_names[rn], p ? "" : "]", u ? "" : "-", u ? offset : -offset, p ? "]" : "");

    uint32_t data = gba->cpu->regs[rd];
    if (rd == REG_PC)
        data += 4;

    if (b)
        gba_bus_write_byte(gba, addr, data);
    else
        gba_bus_write_word(gba, addr, data);

    if (!p)
        addr += offset;

    if (w || !p)
        gba->cpu->regs[rn] = addr;

    return true;
}

static bool ldr_handler(gba_t *gba, uint32_t instr) {
    bool i = CHECK_BIT(instr, 25);
    bool p = CHECK_BIT(instr, 24);
    bool u = CHECK_BIT(instr, 23);
    bool b = CHECK_BIT(instr, 22);
    bool w = CHECK_BIT(instr, 21);
    uint8_t rn = (instr >> 16) & 0x0F;
    uint8_t rd = (instr >> 12) & 0x0F;

    uint32_t addr = gba->cpu->regs[rn];
    uint32_t offset;

    if (i) {
        uint8_t rm = instr & 0x0F;
        offset = gba->cpu->regs[rm];

        uint8_t shift = (instr >> 4) & 0xFF;
        uint8_t shift_type = (shift & 0b00000110) >> 1;
        uint8_t amount = (shift & 0xF8) >> 3;

        bool c;
        offset = shift_offset(gba->cpu, shift_type, offset, amount, true, &c);
    } else {
        offset = instr & 0x0FFF;
    }

    if (!u)
        offset = -offset;

    if (p)
        addr += offset;

    LOG_DEBUG("(0x%08X) LDR%s%s%s %s,[%s%s,#%s0x%X%s\n", instr, cond_names[ARM_INSTR_GET_COND(instr)], b ? "B" : "", w ? "T" : "", reg_names[rd], reg_names[rn], p ? "" : "]", u ? "" : "-", u ? offset : -offset, p ? "]" : "");

    uint32_t data;
    if (b) {
        data = gba_bus_read_byte(gba, addr);
    } else {
        data = gba_bus_read_word(gba, addr);
        uint8_t amount = (addr & 0x03) << 3;
        data = ROR(data, amount);
    }

    if (!p)
        addr += offset;

    if (w || !p)
        gba->cpu->regs[rn] = addr;

    gba->cpu->regs[rd] = data;

    if (rd == REG_PC) {
        gba->cpu->pipeline[PIPELINE_FETCHING] = 0x00000000;
        gba->cpu->pipeline[PIPELINE_DECODING] = 0x00000000;

        return false;
    }

    return true;
}

static bool stm_handler(gba_t *gba, uint32_t instr) {
    bool p = CHECK_BIT(instr, 24);
    bool u = CHECK_BIT(instr, 23);
    bool s = CHECK_BIT(instr, 22);
    bool w = CHECK_BIT(instr, 21);
    uint8_t rn = (instr >> 16) & 0x0F;
    uint16_t rlist = instr & 0xFFFF;

    if (s)
        todo("PSR & force user");

    STM(gba, rn, rlist, p, u, w);

    LOG_DEBUG("(0x%08X) STM%s%s %s%s, {%s}%s\n", instr, cond_names[ARM_INSTR_GET_COND(instr)], stm_ldm_addr_mode_names[rn == REG_SP ? 0 : 1][(p << 1) | u], reg_names[rn], w ? "!" : "", rlist_to_str(rlist), s ? "^" : "");

    return true;
}

static bool ldm_handler(gba_t *gba, uint32_t instr) {
    bool p = CHECK_BIT(instr, 24);
    bool u = CHECK_BIT(instr, 23);
    bool s = CHECK_BIT(instr, 22);
    bool w = CHECK_BIT(instr, 21);
    uint8_t rn = (instr >> 16) & 0x0F;
    uint16_t rlist = instr & 0xFFFF;

    if (s)
        todo("PSR & force user");

    LDM(gba, rn, rlist, p, u, w);

    LOG_DEBUG("(0x%08X) LDM%s%s %s%s, {%s}%s\n", instr, cond_names[ARM_INSTR_GET_COND(instr)], stm_ldm_addr_mode_names[rn == REG_SP ? 0 : 1][(u << 1) | p], reg_names[rn], w ? "!" : "", rlist_to_str(rlist), s ? "^" : "");

    if (CHECK_BIT(rlist, REG_PC)) {
        gba->cpu->pipeline[PIPELINE_FETCHING] = 0x00000000;
        gba->cpu->pipeline[PIPELINE_DECODING] = 0x00000000;
        return false;
    } else {
        return true;
    }
}

static bool b_handler(gba_t *gba, uint32_t instr) {
    uint32_t offset = instr & 0x00FFFFFF;

    // TODO prefetch operation
    if (CHECK_BIT(offset, 23))
        offset |= 0xFF000000;
    offset <<= 2;

    gba->cpu->regs[REG_PC] += offset;

    gba->cpu->pipeline[PIPELINE_FETCHING] = 0x00000000;
    gba->cpu->pipeline[PIPELINE_DECODING] = 0x00000000;

    LOG_DEBUG("(0x%08X) B%s #Lxx_0x%08X\n", instr, cond_names[ARM_INSTR_GET_COND(instr)], gba->cpu->regs[REG_PC]);

    return false;
}

static bool bl_handler(gba_t *gba, uint32_t instr) {
    uint32_t offset = instr & 0x00FFFFFF;

    // TODO prefetch operation
    if (CHECK_BIT(offset, 23))
        offset |= 0xFF000000;
    offset <<= 2;

    gba->cpu->regs[REG_LR] = gba->cpu->regs[REG_PC] - 4; // store addr of next instr before jumping
    gba->cpu->regs[REG_PC] += offset;

    gba->cpu->pipeline[PIPELINE_FETCHING] = 0x00000000;
    gba->cpu->pipeline[PIPELINE_DECODING] = 0x00000000;

    LOG_DEBUG("(0x%08X) BL%s #Lxx_0x%08X\n", instr, cond_names[ARM_INSTR_GET_COND(instr)], gba->cpu->regs[REG_PC]);

    return false;
}

static inline bool verif_cond(gba_cpu_t *cpu, cond_t cond) {
    switch (cond) {
    case COND_EQ:
        return CPSR_CHECK_FLAG(cpu, CPSR_Z);
    case COND_NE:
        return !CPSR_CHECK_FLAG(cpu, CPSR_Z);
    case COND_CS:
        return CPSR_CHECK_FLAG(cpu, CPSR_C);
    case COND_CC:
        return !CPSR_CHECK_FLAG(cpu, CPSR_C);
    case COND_MI:
        return CPSR_CHECK_FLAG(cpu, CPSR_N);
    case COND_PL:
        return !CPSR_CHECK_FLAG(cpu, CPSR_N);
    case COND_VS:
        return CPSR_CHECK_FLAG(cpu, CPSR_V);
    case COND_VC:
        return !CPSR_CHECK_FLAG(cpu, CPSR_V);
    case COND_HI:
        return CPSR_CHECK_FLAG(cpu, CPSR_C) && !CPSR_CHECK_FLAG(cpu, CPSR_Z);
    case COND_LS:
        return !CPSR_CHECK_FLAG(cpu, CPSR_C) || CPSR_CHECK_FLAG(cpu, CPSR_Z);
    case COND_GE:
        return !!CPSR_CHECK_FLAG(cpu, CPSR_N) == !!CPSR_CHECK_FLAG(cpu, CPSR_V);
    case COND_LT:
        return !!CPSR_CHECK_FLAG(cpu, CPSR_N) != !!CPSR_CHECK_FLAG(cpu, CPSR_V);
    case COND_GT:
        return !CPSR_CHECK_FLAG(cpu, CPSR_Z) && (!!CPSR_CHECK_FLAG(cpu, CPSR_N) == !!CPSR_CHECK_FLAG(cpu, CPSR_V));
    case COND_LE:
        return CPSR_CHECK_FLAG(cpu, CPSR_Z) || (!!CPSR_CHECK_FLAG(cpu, CPSR_N) != !!CPSR_CHECK_FLAG(cpu, CPSR_V));
    case COND_AL:
        return true;
    default:
        todo("invalid condition code: 0x%02X", cond);
        return false;
    }
}

void gba_cpu_step(gba_t *gba) {
    gba_cpu_t *cpu = gba->cpu;

    LOG_DEBUG(
        "--------\n[PC=0x%08X] [COND=%c%c%c%c %c%c%c]\n",
        cpu->regs[REG_PC],
        CPSR_CHECK_FLAG(cpu, CPSR_N) ? 'N' : '-',
        CPSR_CHECK_FLAG(cpu, CPSR_Z) ? 'Z' : '-',
        CPSR_CHECK_FLAG(cpu, CPSR_C) ? 'C' : '-',
        CPSR_CHECK_FLAG(cpu, CPSR_V) ? 'V' : '-',
        CPSR_CHECK_FLAG(cpu, CPSR_I) ? 'I' : '-',
        CPSR_CHECK_FLAG(cpu, CPSR_F) ? 'F' : '-',
        CPSR_CHECK_FLAG(cpu, CPSR_T) ? 'T' : '-'
    );

    int pc_increment_shift;
    uint32_t fetched_instr;
    if (CPSR_CHECK_FLAG(gba->cpu, CPSR_T)) {
        pc_increment_shift = 1;
        fetched_instr = gba_bus_read_half(gba, cpu->regs[REG_PC]);
    } else {
        pc_increment_shift = 2;
        fetched_instr = gba_bus_read_word(gba, cpu->regs[REG_PC]);
    }

    uint32_t instr = cpu->pipeline[PIPELINE_DECODING];

    // fetch
    cpu->pipeline[PIPELINE_DECODING] = cpu->pipeline[PIPELINE_FETCHING];
    cpu->pipeline[PIPELINE_FETCHING] = fetched_instr;
    // TODO bus_read can stall CPU (nop instruction inserted) while reading memory (depends on waitstates)
    //      while this stalls, the decode and execute stages continue their operation
    LOG_DEBUG("fetch:   0x%0.*X\n", 1 << pc_increment_shift, cpu->pipeline[PIPELINE_FETCHING]);

    // decode
    LOG_DEBUG("decode:  0x%0.*X\n", 1 << pc_increment_shift, cpu->pipeline[PIPELINE_DECODING]);

    // execute
    // TODO
    // if (cpu->stall) {
        // cpu->stall--;
        // LOG_DEBUG("CPU stalled, remaining: %d\n", cpu->stall);
        // return;
    // }
#ifdef DEBUG
    LOG_DEBUG("execute: 0x%0.*X\n", 1 << pc_increment_shift, instr);
    for (size_t i = 0; i < sizeof(cpu->regs) / sizeof(*cpu->regs); i++)
        LOG_DEBUG("\t%s=0x%08X\n", reg_names[i], gba->cpu->regs[i]);
#endif

    bool increment_pc = true;
    if (CPSR_CHECK_FLAG(gba->cpu, CPSR_T)) {
        uint_fast8_t instr_hash = instr >> 8;
        increment_pc = handlers[thumb_handlers[instr_hash]](gba, instr);
    } else if (verif_cond(gba->cpu, ARM_INSTR_GET_COND(instr))) {
        uint_fast16_t instr_hash = ((instr & 0x0FF00000) >> 16) | ((instr & 0x000000F0) >> 4);
        increment_pc = handlers[arm_handlers[instr_hash]](gba, instr);
    }

    gba->cpu->regs[REG_PC] += increment_pc << pc_increment_shift;
}

static bool thumb_lsl_handler(gba_t *gba, uint32_t instr) {
    // ignore upper 16 bits of instr

    uint8_t offset5 = (instr & 0x7C0) >> 6;
    uint8_t rs = (instr & 0x38) >> 3;
    uint8_t rd = instr & 0x7;

    uint32_t res = LSL(gba->cpu->regs[rs], offset5);

    compute_flags_nz(gba->cpu, res);
    if (offset5 != 0)
        CPSR_CHANGE_FLAG(gba->cpu, CPSR_C, GET_BIT(gba->cpu->regs[rd], 31));

    gba->cpu->regs[rd] = res;

    LOG_DEBUG("(0x%04X) LSL %s, %s, #0x%01X\n", instr, reg_names[rd], reg_names[rs], offset5);

    return true;
}

static bool thumb_lsr_handler(gba_t *gba, uint32_t instr) {
    // ignore upper 16 bits of instr

    uint8_t offset5 = (instr & 0x7C0) >> 6;
    uint8_t rs = (instr & 0x38) >> 3;
    uint8_t rd = instr & 0x7;

    uint32_t res = LSR(gba->cpu->regs[rs], offset5);

    compute_flags_nz(gba->cpu, res);
    CPSR_CHANGE_FLAG(gba->cpu, CPSR_C, GET_BIT(gba->cpu->regs[rd], 31));

    gba->cpu->regs[rd] = res;

    LOG_DEBUG("(0x%04X) LSR %s, %s, #0x%01X\n", instr, reg_names[rd], reg_names[rs], offset5);

    return true;
}

static bool thumb_asr_handler(gba_t *gba, uint32_t instr) {
    // ignore upper 16 bits of instr
    todo();
    return true;
}

static bool thumb_add_handler(gba_t *gba, uint32_t instr) {
    uint8_t rd = instr & 0x07;
    uint8_t rs = (instr >> 3) & 0x07;
    uint8_t rn_offset3 = (instr >> 6) & 0x07;

    uint32_t op1 = gba->cpu->regs[rs];
    uint32_t op2;

    if (CHECK_BIT(instr, 10)) {
        LOG_DEBUG("(0x%04X) ADD %s, %s, #0x%01X\n", instr, reg_names[rd], reg_names[rs], rn_offset3);

        op2 = rn_offset3;
    } else {
        LOG_DEBUG("(0x%04X) ADD %s, %s, %s\n", instr, reg_names[rd], reg_names[rs], reg_names[rn_offset3]);

        op2 = gba->cpu->regs[rn_offset3];
    }

    gba->cpu->regs[rd] = op1 + op2;

    ADD_SET_FLAGS(gba->cpu, gba->cpu->regs[rd], op1, op2);

    return true;
}

static bool thumb_sub_handler(gba_t *gba, uint32_t instr) {
    uint8_t rd = instr & 0x07;
    uint8_t rs = (instr >> 3) & 0x07;
    uint8_t rn_offset3 = (instr >> 6) & 0x07;

    uint32_t op1 = gba->cpu->regs[rs];
    uint32_t op2;

    if (CHECK_BIT(instr, 10)) {
        LOG_DEBUG("(0x%04X) SUB %s, %s, #0x%01X\n", instr, reg_names[rd], reg_names[rs], rn_offset3);

        op2 = rn_offset3;
    } else {
        LOG_DEBUG("(0x%04X) SUB %s, %s, %s\n", instr, reg_names[rd], reg_names[rs], reg_names[rn_offset3]);

        op2 = gba->cpu->regs[rn_offset3];
    }

    uint32_t res = op1 - op2;
    SUB_SET_FLAGS(gba->cpu, res, op1, op2);
    gba->cpu->regs[rd] = res;

    return true;
}

static bool thumb_mov_handler(gba_t *gba, uint32_t instr) {
    // ignore upper 16 bits of instr
    uint8_t offset8 = instr & 0xFF;
    uint8_t rd = (instr >> 8) & 0x07;

    gba->cpu->regs[rd] = offset8;

    compute_flags_nz(gba->cpu, gba->cpu->regs[rd]);

    LOG_DEBUG("(0x%04X) MOV %s, #0x%01X\n", instr, reg_names[rd], offset8);

    return true;
}

static bool thumb_sub_imm_handler(gba_t *gba, uint32_t instr) {
    // ignore upper 16 bits of instr
    uint8_t offset8 = instr & 0xFF;
    uint8_t rd = (instr >> 8) & 0x07;

    uint32_t res = gba->cpu->regs[rd] - offset8;
    SUB_SET_FLAGS(gba->cpu, res, gba->cpu->regs[rd], offset8);
    gba->cpu->regs[rd] = res;

    LOG_DEBUG("(0x%04X) SUB %s, #0x%01X\n", instr, reg_names[rd], offset8);

    return true;
}

static bool thumb_alu_ops_handler(gba_t *gba, uint32_t instr) {
    uint8_t op = (instr >> 6) & 0x0F;
    uint8_t rd = instr & 0x07;
    uint8_t rs = (instr >> 3) & 0x07;

    uint32_t res;

    switch (op) {
    case 0b0000:
        todo();
        break;
    case 0b0001:
        todo();
        break;
    case 0b0010:
        todo();
        break;
    case 0b0011:
        todo();
        break;
    case 0b0100:
        todo();
        break;
    case 0b0101:
        todo();
        break;
    case 0b0110:
        todo();
        break;
    case 0b0111:
        todo();
        break;
    case 0b1000:
        res = gba->cpu->regs[rd] & gba->cpu->regs[rs];
        compute_flags_nz(gba->cpu, res);
        LOG_DEBUG("(0x%04X) TST %s, %s\n", instr, reg_names[rd], reg_names[rs]);
        break;
    case 0b1001:
        todo();
        break;
    case 0b1010:
        todo();
        break;
    case 0b1011:
        todo();
        break;
    case 0b1100:
        gba->cpu->regs[rd] |= gba->cpu->regs[rs];
        compute_flags_nz(gba->cpu, gba->cpu->regs[rd]);
        LOG_DEBUG("(0x%04X) ORR %s, %s\n", instr, reg_names[rd], reg_names[rs]);
        break;
    case 0b1101:
        todo();
        break;
    case 0b1110:
        gba->cpu->regs[rd] &= ~gba->cpu->regs[rs];
        compute_flags_nz(gba->cpu, gba->cpu->regs[rd]);
        LOG_DEBUG("(0x%04X) BIC %s, %s\n", instr, reg_names[rd], reg_names[rs]);
        break;
    case 0b1111:
        gba->cpu->regs[rd] = ~gba->cpu->regs[rs];
        compute_flags_nz(gba->cpu, gba->cpu->regs[rd]);
        LOG_DEBUG("(0x%04X) MVN %s, %s\n", instr, reg_names[rd], reg_names[rs]);
        break;
    }

    return true;
}

static bool thumb_mov_hi_reg_handler(gba_t *gba, uint32_t instr) {
    uint8_t hi_op_flag = (instr >> 6) & 0x03;
    uint8_t rs_hs = (instr >> 3) & 0x07;
    uint8_t rd_hd = instr & 0x07;

    switch (hi_op_flag) {
    case 0b01:
        gba->cpu->regs[rd_hd] = gba->cpu->regs[rs_hs + 8];
        LOG_DEBUG("(0x%04X) MOV %s %s\n", instr, reg_names[rd_hd], reg_names[rs_hs + 8]);
        break;
    case 0b10:
        gba->cpu->regs[rd_hd + 8] = gba->cpu->regs[rs_hs];
        LOG_DEBUG("(0x%04X) MOV %s %s\n", instr, reg_names[rd_hd + 8], reg_names[rs_hs]);
        break;
    case 0b11:
        gba->cpu->regs[rd_hd + 8] = gba->cpu->regs[rs_hs + 8];
        LOG_DEBUG("(0x%04X) MOV %s %s\n", instr, reg_names[rd_hd + 8], reg_names[rs_hs + 8]);
        todo();
        break;
    default:
        todo("undefined behaviour");
        break;
    }

    return true;
}

static bool thumb_bx_handler(gba_t *gba, uint32_t instr) {
    if (CHECK_BIT(instr, 7))
        todo("undefined behaviour");

    uint8_t rd = (instr >> 3) & 0x07;
    uint8_t rn = CHECK_BIT(instr, 6) ? rd + 8 : rd;

    uint32_t pc_dest = gba->cpu->regs[rn];

    // TODO this should be undefined behaviour only if jumping from a non word aligned address (with -4 or -2 offset? or none?) AND rn == REG_PC
    if (rn == REG_PC)
        todo("undefined behaviour");

    // change cpu state to THUMB/ARM
    CPSR_CHANGE_FLAG(gba->cpu, CPSR_T, GET_BIT(pc_dest, 0));

    // TODO 2S + 1N cycles --> does this take into account the pipeline flush (1 cycle to execute, 2 cycles to fill pipeline until next instruction execution)?
    //                       ------> NO BUT IT COINCIDES BECAUSE IT THE SAME TIME

    if (CPSR_CHECK_FLAG(gba->cpu, CPSR_T))
        pc_dest &= 0xFFFFFFFC;
    else
        pc_dest &= 0xFFFFFFFE;

    LOG_DEBUG("(0x%04X) BX %s\n", instr, reg_names[rn]);

    gba->cpu->regs[REG_PC] = pc_dest;

    gba->cpu->pipeline[PIPELINE_FETCHING] = 0x00000000;
    gba->cpu->pipeline[PIPELINE_DECODING] = 0x00000000;

    return false;
}

static bool thumb_pc_relative_ldr_handler(gba_t *gba, uint32_t instr) {
    uint32_t word8 = (instr & 0xFF) << 2;
    uint8_t rd = (instr & 0x700) >> 8;

    uint32_t val = (gba->cpu->regs[REG_PC] & ~2) + (word8 << 2);

    gba->cpu->regs[rd] = gba_bus_read_word(gba, val);

    LOG_DEBUG("(0x%04X) LDR %s, Lxx_#0x%08X\n", instr, reg_names[rd], val);

    return true;
}

static bool thumb_reg_str_handler(gba_t *gba, uint32_t instr) {
    uint8_t rd = instr & 0x07;
    uint8_t rb = (instr >> 3) & 0x07;
    uint8_t ro = (instr >> 6) & 0x07;

    bool b = CHECK_BIT(instr, 10);

    LOG_DEBUG("(0x%04X) STR%s %s, [%s, %s]\n", instr, b ? "B" : "", reg_names[rd], reg_names[rb], reg_names[ro]);

    LOG_DEBUG("%d %d %X\n", gba->cpu->regs[rb], gba->cpu->regs[rb], (int) gba->cpu->regs[rb]);
    // todo("%u %d %X", (unsigned int) gba->cpu->regs[ro], gba->cpu->regs[ro], (int) gba->cpu->regs[ro]);

    if (b)
        gba_bus_write_byte(gba, gba->cpu->regs[rd], gba->cpu->regs[rb] + gba->cpu->regs[ro]);
    else
        gba_bus_write_word(gba, gba->cpu->regs[rd], gba->cpu->regs[rb] + gba->cpu->regs[ro]);

    return true;
}

static bool thumb_strh_handler(gba_t *gba, uint32_t instr) {
    uint8_t offset5 = (instr >> 6) & 0x1F;
    uint8_t rb = (instr >> 3) & 0x07;
    uint8_t rd = instr & 0x07;

    gba_bus_write_half(gba, gba->cpu->regs[rb] + offset5, gba->cpu->regs[rd]);

    LOG_DEBUG("(0x%04X) STRH %s, [%s, #0x%02X]\n", instr, reg_names[rd], reg_names[rb], offset5);

    return true;
}

static bool thumb_str_sp_handler(gba_t *gba, uint32_t instr) {
    uint8_t rd = (instr >> 8) & 0x07;
    uint16_t offset = (instr & 0x00FF) << 2;

    LOG_DEBUG("(0x%04X) STR %s, [%s, #0x%04X]\n", instr, reg_names[rd], reg_names[REG_SP], offset);

    gba_bus_write_word(gba, gba->cpu->regs[REG_SP] + offset, gba->cpu->regs[rd]);

    return true;
}

static bool thumb_add_addr_handler(gba_t *gba, uint32_t instr) {
    bool sp = CHECK_BIT(instr, 11);
    uint8_t rd = (instr >> 8) & 0x07;
    uint16_t offset = (instr & 0xFF) << 2;

    LOG_DEBUG("(0x%04X) ADD %s, %s, #0x%04X\n", instr, reg_names[rd], reg_names[sp ? REG_SP : REG_PC], offset);

    uint32_t addr = offset;
    if (sp)
        addr += gba->cpu->regs[REG_SP];
    else
        addr += gba->cpu->regs[REG_PC] & ~2;

    gba->cpu->regs[rd] = addr;

    return true;
}

static bool thumb_add_sp_handler(gba_t *gba, uint32_t instr) {
    bool s = CHECK_BIT(instr, 7);
    uint16_t offset = ((instr & 0x007F) << 2) * s;

    LOG_DEBUG("(0x%04X) ADD %s #%s0x%04X\n", instr, reg_names[REG_SP], s ? "-" : "", offset);

    gba->cpu->regs[REG_SP] += offset;

    return true;
}

static bool thumb_stm_handler(gba_t *gba, uint32_t instr) {
    uint8_t rb = (instr >> 8) & 0x07;
    uint8_t rlist = instr & 0xFF;

    todo();
    STM(gba, rb, rlist, 4, false, true);

    LOG_DEBUG("(0x%04X) STMIA %s! {%s}\n", instr, reg_names[rb], rlist_to_str(rlist));

    return true;
}

static bool thumb_push_handler(gba_t *gba, uint32_t instr) {
    bool r = CHECK_BIT(instr, 8);
    uint8_t rlist = instr & 0xFF;

    if (r) {
        gba_bus_write_word(gba, gba->cpu->regs[REG_SP], gba->cpu->regs[REG_LR]);
        gba->cpu->regs[REG_SP] -= 4;
    }

    todo();
    STM(gba, REG_SP, rlist, -4, false, true);

    LOG_DEBUG("(0x%04X) PUSH {%s%s%s}\n", instr, rlist_to_str(rlist), rlist ? "," : "", r ? reg_names[REG_LR] : "");

    return true;
}

static bool thumb_pop_handler(gba_t *gba, uint32_t instr) {
    todo("thumb_pop_handler");
    return true;
}

static bool thumb_b_cond_handler(gba_t *gba, uint32_t instr) {
    int8_t soffset8 = ((int8_t) (instr & 0xFF)) << 1;
    uint8_t cond = (instr >> 8) & 0x0F;

    // TODO this should be done in cpu_step() ?
    if (!verif_cond(gba->cpu, cond))
        return true;

    gba->cpu->regs[REG_PC] += soffset8;

    gba->cpu->pipeline[PIPELINE_FETCHING] = 0x00000000;
    gba->cpu->pipeline[PIPELINE_DECODING] = 0x00000000;

    LOG_DEBUG("(0x%04X) B%s Lxx_#0x%04X\n", instr, cond_names[cond], gba->cpu->regs[REG_PC]);

    return false;
}

static bool thumb_b_handler(gba_t *gba, uint32_t instr) {
    int16_t offset11 = ((int16_t) (instr & 0x07FF)) << 1;

    gba->cpu->regs[REG_PC] += offset11;

    gba->cpu->pipeline[PIPELINE_FETCHING] = 0x00000000;
    gba->cpu->pipeline[PIPELINE_DECODING] = 0x00000000;

    LOG_DEBUG("(0x%04X) B Lxx_#0x%04X\n", instr, gba->cpu->regs[REG_PC]);

    return false;
}

static bool thumb_bl_handler(gba_t *gba, uint32_t instr) {
    bool h = CHECK_BIT(instr, 11);
    uint16_t offset = instr & 0x07FF;

    if (h) { // instruction 2
        // gba->cpu->regs[REG_LR] += offset << 1;

        // uint32_t tmp = gba->cpu->regs[REG_LR];
        // gba->cpu->regs[REG_LR] = (gba->cpu->regs[REG_PC] - 2) | 1;
        // gba->cpu->regs[REG_PC] = tmp;

        uint32_t tmp = gba->cpu->regs[REG_PC] - 2;
        gba->cpu->regs[REG_PC] = (gba->cpu->regs[REG_LR] + (offset << 1)) & ~1;
        gba->cpu->regs[REG_LR] = tmp | 1;

        gba->cpu->pipeline[PIPELINE_FETCHING] = 0x00000000;
        gba->cpu->pipeline[PIPELINE_DECODING] = 0x00000000;

        LOG_DEBUG("(0x%04X) BL Lxx_#%08X\n", instr, gba->cpu->regs[REG_PC]);

        return false;
    } else { // instruction 1
        // gba->cpu->regs[REG_LR] = gba->cpu->regs[REG_PC] + (offset << 12);
        offset <<= 12;
        if (offset & 0x00400000) // sign extend?
            offset |= 0xFF800000;
        gba->cpu->regs[REG_LR] = gba->cpu->regs[REG_PC] + offset;

        LOG_DEBUG("(0x%04X) BL FIRST STEP\n", instr);

        return true;
    }
}

#define FOREACH_HANDLER(X)           \
    X(not_implemented_handler)       \
    X(and_handler)                   \
    X(msr_handler)                   \
    X(mrs_handler)                   \
    X(eor_handler)                   \
    X(sub_handler)                   \
    X(rsb_handler)                   \
    X(add_handler)                   \
    X(adc_handler)                   \
    X(sbc_handler)                   \
    X(rsc_handler)                   \
    X(tst_handler)                   \
    X(teq_handler)                   \
    X(cmp_handler)                   \
    X(cmn_handler)                   \
    X(orr_handler)                   \
    X(bic_handler)                   \
    X(mvn_handler)                   \
    X(mov_handler)                   \
    X(bx_handler)                    \
    X(mul_handler)                   \
    X(mla_handler)                   \
    X(mull_handler)                  \
    X(strh_reg_handler)              \
    X(strh_imm_handler)              \
    X(str_handler)                   \
    X(ldr_handler)                   \
    X(stm_handler)                   \
    X(ldm_handler)                   \
    X(b_handler)                     \
    X(bl_handler)                    \
    X(thumb_lsl_handler)             \
    X(thumb_lsr_handler)             \
    X(thumb_asr_handler)             \
    X(thumb_add_handler)             \
    X(thumb_sub_handler)             \
    X(thumb_mov_handler)             \
    X(thumb_sub_imm_handler)         \
    X(thumb_alu_ops_handler)         \
    X(thumb_mov_hi_reg_handler)      \
    X(thumb_bx_handler)              \
    X(thumb_pc_relative_ldr_handler) \
    X(thumb_reg_str_handler)         \
    X(thumb_strh_handler)            \
    X(thumb_str_sp_handler)          \
    X(thumb_add_addr_handler)        \
    X(thumb_add_sp_handler)          \
    X(thumb_push_handler)            \
    X(thumb_pop_handler)             \
    X(thumb_stm_handler)             \
    X(thumb_b_cond_handler)          \
    X(thumb_b_handler)               \
    X(thumb_bl_handler)
#define HANDLER_ID(name) name##_id
#define HANDLER_ID_GENERATOR(name) HANDLER_ID(name),
#define HANDLER_FUNC_PTR_GENERATOR(name) name,

typedef enum {
    FOREACH_HANDLER(HANDLER_ID_GENERATOR)
} handler_id_t;

handler_t handlers[] = {
    FOREACH_HANDLER(HANDLER_FUNC_PTR_GENERATOR)
};

typedef struct {
    const char match_string[32]; // '1': bit MUST be set, '0': bit MUST be reset, '*': bit can be set OR reset
    uint8_t handler_id;
} decoder_rule_t;
// arm_decoder_rules definition order is important: they are matched in the order they are defined in the arm_decoder_rules array
// TODO '_' == '*' for now, its to visually help me find relevant bits: '_' are alway ignored because they are not relevant in the instruction hash
decoder_rule_t arm_decoder_rules[] = {
    // TODO bx handler and msr handler are confondus
    // --> needs another way to distinguish them
    {"____00010010____________0001____", HANDLER_ID(bx_handler)}, // Branch and exchange
    {"____0000000*____________1001____", HANDLER_ID(mul_handler)}, // Multiply
    {"____0000001*____________1001____", HANDLER_ID(mla_handler)}, // Multiply
    {"____00001*0*____________1001____", HANDLER_ID(mull_handler)}, // Multiply Long
    {"____00001*1*____________1001____", HANDLER_ID(/*mlal_handler*/ not_implemented_handler)}, // Multiply Long
    {"____00*0000*____________****____", HANDLER_ID(and_handler)}, // Data processing
    {"____00010*00____________0000____", HANDLER_ID(mrs_handler)}, // PSR Transfer
    {"____00*10*10____________****____", HANDLER_ID(msr_handler)}, // PSR Transfer
    {"____000**0**____________1**1____", HANDLER_ID(strh_reg_handler)}, // Halfword Data Transfer: register offset
    {"____000**1**____________1**1____", HANDLER_ID(strh_imm_handler)}, // Halfword Data Transfer: register offset
    {"____00*0001*____________****____", HANDLER_ID(eor_handler)}, // Data processing
    {"____00*0010*____________****____", HANDLER_ID(sub_handler)}, // Data processing
    {"____00*0011*____________****____", HANDLER_ID(rsb_handler)}, // Data processing
    {"____00*0100*____________****____", HANDLER_ID(add_handler)}, // Data processing
    {"____00*0101*____________****____", HANDLER_ID(adc_handler)}, // Data processing
    {"____00*0110*____________****____", HANDLER_ID(sbc_handler)}, // Data processing
    {"____00*0111*____________****____", HANDLER_ID(rsc_handler)}, // Data processing
    {"____00*10001____________****____", HANDLER_ID(tst_handler)}, // Data processing
    {"____00*10011____________****____", HANDLER_ID(teq_handler)}, // Data processing
    {"____00*10101____________****____", HANDLER_ID(cmp_handler)}, // Data processing
    {"____00*10111____________****____", HANDLER_ID(cmn_handler)}, // Data processing
    {"____00*1100*____________****____", HANDLER_ID(orr_handler)}, // Data processing
    {"____00*1101*____________****____", HANDLER_ID(mov_handler)}, // Data processing
    {"____00*1110*____________****____", HANDLER_ID(bic_handler)}, // Data processing
    {"____00*1111*____________****____", HANDLER_ID(mvn_handler)}, // Data processing
    {"____00010*00____________1001____", HANDLER_ID(not_implemented_handler)}, // Single Data Swap
    {"____000****1____________1**1____", HANDLER_ID(/*ldmh_handler*/ not_implemented_handler)}, // Halfword Data Transfer: register offset
    {"____01*****0____________****____", HANDLER_ID(str_handler)}, // Single Data Transfer
    {"____01*****1____________****____", HANDLER_ID(ldr_handler)}, // Single Data Transfer
    {"____100****0____________****____", HANDLER_ID(stm_handler)}, // Block Data Transfer
    {"____100****1____________****____", HANDLER_ID(ldm_handler)}, // Block Data Transfer
    {"____1010****____________****____", HANDLER_ID(b_handler)},   // Branch
    {"____1011****____________****____", HANDLER_ID(bl_handler)},  // Branch
    {"____1111****____________****____", HANDLER_ID(/*swi_handler*/ not_implemented_handler)}, // Software Interrupt
};
uint8_t get_arm_handler(uint32_t instr) {
    // TODO
    for (size_t i = 0; i < sizeof(arm_decoder_rules) / sizeof(*arm_decoder_rules); i++) {
        decoder_rule_t *rule = &arm_decoder_rules[i];

        bool match = true;
        for (size_t j = 0; j < sizeof(rule->match_string) && match; j++) {
            switch (rule->match_string[sizeof(rule->match_string) - j - 1]) {
            case '*':
                break;
            case '_':
                // jump to next relevant char
                if (j == 0)
                    j = 3;
                else if (j == 8)
                    j = 19;
                else
                    j = sizeof(rule->match_string);
                break;
            case '0':
                match = GET_BIT(instr, j) == 0;
                break;
            case '1':
                match = GET_BIT(instr, j) == 1;
                break;
            }
        }

        if (match)
            return rule->handler_id;
    }

    return HANDLER_ID(not_implemented_handler);
}

static void arm_handlers_init(void) {
    for (size_t i = 0; i < sizeof(arm_handlers) / sizeof(*arm_handlers); i++) {
        uint8_t hi = (i & 0xFF0) >> 4;
        uint8_t lo = (i & 0x00F);
        uint32_t instr = (hi << 20) | (lo << 4);
        arm_handlers[i] = get_arm_handler(instr);
    }
}

// thumb_decoder_rules definition order is important: they are matched in the order they are defined in the thumb_decoder_rules array
// TODO '_' == '*' for now, its to visually help me find relevant bits: '_' are alway ignored because they are not relevant in the instruction hash
decoder_rule_t thumb_decoder_rules[] = {
    // --> needs another way to distinguish them
    {"00000***________", HANDLER_ID(thumb_lsl_handler)},             // Move shifted register
    {"00001***________", HANDLER_ID(thumb_lsr_handler)},             // Move shifted register
    {"00010***________", HANDLER_ID(thumb_asr_handler)},             // Move shifted register
    {"00011*0*________", HANDLER_ID(thumb_add_handler)},             // Add/substract
    {"00011*1*________", HANDLER_ID(thumb_sub_handler)},             // Add/substract
    {"00100***________", HANDLER_ID(thumb_mov_handler)},             // Move/compare/add/substract immediate
    {"00111***________", HANDLER_ID(thumb_sub_imm_handler)},         // Move/compare/add/substract immediate
    {"01000110________", HANDLER_ID(thumb_mov_hi_reg_handler)},      // Hi register operations/branch exchange
    {"01000111________", HANDLER_ID(thumb_bx_handler)},              // Hi register operations/branch exchange
    {"010000**________", HANDLER_ID(thumb_alu_ops_handler)},         // ALU operations
    {"01001***________", HANDLER_ID(thumb_pc_relative_ldr_handler)}, // PC-relative load
    {"01010*0*________", HANDLER_ID(thumb_reg_str_handler)},         // Load/store with register offset
    {"10000***________", HANDLER_ID(thumb_strh_handler)},            // Load/store halfword
    // {"10001***________", HANDLER_ID(thumb_ldrh_handler)},            // Load/store halfword
    {"10010***________", HANDLER_ID(thumb_str_sp_handler)},          // SP-relative load/store
    // {"10011***________", HANDLER_ID(thumb_ldr_sp_handler)},          // SP-relative load/store
    {"1010****________", HANDLER_ID(thumb_add_addr_handler)},          // Load address
    {"10110000________", HANDLER_ID(thumb_add_sp_handler)},          // Add offset to stack pointer
    {"1011010*________", HANDLER_ID(thumb_push_handler)},            // Push/pop registers
    {"1011110*________", HANDLER_ID(thumb_pop_handler)},             // Push/pop registers
    {"11000***________", HANDLER_ID(thumb_stm_handler)},          // Multiple load/store
    // {"11001***________", HANDLER_ID(thumb_ldm_handler)},          // Multiple load/store
    {"1101****________", HANDLER_ID(thumb_b_cond_handler)},          // Conditonal branch
    {"11100***________", HANDLER_ID(thumb_b_handler)},               // Unconditional branch
    {"1111****________", HANDLER_ID(thumb_bl_handler)},              // Long branch with link
    // {"11011111________", HANDLER_ID(thumb_sw_int_handler)}, // Software interrupt
};
uint8_t get_thumb_handler(uint32_t instr) {
    // TODO

    for (size_t i = 0; i < sizeof(thumb_decoder_rules) / sizeof(*thumb_decoder_rules); i++) {
        decoder_rule_t *rule = &thumb_decoder_rules[i];

        bool match = true;
        for (size_t j = 0; j < sizeof(rule->match_string) / 2 && match; j++) {
            switch (rule->match_string[(sizeof(rule->match_string) / 2) - j - 1]) {
            case '*':
                break;
            case '_':
                // jump to next relevant char
                j = 7;
                break;
            case '0':
                match = GET_BIT(instr, j) == 0;
                break;
            case '1':
                match = GET_BIT(instr, j) == 1;
                break;
            }
        }

        if (match)
            return rule->handler_id;
    }

    return HANDLER_ID(not_implemented_handler);
}

static void thumb_handlers_init(void) {
    for (size_t i = 0; i < sizeof(thumb_handlers) / sizeof(*thumb_handlers); i++) {
        uint32_t instr = i << 8;
        thumb_handlers[i] = get_thumb_handler(instr);
    }
}

void gba_cpu_init(gba_t *gba) {
    gba->cpu = xcalloc(1, sizeof(*gba->cpu));

    CPSR_CHANGE_FLAG(gba->cpu, CPSR_I, 1);
    CPSR_CHANGE_FLAG(gba->cpu, CPSR_F, 1);
    CPSR_SET_MODE(gba->cpu, CPSR_MODE_SVC);

    // TODO what are reg values at reset?
    // gba->cpu->regs[REG_PC] = VECTOR_RESET;

    // TODO reg values after bios boot, remove to boot from bios directly
    CPSR_SET_MODE(gba->cpu, CPSR_MODE_SYS);
    bank_registers(gba->cpu, CPSR_MODE_SYS);
    gba->cpu->banked_regs_13_14[regs_mode_hashes[CPSR_MODE_SVC & 0x0F]][0] = 0x03007FE0;
    gba->cpu->banked_regs_13_14[regs_mode_hashes[CPSR_MODE_IRQ & 0x0F]][0] = 0x03007FA0;
    gba->cpu->regs[13] = 0x03007F00;
    // gba->cpu->regs[14] = 0x08000000;
    gba->cpu->regs[REG_PC] = 0x08000000;
    // gba->cpu->cpsr = 0x0000001F;
    // gba->cpu->spsr[regs_mode_hashes[CPSR_GET_MODE(gba->cpu) & 0x0F]] = 0x00000010;

    gba->cpu->pipeline[PIPELINE_FETCHING] = 0x00000000;
    gba->cpu->pipeline[PIPELINE_DECODING] = 0x00000000;

    // generate handlers only once when initializing the first cpu instance
    static bool handlers_generated = false;
    if (!handlers_generated) {
        printf("Generating ARM7TDMI cpu handlers\n");
        arm_handlers_init();
        thumb_handlers_init();
        handlers_generated = true;
    }
}

void gba_cpu_quit(gba_cpu_t *cpu) {
    free(cpu);
}
