// /**
//  * This needs the test roms from this repository:
//  * https://github.com/c-sp/gameboy-test-roms
//  */

// #define _GNU_SOURCE
// #include <sched.h>
// #include <pthread.h>
// #include <stdlib.h>
// #include <string.h>
// #include <sys/stat.h>
// #include <dirent.h>
// #include <MagickWand/MagickWand.h>

// #include "../core/gb/gb_priv.h"

// #define BOLD "\033[1m"
// #define COLOR_OFF "\033[0m"
// #define COLOR_RED "\033[1;31m"
// #define COLOR_GREEN "\033[1;32m"
// #define COLOR_YELLOW "\033[0;33m"
// #define COLOR_BLUE "\033[1;34m"

// #define BUF_SIZE 256

// size_t num_cpus;
// FILE *output_file;
// size_t next_test = 0;
// pthread_mutex_t next_test_mutex = PTHREAD_MUTEX_INITIALIZER;

// static char root_path[BUF_SIZE];

// static uint8_t dmg_boot_found;
// static uint8_t cgb_boot_found;
// static uint8_t dmg_boot[0x100];
// static uint8_t cgb_boot[0x900];

// typedef struct {
//     char *rom_path;                 // relative the the root_path given in the program's argument
//     char *reference_image_filename; // relative the the root_path given in the program's argument
//     char *result_diff_image_suffix;
//     gb_mode_t mode;
//     int running_ms;
//     uint8_t exit_opcode;
//     char *input_sequence;
//     int is_gbmicrotest;
// } test_t;

// static test_t tests[] = {
//     #include "./tests.txt"
// };

// static void load_bootroms(void) {
//     FILE *dmg_f = fopen("dmg_boot.bin", "r");
//     if (dmg_f) {
//         if (fread(dmg_boot, 1, sizeof(dmg_boot), dmg_f) == 0x100)
//             dmg_boot_found = 1;
//         else
//             printf("Cannot read dmg_boot.bin, using default DMG boot ROM...\n");
//         fclose(dmg_f);
//     } else {
//         printf("Cannot open dmg_boot.bin, using default DMG boot ROM...\n");
//     }

//     FILE *cgb_f = fopen("cgb_boot.bin", "r");
//     if (cgb_f) {
//         if (fread(cgb_boot, 1, sizeof(cgb_boot), cgb_f) == 0x900)
//             cgb_boot_found = 1;
//         else
//             printf("Cannot read cgb_boot.bin, using default CGB boot ROM...\n");
//         fclose(cgb_f);
//     } else {
//         printf("Cannot open cgb_boot.bin, using default CGB boot ROM...\n");
//     }
// }

// static uint8_t *get_rom(const char *path, size_t *rom_size) {
//     const char *dot = strrchr(path, '.');
//     if (!dot || (strncmp(dot, ".gb", MAX(strlen(dot), sizeof(".gb"))) && strncmp(dot, ".gbc", MAX(strlen(dot), sizeof(".gbc"))))) {
//         eprintf("%s: wrong file extension (expected .gb or .gbc)\n", path);
//         return NULL;
//     }

//     FILE *f = fopen(path, "rb");
//     if (!f) {
//         errnoprintf("opening file %s", path);
//         return NULL;
//     }

//     fseek(f, 0, SEEK_END);
//     size_t len = ftell(f);
//     fseek(f, 0, SEEK_SET);

//     uint8_t *buf = xmalloc(len);
//     if (!fread(buf, len, 1, f)) {
//         errnoprintf("reading %s", path);
//         fclose(f);
//         return NULL;
//     }
//     fclose(f);

//     if (rom_size)
//         *rom_size = len;
//     return buf;
// }

// static int dir_exists(const char *directory_path) {
//     DIR *dir = opendir(directory_path);
// 	if (dir == NULL) {
// 		if (errno == ENOENT)
// 			return 0;
// 		errnoprintf("opendir");
//         exit(EXIT_FAILURE);
// 	}
// 	closedir(dir);
// 	return 1;
// }

// static void mkdirp(const char *directory_path) {
//     char buf[256];
//     snprintf(buf, sizeof(buf), "%s", directory_path);
//     size_t len = strlen(buf);

