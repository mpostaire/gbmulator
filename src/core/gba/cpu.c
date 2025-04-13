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
#define ASR(op, amount) (((int32_t) (op)) < 0 ? ~(~(op) >> (amount)) : (op) >> (amount))
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
        "IA",
        "DB",
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

static inline void compute_flags_nzcv(gba_cpu_t *cpu, uint32_t res, uint32_t op1, uint32_t op2) {
    compute_flags_nz(cpu, res);
    CPSR_CHANGE_FLAG(cpu, CPSR_C, res < op1);
    CPSR_CHANGE_FLAG(cpu, CPSR_V, (~(op1 ^ op2) & (op2 ^ res)) >> 31);
}

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

#define rlist_to_str(rlist) _rlist_to_str((rlist), (char[32]) {}, 32)
#endif

static bool not_implemented_handler(UNUSED gba_t *gba, uint32_t instr) {
    int instr_size = CPSR_CHECK_FLAG(gba->cpu, CPSR_T) ? 2 : 4;
    todo("not implemented instruction: 0x%0*X (0b%0.*b)", instr_size * 2, instr, instr_size * 8, instr);
    return true;
}

static bool data_processing_begin(gba_t *gba, uint32_t instr, uint8_t *rd, uint32_t *op1, uint32_t *op2) {
    bool i = CHECK_BIT(instr, 25);
    bool s = CHECK_BIT(instr, 20); // TODO
    uint8_t rn = (instr & 0x000F0000) >> 16;

    *rd = (instr & 0x0000F000) >> 12;
    *op1 = gba->cpu->regs[rn];
    *op2 = instr & 0x00000FFF;

    // LOG_DEBUG("%c%c rd=0x%02X op1=0x%08X op2=0x%08X\n", i ? 'i' : '-', s ? 's' : '-', *rd, *op1, *op2);

    // if (s)
    //     todo("data processing 's' bit");

    // TODO it seems that there is a special case if rd is register 15 (REG_PC)

    if (i) {
        uint8_t rotate = (*op2 & 0xF00) >> 8;
        uint32_t imm = (*op2 & 0x0FF);
        *op2 = ROR(imm, 2 * rotate);
    } else {
        uint8_t rm = *op2 & 0x00F;
        uint16_t shift = (*op2 & 0xFF0) >> 4;

        uint8_t amount = CHECK_BIT(shift, 0) ? gba->cpu->regs[(shift & 0xF0) >> 4] : (shift & 0xF8) >> 3;

        switch ((shift & 0b00000110) >> 1) {
        case 0b00: // shift left
            *op2 = LSL(gba->cpu->regs[rm], amount);
            break;
        case 0b01: // shift right
            *op2 = LSR(gba->cpu->regs[rm], amount);
            break;
        case 0b10: // arithmetic shift right
            *op2 = ASR(gba->cpu->regs[rm], amount);
            break;
        case 0b11: // rotate right
            *op2 = ROR(gba->cpu->regs[rm], amount);
            break;
        }
    }

    return s && rd != REG_PC;
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
    bool set_flags = data_processing_begin(gba, instr, &rd, &op1, &op2);

    LOG_DEBUG("(0x%08X) AND%s%s %s, %s, 0x%X\n", instr, cond_names[ARM_INSTR_GET_COND(instr)], s ? "S" : "", reg_names[rd], reg_names[rn], op2);

    gba->cpu->regs[rd] = op1 & op2;

    if (set_flags) {
        // compute_flags_nzcv(gba->cpu, ...);
        todo("set_flags");
    }

    return true;
}

