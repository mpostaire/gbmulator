#!/usr/bin/env python3

import sys
import os
import glob
import re

blargg_times = {
    "GBMULATOR_MODE_GB": {
        "cpu_instrs.gb": 55 * 1000,
        "dmg_sound.gb": 36 * 1000,
        "halt_bug.gb": 2 * 1000,
        "instr_timing.gb": 1 * 1000,
        "interrupt_time.gb": 2 * 1000,
        "mem_timing.gb": 4 * 1000,
        "oam_bug.gb": 21 * 1000
    },
    "GBMULATOR_MODE_GBC": {
        "cgb_sound.gb": 37 * 1000,
        "cpu_instrs.gb": 31 * 1000,
        "halt_bug.gb": 2 * 1000,
        "instr_timing.gb": 1 * 1000,
        "interrupt_time.gb": 2 * 1000,
        "mem_timing.gb": 4 * 1000,
        "oam_bug.gb": 21 * 1000
    }
}


def path_remove_first_elements(path, tests_root):
    ret = path.split(tests_root)[1]
    return ret[1:] if ret[0] == os.sep else ret

def generate_acid_tests(output_file):
    output_file.write('{"cgb-acid-hell/cgb-acid-hell.gbc", "cgb-acid-hell/cgb-acid-hell.png", NULL, GBMULATOR_MODE_GBC, 0, 0x40, NULL, 0},\n')
    output_file.write('{"cgb-acid2/cgb-acid2.gbc", "cgb-acid2/cgb-acid2.png", NULL, GBMULATOR_MODE_GBC, 0, 0x40, NULL, 0},\n')
    output_file.write('{"dmg-acid2/dmg-acid2.gb", "dmg-acid2/dmg-acid2-dmg.png", NULL, GBMULATOR_MODE_GB, 0, 0x40, NULL, 0},\n')
    output_file.write('{"dmg-acid2/dmg-acid2.gb", "dmg-acid2/dmg-acid2-cgb.png", NULL, GBMULATOR_MODE_GBC, 0, 0x40, NULL, 0},\n')

def generate_rtc3tests(output_file):
    output_file.write('{"rtc3test/rtc3test.gb", "rtc3test/rtc3test-basic-tests-dmg.png", "basic-tests", GBMULATOR_MODE_GB, 13000, 0, "1:a", 0},\n')
    output_file.write('{"rtc3test/rtc3test.gb", "rtc3test/rtc3test-basic-tests-cgb.png", "basic-tests", GBMULATOR_MODE_GBC, 13000, 0, "1:a", 0},\n')
    output_file.write('{"rtc3test/rtc3test.gb", "rtc3test/rtc3test-range-tests-dmg.png", "range-tests", GBMULATOR_MODE_GB, 8000, 0, "1:down,1:a", 0},\n')
    output_file.write('{"rtc3test/rtc3test.gb", "rtc3test/rtc3test-range-tests-cgb.png", "range-tests", GBMULATOR_MODE_GBC, 8000, 0, "1:down,1:a", 0},\n')
    output_file.write('{"rtc3test/rtc3test.gb", "rtc3test/rtc3test-sub-second-writes-dmg.png", "sub-second-writes", GBMULATOR_MODE_GB, 26000, 0, "1:down,1:down,1:a", 0},\n')
    output_file.write('{"rtc3test/rtc3test.gb", "rtc3test/rtc3test-sub-second-writes-cgb.png", "sub-second-writes", GBMULATOR_MODE_GBC, 26000, 0, "1:down,1:down,1:a", 0},\n')

def generate_bully_tests(output_file):
    output_file.write('{"bully/bully.gb", "bully/bully.png", NULL, GBMULATOR_MODE_GB, 500, 0, NULL, 0},\n')
    output_file.write('{"bully/bully.gb", "bully/bully.png", NULL, GBMULATOR_MODE_GBC, 500, 0, NULL, 0},\n')