//     if (buf[len - 1] == '/')
//         buf[len - 1] = 0;

//     for (char *p = buf + 1; *p; p++) {
//         if (*p == '/') {
//             *p = 0;
//             if (mkdir(buf, 0744) && errno != EEXIST) {
//                 perror("mkdir");
//                 exit(EXIT_FAILURE);
//             }
//             *p = '/';
//         }
//     }

//     if (mkdir(buf, 0744) && errno != EEXIST) {
//         perror("mkdir");
//         exit(EXIT_FAILURE);
//     }
// }

// static void make_parent_dirs(const char *filepath) {
//     char *last_slash = strrchr(filepath, '/');
//     int last_slash_index = last_slash ? (int) (last_slash - filepath) : -1;

//     if (last_slash_index != -1) {
//         char directory_path[last_slash_index + 1];
//         snprintf(directory_path, last_slash_index + 1, "%s", filepath);

//         if (!dir_exists(directory_path))
//             mkdirp(directory_path);
//     }
// }

// static void magick_wand_error(MagickWand *wand) {
//     ExceptionType severity;
//     char *description = MagickGetException(wand, &severity);
//     (void) printf("\n%s %s %lu %s\n", GetMagickModule(), description);
//     description = (char *) MagickRelinquishMemory(description);
//     exit(EXIT_FAILURE);
// }

// static int save_and_check_result(test_t *test, gb_t *gb, char *rom_path) {
//     // get paths

//     char *path_from_category = strchr(rom_path, '/') + 1;
//     char *rom_name = strrchr(rom_path, '/') + 1;
//     int path_until_extension_len = strrchr(rom_path, '.') - path_from_category;
//     int new_dir_path_len = rom_name - 1 - path_from_category;

//     char *reference_old_path = NULL;
//     char *reference_new_path = NULL;
//     char *diff_path = NULL;

//     char result_path[BUF_SIZE];
//     char *label = test->mode == GB_MODE_CGB ? "cgb" : "dmg";
//     char *suffix = test->result_diff_image_suffix ? test->result_diff_image_suffix : "";
//     snprintf(result_path, sizeof(result_path), "results/%.*s-%s%s.result.png", path_until_extension_len, path_from_category, suffix, label);

//     if (test->reference_image_filename) {
//         reference_old_path = xmalloc(BUF_SIZE + 2);
//         reference_new_path = xmalloc(BUF_SIZE);
//         diff_path = xmalloc(BUF_SIZE);

//         snprintf(reference_old_path, BUF_SIZE + 2, "%s/%s", root_path, test->reference_image_filename);

//         char *reference_extension = strrchr(test->reference_image_filename, '.');
//         char *reference_last_slash = strrchr(test->reference_image_filename, '/');
//         int new_path_until_extension_len = reference_extension - reference_last_slash;
//         snprintf(reference_new_path, BUF_SIZE, "results/%.*s/%.*s.expected.png", new_dir_path_len, path_from_category, new_path_until_extension_len, reference_last_slash);

//         snprintf(diff_path, BUF_SIZE, "results/%.*s-%s%s.diff.png", path_until_extension_len, path_from_category, suffix, label);
//     }

//     // create dir structure
//     make_parent_dirs(result_path);

//     // save and check

//     MagickWand *result_wand = NewMagickWand();
//     PixelWand *pixel_wand = NewPixelWand();

//     if (!MagickNewImage(result_wand, GB_SCREEN_WIDTH, GB_SCREEN_HEIGHT, pixel_wand))
//         magick_wand_error(result_wand);

//     if (!MagickImportImagePixels(result_wand, 0, 0, GB_SCREEN_WIDTH, GB_SCREEN_HEIGHT, "RGBA", CharPixel, gb->ppu->pixels))
//         magick_wand_error(result_wand);

//     if (!MagickWriteImage(result_wand, result_path))
//         magick_wand_error(result_wand);

//     if (test->reference_image_filename) {

//         MagickWand *reference_wand = NewMagickWand();
//         if (!MagickReadImage(reference_wand, reference_old_path))
//             magick_wand_error(reference_wand);