static bool mrs_handler(gba_t *gba, uint32_t instr) {
    todo();
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

        todo("more register banking");
    }

    // backup old regs
    cpu->banked_regs_13_14[old_mode_reg_index][0] = cpu->regs[13];
    cpu->banked_regs_13_14[old_mode_reg_index][1] = cpu->regs[14];

    // use new regs
    cpu->regs[13] = cpu->banked_regs_13_14[new_mode_reg_index][0];
    cpu->regs[14] = cpu->banked_regs_13_14[new_mode_reg_index][1];

    // // TODO maybe use 2D array transision table instead of this (takes values from mode hash to check if a transition is allowed)
    // // --> maybe bad idea because it will be an array of size 256
    // switch (new_mode) {
    // case CPSR_MODE_USR:
    //     todo("mode changed to CPSR_MODE_USR\n");
    //     break;
    // case CPSR_MODE_FIQ:
    //     todo("mode changed to CPSR_MODE_FIQ\n");
    //     break;
    // case CPSR_MODE_IRQ:
    //     todo("mode changed to CPSR_MODE_IRQ\n");
    //     break;
    // case CPSR_MODE_SVC:
    //     //todo("mode changed to CPSR_MODE_SVC\n");
    //     break;
    // case CPSR_MODE_ABT:
    //     todo("mode changed to CPSR_MODE_ABT\n");
    //     break;
    // case CPSR_MODE_SYS:
    //     if (old_mode == CPSR_MODE_USR) {
    //         todo("attempt mode change to CPSR_MODE_SYS (maybe illegal?)");
    //         break;
    //     }

    //     // todo("mode changed to CPSR_MODE_SYS");
    //     break;
    // case CPSR_MODE_UND:
    //     todo("mode changed to CPSR_MODE_UND\n");
    //     break;
    // default:
    //     todo("undefined behaviour");
    //     break;
    // }
}

static bool msr_handler(gba_t *gba, uint32_t instr) {
    bool pd = CHECK_BIT(instr, 22);
    // TODO handle R15
    // TODO add undefined behaviour if CPSR_T bit is changed
    // TODO what happens when reserved bits are changed/user mode tries to change mode bits?

    if (CHECK_BIT(instr, 16)) { // transfer register contents to PSR
        uint8_t rm = instr & 0x0F;

        if (pd) {
            LOG_DEBUG("(0x%08X) MSR%s SPSR_fc, %s\n", instr, cond_names[ARM_INSTR_GET_COND(instr)], reg_names[rm]);
            // gba->cpu->spsr = CHANGE_BITS(gba->cpu->cpsr, 0xF0000000, gba->cpu->regs[rm]);
            if (CPSR_GET_MODE(gba->cpu) == CPSR_MODE_USR)
                todo("undefined behaviour");

            gba->cpu->spsr[regs_mode_hashes[CPSR_GET_MODE(gba->cpu) & 0x0F]] = gba->cpu->regs[rm];
        } else {
            LOG_DEBUG("(0x%08X) MSR%s CPSR_fc, %s\n", instr, cond_names[ARM_INSTR_GET_COND(instr)], reg_names[rm]);
            bank_registers(gba->cpu, gba->cpu->regs[rm] & CPSR_MODE_MASK);
            gba->cpu->cpsr = gba->cpu->regs[rm];
        }

    } else { // transfer register contents or immediate value to PSR flag bits only
        if (CHECK_BIT(instr, 25)) { // source operand is an immediate value
            LOG_DEBUG("(0x%08X) MSR%s <psrf>_flg, <expression>\n", instr, cond_names[ARM_INSTR_GET_COND(instr)]);
            todo();
            // gba->cpu->cpsr = CHANGE_BITS(gba->cpu->cpsr, 0xF0000000, gba->cpu->regs[rm]);
        } else { // source operand is a register
            uint8_t rm = instr & 0x0F;
            LOG_DEBUG("(0x%08X) MSR%s <psrf>_flg, %s\n", instr, cond_names[ARM_INSTR_GET_COND(instr)], reg_names[rm]);
            todo();
            // gba->cpu->cpsr = CHANGE_BITS(gba->cpu->cpsr, 0xF0000000, gba->cpu->regs[rm]);
        }
        todo();
    }

    return true;
}

static bool msr_bx_dispatcher(gba_t *gba, uint32_t instr) {
    if ((instr & 0x0FFFFFF0) == 0b00000001001011111111111100010000)
        return bx_handler(gba, instr);
    else
        return msr_handler(gba, instr);
}

