/**
 * This needs the test roms from this repository:
 * https://github.com/c-sp/gameboy-test-roms
 */

#include <stdlib.h>
#include <string.h>
#include <MagickWand/MagickWand.h>

#include "../emulator/emulator.h"

#define COLOR_OFF "\033[0m"
#define COLOR_RED "\033[0;31m"
#define COLOR_GREEN "\033[0;32m"
#define COLOR_YELLOW "\033[0;33m"
#define COLOR_BLUE "\033[0;34m"

#define BUF_SIZE 256

typedef struct {
    char *rom_path;                 // relative the the root_path given in the program's argument
    char *reference_image_filename; // relative to the dir of the rom_path
    emulator_mode_t mode;
    int time;
} screenshot_test_t;

// TODO make emulator options to disable color correction (otherwise CGB tests all fail)
// TODO I think the blargg cpu instrs tests for CGB need (STOP?) and double speed to have such low delay
screenshot_test_t screenshot_tests[] = {
    {"blargg/cgb_sound/cgb_sound.gb", "cgb_sound-cgb.png", CGB, 37},
    {"blargg/dmg_sound/dmg_sound.gb", "dmg_sound-dmg.png", DMG, 36},
    {"blargg/cpu_instrs/cpu_instrs.gb", "cpu_instrs-dmg-cgb.png", DMG, 55},
    {"blargg/cpu_instrs/cpu_instrs.gb", "cpu_instrs-dmg-cgb.png", CGB, 31},
    {"blargg/halt_bug.gb", "halt_bug-dmg-cgb.png", DMG, 2},
    {"blargg/halt_bug.gb", "halt_bug-dmg-cgb.png", CGB, 2},
    {"blargg/instr_timing/instr_timing.gb", "instr_timing-dmg-cgb.png", DMG, 1},
    {"blargg/instr_timing/instr_timing.gb", "instr_timing-dmg-cgb.png", CGB, 1},
    {"blargg/interrupt_time/interrupt_time.gb", "interrupt_time-dmg.png", DMG, 2},
    {"blargg/interrupt_time/interrupt_time.gb", "interrupt_time-cgb.png", CGB, 2},
    {"blargg/mem_timing/mem_timing.gb", "mem_timing-dmg-cgb.png", DMG, 3},
    {"blargg/mem_timing/mem_timing.gb", "mem_timing-dmg-cgb.png", CGB, 3},
    {"blargg/mem_timing-2/mem_timing.gb", "mem_timing-dmg-cgb.png", DMG, 4},
    {"blargg/mem_timing-2/mem_timing.gb", "mem_timing-dmg-cgb.png", CGB, 4},
    {"blargg/oam_bug/oam_bug.gb", "oam_bug-dmg.png", DMG, 21},
    {"blargg/oam_bug/oam_bug.gb", "oam_bug-cgb.png", CGB, 21} // TODO not sure of this time value
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

static void magick_wand_error(MagickWand *wand) {
    ExceptionType severity;
    char *description = MagickGetException(wand, &severity);
    (void) printf("%s %s %lu %s\n", GetMagickModule(), description);
    description = (char *) MagickRelinquishMemory(description);
    exit(EXIT_FAILURE);
}

static int save_and_check_result(screenshot_test_t *test, byte_t *pixels, char *rom_path) {
    // get paths

    char *rom_extension = strrchr(rom_path, '.');

    char result_path[BUF_SIZE];
    snprintf(result_path, sizeof(result_path), "%.*s-%s.result.png", (int) (rom_extension - rom_path), rom_path, test->mode == CGB ? "cgb" : "dmg");

    char reference_path[BUF_SIZE];
    char *rom_name = strrchr(rom_path, '/');
    snprintf(reference_path, sizeof(reference_path), "%.*s/%s", (int) (rom_name - rom_path), rom_path, test->reference_image_filename);

    char diff_path[BUF_SIZE];
    snprintf(diff_path, sizeof(diff_path), "%.*s-%s.diff.png", (int) (rom_extension - rom_path), rom_path, test->mode == CGB ? "cgb" : "dmg");

    // save and check

    MagickWand *result_wand = NewMagickWand();
    PixelWand *pixel_wand = NewPixelWand();

    if (!MagickNewImage(result_wand, GB_SCREEN_WIDTH, GB_SCREEN_HEIGHT, pixel_wand))
        magick_wand_error(result_wand);

    if (!MagickImportImagePixels(result_wand, 0, 0, GB_SCREEN_WIDTH, GB_SCREEN_HEIGHT, "RGB", CharPixel, pixels))
        magick_wand_error(result_wand);

    if (!MagickWriteImage(result_wand, result_path))
        magick_wand_error(result_wand);

    MagickWand *reference_wand = NewMagickWand();
    if (!MagickReadImage(reference_wand, reference_path))
        magick_wand_error(reference_wand);

    double distortion;
    MagickWand *diff_wand = MagickCompareImages(result_wand, reference_wand, AbsoluteErrorMetric, &distortion);

    if (!MagickWriteImage(diff_wand, diff_path))
        magick_wand_error(diff_wand);

    DestroyMagickWand(result_wand);
    DestroyMagickWand(reference_wand);
    DestroyMagickWand(diff_wand);
    DestroyPixelWand(pixel_wand);

    return !distortion;
}

static int run_screenshot_test(screenshot_test_t *test, char *rom_path) {
    MagickWandGenesis();

    size_t rom_size = 0;
    byte_t *rom_data = get_rom_data(rom_path, &rom_size);
    if (!rom_data)
        return 0;

    emulator_options_t opts = {
        .mode = test->mode,
        .skip_boot = 1,
        .palette = PPU_COLOR_PALETTE_GRAY,
        .disable_cgb_color_correction = 1
    };
    emulator_t *emu = emulator_init(rom_data, rom_size, &opts);
    free(rom_data);
    if (!emu)
        return 0;

    emulator_run_cycles(emu, test->time * GB_CPU_FREQ);

    // take screenshot, save it and compare to the reference
    int ret = save_and_check_result(test, emu->ppu->pixels, rom_path);
    emulator_quit(emu);

    // compare_with_expected_image();
    MagickWandTerminus();

    return ret;
}

static void run_tests(char *root_path) {
    fclose(stderr); // close stderr to prevent error messages from the emulator to mess with the tests' output

    printf("---- TESTING ----\n");

    char root_path_last_char = root_path[strlen(root_path) - 1];
    char *path_separator = root_path_last_char == '/' ? "" : "/";
    char rom_path[BUF_SIZE];
    size_t num_tests = sizeof(screenshot_tests) / sizeof(*screenshot_tests);
    int succeeded = 0;

    for (size_t i = 0; i < num_tests; i++) {
        snprintf(rom_path, sizeof(rom_path), "%s%s%s", root_path, path_separator, screenshot_tests[i].rom_path);

        printf(COLOR_BLUE "[TEST]" COLOR_OFF " (%s) %s", screenshot_tests[i].mode == CGB ? "CGB" : "DMG", rom_path);
        fflush(stdout);

        int success = run_screenshot_test(&screenshot_tests[i], rom_path);
        if (success)
            printf(COLOR_GREEN "\r[PASS]" COLOR_OFF " (%s) %s\n", screenshot_tests[i].mode == CGB ? "CGB" : "DMG", rom_path);
        else
            printf(COLOR_RED "\r[FAIL]" COLOR_OFF " (%s) %s\n", screenshot_tests[i].mode == CGB ? "CGB" : "DMG", rom_path);

        succeeded += success;
    }

    printf("---- SUMMARY ----\n");
    printf("Passed %d/%ld tests (%d%%)\n", succeeded, num_tests, (int) ((succeeded / (float) num_tests) * 100.0f));
}

int main(int argc, char **argv) {
    if (argc < 2) {
        eprintf("Usage: %s /path/to/test/root/dir\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *root_path = argv[1];
    run_tests(root_path);
}
