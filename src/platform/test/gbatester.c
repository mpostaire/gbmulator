#include <stdio.h>
#include <stdlib.h>
#include "../../core/gba/gba_priv.h"
#include "../common/utils.h"

#define MAGIC 0xD33DBAE0

#define CPSR_T                     (((uint32_t) 1) << 5) // State bit
#define CPSR_CHECK_FLAG(cpu, flag) ((cpu)->cpsr & (flag))

typedef enum {
    REG_IDX_USR_SYS = 0,
    REG_IDX_FIQ,
    REG_IDX_SVC,
    REG_IDX_ABT,
    REG_IDX_IRQ,
    REG_IDX_UND,
    REG_IDX_INVALID_MODE
} cpu_mode_reg_indexes_t;

typedef struct {
    bool is_done;

    enum {
        GBA_BUS_TRANSACTION_KIND_INSTR_READ,
        GBA_BUS_TRANSACTION_KIND_READ,
        GBA_BUS_TRANSACTION_KIND_WRITE
    } kind;
    uint32_t size;
    uint32_t addr;
    uint32_t data;
    uint32_t cycle;
    uint32_t access;
} gba_bus_transaction_t;

typedef struct {
    uint8_t      rom[256];
    gbmulator_t *init;
    gbmulator_t *expected;
} gba_cpu_tester_t;

static gba_cpu_tester_t gba_cpu_tester = { .rom = { [0xB2] = 0x96 } };

static size_t                next_transaction  = 0;
static size_t                transactions_size = 0;
static gba_bus_transaction_t transactions[64];

uint8_t __wrap_gba_bus_read(UNUSED gba_t *gba, uint8_t mode, uint32_t address) {
    bool is_same_addr                      = address == transactions[next_transaction].addr;
    bool is_read                           = transactions[next_transaction].kind != GBA_BUS_TRANSACTION_KIND_WRITE;
    bool is_same_size                      = transactions[next_transaction].size == BUS_ACCESS_GET_SIZE(mode);
    transactions[next_transaction].is_done = is_same_addr && is_read && is_same_size;

    if (next_transaction >= transactions_size) {
        printf("transaction[%zu]: expected nothing, got read of size %u @ 0x%08X\n", next_transaction, transactions[next_transaction].size, transactions[next_transaction].addr, BUS_ACCESS_GET_SIZE(mode), address);
    } else if (!transactions[next_transaction].is_done) {
        if (is_read) {
            printf("transaction[%zu]: expected read of size %u @ 0x%08X, got read of size %u @ 0x%08X\n", next_transaction, transactions[next_transaction].size, transactions[next_transaction].addr, BUS_ACCESS_GET_SIZE(mode), address);
        } else {
            printf("transaction[%zu]: expected write, got read of size %u @ 0x%08X\n\n", next_transaction, BUS_ACCESS_GET_SIZE(mode), address);
        }
    }

    return transactions[next_transaction++].data;
}

void __wrap_gba_bus_write(UNUSED gba_t *gba, uint8_t mode, uint32_t address, uint32_t data) {
    bool is_same_addr                        = address == transactions[next_transaction].addr;
    bool is_same_data                        = data == transactions[next_transaction].data;
    bool is_write                            = transactions[next_transaction].kind == GBA_BUS_TRANSACTION_KIND_WRITE;
    bool is_same_size                        = transactions[next_transaction].size == BUS_ACCESS_GET_SIZE(mode);
    transactions[next_transaction++].is_done = is_same_addr && is_same_data && is_write && is_same_size;

    if (next_transaction >= transactions_size) {
        printf("transaction[%zu]: expected nothing, got [0x%08X]=0x%08X\n", next_transaction, transactions[next_transaction].addr, address);
    } else if (!transactions[next_transaction - 1].is_done) {
        if (is_write) {
            printf("transaction[%zu]: expected [0x%08X]=0x%08X, got [0x%08X]=0x%08X\n", next_transaction - 1, transactions[next_transaction - 1].addr, transactions[next_transaction - 1].data, address, data);
        } else {
            printf("transaction[%zu] is not a write\n", next_transaction - 1);
        }
    }
}

