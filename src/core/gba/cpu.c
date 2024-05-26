#include <stdlib.h>

#include "bus.h"
#include "gba_priv.h"

// // this link is more for the gbmulator IHM
// https://gnome.pages.gitlab.gnome.org/libadwaita/doc/main/migrating-to-adaptive-dialogs.html#port-adwaboutwindow-to-adwaboutdialog

// // links I used while implementing this
// https://emudev.org/system_resources
// https://mgba.io/2015/06/27/cycle-counting-prefetch/
// https://problemkaputt.de/gbatek.htm#armcpureference
// https://github.com/nba-emu/NanoBoyAdvance/blob/master/src/nba/src/arm/handlers/arithmetic.inl#L84
// file:///home/maxime/T%C3%A9l%C3%A9chargements/DDI0029-1.pdf
// https://vision.gel.ulaval.ca/~jflalonde/cours/1001/h17/docs/arm-instructionset.pdf
// https://github.com/Normmatt/gba_bios/blob/master/asm/bios.s

#define CYCLE_I 1
#define CYCLE_N 1
#define CYCLE_S 1

#define REG_SP 13
#define REG_LR 14
#define REG_PC 15

#define PIPELINE_FETCHING 0
#define PIPELINE_DECODING 1

#define CPSR_N (1 << 31) // Negative or less than
#define CPSR_Z (1 << 30) // Zero
#define CPSR_C (1 << 29) // Carry or borrow or extend
#define CPSR_V (1 << 28) // Overflow
#define CPSR_MODE 0x0000000F // Mode bits
#define CPSR_I (1 << 7) // IRQ disable
#define CPSR_F (1 << 6) // FIQ disable
#define CPSR_T (1 << 5) // State bit

#define CPSR_CHECK_FLAG(cpu, flag) ((cpu)->cpsr & (flag))
#define CPSR_CHANGE_FLAG(cpu, pos, value) ((cpu)->cpsr ^= (-(value) ^ (cpu)->cpsr) & (pos))

#define RESET_VECTOR 0x00000000

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
    X(AL)
#define COND_GENERATOR(name) COND_##name,
#define COND_NAME_GENERATOR(name) STRINGIFY(name),

typedef enum {
    FOREACH_COND(COND_GENERATOR)
} cond_t;
static const char *cond_names[] = {
    FOREACH_COND(COND_NAME_GENERATOR)
};

typedef void (*handler_t)(gba_t *gba, uint32_t instr);
handler_t handlers[];

// using double lookup tables to reduce impact on host CPU cache
uint8_t arm_handlers[1 << 12];
uint8_t thumb_handlers[1 << 8]; // TODO not sure of this size

static void not_implemented_handler(UNUSED gba_t *gba, uint32_t instr) {
    todo("not implemented instruction: 0x%08X (0b%032b)", instr, instr);
}

