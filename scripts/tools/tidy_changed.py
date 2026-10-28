#!/usr/bin/env python3
# SPDX-FileCopyrightText: Fission359 contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""Runs clang-tidy on the C++ files that changed compared to a base revision.

Usage: tidy_changed.py --build-dir build/linux-asan [--base origin/main] [--strict]

Findings are printed as GitHub annotations (::warning file=...) so they show up in pull
requests. By default the exit code is 0 (advisory); --strict makes findings fail the run.
Needs compile_commands.json in the build directory (CMake writes it).
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

SOURCE_SUFFIXES = {".cpp", ".cc"}
SKIPPED = ("third_party/", ".vcpkg/", "build/")
FINDING = re.compile(r"^(?P<file>.+?):(?P<line>\d+):(?P<col>\d+): "
                     r"(?P<level>warning|error): (?P<msg>.*)$")


def changed_files(root: Path, base: str):
    diff = subprocess.run(["git", "diff", "--name-only", "--diff-filter=d", f"{base}...HEAD"],
                          capture_output=True, text=True, cwd=root)
    if diff.returncode != 0:
        print(f"git diff against {base} failed: {diff.stderr.strip()}", file=sys.stderr)
        return None
    return [Path(name) for name in diff.stdout.split()
            if Path(name).suffix in SOURCE_SUFFIXES and not name.startswith(SKIPPED)]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", required=True)
    parser.add_argument("--base", default="origin/main")
    parser.add_argument("--strict", action="store_true")
    args = parser.parse_args()

    root = Path(".").resolve()
    binary = os.environ.get("CLANG_TIDY") or shutil.which("clang-tidy")
    if not binary:
        print("clang-tidy not found (set CLANG_TIDY or install it).", file=sys.stderr)
        return 2

    files = changed_files(root, args.base)
    if files is None:
        return 2
    if not files:
        print(f"No changed C++ files compared to {args.base}; nothing to check.")
        return 0

    findings = 0
    for path in files:
        result = subprocess.run([binary, "-p", args.build_dir, "--quiet", str(path)],
                                capture_output=True, text=True, cwd=root)
        for line in result.stdout.splitlines():
            match = FINDING.match(line)
            if not match or "/third_party/" in match["file"] or "/.vcpkg/" in match["file"]:
                continue
            findings += 1
            relative = os.path.relpath(match["file"], root)
            print(f"::{match['level']} file={relative},line={match['line']},"
                  f"col={match['col']}::{match['msg']}")
    print(f"clang-tidy: {findings} finding(s) in {len(files)} changed file(s).")
    return 1 if (findings and args.strict) else 0


if __name__ == "__main__":
    sys.exit(main())
