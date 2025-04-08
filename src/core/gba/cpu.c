#include <stdlib.h>

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

// TODO this only works in ARM mode
#define INSTR_GET_COND(instr) ((instr) >> 28)

// shift left
#define LSL(op, amount) ((op) << (amount))
// shift right
#define LSR(op, amount) ((op) >> (amount))
// arithmetic shift right (https://stackoverflow.com/a/53766752)
#define ASR(op, amount) (((int32_t) (op)) < 0 ? ~(~(op) >> (amount)) : (op) >> (amount))
// rotate right
#define ROR(op, amount) (((op) >> (amount)) | ((op) << (32 - (amount))))

#define FOREACH_COND(X) \
    X(EQ)          \
    X(NE)          \
    X(CS)          \
    X(CC)          \
    X(MI)          \
    X(PL)          \
    X(VS)          \
    X(VC)          \
    X(HI)          \
    X(LS)          \
    X(GE)          \
    X(LT)          \
    X(GT)          \
    X(LE)          \
    Y(AL)
#define COND_GENERATOR(name) COND_##name,
#define COND_NAME_GENERATOR(name) STRINGIFY(name),

#define Y COND_GENERATOR
typedef enum {
    FOREACH_COND(COND_GENERATOR)
} cond_t;
#undef Y
#define Y(name) ""
static const char *cond_names[] = {
    FOREACH_COND(COND_NAME_GENERATOR)
};
#undef Y

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
    "R13",
    "R14",
    "PC"
};

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

typedef void (*handler_t)(gba_t *gba, uint32_t instr);
handler_t handlers[];

// using double lookup tables to reduce impact on host CPU cache
uint8_t arm_handlers[1 << 12];
uint8_t thumb_handlers[1 << 8]; // TODO not sure of this size

static void not_implemented_handler(UNUSED gba_t *gba, uint32_t instr) {
    int instr_size = CPSR_CHECK_FLAG(gba->cpu, CPSR_T) ? 2 : 4;
    todo("not implemented instruction: 0x%0*X (0b%0.*b)", instr_size * 2, instr, instr_size * 8, instr);
}

static void data_processing_begin(gba_t *gba, uint32_t instr, uint8_t *rd, uint32_t *op1, uint32_t *op2) {
    uint32_t i = CHECK_BIT(instr, 25);
    uint32_t s = CHECK_BIT(instr, 20); // TODO
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
}

static void bx_handler(gba_t *gba, uint32_t instr) {
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

    LOG_DEBUG("(0x%08X) BX%s %s (0x%08X)\n", instr, cond_names[INSTR_GET_COND(instr)], reg_names[rn], pc_dest);

    // TODO is this sure?
    gba->cpu->regs[REG_PC] = pc_dest - 4; // -4 here to counterbalance the REG_PC += 4 at the end of cpu_step()

    // TODO flush pipeline? --> should be filled with 0: this is an andeq r0, r0, r0 and is equivalent to a nop
    gba->cpu->pipeline[PIPELINE_FETCHING] = 0x00000000;
    gba->cpu->pipeline[PIPELINE_DECODING] = 0x00000000;
}

static void and_handler(gba_t *gba, uint32_t instr) {
    uint8_t rn = (instr & 0x000F0000) >> 16;
    uint32_t s = CHECK_BIT(instr, 20);

    uint32_t op1;
    uint32_t op2;
    uint8_t rd;
    data_processing_begin(gba, instr, &rd, &op1, &op2);

    LOG_DEBUG("(0x%08X) AND%s%s %s, %s, 0x%X\n", instr, cond_names[INSTR_GET_COND(instr)], s ? "S" : "", reg_names[rd], reg_names[rn], op2);

    gba->cpu->regs[rd] = op1 & op2;

    if (s) {
        todo("s");

        CPSR_CHANGE_FLAG(gba->cpu, CPSR_N, gba->cpu->regs[rd] >> 31);
        CPSR_CHANGE_FLAG(gba->cpu, CPSR_Z, gba->cpu->regs[rd] == 0);
        // CPSR_CHANGE_FLAG(gba->cpu, CPSR_C, res < op1);
        // CPSR_CHANGE_FLAG(gba->cpu, CPSR_V, (~(op1 ^ op2) & (op2 ^ res)) >> 31);
    }
}

