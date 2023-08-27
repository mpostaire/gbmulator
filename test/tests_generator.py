#!/usr/bin/env python3

import sys
import os
import glob
import re

blargg_times = {
    "DMG": {
        "cpu_instrs.gb": 55 * 60,
        "dmg_sound.gb": 36 * 60,
        "halt_bug.gb": 2 * 60,
        "instr_timing.gb": 1 * 60,
        "interrupt_time.gb": 2 * 60,
        "mem_timing.gb": 4 * 60,
        "oam_bug.gb": 21 * 60
    },
    "CGB": {
        "cgb_sound.gb": 37 * 60,
        "cpu_instrs.gb": 31 * 60,
        "halt_bug.gb": 2 * 60,
        "instr_timing.gb": 1 * 60,
        "interrupt_time.gb": 2 * 60,
        "mem_timing.gb": 4 * 60,
        "oam_bug.gb": 21 * 60
    }
}


def path_remove_first_elements(path, tests_root):
    ret = path.split(tests_root)[1]
    return ret[1:] if ret[0] == os.sep else ret

def generate_acid_tests(output_file):
    output_file.write('{"cgb-acid-hell/cgb-acid-hell.gbc", "cgb-acid-hell.png", NULL, CGB, 0, 0x40, NULL, 0},\n')
    output_file.write('{"cgb-acid2/cgb-acid2.gbc", "cgb-acid2.png", NULL, CGB, 0, 0x40, NULL, 0},\n')
    output_file.write('{"dmg-acid2/dmg-acid2.gb", "dmg-acid2-dmg.png", NULL, DMG, 0, 0x40, NULL, 0},\n')
    output_file.write('{"dmg-acid2/dmg-acid2.gb", "dmg-acid2-cgb.png", NULL, CGB, 0, 0x40, NULL, 0},\n')

def generate_rtc3tests(output_file):
    output_file.write('{"rtc3test/rtc3test.gb", "rtc3test-basic-tests-dmg.png", "basic-tests", DMG, 780, 0, "1:a", 0},\n')
    output_file.write('{"rtc3test/rtc3test.gb", "rtc3test-basic-tests-cgb.png", "basic-tests", CGB, 780, 0, "1:a", 0},\n')
    output_file.write('{"rtc3test/rtc3test.gb", "rtc3test-range-tests-dmg.png", "range-tests", DMG, 480, 0, "1:down,1:a", 0},\n')
    output_file.write('{"rtc3test/rtc3test.gb", "rtc3test-range-tests-cgb.png", "range-tests", CGB, 480, 0, "1:down,1:a", 0},\n')
    output_file.write('{"rtc3test/rtc3test.gb", "rtc3test-sub-second-writes-dmg.png", "sub-second-writes", DMG, 1560, 0, "1:down,1:down,1:a", 0},\n')
    output_file.write('{"rtc3test/rtc3test.gb", "rtc3test-sub-second-writes-cgb.png", "sub-second-writes", CGB, 1560, 0, "1:down,1:down,1:a", 0},\n')

def generate_bully_tests(output_file):
    output_file.write('{"bully/bully.gb", "bully.png", NULL, DMG, 60, 0, NULL, 0},\n')
    output_file.write('{"bully/bully.gb", "bully.png", NULL, CGB, 60, 0, NULL, 0},\n')

def generate_little_things_tests(output_file):
    output_file.write('{"little-things-gb/firstwhite.gb", "firstwhite-dmg-cgb.png", NULL, DMG, 60, 0, NULL, 0},\n')
    output_file.write('{"little-things-gb/firstwhite.gb", "firstwhite-dmg-cgb.png", NULL, CGB, 60, 0, NULL, 0},\n')
    output_file.write('{"little-things-gb/tellinglys.gb", "tellinglys-dmg.png", NULL, DMG, 300, 0, "1:right,1:left,1:up,1:down,1:a,1:b,1:select,1:start", 0},\n')
    output_file.write('{"little-things-gb/tellinglys.gb", "tellinglys-cgb.png", NULL, CGB, 300, 0, "1:right,1:left,1:up,1:down,1:a,1:b,1:select,1:start", 0},\n')

def generate_strikethrough_tests(output_file):
    output_file.write('{"strikethrough/strikethrough.gb", "strikethrough-dmg.png", NULL, DMG, 60, 0, NULL, 0},\n')
    output_file.write('{"strikethrough/strikethrough.gb", "strikethrough-cgb.png", NULL, CGB, 60, 0, NULL, 0},\n')


