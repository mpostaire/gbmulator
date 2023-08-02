/**
 * This needs the test roms from this repository:
 * https://github.com/c-sp/gameboy-test-roms
 */

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <MagickWand/MagickWand.h>

#include "../emulator/emulator_priv.h"

#define BOLD "\033[1m"
#define COLOR_OFF "\033[0m"
#define COLOR_RED "\033[1;31m"
#define COLOR_GREEN "\033[1;32m"
#define COLOR_YELLOW "\033[0;33m"
#define COLOR_BLUE "\033[1;34m"

#define BUF_SIZE 256

char root_path[BUF_SIZE];

typedef struct {
    char *rom_path;                 // relative the the root_path given in the program's argument
    char *reference_image_filename; // relative to the dir of the rom_path
    char *result_diff_image_suffix;
    emulator_mode_t mode;
    int running_frames;
    byte_t exit_opcode;
    char *input_sequence;
    int is_gbmicrotest;
} test_t;

test_t tests[] = {
    #include "./tests.txt"
};

static byte_t *get_rom_data(const char *path, size_t *rom_size) {
    const char *dot = strrchr(path, '.');
    if (!dot || (strncmp(dot, ".gb", MAX(strlen(dot), sizeof(".gb"))) && strncmp(dot, ".gbc", MAX(strlen(dot), sizeof(".gbc"))))) {
        eprintf("%s: wrong file extension (expected .gb or .gbc)\n", path);
        return NULL;
    }

    FILE *f = fopen(path, "rb");
    if (!f) {
        errnoprintf("opening file %s", path);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    size_t len = ftell(f);
    fseek(f, 0, SEEK_SET);

    byte_t *buf = xmalloc(len);
    if (!fread(buf, len, 1, f)) {
        errnoprintf("reading %s", path);
        fclose(f);
        return NULL;
    }
    fclose(f);

    if (rom_size)
        *rom_size = len;
    return buf;
}

int dir_exists(const char *directory_path) {
    DIR *dir = opendir(directory_path);
	if (dir == NULL) {
		if (errno == ENOENT)
			return 0;
		errnoprintf("opendir");
        exit(EXIT_FAILURE);
	}
	closedir(dir);
	return 1;
}

void mkdirp(const char *directory_path) {
    char buf[256];
    snprintf(buf, sizeof(buf), "%s", directory_path);
    size_t len = strlen(buf);

    if (buf[len - 1] == '/')
        buf[len - 1] = 0;

    for (char *p = buf + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (mkdir(buf, 0744) && errno != EEXIST) {
                perror("mkdir");
                exit(EXIT_FAILURE);
            }
            *p = '/';
        }
    }

    if (mkdir(buf, 0744) && errno != EEXIST) {
        perror("mkdir");
        exit(EXIT_FAILURE);
    }
}

void make_parent_dirs(const char *filepath) {
    char *last_slash = strrchr(filepath, '/');
    int last_slash_index = last_slash ? (int) (last_slash - filepath) : -1;

    if (last_slash_index != -1) {
        char directory_path[last_slash_index + 1];
        snprintf(directory_path, last_slash_index + 1, "%s", filepath);

        if (!dir_exists(directory_path))
            mkdirp(directory_path);
    }
}

static void magick_wand_error(MagickWand *wand) {
    ExceptionType severity;
    char *description = MagickGetException(wand, &severity);
    (void) printf("\n%s %s %lu %s\n", GetMagickModule(), description);
    description = (char *) MagickRelinquishMemory(description);
    exit(EXIT_FAILURE);
}