static bool cpu_reg_equals(uint8_t reg, uint32_t expected, uint32_t got, uint32_t tested_opcode, bool is_arm_str_ldr, bool is_arm_mull_mlal_mul_mla) {
    gba_t *init   = gba_cpu_tester.init->impl;
    bool   is_arm = !CPSR_CHECK_FLAG(&init->cpu, CPSR_T);

    if ((reg == REG_PC) && (is_arm)) {
        if (((tested_opcode & 0b00001111'10110000'11110000'11110000) == 0b00000001'00000000'11110000'00000000)) {
            // MRS with PC as destination register is undefined
            uint8_t rd = (tested_opcode >> 12) & 0x0F;
            if (rd == REG_PC)
                return true;
        }
    }

    uint32_t reg_mask = 0xFFFFFFFF;
    if (reg == REG_PC && is_arm_str_ldr)
        reg_mask = 0xFFFF0000;

    return (expected & reg_mask) == (got & reg_mask);
}

static bool cpu_equals(gba_cpu_t *expected, gba_cpu_t *got, uint32_t tested_opcode, bool is_arm_str_ldr, bool is_arm_mull_mlal_mul_mla) {
    bool success = true;

    gba_t *init         = gba_cpu_tester.init->impl;
    bool   is_thumb_mul = CPSR_CHECK_FLAG(&init->cpu, CPSR_T) && ((tested_opcode & 0b11111111'11000000) == 0b01000011'01000000);

    if (is_arm_mull_mlal_mul_mla || is_thumb_mul) {
        uint8_t rd = (tested_opcode >> 16) & 0x0F;
        uint8_t rn = (tested_opcode >> 12) & 0x0F;
        uint8_t rs = (tested_opcode >> 8) & 0x0F;
        uint8_t rm = tested_opcode & 0x0F;

        // R15 is forbidden in multiplication instructions
        if (rd == REG_PC || rn == REG_PC || rs == REG_PC || rm == REG_PC)
            return true;
        // all regs must be different in multiplication instructions
        if (rd == rn || rd == rs || rd == rm || rn == rs || rn == rm || rs == rm)
            return true;
    }

    for (size_t i = 0; i < sizeof(expected->regs) / sizeof(*expected->regs); i++) {
        if (!cpu_reg_equals(i, expected->regs[i], got->regs[i], tested_opcode, is_arm_str_ldr, is_arm_mull_mlal_mul_mla)) {
            success = false;
            printf("R%zu expected 0x%08X, got 0x%08X\n", i, expected->regs[i], got->regs[i]);
        }
    }

    for (size_t i = REG_IDX_FIQ; i < sizeof(expected->banked_regs_8_12) / sizeof(*expected->banked_regs_8_12); i++) {
        for (size_t j = 0; j < sizeof(*expected->banked_regs_8_12) / sizeof(**expected->banked_regs_8_12); j++) {
            if (!cpu_reg_equals(j, expected->banked_regs_8_12[i][j], got->banked_regs_8_12[i][j], tested_opcode, is_arm_str_ldr, is_arm_mull_mlal_mul_mla)) {
                success = false;
                printf("R%zu (bank %zu) expected 0x%08X, got 0x%08X\n", j + 8, i, expected->banked_regs_8_12[i][j], got->banked_regs_8_12[i][j]);
            }
        }
    }

    for (size_t j = 0; j < sizeof(*expected->banked_regs_13_14) / sizeof(**expected->banked_regs_13_14); j++) {
        if (!cpu_reg_equals(j, expected->banked_regs_13_14[REG_IDX_FIQ][j], got->banked_regs_13_14[REG_IDX_FIQ][j], tested_opcode, is_arm_str_ldr, is_arm_mull_mlal_mul_mla)) {
            success = false;
            printf("R%zu (bank %zu) expected 0x%08X, got 0x%08X\n", j + 13, (size_t) REG_IDX_FIQ, expected->banked_regs_13_14[REG_IDX_FIQ][j], got->banked_regs_13_14[REG_IDX_FIQ][j]);
        }
    }

    // TODO CPSR_V, CPSR_C, CPSR_N flag for multiplication instructions ignored for now
    uint32_t cpsr_mask = 0xFFFFFFFF;
    if (is_arm_mull_mlal_mul_mla)
        cpsr_mask = ~((1 << 28) | (1 << 29));
    if (is_thumb_mul)
        cpsr_mask = ~((1 << 31) | (1 << 28) | (1 << 29));

    if ((expected->cpsr & cpsr_mask) != (got->cpsr & cpsr_mask)) {
        success = false;
        printf("CPSR expected 0x%08X, got 0x%08X\n", expected->cpsr, got->cpsr);
    }

    for (size_t i = 0; i < sizeof(expected->spsr) / sizeof(*expected->spsr); i++) {
        if ((expected->spsr[i] & cpsr_mask) != (got->spsr[i] & cpsr_mask)) {
            success = false;
            printf("spsr[%zu] expected 0x%08X, got 0x%08X\n", i, expected->spsr[i], got->spsr[i]);
        }
    }

    // // int pipeline = memcmp(a->pipeline, b->pipeline, sizeof(a->pipeline) / sizeof(*a->pipeline));
    // int pipeline = 0; // TODO

    return success;
}

static bool check_transactions(void) {
    // TODO transaction check sequential/non sequential
    for (size_t i = 0; i < transactions_size; i++)
        if (!transactions[i].is_done)
            return false; // TODO print details of failed transactions

    return true;
}

static uint32_t parse_u32(uint8_t **test_data) {
    uint32_t ret  = *((uint32_t *) *test_data);
    *test_data   += sizeof(ret);
    return ret;
}

static void parse_u32_array(uint8_t **test_data, uint32_t *array, size_t n) {
    for (size_t i = 0; i < n; i++)
        array[i] = parse_u32(test_data);
}

static void parse_state(uint8_t **test_data, gba_cpu_t *cpu) {
    /* uint32_t full_sz = */ parse_u32(test_data);
    parse_u32(test_data); // ignore 4 bytes

    // R
    parse_u32_array(test_data, cpu->regs, 16);
    memcpy(cpu->banked_regs_8_12[0], &cpu->regs[8], sizeof(cpu->banked_regs_8_12[0]));
    memcpy(cpu->banked_regs_13_14[0], &cpu->regs[13], sizeof(cpu->banked_regs_13_14[0]));
    // R_fiq
    parse_u32_array(test_data, cpu->banked_regs_8_12[REG_IDX_FIQ], 5);
    parse_u32_array(test_data, cpu->banked_regs_13_14[REG_IDX_FIQ], 2);
    // R_svc
    parse_u32_array(test_data, cpu->banked_regs_13_14[REG_IDX_SVC], 2);
    // R_abt
    parse_u32_array(test_data, cpu->banked_regs_13_14[REG_IDX_ABT], 2);
    // R_irq
    parse_u32_array(test_data, cpu->banked_regs_13_14[REG_IDX_IRQ], 2);
    // R_und
    parse_u32_array(test_data, cpu->banked_regs_13_14[REG_IDX_UND], 2);

    cpu->cpsr    = parse_u32(test_data);
    cpu->spsr[0] = cpu->cpsr;

    parse_u32_array(test_data, &cpu->spsr[1], 5);
    parse_u32_array(test_data, cpu->pipeline, 2);

    /* uint32_t access = */ parse_u32(test_data);
}

static void parse_transactions(uint8_t **test_data) {
    transactions_size = 0;
    next_transaction  = 0;

    /* uint32_t full_sz = */ parse_u32(test_data);
    /* uint32_t magic = */ parse_u32(test_data);
    uint32_t num_transactions = parse_u32(test_data);

    for (uint32_t i = 0; i < num_transactions; i++) {
        transactions[transactions_size].kind   = parse_u32(test_data);
        transactions[transactions_size].size   = parse_u32(test_data);
        transactions[transactions_size].addr   = parse_u32(test_data);
        transactions[transactions_size].data   = parse_u32(test_data);
        transactions[transactions_size].cycle  = parse_u32(test_data);
        transactions[transactions_size].access = parse_u32(test_data);

        transactions[transactions_size].is_done = false;
        transactions_size++;
    }
}

static uint32_t parse_opcodes(uint8_t **test_data, gba_cpu_t *cpu) {
    /* uint32_t full_sz = */ parse_u32(test_data);
    parse_u32(test_data); // ignore 4 bytes
    uint32_t opcode = parse_u32(test_data);
    /* uint32_t base_addr = */ parse_u32(test_data);

    cpu->pipeline[1] = opcode;

    return opcode;
}

static void gba_cpu_tester_init(void) {
    uint8_t checksum = 0;
    for (int i = 0xA0; i < 0xBC; i++)
        checksum -= gba_cpu_tester.rom[i];
    checksum -= 0x19;

    gba_cpu_tester.rom[0xBD] = checksum;

    gbmulator_options_t opts = {
        .rom      = gba_cpu_tester.rom,
        .rom_size = sizeof(gba_cpu_tester.rom),
        .mode     = GBMULATOR_MODE_GBA
    };

    gba_cpu_tester.init     = gbmulator_init(&opts);
    gba_cpu_tester.expected = gbmulator_init(&opts);
}

static void gba_cpu_tester_quit(void) {
    gbmulator_quit(gba_cpu_tester.init);
    gbmulator_quit(gba_cpu_tester.expected);
}

static bool gba_cpu_tester_run(const char *path) {
    if (!path)
        return false;

    size_t   len;
    uint8_t *test_data = read_file(path, &len);

    uint8_t *test_data_ptr = test_data;

    uint32_t magic = parse_u32(&test_data_ptr);
    if (magic != MAGIC)
        return false;

    uint32_t num_tests = parse_u32(&test_data_ptr);
    printf("num_tests=%u\n", num_tests);

#define CPSR_MODE_MASK     0x0000001F // Mode bits
#define CPSR_GET_MODE(cpu) ((cpu)->cpsr & CPSR_MODE_MASK)

    bool is_arm_str_ldr           = !strncmp(path, "ARM7TDMI/v1/arm_ldrh_strh.json.bin", 35) || !strncmp(path, "ARM7TDMI/v1/arm_ldrsb_ldrsh.json.bin", 37) || !strncmp(path, "ARM7TDMI/v1/arm_ldr_str_immediate_offset.json.bin", 50) || !strncmp(path, "ARM7TDMI/v1/arm_ldr_str_register_offset.json.bin", 49) || !strncmp(path, "ARM7TDMI/v1/arm_ldrsb_ldrsh.json.bin", 37);
    bool is_arm_mull_mlal_mul_mla = !strncmp(path, "ARM7TDMI/v1/arm_mull_mlal.json.bin", 35) || !strncmp(path, "ARM7TDMI/v1/arm_mul_mla.json.bin", 33);

    gba_t *init     = gba_cpu_tester.init->impl;
    gba_t *expected = gba_cpu_tester.expected->impl;

    uint32_t errors = 0;
    for (uint32_t i = 0; i < num_tests; i++) {
        // uint8_t *start_ptr = test_data_ptr;
        /* uint32_t full_sz = */ parse_u32(&test_data_ptr);

        parse_state(&test_data_ptr, &init->cpu);
        parse_state(&test_data_ptr, &expected->cpu);
        parse_transactions(&test_data_ptr);
        uint32_t opcode = parse_opcodes(&test_data_ptr, &init->cpu);

        uint8_t mode = CPSR_GET_MODE(&init->cpu);
        // uint8_t bank = regs_mode_hashes[mode & 0x0F];

        bank_registers(&init->cpu, 0, mode); // from usr_sys mode to mode of current test

        init->cpu.pipeline_flush_cycles = 0;
        gba_cpu_step(init);

        while (init->cpu.pipeline_flush_cycles > 0)
            gba_cpu_step(init);

        mode = CPSR_GET_MODE(&init->cpu);
        bank_registers(&init->cpu, mode, 0); // go back to usr_sys mode

        // TODO when cpu sets cpsr, we shouldn't always (never?) mirror it to spsr[0]
        // ----> understand exactly when/where spsr is written

        if (!cpu_equals(&expected->cpu, &init->cpu, opcode, is_arm_str_ldr, is_arm_mull_mlal_mul_mla)) {
            printf("❌ CPU state mismatch (%u)!\n", i);
            errors++;
            break;
        }
    }

    free(test_data);

    if (errors > 0) {
        printf("errors: %u/%u\n", errors, num_tests);
    }

    return errors == 0;
}

int gbatester_main(int argc, char **argv) {
    gba_cpu_tester_init();

    static const char *test_paths[] = {
        "ARM7TDMI/v1/arm_b_bl.json.bin",                      // OK
        "ARM7TDMI/v1/arm_bx.json.bin",                        // OK
        "ARM7TDMI/v1/arm_cdp.json.bin",                       // OK
        "ARM7TDMI/v1/arm_data_proc_immediate.json.bin",       // OK
        "ARM7TDMI/v1/arm_data_proc_immediate_shift.json.bin", // OK
        "ARM7TDMI/v1/arm_data_proc_register_shift.json.bin",  // OK
        "ARM7TDMI/v1/arm_ldm_stm.json.bin",                   // 0
        "ARM7TDMI/v1/arm_ldrh_strh.json.bin",                 // OK
        "ARM7TDMI/v1/arm_ldrsb_ldrsh.json.bin",               // OK
        "ARM7TDMI/v1/arm_ldr_str_immediate_offset.json.bin",  // OK
        "ARM7TDMI/v1/arm_ldr_str_register_offset.json.bin",   // OK
        "ARM7TDMI/v1/arm_mcr_mrc.json.bin",                   // OK
        "ARM7TDMI/v1/arm_mrs.json.bin",                       // OK
        "ARM7TDMI/v1/arm_msr_imm.json.bin",                   // OK
        "ARM7TDMI/v1/arm_msr_reg.json.bin",                   // OK
        "ARM7TDMI/v1/arm_mull_mlal.json.bin",                 // OK
        "ARM7TDMI/v1/arm_mul_mla.json.bin",                   // OK
        "ARM7TDMI/v1/arm_stc_ldc.json.bin",                   // OK
        "ARM7TDMI/v1/arm_swi.json.bin",                       // OK
        "ARM7TDMI/v1/arm_swp.json.bin",                       // OK
        "ARM7TDMI/v1/thumb_add_cmp_mov_hi.json.bin",          // OK
        "ARM7TDMI/v1/thumb_add_sp_or_pc.json.bin",            // OK
        "ARM7TDMI/v1/thumb_add_sub.json.bin",                 // OK
        "ARM7TDMI/v1/thumb_add_sub_sp.json.bin",              // OK
        "ARM7TDMI/v1/thumb_bcc.json.bin",                     // OK
        "ARM7TDMI/v1/thumb_b.json.bin",                       // OK
        "ARM7TDMI/v1/thumb_bl_blx_prefix.json.bin",           // OK
        "ARM7TDMI/v1/thumb_bl_suffix.json.bin",               // OK
        "ARM7TDMI/v1/thumb_bx.json.bin",                      // OK
        "ARM7TDMI/v1/thumb_data_proc.json.bin",               // OK
        "ARM7TDMI/v1/thumb_ldm_stm.json.bin",                 // OK
        "ARM7TDMI/v1/thumb_ldrb_strb_imm_offset.json.bin",    // OK
        "ARM7TDMI/v1/thumb_ldrh_strh_imm_offset.json.bin",    // OK
        "ARM7TDMI/v1/thumb_ldrh_strh_reg_offset.json.bin",    // OK
        "ARM7TDMI/v1/thumb_ldr_pc_rel.json.bin",              // OK
        "ARM7TDMI/v1/thumb_ldrsb_strb_reg_offset.json.bin",   // OK
        "ARM7TDMI/v1/thumb_ldrsh_ldrsb_reg_offset.json.bin",  // OK
        "ARM7TDMI/v1/thumb_ldr_str_imm_offset.json.bin",      // OK
        "ARM7TDMI/v1/thumb_ldr_str_reg_offset.json.bin",      // OK
        "ARM7TDMI/v1/thumb_ldr_str_sp_rel.json.bin",          // OK
        "ARM7TDMI/v1/thumb_lsl_lsr_asr.json.bin",             // OK
        "ARM7TDMI/v1/thumb_mov_cmp_add_sub.json.bin",         // OK
        "ARM7TDMI/v1/thumb_push_pop.json.bin",                // OK
        "ARM7TDMI/v1/thumb_swi.json.bin",                     // OK
        "ARM7TDMI/v1/thumb_undefined_bcc.json.bin"            // OK
    };

    bool success = true;
    for (size_t i = 0; i < sizeof(test_paths) / sizeof(*test_paths); i++) {
        printf("Testing: %s\n", test_paths[i]);

        if (!gba_cpu_tester_run(test_paths[i])) {
            success = false;
            break;
        }
    }

    gba_cpu_tester_quit();

    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