static bool add_handler(gba_t *gba, uint32_t instr) {
    uint8_t rn = (instr & 0x000F0000) >> 16;
    bool s = CHECK_BIT(instr, 20);

    uint32_t op1;
    uint32_t op2;
    uint8_t rd;
    bool set_flags = data_processing_begin(gba, instr, &rd, &op1, &op2);

    LOG_DEBUG("(0x%08X) ADD%s%s %s, %s, 0x%X\n", instr, cond_names[ARM_INSTR_GET_COND(instr)], s ? "S" : "", reg_names[rd], reg_names[rn], op2);

    gba->cpu->regs[rd] = op1 + op2;

    if (set_flags) {
        // compute_flags_nzcv(gba->cpu, ...);
        todo("set_flags");
    }

    return true;
}

static bool teq_handler(gba_t *gba, uint32_t instr) {
    uint8_t rn = (instr & 0x000F0000) >> 16;
    bool s = CHECK_BIT(instr, 20);

    uint32_t op1;
    uint32_t op2;
    uint8_t rd;
    bool set_flags = data_processing_begin(gba, instr, &rd, &op1, &op2);

    LOG_DEBUG("(0x%08X) TEQ%s %s, #0x%X\n", instr, cond_names[ARM_INSTR_GET_COND(instr)], reg_names[rn], op2);

    if (set_flags)
        compute_flags_nzcv(gba->cpu, op1 ^ op2, op1, op2);

    return true;
}

static bool cmp_handler(gba_t *gba, uint32_t instr) {
    uint8_t rn = (instr & 0x000F0000) >> 16;

    uint32_t op1;
    uint32_t op2;
    uint8_t rd;
    bool set_flags = data_processing_begin(gba, instr, &rd, &op1, &op2);

    LOG_DEBUG("(0x%08X) CMP%s %s, #0x%X\n", instr, cond_names[ARM_INSTR_GET_COND(instr)], reg_names[rn], op2);

    if (set_flags)
        compute_flags_nzcv(gba->cpu, op1 - op2, op1, op2);

    return true;
}

static bool orr_handler(gba_t *gba, uint32_t instr) {
    uint8_t rn = (instr & 0x000F0000) >> 16;
    bool s = CHECK_BIT(instr, 20);

    uint32_t op1;
    uint32_t op2;
    uint8_t rd;
    bool set_flags = data_processing_begin(gba, instr, &rd, &op1, &op2);

    gba->cpu->regs[rd] = op1 | op2;

    LOG_DEBUG("(0x%08X) ORR%s %s, #0x%X\n", instr, cond_names[ARM_INSTR_GET_COND(instr)], reg_names[rn], op2);

    if (set_flags) {
        compute_flags_nz(gba->cpu, gba->cpu->regs[rd]);
        CPSR_CHANGE_FLAG(gba->cpu, CPSR_C, GET_BIT(gba->cpu->regs[rd], 31)); // TODO
    }

    return true;
}

static bool mov_handler(gba_t *gba, uint32_t instr) {
    bool s = CHECK_BIT(instr, 20);

    uint32_t op1;
    uint32_t op2;
    uint8_t rd;
    bool set_flags = data_processing_begin(gba, instr, &rd, &op1, &op2);

    LOG_DEBUG("(0x%08X) MOV%s%s %s, #0x%X\n", instr, cond_names[ARM_INSTR_GET_COND(instr)], s ? "S" : "", reg_names[rd], op2);

    gba->cpu->regs[rd] = op2;

    if (set_flags) {
        compute_flags_nz(gba->cpu, op1 | op2);
        CPSR_CHANGE_FLAG(gba->cpu, CPSR_C, GET_BIT(gba->cpu->regs[rd], 31)); // TODO
    }

    return true;
}