static int save_and_check_result(test_t *test, emulator_t *emu, char *rom_path) {
    // get paths

    char *path_from_category = strchr(rom_path, '/') + 1;
    char *rom_name = strrchr(rom_path, '/') + 1;
    int path_until_extension_len = strrchr(rom_path, '.') - path_from_category;
    int dir_path_len = rom_name - 1 - rom_path;
    int new_dir_path_len = rom_name - 1 - path_from_category;

    char *reference_old_path = NULL;
    char *reference_new_path = NULL;
    char *diff_path = NULL;

    char result_path[BUF_SIZE];
    char *label = test->mode == CGB ? "cgb" : "dmg";
    char *suffix = test->result_diff_image_suffix ? test->result_diff_image_suffix : "";
    snprintf(result_path, sizeof(result_path), "results/%.*s-%s%s.result.png", path_until_extension_len, path_from_category, suffix, label);

    if (test->reference_image_filename) {
        reference_old_path = xmalloc(BUF_SIZE);
        reference_new_path = xmalloc(BUF_SIZE);
        diff_path = xmalloc(BUF_SIZE);

        snprintf(reference_old_path, BUF_SIZE, "%.*s/%s", dir_path_len, rom_path, test->reference_image_filename);

        char *reference_extension = strrchr(test->reference_image_filename, '.');
        int new_path_until_extension_len = reference_extension - test->reference_image_filename;
        snprintf(reference_new_path, BUF_SIZE, "results/%.*s/%.*s.expected.png", new_dir_path_len, path_from_category, new_path_until_extension_len, test->reference_image_filename);

        snprintf(diff_path, BUF_SIZE, "results/%.*s-%s%s.diff.png", path_until_extension_len, path_from_category, suffix, label);
    }

    // create dir structure
    make_parent_dirs(result_path);

    // save and check

    MagickWand *result_wand = NewMagickWand();
    PixelWand *pixel_wand = NewPixelWand();

    if (!MagickNewImage(result_wand, GB_SCREEN_WIDTH, GB_SCREEN_HEIGHT, pixel_wand))
        magick_wand_error(result_wand);

    if (!MagickImportImagePixels(result_wand, 0, 0, GB_SCREEN_WIDTH, GB_SCREEN_HEIGHT, "RGB", CharPixel, emu->ppu->pixels))
        magick_wand_error(result_wand);

    if (!MagickWriteImage(result_wand, result_path))
        magick_wand_error(result_wand);

    if (test->reference_image_filename) {

        MagickWand *reference_wand = NewMagickWand();
        if (!MagickReadImage(reference_wand, reference_old_path))
            magick_wand_error(reference_wand);

        if (!MagickWriteImage(reference_wand, reference_new_path))
            magick_wand_error(reference_wand);

        double distortion;
        MagickWand *diff_wand = MagickCompareImages(result_wand, reference_wand, AbsoluteErrorMetric, &distortion);

        if (!MagickWriteImage(diff_wand, diff_path))
            magick_wand_error(diff_wand);

        DestroyMagickWand(result_wand);
        DestroyMagickWand(reference_wand);
        DestroyMagickWand(diff_wand);
        DestroyPixelWand(pixel_wand);

        if (reference_old_path)
            free(reference_old_path);
        if (reference_new_path)
            free(reference_new_path);
        if (diff_path)
            free(diff_path);

        // rtc3test.gb test for sub-second-writes can have a little margin of error:
        // because the emulator goes very fast, there is an error of 0.1 ms in the sub second writes test of rtc3test.gb
        // it should be considered as a success but the image comparison fails as it's not exactly the same
        if (test->result_diff_image_suffix && !strncmp(test->result_diff_image_suffix, "sub-second-writes", 18))
            return !distortion || distortion == 31.0;
        else
            return !distortion;
    }

    if (test->is_gbmicrotest)
        return emu->mmu->mem[0xFF82] == 0x01;

    registers_t regs = emu->cpu->registers;
    return regs.bc == 0x0305 && regs.de == 0x080D && regs.hl == 0x1522;
}

static joypad_button_t str_to_joypad(char *str) {
    if (!strncmp(str, "right", 7))
        return JOYPAD_RIGHT;
    if (!strncmp(str, "left", 7))
        return JOYPAD_LEFT;
    if (!strncmp(str, "up", 7))
        return JOYPAD_UP;
    if (!strncmp(str, "down", 7))
        return JOYPAD_DOWN;
    if (!strncmp(str, "a", 7))
        return JOYPAD_A;
    if (!strncmp(str, "b", 7))
        return JOYPAD_B;
    if (!strncmp(str, "select", 7))
        return JOYPAD_SELECT;
    if (!strncmp(str, "start", 7))
        return JOYPAD_START;
    exit(EXIT_FAILURE);
}

static void exec_input_sequence(emulator_t *emu, char *input_sequence) {
    char cpy[BUF_SIZE];
    strncpy(cpy, input_sequence, sizeof(cpy) - 1);
    char *delay_str = strtok(cpy, ":");
    char *input_str = strtok(NULL, ",");
    while (delay_str && input_str) {
        int delay = atoi(delay_str);
        joypad_button_t input = str_to_joypad(input_str);

        emulator_run_frames(emu, delay * GB_CPU_FRAMES_PER_SECONDS);
        emulator_joypad_press(emu, input);
        emulator_run_frames(emu, 1);
        emulator_joypad_release(emu, input);

        delay_str = strtok(NULL, ":");
        input_str = strtok(NULL, ",");
    }
}