def generate_little_things_tests(output_file):
    output_file.write('{"little-things-gb/firstwhite.gb", "little-things-gb/firstwhite-dmg-cgb.png", NULL, GBMULATOR_MODE_GB, 500, 0, NULL, 0},\n')
    output_file.write('{"little-things-gb/firstwhite.gb", "little-things-gb/firstwhite-dmg-cgb.png", NULL, GBMULATOR_MODE_GBC, 500, 0, NULL, 0},\n')
    output_file.write('{"little-things-gb/tellinglys.gb", "little-things-gb/tellinglys-dmg.png", NULL, GBMULATOR_MODE_GB, 5000, 0, "1:right,1:left,1:up,1:down,1:a,1:b,1:select,1:start", 0},\n')
    output_file.write('{"little-things-gb/tellinglys.gb", "little-things-gb/tellinglys-cgb.png", NULL, GBMULATOR_MODE_GBC, 5000, 0, "1:right,1:left,1:up,1:down,1:a,1:b,1:select,1:start", 0},\n')

def generate_strikethrough_tests(output_file):
    output_file.write('{"strikethrough/strikethrough.gb", "strikethrough/strikethrough-dmg.png", NULL, GBMULATOR_MODE_GB, 500, 0, NULL, 0},\n')
    output_file.write('{"strikethrough/strikethrough.gb", "strikethrough/strikethrough-cgb.png", NULL, GBMULATOR_MODE_GBC, 500, 0, NULL, 0},\n')

def generate_other_tests(output_file):
    output_file.write('{"other/windesync-validate/windesync-validate.gb", "other/windesync-validate/windesync-reference-sgb.png", NULL, GBMULATOR_MODE_GB, 80, 0, NULL, 0},\n')


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

            a = reference_image_getter("GBMULATOR_MODE_GB", full_rom_path)

            dmg_ref_images = ["/".join(x.split("/")[1:]) for x in a]
            cgb_ref_images = ["/".join(x.split("/")[1:]) for x in reference_image_getter("GBMULATOR_MODE_GBC", full_rom_path)]

            if not dmg_ref_images and not cgb_ref_images:
                # parse internal state test
                if internal_state_test_generator is not None:
                    tests = internal_state_test_generator(rom_path)
                    for test in tests:
                        output.append(test)
            elif screenshot_test_generator is not None:
                # parse screenshot test
                for image in dmg_ref_images:
                    test = screenshot_test_generator("GBMULATOR_MODE_GB", rom_path, image)
                    if test is not None:
                        output.append(test)
                for image in cgb_ref_images:
                    test = screenshot_test_generator("GBMULATOR_MODE_GBC", rom_path, image)
                    if test is not None:
                        output.append(test)

    output_file.writelines(output)


def blargg_reference_image_getter(mode, full_rom_path):
    rom_name = os.path.basename(full_rom_path)
    rom_dir = os.path.dirname(full_rom_path)
    # full_rom_dir = os.path.dirname(full_rom_path)
    image_glob = f'{os.path.splitext(rom_name)[0]}*{mode.lower()[-3:]}*.png'
    image_glob = os.path.join(rom_dir, image_glob)
    return glob.glob(image_glob)

def blargg_screenshot_test_generator(mode, rom_path, reference_image_path):
    return f'{{"{rom_path}", "{reference_image_path}", NULL, {mode}, {blargg_times[mode][os.path.basename(rom_path)]}, 0, NULL, 0}},\n'


def age_reference_image_getter(mode, full_rom_path):
    if mode == "GBMULATOR_MODE_GB":
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
    if re.match(r".*-dmg.*C.*.gb$", rom_name):
        ret.append(f'{{"{rom_path}", NULL, NULL, GBMULATOR_MODE_GB, 0, 0x40, NULL, 0}},\n')
    if re.match(r".*-cgb.*C.*.gb$", rom_name):
        ret.append(f'{{"{rom_path}", NULL, NULL, GBMULATOR_MODE_GBC, 0, 0x40, NULL, 0}},\n')
    if re.match(r".*-ncm.*C.*.gb$", rom_name):
        ret.append(f'{{"{rom_path}", NULL, NULL, GBMULATOR_MODE_GBC, 0, 0x40, NULL, 0}},\n')
    return ret