static bool strh_handler(gba_t *gba, uint32_t instr) {
    bool p = CHECK_BIT(instr, 24);
    bool u = CHECK_BIT(instr, 23);
    bool w = CHECK_BIT(instr, 21);
    bool l = CHECK_BIT(instr, 20);
    bool s = CHECK_BIT(instr, 6);
    bool h = CHECK_BIT(instr, 5);

    uint8_t rn = (instr >> 16) & 0x07;
    uint8_t rd = (instr >> 12) & 0x07;
    uint32_t offset = ((instr & 0x0F00) >> 4) | (instr & 0x0F);

    uint32_t base_addr = gba->cpu->regs[rn];

    if (!u)
        offset = -offset;

    if (p)
        base_addr += offset;

    if (w)
        todo("w");
    if (l)
        todo("l");

    switch ((s << 1) | h) {
    case 0b00:
        todo("0b00");
        break;
    case 0b01:
        gba_bus_write_half(gba, base_addr, gba->cpu->regs[rd]);
        break;
    case 0b10:
        todo("0b10");
        break;
    case 0b11:
        todo("0b11");
        break;
    default:
        todo();
    }

    if (!p)
        base_addr += offset;

    LOG_DEBUG("(0x%08X) STR%s%s%s %s, <addr>\n", instr, cond_names[ARM_INSTR_GET_COND(instr)], s ? "S" : "", h ? "H" : "", reg_names[rd]);

    return true;
}

// TODO ldm and str handlers are VERY similar --> break into subfunctions
static bool str_handler(gba_t *gba, uint32_t instr) {
    bool b = CHECK_BIT(instr, 22);
    bool w = CHECK_BIT(instr, 21);
    uint8_t rn = (instr >> 16) & 0x0F;

    uint32_t addr = gba->cpu->regs[rn];
    uint32_t offset;

    // TODO split this into 2 different handler functions to avoid branching (also do this to all other branches in any instruction)

    if (w)
        todo();

    // TODO generic offset compute inline function (because lots of instructions uses this BUT check if this can be done because they may differ a bit)
    bool i = CHECK_BIT(instr, 25);
    if (i) {
        uint8_t rm = instr & 0x0F;
        uint8_t shift = (instr >> 4) & 0xFF;
        todo();
    } else {
        offset = instr & 0x0FFF;
    }

    bool u = GET_BIT(instr, 23);
    offset *= -1 * -u;

    bool p = CHECK_BIT(instr, 24);
    if (p)
        addr += offset;
    else
        todo();

    uint8_t rd = (instr >> 12) & 0x0F;

    LOG_DEBUG("(0x%08X) STR%s%s%s %s, [%s, #0x%X]\n", instr, cond_names[ARM_INSTR_GET_COND(instr)], b ? "B" : "", w ? "T" : "", reg_names[rd], reg_names[rn], offset);

    if (b)
        gba_bus_write_byte(gba, addr, rd);
    else
        gba_bus_write_word(gba, addr, rd);

    return true;
}

