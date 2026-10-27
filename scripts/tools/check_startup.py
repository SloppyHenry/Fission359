#!/usr/bin/env python3
# SPDX-FileCopyrightText: Fission359 contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""Measures the start time (process start to the first drawn frame) and fails above the budget.

Usage: check_startup.py <fission359> [--budget-ms 3000] [--runs 3] [-- extra program arguments]

The program is started `runs` times with --measure-startup; the best run counts (the first run
after a build also pays for cold disk caches). FISSION_STARTUP_BUDGET_MS overrides the budget.
"""

import argparse
import os
import re
import subprocess
import sys


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("program")
    parser.add_argument("--budget-ms", type=int, default=3000)
    parser.add_argument("--runs", type=int, default=3)
    parser.add_argument("extra", nargs="*")
    args = parser.parse_args()
    budget = int(os.environ.get("FISSION_STARTUP_BUDGET_MS", args.budget_ms))

    times = []
    for run in range(args.runs):
        result = subprocess.run([args.program, "--measure-startup", *args.extra],
                                capture_output=True, text=True, timeout=120)
        match = re.search(r"startup_ms=(\d+)", result.stdout)
        if result.returncode != 0 or not match:
            print(f"run {run + 1} failed (exit {result.returncode}):\n{result.stdout}\n{result.stderr}")
            return 1
        times.append(int(match.group(1)))
        print(f"run {run + 1}: {times[-1]} ms")
    best = min(times)
    print(f"best of {len(times)}: {best} ms (budget {budget} ms)")
    return 0 if best <= budget else 1


if __name__ == "__main__":
    sys.exit(main())