//         if (!MagickWriteImage(reference_wand, reference_new_path))
//             magick_wand_error(reference_wand);

//         double distortion;
//         MagickWand *diff_wand = MagickCompareImages(result_wand, reference_wand, AbsoluteErrorMetric, &distortion);

//         if (!MagickWriteImage(diff_wand, diff_path))
//             magick_wand_error(diff_wand);

//         DestroyMagickWand(result_wand);
//         DestroyMagickWand(reference_wand);
//         DestroyMagickWand(diff_wand);
//         DestroyPixelWand(pixel_wand);

//         if (reference_old_path)
//             free(reference_old_path);
//         if (reference_new_path)
//             free(reference_new_path);
//         if (diff_path)
//             free(diff_path);

//         // rtc3test.gb test for sub-second-writes can have a little margin of error:
//         // because the emulator goes very fast, there is an error of 0.1 ms in the sub second writes test of rtc3test.gb
//         // it should be considered as a success but the image comparison fails as it's not exactly the same
//         if (test->result_diff_image_suffix && !strncmp(test->result_diff_image_suffix, "sub-second-writes", 18))
//             return !distortion || distortion == 31.0;
//         else
//             return !distortion;
//     }

//     if (test->is_gbmicrotest)
//         return gb->mmu->hram[0xFF82 - MMU_HRAM] == 0x01;

//     gb_registers_t regs = gb->cpu->registers;
//     return regs.bc == 0x0305 && regs.de == 0x080D && regs.hl == 0x1522;
// }

// static gbmulator_joypad_t str_to_joypad(char *str) {
//     if (!strncmp(str, "right", 7))
//         return GBMULATOR_JOYPAD_RIGHT;
//     if (!strncmp(str, "left", 7))
//         return GBMULATOR_JOYPAD_LEFT;
//     if (!strncmp(str, "up", 7))
//         return GBMULATOR_JOYPAD_UP;
//     if (!strncmp(str, "down", 7))
//         return GBMULATOR_JOYPAD_DOWN;
//     if (!strncmp(str, "a", 7))
//         return GBMULATOR_JOYPAD_A;
//     if (!strncmp(str, "b", 7))
//         return GBMULATOR_JOYPAD_B;
//     if (!strncmp(str, "select", 7))
//         return GBMULATOR_JOYPAD_SELECT;
//     if (!strncmp(str, "start", 7))
//         return GBMULATOR_JOYPAD_START;
//     exit(EXIT_FAILURE);
// }

// static void exec_input_sequence(gb_t *gb, char *input_sequence) {
//     static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
//     pthread_mutex_lock(&mutex);

//     char cpy[BUF_SIZE];
//     strncpy(cpy, input_sequence, sizeof(cpy) - 1);
//     char *delay_str = strtok(cpy, ":");
//     char *input_str = strtok(NULL, ",");

//     while (delay_str && input_str) {
//         int delay = atoi(delay_str);
//         gbmulator_joypad_t input = str_to_joypad(input_str);

//         gb_run_frames(gb, delay * GB_FRAMES_PER_SECOND);
//         gb_joypad_press(gb, input);
//         gb_run_frames(gb, 1);
//         gb_joypad_release(gb, input);

//         delay_str = strtok(NULL, ":");
//         input_str = strtok(NULL, ",");
//     }

//     pthread_mutex_unlock(&mutex);
// }

// static int run_test(test_t *test) {
//     char rom_path[BUF_SIZE];
//     if (snprintf(rom_path, BUF_SIZE, "%s/%s", root_path, test->rom_path) < 0)
//         exit(EXIT_FAILURE);

//     size_t rom_size = 0;
//     uint8_t *rom = get_rom(rom_path, &rom_size);
//     if (!rom)
//         return 0;

//     gb_options_t opts = {
//         .mode = test->mode,
//         .palette = PPU_COLOR_PALETTE_GRAY,
//         .disable_cgb_color_correction = 1
//     };
//     gb_t *gb = gb_init(rom, rom_size, &opts);
//     free(rom);
//     if (!gb)
//         return 0;