static bool ldr_handler(gba_t *gba, uint32_t instr) {
    bool b = CHECK_BIT(instr, 22);
    bool w = CHECK_BIT(instr, 21);
    uint8_t rn = (instr >> 16) & 0x0F;

    uint32_t addr = gba->cpu->regs[rn];
    uint32_t offset;

    // TODO split this into 2 different handler functions to avoid branching (also do this to all other branches in any instruction)

    if (w)
        todo();

    bool i = CHECK_BIT(instr, 25);
    if (i) {
        uint8_t rm = instr & 0x0F;
        uint8_t shift = (instr >> 4) & 0xFF;
        todo();
    } else {
        offset = instr & 0x0FFF;
    }

    bool u = GET_BIT(instr, 23);
    offset *= -1 * -u;

    bool p = CHECK_BIT(instr, 24);
    if (p)
        addr += offset;
    else
        todo();

    uint32_t res;
    if (b)
        res = gba_bus_read_byte(gba, addr);
    else
        res = gba_bus_read_word(gba, addr);

    uint8_t rd = (instr >> 12) & 0x0F;

    LOG_DEBUG("(0x%08X) LDR%s%s%s %s, [%s, #0x%X]\n", instr, cond_names[ARM_INSTR_GET_COND(instr)], b ? "B" : "", w ? "T" : "", reg_names[rd], reg_names[rn], offset);

    gba->cpu->regs[rd] = res;

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
    uint32_t offset = instr & 0x00FFFFFF; // TODO sign extend offset to 32 bits

    // sign extend from 28 to 32 bits
    struct {signed int x:28;} s;
    // TODO prefetch operation
    int32_t offset_extended = s.x = (offset << 2);

    // TODO 2S + 1N cycles --> does this take into account the pipeline flush (1 cycle to execute, 2 cycles to fill pipeline until next instruction execution)?
    //                       ------> NO BUT IT COINCIDES BECAUSE IT THE SAME TIME

    gba->cpu->regs[REG_PC] += offset_extended;

    gba->cpu->pipeline[PIPELINE_FETCHING] = 0x00000000;
    gba->cpu->pipeline[PIPELINE_DECODING] = 0x00000000;

    LOG_DEBUG("(0x%08X) B%s #Lxx_0x%08X\n", instr, cond_names[ARM_INSTR_GET_COND(instr)], gba->cpu->regs[REG_PC]);

    return false;
}

static bool bl_handler(gba_t *gba, uint32_t instr) {
    uint32_t offset = instr & 0x00FFFFFF; // TODO sign extend offset to 32 bits

    // sign extend from 28 to 32 bits
    struct {signed int x:28;} s;
    // TODO prefetch operation
    int32_t offset_extended = s.x = (offset << 2);

    // TODO 2S + 1N cycles --> does this take into account the pipeline flush (1 cycle to execute, 2 cycles to fill pipeline until next instruction execution)?
    //                       ------> NO BUT IT COINCIDES BECAUSE IT THE SAME TIME

    gba->cpu->regs[REG_LR] = gba->cpu->regs[REG_PC] - 4; // store addr of next instr before jumping
    gba->cpu->regs[REG_PC] += offset_extended;

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
        return CPSR_CHECK_FLAG(cpu, CPSR_N) == CPSR_CHECK_FLAG(cpu, CPSR_V);
    case COND_LT:
        return CPSR_CHECK_FLAG(cpu, CPSR_N) != CPSR_CHECK_FLAG(cpu, CPSR_V);
    case COND_GT:
        return !CPSR_CHECK_FLAG(cpu, CPSR_Z) && (CPSR_CHECK_FLAG(cpu, CPSR_N) == CPSR_CHECK_FLAG(cpu, CPSR_V));
    case COND_LE:
        return CPSR_CHECK_FLAG(cpu, CPSR_Z) || (CPSR_CHECK_FLAG(cpu, CPSR_N) != CPSR_CHECK_FLAG(cpu, CPSR_V));
    case COND_AL:
        return true;
    default:
        eprintf("invalid condition code: 0x%02X", cond);
        return false;
    }
}