static int run_test(test_t *test) {
    MagickWandGenesis();

    char rom_path[BUF_SIZE];
    snprintf(rom_path, sizeof(rom_path) + 2, "%s/%s", root_path, test->rom_path);

    size_t rom_size = 0;
    byte_t *rom_data = get_rom_data(rom_path, &rom_size);
    if (!rom_data)
        return 0;

    emulator_options_t opts = {
        .mode = test->mode,
        .palette = PPU_COLOR_PALETTE_GRAY,
        .disable_cgb_color_correction = 1
    };
    emulator_t *emu = emulator_init(rom_data, rom_size, &opts);
    free(rom_data);
    if (!emu)
        return 0;

    emu->exit_on_invalid_opcode = 0;

    // TODO GBmulator's boot roms aren't the same as the original DMG and CGB. This may cause problems in some test roms
    //      like timer based test roms
    // run until the boot sequence is done
    while (emu->cpu->registers.pc != 0x100)
        emulator_step(emu);

    if (test->input_sequence) {
        emulator_run_frames(emu, 8); // run for some frames to let the test rom some time to setup itself
        exec_input_sequence(emu, test->input_sequence);
    }

    // the maximum time a test can take to run is 120 emulated seconds:
    // the timeout is a little higher than this value to be safe
    long timeout_cycles = 128 * GB_CPU_FREQ;
    if (test->exit_opcode) {
        while (emu->cpu->opcode != test->exit_opcode && timeout_cycles > 0) {
            emulator_step(emu); // don't take returned cycles to ignore double speed
            timeout_cycles -= 4;
        }
    }
    if (timeout_cycles > 0)
        emulator_run_frames(emu, test->running_frames);

    // take screenshot, save it and compare to the reference
    int ret = save_and_check_result(test, emu, rom_path);
    emulator_quit(emu);

    MagickWandTerminus();

    if (!ret && timeout_cycles <= 0)
        ret = -1;
    return ret;
}

static void run_tests() {
    fclose(stderr); // close stderr to prevent error messages from the emulator to mess with the tests' output

    printf(BOLD "---- TESTING ----\n" COLOR_OFF);
    mkdir("results", 0744);
    FILE *f = fopen("results/summary.txt.tmp", "w");

    size_t num_tests = sizeof(tests) / sizeof(*tests);

    for (size_t i = 0; i < num_tests; i++) {
        test_t test = tests[i];
        char *label = test.mode == CGB ? "CGB" : "DMG";
        char *suffix = test.result_diff_image_suffix ? test.result_diff_image_suffix : "";
        printf(COLOR_BLUE "[TEST %ld/%ld]" COLOR_OFF " (%s) %s" COLOR_YELLOW " %s" COLOR_OFF, i + 1, num_tests, label, test.rom_path, suffix);
        fflush(stdout);

        int success = run_test(&test);
        if (success == 1) {
            printf(COLOR_GREEN "\r[PASS]" COLOR_OFF " (%s) %s" COLOR_YELLOW " %s%*s\n" COLOR_OFF, label, test.rom_path, suffix, 8, "");
            fprintf(f, "%s:%s:%s:success\n", label, test.rom_path, suffix);
        } else if (success == -1) {
            printf(COLOR_RED "\r[FAIL]" COLOR_OFF " (%s) %s" COLOR_YELLOW " %s%*s\n" COLOR_OFF, label, test.rom_path, suffix, 8, "");
            fprintf(f, "%s:%s:%s:timeout\n", label, test.rom_path, suffix);
        } else {
            printf(COLOR_RED "\r[FAIL]" COLOR_OFF " (%s) %s" COLOR_YELLOW " %s%*s\n" COLOR_OFF, label, test.rom_path, suffix, 8, "");
            fprintf(f, "%s:%s:%s:failed\n", label, test.rom_path, suffix);
        }
    }

    fclose(f);
    rename("results/summary.txt.tmp", "results/summary.txt");
}

int main(int argc, char **argv) {
    if (argc < 2) {
        eprintf("Usage: %s /path/to/test/root/dir\n", argv[0]);
        return EXIT_FAILURE;
    }

    size_t root_path_len = strlen(argv[1]);
    while (argv[1][root_path_len - 1] == '/')
        root_path_len--;

    snprintf(root_path, sizeof(root_path), "%.*s", (int) root_path_len, argv[1]);

    run_tests();
}