def mooneye_reference_image_getter(mode, full_rom_path):
    rom_name = os.path.basename(full_rom_path)
    rom_dir = os.path.dirname(full_rom_path)
    # full_rom_dir = os.path.dirname(full_rom_path)
    image_glob = f'{os.path.splitext(rom_name)[0]}-*{"dmg" if mode == "GBMULATOR_MODE_GB" else "cgb"}.png'
    image_glob = os.path.join(rom_dir, image_glob)
    return glob.glob(image_glob)

def mooneye_screenshot_test_generator(mode, rom_path, reference_image_path):
    return f'{{"{rom_path}", "{reference_image_path}", NULL, {mode}, 0, 0x40, NULL, 0}},\n'

def mooneye_internal_state_test_generator(rom_path):
    ret = []
    if "madness" in rom_path or "utils" in rom_path:
        return ret
    rom_name = os.path.basename(rom_path)
    if re.match(r".*(?:-S|-A|-dmg0|-mgb|-sgb|-sgb2|-cgb0)\.gb$", rom_name):
        return ret
    if re.match(r".*(?:-C|-cgb.*C.*|-cgb)\.gb$", rom_name):
        ret.append(f'{{"{rom_path}", NULL, NULL, GBMULATOR_MODE_GBC, 0, 0x40, NULL, 0}},\n')
    if re.match(r".*(?:-G.*|-dmg.*C.*)\.gb$", rom_name):
        ret.append(f'{{"{rom_path}", NULL, NULL, GBMULATOR_MODE_GB, 0, 0x40, NULL, 0}},\n')
    if not ret:
        return [f'{{"{rom_path}", NULL, NULL, {mode}, 0, 0x40, NULL, 0}},\n' for mode in ["GBMULATOR_MODE_GB", "GBMULATOR_MODE_GBC"]]
    return ret


def mooneye_wilbertpol_internal_state_test_generator(rom_path):
    ret = []
    if "acceptance/gpu" not in rom_path and "timer/timer_if" not in rom_path:
        return ret
    rom_name = os.path.basename(rom_path)
    if re.match(r".*(?:-S|-A|-dmg0|-mgb|-sgb|-sgb2|-cgb0)\.gb$", rom_name):
        return ret
    if re.match(r".*(?:-C|-cgb.*C.*|-cgb)\.gb$", rom_name):
        ret.append(f'{{"{rom_path}", NULL, NULL, GBMULATOR_MODE_GBC, 0, 0xED, NULL, 0}},\n')
    if re.match(r".*(?:-G.*|-dmg.*C.*)\.gb$", rom_name):
        ret.append(f'{{"{rom_path}", NULL, NULL, GBMULATOR_MODE_GB, 0, 0xED, NULL, 0}},\n')
    if not ret:
        return [f'{{"{rom_path}", NULL, NULL, {mode}, 0, 0xED, NULL, 0}},\n' for mode in ["GBMULATOR_MODE_GB", "GBMULATOR_MODE_GBC"]]
    return ret


def mealybug_reference_image_getter(mode, full_rom_path):
    rom_name = os.path.basename(full_rom_path)
    rom_dir = os.path.dirname(full_rom_path)
    # full_rom_dir = os.path.dirname(full_rom_path)
    image_glob = f'{os.path.splitext(rom_name)[0]}_{"dmg_blob" if mode == "GBMULATOR_MODE_GB" else "cgb_c"}*.png'
    image_glob = os.path.join(rom_dir, image_glob)
    return glob.glob(image_glob)

def mealybug_screenshot_test_generator(mode, rom_path, reference_image_path):
    return f'{{"{rom_path}", "{reference_image_path}", NULL, {mode}, 0, 0x40, NULL, 0}},\n'