def generate_tests(tests_root, category, max_depth, output_file,
                   reference_image_getter=None, screenshot_test_generator=None,
                   internal_state_test_generator=None):
    output = []
    category_path = os.path.join(tests_root, category)
    for path, subdirs, files in os.walk(category_path):
        if path[len(category_path):].count(os.sep) > max_depth:
            continue
        for name in files:
            name_and_extension = os.path.splitext(name)
            if name_and_extension[1] != ".gb" and name_and_extension[1] != ".gbc":
                continue

            full_rom_path = os.path.join(path, name)
            rom_path = path_remove_first_elements(full_rom_path, tests_root)

            if reference_image_getter is None:
                if internal_state_test_generator is not None:
                    tests = internal_state_test_generator(rom_path)
                    for test in tests:
                        output.append(test)
                    continue
                continue

            dmg_ref_images = reference_image_getter("DMG", full_rom_path)
            cgb_ref_images = reference_image_getter("CGB", full_rom_path)

            if not dmg_ref_images and not cgb_ref_images:
                # parse internal state test
                if internal_state_test_generator is not None:
                    tests = internal_state_test_generator(rom_path)
                    for test in tests:
                        output.append(test)
            elif screenshot_test_generator is not None:
                # parse screenshot test
                for image in dmg_ref_images:
                    test = screenshot_test_generator(
                        "DMG", rom_path, os.path.basename(image))
                    if test is not None:
                        output.append(test)
                for image in cgb_ref_images:
                    test = screenshot_test_generator(
                        "CGB", rom_path, os.path.basename(image))
                    if test is not None:
                        output.append(test)

    output_file.writelines(sorted(output))


def blargg_reference_image_getter(mode, full_rom_path):
    rom_name = os.path.basename(full_rom_path)
    rom_dir = os.path.dirname(full_rom_path)
    # full_rom_dir = os.path.dirname(full_rom_path)
    image_glob = f'{os.path.splitext(rom_name)[0]}*{mode.lower()}*.png'
    image_glob = os.path.join(rom_dir, image_glob)
    return glob.glob(image_glob)

def blargg_screenshot_test_generator(mode, rom_path, reference_image_path):
    return f'{{"{rom_path}", "{reference_image_path}", NULL, {mode}, {blargg_times[mode][os.path.basename(rom_path)]}, 0, NULL, 0}},\n'


def age_reference_image_getter(mode, full_rom_path):
    if mode == "DMG":
        ret = glob.glob(f'{os.path.splitext(full_rom_path)[0]}-dmg*C*.png')
        return ret
    else:
        ret = glob.glob(f'{os.path.splitext(full_rom_path)[0]}-cgb*C*.png')
        ret += glob.glob(f'{os.path.splitext(full_rom_path)[0]}-ncm*C*.png')
        return ret

def age_screenshot_test_generator(mode, rom_path, reference_image_path):
    return f'{{"{rom_path}", "{reference_image_path}", NULL, {mode}, 0, 0x40, NULL, 0}},\n'

def age_internal_state_test_generator(rom_path):
    rom_name = os.path.basename(rom_path)
    ret = []
    if re.match(".*-dmg.*C.*.gb$", rom_name):
        ret.append(f'{{"{rom_path}", NULL, NULL, DMG, 60, 0x40, NULL, 0}},\n')
    if re.match(".*-cgb.*C.*.gb$", rom_name):
        ret.append(f'{{"{rom_path}", NULL, NULL, CGB, 60, 0x40, NULL, 0}},\n')
    if re.match(".*-ncm.*C.*.gb$", rom_name):
        ret.append(f'{{"{rom_path}", NULL, NULL, CGB, 60, 0x40, NULL, 0}},\n')
    return ret


def mooneye_screenshot_test_generator(mode, rom_path, reference_image_path):
    return f'{{"{rom_path}", "{reference_image_path}", NULL, {mode}, 0, 0x40, NULL, 0}},\n'

def mooneye_internal_state_test_generator(rom_path):
    ret = []
    if "madness" in rom_path or "utils" in rom_path:
        return ret
    rom_name = os.path.basename(rom_path)
    if re.match(".*(?:-S|-A|-dmg0|-mgb|-sgb|-sgb2|-cgb0)\.gb$", rom_name):
        return ret
    if re.match(".*(?:-C|-cgb.*C.*|-cgb)\.gb$", rom_name):
        ret.append(f'{{"{rom_path}", NULL, NULL, CGB, 0, 0x40, NULL, 0}},\n')
    if re.match(".*(?:-G.*|-dmg.*C.*)\.gb$", rom_name):
        ret.append(f'{{"{rom_path}", NULL, NULL, DMG, 0, 0x40, NULL, 0}},\n')
    if not ret:
        return [f'{{"{rom_path}", NULL, NULL, {mode}, 0, 0x40, NULL, 0}},\n' for mode in ["DMG", "CGB"]]
    return ret