static void data_processing_begin(gba_t *gba, uint32_t instr, uint8_t *rd, uint32_t *op1, uint32_t *op2) {
    bool i = CHECK_BIT(instr, 25);
    bool s = CHECK_BIT(instr, 20); // TODO
    uint8_t rn = (instr & 0x000F0000) >> 16;

    *rd = (instr & 0x0000F000) >> 12;
    *op1 = gba->cpu->regs[rn];
    *op2 = instr & 0x00000FFF;

    // fprintf(stderr, "%c%c rd=0x%02X op1=0x%08X op2=0x%08X\n", i ? 'i' : '-', s ? 's' : '-', *rd, *op1, *op2);

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

static void and_handler(gba_t *gba, uint32_t instr) {
    uint8_t rn = (instr & 0x000F0000) >> 16;
    bool s = CHECK_BIT(instr, 20);

    uint32_t op1;
    uint32_t op2;
    uint8_t rd;
    data_processing_begin(gba, instr, &rd, &op1, &op2);

    fprintf(stderr, "(0x%08X) AND%s%s R%d, R%d, 0x%X\n", instr, cond_names[INSTR_GET_COND(instr)], s ? "S" : "", rd, rn, op2);

    gba->cpu->regs[rd] = op1 & op2;

    if (s) {
        todo("s");

        CPSR_CHANGE_FLAG(gba->cpu, CPSR_N, gba->cpu->regs[rd] >> 31);
        CPSR_CHANGE_FLAG(gba->cpu, CPSR_Z, gba->cpu->regs[rd] == 0);
        // CPSR_CHANGE_FLAG(gba->cpu, CPSR_C, op1 >= op2);
        // CPSR_CHANGE_FLAG(gba->cpu, CPSR_V, (((op1 ^ op2) & (op1 ^ res)) >> 31));
    }
}

static void cmp_handler(gba_t *gba, uint32_t instr) {
    uint8_t rn = (instr & 0x000F0000) >> 16;

    uint32_t op1;
    uint32_t op2;
    uint8_t rd;
    data_processing_begin(gba, instr, &rd, &op1, &op2);

    fprintf(stderr, "(0x%08X) CMP%s R%d, 0x%X\n", instr, cond_names[INSTR_GET_COND(instr)], rn, op2);

    uint32_t res = op1 - op2;

    // TODO update ARM CPSR flags: https://www.dmulholl.com/notes/arm-condition-flags.html
    CPSR_CHANGE_FLAG(gba->cpu, CPSR_N, res >> 31);
    CPSR_CHANGE_FLAG(gba->cpu, CPSR_Z, res == 0);
    CPSR_CHANGE_FLAG(gba->cpu, CPSR_C, op1 >= op2);
    CPSR_CHANGE_FLAG(gba->cpu, CPSR_V, (((op1 ^ op2) & (op1 ^ res)) >> 31));
}

static void mov_handler(gba_t *gba, uint32_t instr) {
    bool s = CHECK_BIT(instr, 20);

    uint32_t op1;
    uint32_t op2;
    uint8_t rd;
    data_processing_begin(gba, instr, &rd, &op1, &op2);

    fprintf(stderr, "(0x%08X) MOV%s%s R%d, 0x%X\n", instr, cond_names[INSTR_GET_COND(instr)], s ? "S" : "", rd, op2);

    gba->cpu->regs[rd] = op2;

    if (s) {
        todo("s");

        CPSR_CHANGE_FLAG(gba->cpu, CPSR_N, gba->cpu->regs[rd] >> 31);
        CPSR_CHANGE_FLAG(gba->cpu, CPSR_Z, gba->cpu->regs[rd] == 0);
        // CPSR_CHANGE_FLAG(gba->cpu, CPSR_C, op1 >= op2);
        // CPSR_CHANGE_FLAG(gba->cpu, CPSR_V, (((op1 ^ op2) & (op1 ^ res)) >> 31));
    }
}

static void str_handler(gba_t *gba, uint32_t instr) {
    todo("str_handler: 0x%08X", instr);
}

static void ldr_handler(gba_t *gba, uint32_t instr) {
    bool b = CHECK_BIT(instr, 22);
    bool w = CHECK_BIT(instr, 21);
    uint8_t rn = (instr >> 16) & 0x0F;
    uint32_t addr = 0;
    todo("(0x%08X) LDR%s%s%s R%d, 0x%X", instr, cond_names[INSTR_GET_COND(instr)], b ? "B" : "", w ? "T" : "", rn, addr);
}

static void branch_handler(gba_t *gba, uint32_t instr) {
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

    fprintf(stderr, "(0x%08X) B%s -- 0x%08X --> PC=0x%08X\n", instr, cond_names[INSTR_GET_COND(instr)], offset, gba->cpu->regs[REG_PC] + 4);
}

static void branch_link_handler(gba_t *gba, uint32_t instr) {
    todo("branch_link_handler: 0x%08X", instr);
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
    fprintf(stderr, "--------\n[PC=0x%08X] [CPSR=%c%c%c%c]\n", cpu->regs[REG_PC], N, Z, C, V);

    uint32_t instr = cpu->pipeline[PIPELINE_DECODING];

    // fetch
    cpu->pipeline[PIPELINE_DECODING] = cpu->pipeline[PIPELINE_FETCHING];
    gba_bus_select(gba, cpu->regs[REG_PC]);
    cpu->pipeline[PIPELINE_FETCHING] = gba_bus_read_word(gba);
    // TODO bus_read can stall CPU (nop instruction inserted) while reading memory (depends on waitstates)
    //      while this stalls, the decode and execute stages continue their operation
    fprintf(stderr, "fetch:   0x%08X\n", cpu->pipeline[PIPELINE_FETCHING]);

    // decode
    fprintf(stderr, "decode:  0x%08X\n", cpu->pipeline[PIPELINE_DECODING]);

    // execute
    // TODO
    // if (cpu->stall) {
        // cpu->stall--;
        // fprintf(stderr, "CPU stalled, remaining: %d\n", cpu->stall);
        // return;
    // }
    fprintf(stderr, "execute: 0x%08X\n", instr);
    if (verif_cond(gba->cpu, INSTR_GET_COND(instr))) {
        uint_fast16_t instr_hash = ((instr & 0x0FF00000) >> 16) | ((instr & 0x000000F0) >> 4);
        handlers[arm_handlers[instr_hash]](gba, instr);
    }

    cpu->regs[REG_PC] += 4;
}

#define FOREACH_HANDLER(X)     \
    X(not_implemented_handler) \
    X(and_handler)             \
    X(cmp_handler)             \
    X(mov_handler)             \
    X(str_handler)             \
    X(ldr_handler)             \
    X(branch_handler)          \
    X(branch_link_handler)
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
// decoder_rules definition order is important: they are matched in the order they are defined in the decoder_rules array
// TODO '_' == '*' for now, its to visually help me find relevant bits: '_' are alway ignored because they are not relevant in the instruction hash
decoder_rule_t decoder_rules[] = {
    {"____00*0000*________________****", HANDLER_ID(and_handler)}, // Data processing
    // {"____00*0001*________________****", HANDLER_ID(eor_handler)}, // Data processing
    // {"____00*0010*________________****", HANDLER_ID(sub_handler)}, // Data processing
    // {"____00*0011*________________****", HANDLER_ID(rsb_handler)}, // Data processing
    // {"____00*0100*________________****", HANDLER_ID(add_handler)}, // Data processing
    // {"____00*0101*________________****", HANDLER_ID(adc_handler)}, // Data processing
    // {"____00*0110*________________****", HANDLER_ID(sbc_handler)}, // Data processing
    // {"____00*0111*________________****", HANDLER_ID(rsc_handler)}, // Data processing
    // {"____00*10001________________****", HANDLER_ID(tst_handler)}, // Data processing
    // {"____00*10011________________****", HANDLER_ID(teq_handler)}, // Data processing
    {"____00*10101________________****", HANDLER_ID(cmp_handler)}, // Data processing
    // {"____00*10111________________****", HANDLER_ID(cmn_handler)}, // Data processing
    // {"____00*1100*________________****", HANDLER_ID(orr_handler)}, // Data processing
    {"____00*1101*________________****", HANDLER_ID(mov_handler)}, // Data processing
    // {"____00*1110*________________****", HANDLER_ID(bic_handler)}, // Data processing
    // {"____00*1111*________________****", HANDLER_ID(mvn_handler)}, // Data processing
    // {"____01*****0________________****", HANDLER_ID(str_handler)}, // Single Data Transfer
    {"____01*****1________________****", HANDLER_ID(ldr_handler)}, // Single Data Transfer
    {"____1010****________________****", HANDLER_ID(branch_handler)}, // Branch
    {"____1011****________________****", HANDLER_ID(branch_link_handler)}, // Branch
};
uint8_t get_handler(uint32_t instr) {
    // TODO
    for (size_t i = 0; i < sizeof(decoder_rules) / sizeof(*decoder_rules); i++) {
        decoder_rule_t rule = decoder_rules[i];

        bool match = true;
        for (size_t j = 0; j < sizeof(rule.match_string) && match; j++) {
            switch (rule.match_string[sizeof(rule.match_string) - j - 1]) {
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
            return rule.handler_id;
    }

    return HANDLER_ID(not_implemented_handler);
}
static void arm_handlers_init(void) {
    for (size_t i = 0; i < sizeof(arm_handlers) / sizeof(*arm_handlers); i++) {
        uint8_t hi = (i & 0xFF0) >> 4;
        uint8_t lo = (i & 0x00F);
        uint32_t instr = (hi << 20) | (lo << 4);
        arm_handlers[i] = get_handler(instr);
    }
}

static void thumb_handlers_init(void) {
    for (size_t i = 0; i < sizeof(thumb_handlers); i++) {
        
    }
}

gba_cpu_t *gba_cpu_init(void) {
    gba_cpu_t *cpu = xcalloc(1, sizeof(*cpu));

    cpu->regs[REG_PC] = RESET_VECTOR;

    // fill pipeline with impossible conditions to emulate the 2 cycles it takes to fill the pipeline
    // TODO flush pipeline? --> should be filled with 0: this is an andeq r0, r0, r0 and is equivalent to a nop
    cpu->pipeline[PIPELINE_FETCHING] = 0x00000000;
    cpu->pipeline[PIPELINE_DECODING] = 0x00000000;

    // generate handlers only once when initializing the first cpu instance
    static bool handlers_generated = false;
    if (!handlers_generated) {
        printf("Generating ARM7TDMI cpu handlers\n");
        arm_handlers_init();
        thumb_handlers_init();
        handlers_generated = true;
    }

    return cpu;
}

void gba_cpu_quit(gba_cpu_t *cpu) {
    free(cpu);
}