def mealybug_internal_state_test_generator(rom_path):
    if "dma/hdma" in rom_path:
        return [f'{{"{rom_path}", NULL, NULL, GBMULATOR_MODE_GBC, 0, 0x40, NULL, 0}},\n']
    return [f'{{"{rom_path}", NULL, NULL, {mode}, 0, 0x40, NULL, 0}},\n' for mode in ["GBMULATOR_MODE_GB", "GBMULATOR_MODE_GBC"]]


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
            ret.append(f'{{"{rom_path}", NULL, NULL, GBMULATOR_MODE_GB, 0, 0x40, NULL, 0}},\n')
        ret.append(f'{{"{rom_path}", NULL, NULL, GBMULATOR_MODE_GBC, 0, 0x40, NULL, 0}},\n')
    elif "same-suite/dma" in rom_path:
        ret.append(f'{{"{rom_path}", NULL, NULL, GBMULATOR_MODE_GBC, 0, 0x40, NULL, 0}},\n')
    elif "same-suite/ppu/blocking_bgpi_increase" in rom_path:
        ret.append(f'{{"{rom_path}", NULL, NULL, GBMULATOR_MODE_GBC, 0, 0x40, NULL, 0}},\n')
    else:
        ret.append(f'{{"{rom_path}", NULL, NULL, GBMULATOR_MODE_GB, 0, 0x40, NULL, 0}},\n')
        ret.append(f'{{"{rom_path}", NULL, NULL, GBMULATOR_MODE_GBC, 0, 0x40, NULL, 0}},\n')

    return ret


def gbmicrotest_internal_state_test_generator(rom_path):
    if "is_if_set_during_ime0.gb" in rom_path:
        return [f'{{"{rom_path}", NULL, NULL, GBMULATOR_MODE_GB, 380, 0, NULL, 1}},\n']
    return [f'{{"{rom_path}", NULL, NULL, GBMULATOR_MODE_GB, 32, 0, NULL, 1}},\n']


def docboy_reference_image_getter(mode, full_rom_path):
    if mode == "GBMULATOR_MODE_GBC":
        return [f"{'/'.join(full_rom_path.split("/")[:2])}/success.png"]
    return [f"{'/'.join(full_rom_path.split("/")[:2])}/success.png"]

def docboy_screenshot_test_generator(mode, rom_path, reference_image_path):
    # if mode == "GBMULATOR_MODE_GBC":
    #     return

    if "docboy-test-suite/cgb" in rom_path and mode != "GBMULATOR_MODE_GBC":
        return
    if "docboy-test-suite/dmg" in rom_path and mode != "GBMULATOR_MODE_GB":
        return

    time = 380
    if "mbc3/no_rtc" in rom_path or "mbc3/rtc_default_enabled" in rom_path or "mbc3/rtc_tick_disabled" in rom_path or "mbc3/rtc_tick_disabled_after" in rom_path:
        time = 4096

    return f'{{"{rom_path}", "{reference_image_path}", NULL, {mode}, {time}, 0, NULL, 0}},\n'


def main():
    if len(sys.argv) != 2:
        print("No test root dir specified")
        return

    tests_root = sys.argv[1]

    # the aim is to implement GBMULATOR_MODE_GB-CPU-C and GBMULATOR_MODE_GBC-CPU-C (because they have the most test roms compatible)
    # the generated tests filter out all the test roms that are not made for these models

    # TODO gambatte (need a different test implementation to work)
    with open(os.path.join(os.path.dirname(__file__), "tests.txt"), "w") as f:
        generate_tests(tests_root, "blargg", 1, f, blargg_reference_image_getter, blargg_screenshot_test_generator)
        generate_tests(tests_root, "age-test-roms", 1, f, age_reference_image_getter, age_screenshot_test_generator, age_internal_state_test_generator)
        generate_tests(tests_root, "mooneye-test-suite", 2, f, mooneye_reference_image_getter, mooneye_screenshot_test_generator, mooneye_internal_state_test_generator)
        generate_tests(tests_root, "mooneye-test-suite-wilbertpol", 2, f, None, None, mooneye_wilbertpol_internal_state_test_generator)
        generate_tests(tests_root, "mealybug-tearoom-tests", 1, f, mealybug_reference_image_getter, mealybug_screenshot_test_generator, mealybug_internal_state_test_generator)
        generate_tests(tests_root, "same-suite", 2, f, None, None, same_internal_state_test_generator)
        generate_tests(tests_root, "gbmicrotest", 1, f, None, None, gbmicrotest_internal_state_test_generator)
        generate_tests(tests_root, "docboy-test-suite", 3, f, docboy_reference_image_getter, docboy_screenshot_test_generator)
        generate_bully_tests(f)
        generate_acid_tests(f)
        generate_little_things_tests(f)
        generate_rtc3tests(f)
        generate_strikethrough_tests(f)
        generate_other_tests(f)


if __name__ == "__main__":
    main()