def mooneye_wilbertpol_internal_state_test_generator(rom_path):
    ret = []
    if "acceptance/gpu" not in rom_path and "timer/timer_if" not in rom_path:
        return ret
    rom_name = os.path.basename(rom_path)
    if re.match(".*(?:-S|-A|-dmg0|-mgb|-sgb|-sgb2|-cgb0)\.gb$", rom_name):
        return ret
    if re.match(".*(?:-C|-cgb.*C.*|-cgb)\.gb$", rom_name):
        ret.append(f'{{"{rom_path}", NULL, NULL, CGB, 0, 0xED, NULL, 0}},\n')
    if re.match(".*(?:-G.*|-dmg.*C.*)\.gb$", rom_name):
        ret.append(f'{{"{rom_path}", NULL, NULL, DMG, 0, 0xED, NULL, 0}},\n')
    if not ret:
        return [f'{{"{rom_path}", NULL, NULL, {mode}, 0, 0xED, NULL, 0}},\n' for mode in ["DMG", "CGB"]]
    return ret


def mealybug_reference_image_getter(mode, full_rom_path):
    rom_name = os.path.basename(full_rom_path)
    rom_dir = os.path.dirname(full_rom_path)
    # full_rom_dir = os.path.dirname(full_rom_path)
    image_glob = f'{os.path.splitext(rom_name)[0]}_{"dmg_blob" if mode == "DMG" else "cgb_c"}*.png'
    image_glob = os.path.join(rom_dir, image_glob)
    return glob.glob(image_glob)

def mealybug_screenshot_test_generator(mode, rom_path, reference_image_path):
    return f'{{"{rom_path}", "{reference_image_path}", NULL, {mode}, 0, 0x40, NULL, 0}},\n'

def mealybug_internal_state_test_generator(rom_path):
    if "dma/hdma" in rom_path:
        return [f'{{"{rom_path}", NULL, NULL, CGB, 0, 0x40, NULL, 0}},\n']
    return [f'{{"{rom_path}", NULL, NULL, {mode}, 0, 0x40, NULL, 0}},\n' for mode in ["DMG", "CGB"]]


def same_internal_state_test_generator(rom_path):
    # CPU-CGB-C fail most tests (useless to test them)
    ret = []
    if "same-suite/sgb/" in rom_path or "apu/channel_1" in rom_path or "apu/channel_2" in rom_path or "apu/channel_4" in rom_path:
        return ret
    if "same-suite/apu" in rom_path:
        rom_name = os.path.basename(rom_path)
        if "-cgb0" in rom_name or "-cgbB" in rom_name:
            return ret
        if "apu/div_write_trigger" in rom_path or "apu/div_write_trigger_10" in rom_path:
            ret.append(f'{{"{rom_path}", NULL, NULL, DMG, 0, 0x40, NULL, 0}},\n')
        ret.append(f'{{"{rom_path}", NULL, NULL, CGB, 0, 0x40, NULL, 0}},\n')
    elif "same-suite/dma" in rom_path:
        ret.append(f'{{"{rom_path}", NULL, NULL, CGB, 0, 0x40, NULL, 0}},\n')
    else:
        ret.append(f'{{"{rom_path}", NULL, NULL, DMG, 0, 0x40, NULL, 0}},\n')
        ret.append(f'{{"{rom_path}", NULL, NULL, CGB, 0, 0x40, NULL, 0}},\n')

    return ret


def gbmicrotest_internal_state_test_generator(rom_path):
    if "is_if_set_during_ime0.gb" in rom_path:
        return [f'{{"{rom_path}", NULL, NULL, DMG, 60, 0, NULL, 1}},\n']
    return [f'{{"{rom_path}", NULL, NULL, DMG, 2, 0, NULL, 1}},\n']


def main():
    if len(sys.argv) != 2:
        print("No test root dir specified")
        return

    tests_root = sys.argv[1]

    # the aim is to implement DMG-CPU-C and CGB-CPU-C (because they have the most test roms compatible)
    # the generated tests filter out all the test roms that are not made for these models

    # TODO gambatte (need a different test implementation to work)
    with open(os.path.join(os.path.dirname(__file__), "tests.txt"), "w") as f:
        generate_tests(tests_root, "blargg", 1, f, blargg_reference_image_getter, blargg_screenshot_test_generator)
        generate_tests(tests_root, "age-test-roms", 1, f, age_reference_image_getter, age_screenshot_test_generator, age_internal_state_test_generator)
        generate_tests(tests_root, "mooneye-test-suite", 2, f, blargg_reference_image_getter, mooneye_screenshot_test_generator, mooneye_internal_state_test_generator)
        generate_tests(tests_root, "mooneye-test-suite-wilbertpol", 2, f, None, None, mooneye_wilbertpol_internal_state_test_generator)
        generate_tests(tests_root, "mealybug-tearoom-tests", 1, f, mealybug_reference_image_getter, mealybug_screenshot_test_generator, mealybug_internal_state_test_generator)
        generate_tests(tests_root, "same-suite", 2, f, None, None, same_internal_state_test_generator)
        # generate_tests(tests_root, "gbmicrotest", 1, f, None, None, gbmicrotest_internal_state_test_generator)
        generate_bully_tests(f)
        generate_acid_tests(f)
        generate_little_things_tests(f)
        generate_rtc3tests(f)
        generate_strikethrough_tests(f)


if __name__ == "__main__":
    main()