//     if (dmg_boot_found)
//         gb->mmu->dmg_boot_rom = dmg_boot;
//     if (cgb_boot_found)
//         gb->mmu->cgb_boot_rom = cgb_boot;

//     // run until the boot sequence is done
//     while (gb->mmu->io_registers[IO_BANK] == 0)
//         gb_step(gb);

//     if (test->input_sequence) {
//         gb_run_frames(gb, 8); // run for some frames to let the test rom some time to setup itself
//         exec_input_sequence(gb, test->input_sequence);
//     }

//     // the maximum time a test can take to run is 120 emulated seconds:
//     // the timeout is a little higher than this value to be safe
//     long timeout_cycles = 128 * GB_CPU_FREQ;
//     if (test->exit_opcode) {
//         while (gb->cpu->opcode != test->exit_opcode && timeout_cycles > 0) {
//             gb_step(gb); // don't take returned cycles to ignore double speed
//             timeout_cycles -= 4;
//         }
//     }
//     if (timeout_cycles > 0)
//         gb_run_steps(gb, test->running_ms * (GB_CPU_STEPS_PER_FRAME / 16));

//     // take screenshot, save it and compare to the reference
//     int ret = save_and_check_result(test, gb, rom_path);
//     gb_quit(gb);

//     if (!ret && timeout_cycles <= 0)
//         ret = -1;
//     return ret;
// }

// static void *run_tests(UNUSED void *arg) {
//     size_t num_tests = sizeof(tests) / sizeof(*tests);

//     pthread_mutex_lock(&next_test_mutex);
//     while (next_test < num_tests) {
//         test_t test = tests[next_test++];
//         pthread_mutex_unlock(&next_test_mutex);

//         char *label = test.mode == GB_MODE_CGB ? "CGB" : "DMG";
//         char *suffix = test.result_diff_image_suffix ? test.result_diff_image_suffix : "";

//         int success = run_test(&test);
//         if (success == 1) {
//             printf(COLOR_GREEN "[PASS]" COLOR_OFF " (%s) %s" COLOR_YELLOW " %s%*s\n" COLOR_OFF, label, test.rom_path, suffix, 10, "");
//             fprintf(output_file, "%s:%s:%s:success\n", label, test.rom_path, suffix);
//         } else if (success == -1) {
//             printf(COLOR_RED "[FAIL]" COLOR_OFF " (%s) %s" COLOR_YELLOW " %s%*s\n" COLOR_OFF, label, test.rom_path, suffix, 10, "");
//             fprintf(output_file, "%s:%s:%s:timeout\n", label, test.rom_path, suffix);
//         } else {
//             printf(COLOR_RED "[FAIL]" COLOR_OFF " (%s) %s" COLOR_YELLOW " %s%*s\n" COLOR_OFF, label, test.rom_path, suffix, 10, "");
//             fprintf(output_file, "%s:%s:%s:failed\n", label, test.rom_path, suffix);
//         }

//         pthread_mutex_lock(&next_test_mutex);
//     }
//     pthread_mutex_unlock(&next_test_mutex);

//     return NULL;
// }

// int main(int argc, char **argv) {
//     if (argc < 2) {
//         eprintf("Usage: %s /path/to/test/root/dir\n", argv[0]);
//         return EXIT_FAILURE;
//     }

//     cpu_set_t cpuset;
//     sched_getaffinity(0, sizeof(cpuset), &cpuset);
//     num_cpus = CPU_COUNT(&cpuset);

//     size_t root_path_len = strlen(argv[1]);
//     while (argv[1][root_path_len - 1] == '/')
//         root_path_len--;

//     snprintf(root_path, sizeof(root_path), "%.*s", (int) root_path_len, argv[1]);

//     load_bootroms();

//     fclose(stderr); // close stderr to prevent error messages from the emulator to mess with the tests' output

//     setvbuf(stdout, NULL, _IONBF, 0);

//     printf(BOLD "---- TESTING ----\n" COLOR_OFF);
//     mkdir("results", 0744);
//     output_file = fopen("results/summary.txt.tmp", "w");

//     MagickWandGenesis();