void gba_cpu_step(gba_t *gba) {
    gba_cpu_t *cpu = gba->cpu;

    char N = CPSR_CHECK_FLAG(cpu, CPSR_N) ? 'N' : '-';
    char Z = CPSR_CHECK_FLAG(cpu, CPSR_Z) ? 'Z' : '-';
    char C = CPSR_CHECK_FLAG(cpu, CPSR_C) ? 'C' : '-';
    char V = CPSR_CHECK_FLAG(cpu, CPSR_V) ? 'V' : '-';

    char I = CPSR_CHECK_FLAG(cpu, CPSR_I) ? 'I' : '-';
    char F = CPSR_CHECK_FLAG(cpu, CPSR_F) ? 'F' : '-';
    char T = CPSR_CHECK_FLAG(cpu, CPSR_T) ? 'T' : '-';

    LOG_DEBUG("--------\n[PC=0x%08X] [COND=%c%c%c%c %c%c%c]\n", cpu->regs[REG_PC], N, Z, C, V, I, F, T);

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
    LOG_DEBUG("execute: 0x%0.*X\n", 1 << pc_increment_shift, instr);
    for (int i = 0; i < sizeof(cpu->regs) / sizeof(*cpu->regs); i++)
        LOG_DEBUG("\t%s=0x%08X\n", reg_names[i], gba->cpu->regs[i]);

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

    uint32_t res = op1 + op2;

    compute_flags_nzcv(gba->cpu, res, op1, op2);

    gba->cpu->regs[rd] = res;

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

    uint32_t res = op1 + op2;

    compute_flags_nzcv(gba->cpu, res, op1, op2);

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

    compute_flags_nzcv(gba->cpu, res, gba->cpu->regs[rd], offset8);

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
        todo();
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

static bool thumb_add_sp_handler(gba_t *gba, uint32_t instr) {
    bool s = GET_BIT(instr, 7);
    uint16_t offset = ((instr & 0x007F) << 2) * s;

    LOG_DEBUG("(0x%04X) ADD %s #%s%d\n", instr, reg_names[REG_SP], s ? "-" : "", offset);

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
    X(mrs_handler)                   \
    X(msr_bx_dispatcher)             \
    X(add_handler)                   \
    X(teq_handler)                   \
    X(cmp_handler)                   \
    X(orr_handler)                   \
    X(mov_handler)                   \
    X(strh_handler)                  \
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
    {"____00*0000*________________****", HANDLER_ID(and_handler)},       // Data processing
    {"____00010*00________________0000", HANDLER_ID(mrs_handler)},       // PSR Transfer
    {"____00*10*10________________0000", HANDLER_ID(msr_bx_dispatcher)}, // Branch and exchange / PSR Transfer
    // {"____00*0001*________________****", HANDLER_ID(eor_handler)}, // Data processing
    // {"____00*0010*________________****", HANDLER_ID(sub_handler)}, // Data processing
    // {"____00*0011*________________****", HANDLER_ID(rsb_handler)}, // Data processing
    {"____00*0100*________________****", HANDLER_ID(add_handler)}, // Data processing
    // {"____00*0101*________________****", HANDLER_ID(adc_handler)}, // Data processing
    // {"____00*0110*________________****", HANDLER_ID(sbc_handler)}, // Data processing
    // {"____00*0111*________________****", HANDLER_ID(rsc_handler)}, // Data processing
    // {"____00*10001________________****", HANDLER_ID(tst_handler)}, // Data processing
    {"____00*10011________________****", HANDLER_ID(teq_handler)}, // Data processing
    {"____00*10101________________****", HANDLER_ID(cmp_handler)}, // Data processing
    // {"____00*10111________________****", HANDLER_ID(cmn_handler)}, // Data processing
    {"____00*1100*________________****", HANDLER_ID(orr_handler)}, // Data processing
    {"____00*1101*________________****", HANDLER_ID(mov_handler)}, // Data processing
    // {"____00*1110*________________****", HANDLER_ID(bic_handler)}, // Data processing
    // {"____00*1111*________________****", HANDLER_ID(mvn_handler)}, // Data processing
    {"____000****0________________****", HANDLER_ID(strh_handler)}, // Halfword Data Transfer: register offset
    // {"____000****1________________****", HANDLER_ID(ldmh_handler)}, // Halfword Data Transfer: register offset
    {"____01*****0________________****", HANDLER_ID(str_handler)}, // Single Data Transfer
    {"____01*****1________________****", HANDLER_ID(ldr_handler)}, // Single Data Transfer
    {"____100****0________________****", HANDLER_ID(stm_handler)}, // Block Data Transfer
    {"____100****1________________****", HANDLER_ID(ldm_handler)}, // Block Data Transfer
    {"____1010****________________****", HANDLER_ID(b_handler)},   // Branch
    {"____1011****________________****", HANDLER_ID(bl_handler)},  // Branch
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
                if (j == 4)
                    j = 19;
                else if (j == 28)
                    j = 31;
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
    // TODO bx handler and msr handler are confondus
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
