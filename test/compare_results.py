#!/usr/bin/env python3

import os

BOLD = '\033[1m'
COLOR_OFF = '\033[0m'
COLOR_RED = '\033[1;31m'         # Red
COLOR_GREEN = '\033[1;32m'       # Green
COLOR_YELLOW = '\033[1;33m'      # Yellow


def compare(old_summary, mode, rom_name, subtest, status):
    for line in old_summary:
        items = line.split(":")
        for i, _ in enumerate(items):
            items[i] = items[i].strip("\n")

        old_mode, old_rom_name, old_subtest, old_status = items
        if old_rom_name == rom_name and old_mode == mode and old_subtest == subtest and old_status != status:
            if status == "success":
                print(f"{COLOR_GREEN}[IMPROVEMENT]{COLOR_OFF} '{mode}:{rom_name}:{subtest}' changed from '{old_status}' to '{status}'")
                return 1
            elif status == "failed" and old_status == "success":
                print(f"{COLOR_RED}[REGRESSION] {COLOR_OFF} '{mode}:{rom_name}:{subtest}' changed from '{old_status}' to '{status}'")
                return -1
            elif status == "timeout" and old_status == "success":
                print(f"{COLOR_RED}[REGRESSION] {COLOR_OFF} '{mode}:{rom_name}:{subtest}' changed from '{old_status}' to '{status}'")
                return -1
            else:
                print(f"{COLOR_YELLOW}[WARNING]    {COLOR_OFF} '{mode}:{rom_name}:{subtest}' changed from '{old_status}' to '{status}'")
                return 0

def main():
    try:
        with open(os.path.join(os.path.dirname(__file__), "results/summary_old.txt")) as f:
            old_summary = f.readlines()
        with open(os.path.join(os.path.dirname(__file__), "results/summary.txt")) as f:
            new_summary = f.readlines()
    except FileNotFoundError:
        return

    print(f"{BOLD}---- CHECKING CHANGES ----{COLOR_OFF}")

    regression_count = 0
    improvement_count = 0
    warning_count = 0

    for line in new_summary:
        items = line.split(":")
        for i, _ in enumerate(items):
            items[i] = items[i].strip("\n")

        ret = compare(old_summary, *items)
        if ret == 1:
            improvement_count += 1
        elif ret == 0:
            warning_count += 1
        elif ret == -1:
            regression_count += 1
    
    print(f"{BOLD}---- SUMMARY ----{COLOR_OFF}")
    print(f"Detected {improvement_count} improvements, {regression_count} regressions (and {warning_count} warnings)")
    if regression_count > 0:
        exit(1)

if __name__ == "__main__":
    main()