//     pthread_t *threads = xcalloc(num_cpus, sizeof(*threads));
//     for (size_t i = 0; i < num_cpus; i++) {
//         if (pthread_create(&threads[i], NULL, run_tests, NULL)) {
//             errnoprintf("");
//             return EXIT_FAILURE;
//         }
//     }

//     for (size_t i = 0; i < num_cpus; i++)
//         pthread_join(threads[i], NULL);

//     free(threads);
//     MagickWandTerminus();

//     fclose(output_file);
//     rename("results/summary.txt.tmp", "results/summary.txt");

//     return EXIT_SUCCESS;
// }

#include <stdio.h>
#include <stdlib.h>
#include "../core/gba/gba_priv.h"

#define MAGIC 0xD33DBAE0

typedef enum {
    REG_IDX_USR_SYS = 0,
    REG_IDX_FIQ,
    REG_IDX_SVC,
    REG_IDX_ABT,
    REG_IDX_IRQ,
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

static size_t next_transaction = 0;
static size_t transactions_size = 0;
static gba_bus_transaction_t transactions[64];

uint8_t __wrap__gba_bus_read_byte(UNUSED gba_t *gba, int mode, uint32_t address) {
    bool is_same_addr = address == transactions[next_transaction].addr;
    bool is_read = transactions[next_transaction].kind != GBA_BUS_TRANSACTION_KIND_WRITE;
    bool is_same_size = transactions[next_transaction].size == 1;
    transactions[next_transaction].is_done = is_same_addr && is_read && is_same_size;

    return transactions[next_transaction++].data;
}

uint16_t __wrap__gba_bus_read_half(UNUSED gba_t *gba, int mode, uint32_t address) {
    bool is_same_addr = address == transactions[next_transaction].addr;
    bool is_read = transactions[next_transaction].kind != GBA_BUS_TRANSACTION_KIND_WRITE;
    bool is_same_size = transactions[next_transaction].size == 2;
    transactions[next_transaction].is_done = is_same_addr && is_read && is_same_size;

    return transactions[next_transaction++].data;
}

uint32_t __wrap__gba_bus_read_word(UNUSED gba_t *gba, int mode, uint32_t address) {
    bool is_same_addr = address == transactions[next_transaction].addr;
    bool is_read = transactions[next_transaction].kind != GBA_BUS_TRANSACTION_KIND_WRITE;
    bool is_same_size = transactions[next_transaction].size == 4;
    transactions[next_transaction].is_done = is_same_addr && is_read && is_same_size;

    return transactions[next_transaction++].data;
}

void __wrap__gba_bus_write_byte(UNUSED gba_t *gba, int mode, uint32_t address, uint8_t data) {
    bool is_same_addr = address == transactions[next_transaction].addr;
    bool is_same_data = data == transactions[next_transaction].data;
    bool is_write = transactions[next_transaction].kind == GBA_BUS_TRANSACTION_KIND_WRITE;
    bool is_same_size = transactions[next_transaction].size == 1;
    transactions[next_transaction++].is_done = is_same_addr && is_same_data && is_write && is_same_size;
}

void __wrap__gba_bus_write_half(UNUSED gba_t *gba, int mode, uint32_t address, uint16_t data) {
    bool is_same_addr = address == transactions[next_transaction].addr;
    bool is_same_data = data == transactions[next_transaction].data;
    bool is_write = transactions[next_transaction].kind == GBA_BUS_TRANSACTION_KIND_WRITE;
    bool is_same_size = transactions[next_transaction].size == 2;
    transactions[next_transaction++].is_done = is_same_addr && is_same_data && is_write && is_same_size;
}

void __wrap__gba_bus_write_word(UNUSED gba_t *gba, int mode, uint32_t address, uint32_t data) {
    bool is_same_addr = address == transactions[next_transaction].addr;
    bool is_same_data = data == transactions[next_transaction].data;
    bool is_write = transactions[next_transaction].kind == GBA_BUS_TRANSACTION_KIND_WRITE;
    bool is_same_size = transactions[next_transaction].size == 4;
    transactions[next_transaction++].is_done = is_same_addr && is_same_data && is_write && is_same_size;
}

static bool cpu_equals(gba_cpu_t *expected, gba_cpu_t *got, bool is_arm_str_ldr) {
    bool success = true;

    for (size_t i = 0; i < sizeof(expected->regs) / sizeof(*expected->regs); i++) {
        if (i == REG_PC && is_arm_str_ldr)
            continue;

        if (expected->regs[i] != got->regs[i]) {
            success = false;
            printf("R%zu expected 0x%08X, got 0x%08X\n", i, expected->regs[i], got->regs[i]);
        }
    }

    for (size_t i = REG_IDX_FIQ; i < sizeof(expected->banked_regs_8_12) / sizeof(*expected->banked_regs_8_12); i++) {
        for (size_t j = 0; j < sizeof(*expected->banked_regs_8_12) / sizeof(**expected->banked_regs_8_12); j++) {
            if (expected->banked_regs_8_12[i][j] != got->banked_regs_8_12[i][j]) {
                success = false;
                printf("R%zu (bank %zu) expected 0x%08X, got 0x%08X\n", j + 8, i, expected->banked_regs_8_12[i][j], got->banked_regs_8_12[i][j]);
            }
        }
    }

    for (size_t j = 0; j < sizeof(*expected->banked_regs_13_14) / sizeof(**expected->banked_regs_13_14); j++) {
        if (expected->banked_regs_13_14[REG_IDX_FIQ][j] != got->banked_regs_13_14[REG_IDX_FIQ][j]) {
            success = false;
            printf("R%zu (bank %zu) expected 0x%08X, got 0x%08X\n", j + 13, REG_IDX_FIQ, expected->banked_regs_13_14[REG_IDX_FIQ][j], got->banked_regs_13_14[REG_IDX_FIQ][j]);
        }
    }

    if (expected->cpsr != got->cpsr) {
        success = false;
        printf("CPSR expected 0x%08X, got 0x%08X\n", expected->cpsr, got->cpsr);
    }

    for (size_t i = 0; i < sizeof(expected->spsr) / sizeof(*expected->spsr); i++) {
        if (expected->spsr[i] != got->spsr[i]) {
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
    uint32_t ret = *((uint32_t *) *test_data);
    *test_data += sizeof(ret);
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

    cpu->cpsr = parse_u32(test_data);
    cpu->spsr[0] = cpu->cpsr;

    parse_u32_array(test_data, &cpu->spsr[1], 5);
    parse_u32_array(test_data, cpu->pipeline, 2);

    /* uint32_t access = */ parse_u32(test_data);
}

static void parse_transactions(uint8_t **test_data) {
    transactions_size = 0;
    next_transaction = 0;

    /* uint32_t full_sz = */ parse_u32(test_data);
    /* uint32_t magic = */ parse_u32(test_data);
    uint32_t num_transactions = parse_u32(test_data);

    for (uint32_t i = 0; i < num_transactions; i++) {
        transactions[transactions_size].kind = parse_u32(test_data);
        transactions[transactions_size].size = parse_u32(test_data);
        transactions[transactions_size].addr = parse_u32(test_data);
        transactions[transactions_size].data = parse_u32(test_data);
        transactions[transactions_size].cycle = parse_u32(test_data);
        transactions[transactions_size].access = parse_u32(test_data);

        transactions[transactions_size].is_done = false;
        transactions_size++;
    }
}

static void parse_opcodes(uint8_t **test_data, gba_cpu_t *cpu) {
    /* uint32_t full_sz = */ parse_u32(test_data);
    parse_u32(test_data); // ignore 4 bytes
    uint32_t opcode = parse_u32(test_data);
    /* uint32_t base_addr = */ parse_u32(test_data);

    cpu->pipeline[1] = opcode;
}

typedef struct {
    uint8_t rom[256];
    gba_t *init;
    gba_t *expected;
} gba_cpu_tester_t;

static gba_cpu_tester_t gba_cpu_tester = {.rom = { [0xB2] = 0x96 }};

static void gba_cpu_tester_init(void) {
    uint8_t checksum = 0;
    for (int i = 0xA0; i < 0xBC; i++)
        checksum -= gba_cpu_tester.rom[i];
    checksum -= 0x19;

    gba_cpu_tester.rom[0xBD] = checksum;

    gba_cpu_tester.init = gba_init(gba_cpu_tester.rom, sizeof(gba_cpu_tester.rom), NULL);
    gba_cpu_tester.expected = gba_init(gba_cpu_tester.rom, sizeof(gba_cpu_tester.rom), NULL);
}

static void gba_cpu_tester_quit(void) {
    gba_quit(gba_cpu_tester.init);
    gba_quit(gba_cpu_tester.expected);
}

static bool gba_cpu_tester_run(const char *path) {
    if (!path)
        return false;

    FILE *f = fopen(path, "r");
    if (!f) {
        errnoprintf("fopen");
        return false;
    }

    fseek(f, 0, SEEK_END);
    size_t len = ftell(f);
    rewind(f);

    uint8_t *test_data = xmalloc(len + 1);

    fread(test_data, 1, len, f);
    test_data[len] = '\0';
    fclose(f);

    uint8_t *test_data_ptr = test_data;

    uint32_t magic = parse_u32(&test_data_ptr);
    if (magic != MAGIC)
        return false;

    uint32_t num_tests = parse_u32(&test_data_ptr);
    printf("num_tests=%u\n", num_tests);


#define CPSR_MODE_MASK 0x0000001F // Mode bits
#define CPSR_GET_MODE(cpu) ((cpu)->cpsr & CPSR_MODE_MASK)

    bool is_arm_str_ldr = !strncmp(path, "ARM7TDMI/v1/arm_ldrh_strh.json.bin", 35)
                            || !strncmp(path, "ARM7TDMI/v1/arm_ldrsb_ldrsh.json.bin", 37)
                            || !strncmp(path, "ARM7TDMI/v1/arm_ldr_str_immediate_offset.json.bin", 50)
                            || !strncmp(path, "ARM7TDMI/v1/arm_ldr_str_register_offset.json.bin", 37)
                            || !strncmp(path, "ARM7TDMI/v1/arm_ldrsb_ldrsh.json.bin", 37);

    uint32_t errors = 0;
    for (uint32_t i = 0; i < num_tests; i++) {
        uint8_t *start_ptr = test_data_ptr;
        uint32_t full_sz = parse_u32(&test_data_ptr);

        parse_state(&test_data_ptr, gba_cpu_tester.init->cpu);
        parse_state(&test_data_ptr, gba_cpu_tester.expected->cpu);
        parse_transactions(&test_data_ptr);
        parse_opcodes(&test_data_ptr, gba_cpu_tester.init->cpu);

        uint8_t mode = CPSR_GET_MODE(gba_cpu_tester.init->cpu);
        uint8_t bank = regs_mode_hashes[mode & 0x0F];

        if (i < 6)
            continue;

        bank_registers(gba_cpu_tester.init->cpu, 0, mode); // from usr_sys mode to mode of current test

        gba_cpu_tester.init->cpu->pipeline_flush_cycles = 0;
        gba_cpu_step(gba_cpu_tester.init);

        while (gba_cpu_tester.init->cpu->pipeline_flush_cycles > 0)
            gba_cpu_step(gba_cpu_tester.init);

        mode = CPSR_GET_MODE(gba_cpu_tester.init->cpu);
        bank_registers(gba_cpu_tester.init->cpu, mode, 0); // go back to usr_sys mode

        // TODO when cpu sets cpsr, we shouldn't always (never?) mirror it to spsr[0]
        // ----> understand exactly when/where spsr is written

        if (cpu_equals(gba_cpu_tester.expected->cpu, gba_cpu_tester.init->cpu, is_arm_str_ldr) && check_transactions()) {
            printf("✅ CPU test passed (%u)!\n", i);
        } else {
            printf("❌ CPU state mismatch (%u)!\n", i);
            break;
            errors++;
        }
    }

    free(test_data);

    if (errors > 0) {
        printf("errors: %u/%u\n", errors, num_tests);
    }

    return errors == 0;
}

int main(int argc, char **argv) {
    gba_cpu_tester_init();

    static const char *test_paths[] = {
        // "ARM7TDMI/v1/arm_b_bl.json.bin", // OK
        // "ARM7TDMI/v1/arm_bx.json.bin", // OK
        // "ARM7TDMI/v1/arm_cdp.json.bin", // OK
        // "ARM7TDMI/v1/arm_data_proc_immediate.json.bin", // OK
        // "ARM7TDMI/v1/arm_data_proc_immediate_shift.json.bin", // OK
        // "ARM7TDMI/v1/arm_data_proc_register_shift.json.bin", // OK
        // "ARM7TDMI/v1/arm_ldm_stm.json.bin", // 0
        // "ARM7TDMI/v1/arm_ldrh_strh.json.bin", // OK
        // "ARM7TDMI/v1/arm_ldrsb_ldrsh.json.bin", // OK
        // "ARM7TDMI/v1/arm_ldr_str_immediate_offset.json.bin", // OK
        // "ARM7TDMI/v1/arm_ldr_str_register_offset.json.bin", // OK
        // "ARM7TDMI/v1/arm_mcr_mrc.json.bin", // OK
        "ARM7TDMI/v1/arm_mrs.json.bin", // OK
        "ARM7TDMI/v1/arm_msr_imm.json.bin", // OK
        "ARM7TDMI/v1/arm_msr_reg.json.bin", // OK
        // "ARM7TDMI/v1/arm_mull_mlal.json.bin", // 101
        // "ARM7TDMI/v1/arm_mul_mla.json.bin", // 100
        // "ARM7TDMI/v1/arm_stc_ldc.json.bin", // OK*
        // "ARM7TDMI/v1/arm_swi.json.bin", // OK
        // "ARM7TDMI/v1/arm_swp.json.bin", // OK
        // "ARM7TDMI/v1/thumb_add_cmp_mov_hi.json.bin", // OK
        // "ARM7TDMI/v1/thumb_add_sp_or_pc.json.bin", // OK
        // "ARM7TDMI/v1/thumb_add_sub.json.bin", // OK
        // "ARM7TDMI/v1/thumb_add_sub_sp.json.bin", // OK
        // "ARM7TDMI/v1/thumb_bcc.json.bin", // OK
        // "ARM7TDMI/v1/thumb_b.json.bin", // OK
        // "ARM7TDMI/v1/thumb_bl_blx_prefix.json.bin", // OK
        // "ARM7TDMI/v1/thumb_bl_suffix.json.bin", // OK
        // "ARM7TDMI/v1/thumb_bx.json.bin", // OK
        // "ARM7TDMI/v1/thumb_data_proc.json.bin", // 117
        // "ARM7TDMI/v1/thumb_ldm_stm.json.bin", // OK
        // "ARM7TDMI/v1/thumb_ldrb_strb_imm_offset.json.bin", // OK
        // "ARM7TDMI/v1/thumb_ldrh_strh_imm_offset.json.bin", // OK
        // "ARM7TDMI/v1/thumb_ldrh_strh_reg_offset.json.bin", // OK
        // "ARM7TDMI/v1/thumb_ldr_pc_rel.json.bin", // OK
        // "ARM7TDMI/v1/thumb_ldrsb_strb_reg_offset.json.bin", // OK
        // "ARM7TDMI/v1/thumb_ldrsh_ldrsb_reg_offset.json.bin", // OK
        // "ARM7TDMI/v1/thumb_ldr_str_imm_offset.json.bin", // OK
        // "ARM7TDMI/v1/thumb_ldr_str_reg_offset.json.bin", // OK
        // "ARM7TDMI/v1/thumb_ldr_str_sp_rel.json.bin", // OK
        // "ARM7TDMI/v1/thumb_lsl_lsr_asr.json.bin", // OK
        // "ARM7TDMI/v1/thumb_mov_cmp_add_sub.json.bin", // OK
        // "ARM7TDMI/v1/thumb_push_pop.json.bin", // OK
        // "ARM7TDMI/v1/thumb_swi.json.bin", // OK
        // "ARM7TDMI/v1/thumb_undefined_bcc.json.bin" // OK
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