static void mrs_handler(gba_t *gba, uint32_t instr) {
    todo();
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

static void msr_handler(gba_t *gba, uint32_t instr) {
    uint32_t pd = CHECK_BIT(instr, 22);
    // TODO handle R15
    // TODO add undefined behaviour if CPSR_T bit is changed
    // TODO what happens when reserved bits are changed/user mode tries to change mode bits?

    if (CHECK_BIT(instr, 16)) { // transfer register contents to PSR
        uint8_t rm = instr & 0x0F;

        if (pd) {
            LOG_DEBUG("(0x%08X) MSR%s SPSR_fc, %s\n", instr, cond_names[INSTR_GET_COND(instr)], reg_names[rm]);
            // gba->cpu->spsr = CHANGE_BITS(gba->cpu->cpsr, 0xF0000000, gba->cpu->regs[rm]);
            if (CPSR_GET_MODE(gba->cpu) == CPSR_MODE_USR)
                todo("undefined behaviour");

            gba->cpu->spsr[regs_mode_hashes[CPSR_GET_MODE(gba->cpu) & 0x0F]] = gba->cpu->regs[rm];
        } else {
            LOG_DEBUG("(0x%08X) MSR%s CPSR_fc, %s\n", instr, cond_names[INSTR_GET_COND(instr)], reg_names[rm]);
            bank_registers(gba->cpu, gba->cpu->regs[rm] & CPSR_MODE_MASK);
            gba->cpu->cpsr = gba->cpu->regs[rm];
        }

    } else { // transfer register contents or immediate value to PSR flag bits only
        if (CHECK_BIT(instr, 25)) { // source operand is an immediate value
            LOG_DEBUG("(0x%08X) MSR%s <psrf>_flg, <expression>\n", instr, cond_names[INSTR_GET_COND(instr)]);
            todo();
            // gba->cpu->cpsr = CHANGE_BITS(gba->cpu->cpsr, 0xF0000000, gba->cpu->regs[rm]);
        } else { // source operand is a register
            uint8_t rm = instr & 0x0F;
            LOG_DEBUG("(0x%08X) MSR%s <psrf>_flg, %s\n", instr, cond_names[INSTR_GET_COND(instr)], reg_names[rm]);
            todo();
            // gba->cpu->cpsr = CHANGE_BITS(gba->cpu->cpsr, 0xF0000000, gba->cpu->regs[rm]);
        }
        todo();
    }
}

static void msr_bx_dispatcher(gba_t *gba, uint32_t instr) {
    if ((instr & 0x0FFFFFF0) == 0b00000001001011111111111100010000)
        bx_handler(gba, instr);
    else
        msr_handler(gba, instr);
}

static void add_handler(gba_t *gba, uint32_t instr) {
    uint8_t rn = (instr & 0x000F0000) >> 16;
    uint32_t s = CHECK_BIT(instr, 20);

    uint32_t op1;
    uint32_t op2;
    uint8_t rd;
    data_processing_begin(gba, instr, &rd, &op1, &op2);

    // TODO don't print R15 but PC, dont print R14 but LR
    LOG_DEBUG("(0x%08X) ADD%s%s %s, %s, 0x%X\n", instr, cond_names[INSTR_GET_COND(instr)], s ? "S" : "", reg_names[rd], reg_names[rn], op2);

    gba->cpu->regs[rd] = op1 + op2;

    if (s) {
        todo("s");

        CPSR_CHANGE_FLAG(gba->cpu, CPSR_N, gba->cpu->regs[rd] >> 31);
        CPSR_CHANGE_FLAG(gba->cpu, CPSR_Z, gba->cpu->regs[rd] == 0);
        // CPSR_CHANGE_FLAG(gba->cpu, CPSR_C, res < op1);
        // CPSR_CHANGE_FLAG(gba->cpu, CPSR_V, (~(op1 ^ op2) & (op2 ^ res)) >> 31);
    }
}

static void teq_handler(gba_t *gba, uint32_t instr) {
    uint8_t rn = (instr & 0x000F0000) >> 16;
    uint32_t s = CHECK_BIT(instr, 20);

    uint32_t op1;
    uint32_t op2;
    uint8_t rd;
    data_processing_begin(gba, instr, &rd, &op1, &op2);

    LOG_DEBUG("(0x%08X) TEQ%s %s, #0x%X\n", instr, cond_names[INSTR_GET_COND(instr)], reg_names[rn], op2);

    uint32_t res = op1 ^ op2;

    CPSR_CHANGE_FLAG(gba->cpu, CPSR_N, res >> 31);
    CPSR_CHANGE_FLAG(gba->cpu, CPSR_Z, res == 0);
    CPSR_CHANGE_FLAG(gba->cpu, CPSR_C, res < op1);
    CPSR_CHANGE_FLAG(gba->cpu, CPSR_V, (~(op1 ^ op2) & (op2 ^ res)) >> 31);
}

static void cmp_handler(gba_t *gba, uint32_t instr) {
    uint8_t rn = (instr & 0x000F0000) >> 16;

    uint32_t op1;
    uint32_t op2;
    uint8_t rd;
    data_processing_begin(gba, instr, &rd, &op1, &op2);

    LOG_DEBUG("(0x%08X) CMP%s %s, #0x%X\n", instr, cond_names[INSTR_GET_COND(instr)], reg_names[rn], op2);

    uint32_t res = op1 - op2;

    CPSR_CHANGE_FLAG(gba->cpu, CPSR_N, res >> 31);
    CPSR_CHANGE_FLAG(gba->cpu, CPSR_Z, res == 0);
    CPSR_CHANGE_FLAG(gba->cpu, CPSR_C, res < op1);
    CPSR_CHANGE_FLAG(gba->cpu, CPSR_V, (~(op1 ^ op2) & (op2 ^ res)) >> 31);
}

static void mov_handler(gba_t *gba, uint32_t instr) {
    uint32_t s = CHECK_BIT(instr, 20);

    uint32_t op1;
    uint32_t op2;
    uint8_t rd;
    data_processing_begin(gba, instr, &rd, &op1, &op2);

    LOG_DEBUG("(0x%08X) MOV%s%s %s, #0x%X\n", instr, cond_names[INSTR_GET_COND(instr)], s ? "S" : "", reg_names[rd], op2);

    gba->cpu->regs[rd] = op2;

    if (s) {
        todo("s");

        CPSR_CHANGE_FLAG(gba->cpu, CPSR_N, gba->cpu->regs[rd] >> 31);
        CPSR_CHANGE_FLAG(gba->cpu, CPSR_Z, gba->cpu->regs[rd] == 0);
        // CPSR_CHANGE_FLAG(gba->cpu, CPSR_C, res < op1);
        // CPSR_CHANGE_FLAG(gba->cpu, CPSR_V, (~(op1 ^ op2) & (op2 ^ res)) >> 31);
    }
}

// TODO ldr and str handlers are VERY similar --> break into subfunctions
static void str_handler(gba_t *gba, uint32_t instr) {
    uint32_t b = CHECK_BIT(instr, 22);
    uint32_t w = CHECK_BIT(instr, 21);
    uint8_t rn = (instr >> 16) & 0x0F;

    uint32_t addr = gba->cpu->regs[rn];
    uint32_t offset;

    // TODO split this into 2 different handler functions to avoid branching (also do this to all other branches in any instruction)

    if (w)
        todo();

    // TODO generic offset compute inline function (because lots of instructions uses this BUT check if this can be done because they may differ a bit)
    uint32_t i = CHECK_BIT(instr, 25);
    if (i) {
        uint8_t rm = instr & 0x0F;
        uint8_t shift = (instr >> 4) & 0xFF;
        todo();
    } else {
        offset = instr & 0x0FFF;
    }

    bool u = GET_BIT(instr, 23);
    offset *= -1 * -u;

    uint32_t p = CHECK_BIT(instr, 24);
    if (p)
        addr += offset;
    else
        todo();

    uint8_t rd = (instr >> 12) & 0x0F;

    LOG_DEBUG("(0x%08X) STR%s%s%s %s, [%s, #0x%X]\n", instr, cond_names[INSTR_GET_COND(instr)], b ? "B" : "", w ? "T" : "", reg_names[rd], reg_names[rn], offset);

    if (b)
        gba_bus_write_byte(gba, gba->cpu->regs[rd], addr);
    else
        gba_bus_write_word(gba, gba->cpu->regs[rd], addr);
}

static void ldr_handler(gba_t *gba, uint32_t instr) {
    uint32_t b = CHECK_BIT(instr, 22);
    uint32_t w = CHECK_BIT(instr, 21);
    uint8_t rn = (instr >> 16) & 0x0F;

    uint32_t addr = gba->cpu->regs[rn];
    uint32_t offset;

    // TODO split this into 2 different handler functions to avoid branching (also do this to all other branches in any instruction)

    if (w)
        todo();

    uint32_t i = CHECK_BIT(instr, 25);
    if (i) {
        uint8_t rm = instr & 0x0F;
        uint8_t shift = (instr >> 4) & 0xFF;
        todo();
    } else {
        offset = instr & 0x0FFF;
    }

    bool u = GET_BIT(instr, 23);
    offset *= -1 * -u;

    uint32_t p = CHECK_BIT(instr, 24);
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

    LOG_DEBUG("(0x%08X) LDR%s%s%s %s, [%s, #0x%X]\n", instr, cond_names[INSTR_GET_COND(instr)], b ? "B" : "", w ? "T" : "", reg_names[rd], reg_names[rn], offset);

    gba->cpu->regs[rd] = res;
}

static void b_handler(gba_t *gba, uint32_t instr) {
    uint32_t offset = instr & 0x00FFFFFF; // TODO sign extend offset to 32 bits

    // sign extend from 28 to 32 bits
    struct {signed int x:28;} s;
    // TODO prefetch operation
    int32_t offset_extended = s.x = (offset << 2);

    // TODO 2S + 1N cycles --> does this take into account the pipeline flush (1 cycle to execute, 2 cycles to fill pipeline until next instruction execution)?
    //                       ------> NO BUT IT COINCIDES BECAUSE IT THE SAME TIME

    gba->cpu->regs[REG_PC] += offset_extended - 4; // -4 here to counterbalance the REG_PC += 4 at the end of cpu_step()
    // TODO flush pipeline? --> should be filled with 0: this is an andeq r0, r0, r0 and is equivalent to a nop
    gba->cpu->pipeline[PIPELINE_FETCHING] = 0x00000000;
    gba->cpu->pipeline[PIPELINE_DECODING] = 0x00000000;

    LOG_DEBUG("(0x%08X) B%s -- 0x%08X --> PC=0x%08X\n", instr, cond_names[INSTR_GET_COND(instr)], offset, gba->cpu->regs[REG_PC] + 4);
}

static void bl_handler(gba_t *gba, uint32_t instr) {
    uint32_t offset = instr & 0x00FFFFFF; // TODO sign extend offset to 32 bits

    // sign extend from 28 to 32 bits
    struct {signed int x:28;} s;
    // TODO prefetch operation
    int32_t offset_extended = s.x = (offset << 2);

    // TODO 2S + 1N cycles --> does this take into account the pipeline flush (1 cycle to execute, 2 cycles to fill pipeline until next instruction execution)?
    //                       ------> NO BUT IT COINCIDES BECAUSE IT THE SAME TIME

    gba->cpu->regs[REG_LR] = gba->cpu->regs[REG_PC] - 4;

    gba->cpu->regs[REG_PC] += offset_extended - 4; // -4 here to counterbalance the REG_PC += 4 at the end of cpu_step()
    // TODO flush pipeline? --> should be filled with 0: this is an andeq r0, r0, r0 and is equivalent to a nop
    gba->cpu->pipeline[PIPELINE_FETCHING] = 0x00000000;
    gba->cpu->pipeline[PIPELINE_DECODING] = 0x00000000;

    LOG_DEBUG("(0x%08X) BL%s -- 0x%08X --> PC=0x%08X\n", instr, cond_names[INSTR_GET_COND(instr)], offset, gba->cpu->regs[REG_PC] + 4);
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

    int pc_increment;
    uint32_t fetched_instr;
    if (CPSR_CHECK_FLAG(gba->cpu, CPSR_T)) {
        pc_increment = 2;
        fetched_instr = gba_bus_read_half(gba, cpu->regs[REG_PC]);
    } else {
        pc_increment = 4;
        fetched_instr = gba_bus_read_word(gba, cpu->regs[REG_PC]);
    }

    uint32_t instr = cpu->pipeline[PIPELINE_DECODING];

    // fetch
    cpu->pipeline[PIPELINE_DECODING] = cpu->pipeline[PIPELINE_FETCHING];
    cpu->pipeline[PIPELINE_FETCHING] = fetched_instr;
    // TODO bus_read can stall CPU (nop instruction inserted) while reading memory (depends on waitstates)
    //      while this stalls, the decode and execute stages continue their operation
    LOG_DEBUG("fetch:   0x%0.*X\n", pc_increment * 2, cpu->pipeline[PIPELINE_FETCHING]);

    // decode
    LOG_DEBUG("decode:  0x%0.*X\n", pc_increment * 2, cpu->pipeline[PIPELINE_DECODING]);

    // execute
    // TODO
    // if (cpu->stall) {
        // cpu->stall--;
        // LOG_DEBUG("CPU stalled, remaining: %d\n", cpu->stall);
        // return;
    // }
    LOG_DEBUG("execute: 0x%0.*X\n", pc_increment * 2, instr);
    LOG_DEBUG("LR=0x%08X\n", gba->cpu->regs[REG_LR]);

    if (CPSR_CHECK_FLAG(gba->cpu, CPSR_T)) {
        uint_fast8_t instr_hash = instr >> 8;
        handlers[thumb_handlers[instr_hash]](gba, instr);
    } else if (verif_cond(gba->cpu, INSTR_GET_COND(instr))) {
        uint_fast16_t instr_hash = ((instr & 0x0FF00000) >> 16) | ((instr & 0x000000F0) >> 4);
        handlers[arm_handlers[instr_hash]](gba, instr);
    }

    cpu->regs[REG_PC] += pc_increment;
}

static void thumb_lsl_handler(gba_t *gba, uint32_t instr) {
    // ignore upper 16 bits of instr

    uint8_t offset5 = (instr & 0x7C0) >> 5;
    uint8_t rs = (instr & 0x38) >> 2;
    uint8_t rd = instr & 0x7;

    uint32_t res = gba->cpu->regs[rs] << offset5;

    CPSR_CHANGE_FLAG(gba->cpu, CPSR_N, res >> 31);
    CPSR_CHANGE_FLAG(gba->cpu, CPSR_Z, res == 0);
    if (offset5 != 0)
        CPSR_CHANGE_FLAG(gba->cpu, CPSR_C, GET_BIT(gba->cpu->regs[rd], 31));

    gba->cpu->regs[rd] = res;

    LOG_DEBUG("(0x%04X) LSL %s, %s, #0x%01X\n", instr, reg_names[rd], reg_names[rs], offset5);
}

static void thumb_lsr_handler(gba_t *gba, uint32_t instr) {
    // ignore upper 16 bits of instr
    todo();
}

static void thumb_asr_handler(gba_t *gba, uint32_t instr) {
    // ignore upper 16 bits of instr
    todo();
}

static void thumb_add_handler(gba_t *gba, uint32_t instr) {
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

    CPSR_CHANGE_FLAG(gba->cpu, CPSR_N, res >> 31);
    CPSR_CHANGE_FLAG(gba->cpu, CPSR_Z, res == 0);
    CPSR_CHANGE_FLAG(gba->cpu, CPSR_C, res < op1);
    CPSR_CHANGE_FLAG(gba->cpu, CPSR_V, (~(op1 ^ op2) & (op2 ^ res)) >> 31);

    gba->cpu->regs[rd] = res;

    LOG_DEBUG("0x%08X + 0x%08X = 0x%08X --> %s = 0x%08X\n", op1, op2, res, reg_names[rd], gba->cpu->regs[rd]);

}

static void thumb_mov_handler(gba_t *gba, uint32_t instr) {
    // ignore upper 16 bits of instr
    uint8_t offset8 = instr & 0xFF;
    uint8_t rd = (instr >> 8) & 0x07;

    gba->cpu->regs[rd] = offset8;

    // TODO
    CPSR_CHANGE_FLAG(gba->cpu, CPSR_N, gba->cpu->regs[rd] >> 31);
    CPSR_CHANGE_FLAG(gba->cpu, CPSR_Z, gba->cpu->regs[rd] == 0);
    // CPSR_CHANGE_FLAG(gba->cpu, CPSR_V, (((op1 ^ op2) & (op1 ^ res)) >> 31));

    LOG_DEBUG("(0x%04X) MOV %s, #0x%01X\n", instr, reg_names[rd], offset8);
}

static void thumb_bx_handler(gba_t *gba, uint32_t instr) {
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

    if (!CPSR_CHECK_FLAG(gba->cpu, CPSR_T))
        pc_dest &= 0xFFFFFFFC; // TODO is this sure? no offset?

    LOG_DEBUG("(0x%04X) BX %s (0x%08X)\n", instr, reg_names[rn], pc_dest);

// todo("lr register should contain 0x000000A0 but got --> 0x%08X", pc_dest);

    gba->cpu->regs[REG_PC] = pc_dest;

    // TODO flush pipeline? --> should be filled with 0: this is an andeq r0, r0, r0 and is equivalent to a nop
    gba->cpu->pipeline[PIPELINE_FETCHING] = 0x00000000;
    gba->cpu->pipeline[PIPELINE_DECODING] = 0x00000000;
}

static void thumb_pc_relative_ldr_handler(gba_t *gba, uint32_t instr) {
    uint32_t word8 = (instr & 0xFF) << 2;
    uint8_t rd = (instr & 0x700) >> 8;

    gba->cpu->regs[rd] = gba_bus_read_word(gba, gba->cpu->regs[REG_PC] - 2 + word8);

    LOG_DEBUG("(0x%04X) LDR %s, [PC, #0x%08X]\n", instr, reg_names[rd], word8);

    LOG_DEBUG("0x%08X 0x%08X\n", gba->cpu->regs[rd], gba->cpu->regs[REG_PC] - 2 + word8);
}

static void thumb_reg_str_handler(gba_t *gba, uint32_t instr) {
    uint8_t rd = instr & 0x07;
    uint8_t rb = (instr >> 3) & 0x07;
    uint8_t ro = (instr >> 6) & 0x07;

    uint32_t b = CHECK_BIT(instr, 10);

    LOG_DEBUG("(0x%04X) STR%s %s, [%s, %s]\n", instr, b ? "B" : "", reg_names[rd], reg_names[rb], reg_names[ro]);

    LOG_DEBUG("%d %d %X\n", gba->cpu->regs[rb], gba->cpu->regs[rb], (int) gba->cpu->regs[rb]);
    // todo("%u %d %X", (unsigned int) gba->cpu->regs[ro], gba->cpu->regs[ro], (int) gba->cpu->regs[ro]);

    if (b)
        gba_bus_write_byte(gba, gba->cpu->regs[rd], gba->cpu->regs[rb] + gba->cpu->regs[ro]);
    else
        gba_bus_write_word(gba, gba->cpu->regs[rd], gba->cpu->regs[rb] + gba->cpu->regs[ro]);
}

static void thumb_push_handler(gba_t *gba, uint32_t instr) {
    uint16_t r = CHECK_BIT(instr, 8);

    // TODO reuse this code for STM/LDM instrs

    if (r) {
        gba->cpu->regs[REG_SP] = gba->cpu->regs[REG_LR];
        gba->cpu->regs[REG_SP]--;
    }

    for (int i = 7; i >= 0; i--) {
        if (CHECK_BIT(instr, i)) {
            gba->cpu->regs[REG_SP] = gba->cpu->regs[i];
            gba->cpu->regs[REG_SP]--;
        }
    }

#ifdef DEBUG
    char buf[128] = {};
    size_t buf_offset = 0;
    buf_offset += snprintf(buf + buf_offset, sizeof(buf) - buf_offset, "(0x%04X) PUSH {", instr);

    int first = -1;
    int last = -1;
    for (int i = 0; i < 8; i++) {
        if (CHECK_BIT(instr, i)) {
            if (first < 0) {
                first = i;
                buf_offset += snprintf(buf + buf_offset, sizeof(buf) - buf_offset, "%sR%d", last >= 0 ? "," : "", i);
            } else if (buf[buf_offset - 1] != '-') {
                buf_offset += snprintf(buf + buf_offset, sizeof(buf) - buf_offset, "-");
            }
            last = i;
        } else {
            if (first >= 0) {
                if (last != first)
                    buf_offset += snprintf(buf + buf_offset, sizeof(buf) - buf_offset, "R%d", last);
                first = -1;
            }
        }
    }

    if (buf[buf_offset - 1] == '-')
        buf_offset += snprintf(buf + buf_offset, sizeof(buf) - buf_offset, "R%d", last);

    buf_offset += snprintf(buf + buf_offset, sizeof(buf) - buf_offset, "%s%s}\n", ((instr & 0x00FF) && r) ? "," : "", r ? "R14" : "");

    LOG_DEBUG(buf);
#endif
}

static void thumb_pop_handler(gba_t *gba, uint32_t instr) {
    todo("thumb_pop_handler");
}

static void thumb_b_handler(gba_t *gba, uint32_t instr) {
    int8_t soffset8 = ((int8_t) (instr & 0xFF)) << 1;
    uint8_t cond = (instr >> 8) & 0x0F;
    LOG_DEBUG("(0x%04X) B%s Lxx_#0x%04X\n", instr, cond_names[cond], soffset8);

    // TODO this should be done in cpu_step() ?
    if (!verif_cond(gba->cpu, cond))
        return;

    gba->cpu->regs[REG_PC] += soffset8 - 2; // -2 here to counterbalance the REG_PC += 2 at the end of cpu_step()
    // TODO flush pipeline? --> should be filled with 0: this is an andeq r0, r0, r0 and is equivalent to a nop
    gba->cpu->pipeline[PIPELINE_FETCHING] = 0x00000000;
    gba->cpu->pipeline[PIPELINE_DECODING] = 0x00000000;
}

#define FOREACH_HANDLER(X)           \
    X(not_implemented_handler)       \
    X(and_handler)                   \
    X(mrs_handler)                   \
    X(msr_bx_dispatcher)             \
    X(add_handler)                   \
    X(teq_handler)                   \
    X(cmp_handler)                   \
    X(mov_handler)                   \
    X(str_handler)                   \
    X(ldr_handler)                   \
    X(b_handler)                     \
    X(bl_handler)                    \
    X(thumb_lsl_handler)             \
    X(thumb_lsr_handler)             \
    X(thumb_asr_handler)             \
    X(thumb_add_handler)             \
    X(thumb_mov_handler)             \
    X(thumb_bx_handler)              \
    X(thumb_pc_relative_ldr_handler) \
    X(thumb_reg_str_handler)         \
    X(thumb_push_handler)            \
    X(thumb_pop_handler)             \
    X(thumb_b_handler)
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
    {"____00*0000*________________****", HANDLER_ID(and_handler)}, // Data processing
    {"____00010*00________________0000", HANDLER_ID(mrs_handler)}, // PSR Transfer
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
    // {"____00*1100*________________****", HANDLER_ID(orr_handler)}, // Data processing
    {"____00*1101*________________****", HANDLER_ID(mov_handler)}, // Data processing
    // {"____00*1110*________________****", HANDLER_ID(bic_handler)}, // Data processing
    // {"____00*1111*________________****", HANDLER_ID(mvn_handler)}, // Data processing
    {"____01*****0________________****", HANDLER_ID(str_handler)},         // Single Data Transfer
    {"____01*****1________________****", HANDLER_ID(ldr_handler)},         // Single Data Transfer
    {"____1010****________________****", HANDLER_ID(b_handler)},      // Branch
    {"____1011****________________****", HANDLER_ID(bl_handler)}, // Branch
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
    {"0000000*________", HANDLER_ID(thumb_lsl_handler)}, // Move shifted register
    {"00011*0*________", HANDLER_ID(thumb_add_handler)}, // Add/substract
    {"00100***________", HANDLER_ID(thumb_mov_handler)}, // Move/compare/add/substract immediate
    {"01000111________", HANDLER_ID(thumb_bx_handler)}, // Hi register operations/branch exchange
    {"01001***________", HANDLER_ID(thumb_pc_relative_ldr_handler)}, // PC-relative load
    {"01010*0*________", HANDLER_ID(thumb_reg_str_handler)}, // Load/store with register offset
    {"1011010*________", HANDLER_ID(thumb_push_handler)}, // Push/pop registers
    {"1011110*________", HANDLER_ID(thumb_pop_handler)}, // Push/pop registers
    {"1101****________", HANDLER_ID(thumb_b_handler)}, // Conditonal branch

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
                j = 8;
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

    gba->cpu->regs[REG_PC] = VECTOR_RESET;

    // fill pipeline with impossible conditions to emulate the 2 cycles it takes to fill the pipeline
    // TODO flush pipeline? --> should be filled with 0: this is an andeq r0, r0, r0 and is equivalent to a nop
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
