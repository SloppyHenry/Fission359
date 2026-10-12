#!/usr/bin/env python3
# SPDX-FileCopyrightText: Fission3D contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""Checks that all C++ sources follow .clang-format.

Usage: check_format.py [--fix] [repo-root]

The clang-format binary comes from $CLANG_FORMAT or the PATH. CI uses a pinned version
(pip package clang-format==21.1.8); clang-format 21 and 23 format this code base identically.
"""

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

SOURCE_SUFFIXES = {".cpp", ".cc", ".h", ".hpp", ".hxx"}
SOURCE_DIRS = ("src", "apps", "tests")


def find_sources(root: Path):
    for top in SOURCE_DIRS:
        for path in sorted((root / top).rglob("*")):
            if path.is_file() and path.suffix in SOURCE_SUFFIXES:
                yield path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("root", nargs="?", default=".")
    parser.add_argument("--fix", action="store_true", help="reformat the files in place")
    args = parser.parse_args()

    binary = os.environ.get("CLANG_FORMAT") or shutil.which("clang-format")
    if not binary:
        print("clang-format not found (set CLANG_FORMAT or install it, see scripts/setup).",
              file=sys.stderr)
        return 2

    root = Path(args.root).resolve()
    files = [str(path) for path in find_sources(root)]
    version = subprocess.run([binary, "--version"], capture_output=True, text=True).stdout.strip()

    if args.fix:
        subprocess.run([binary, "-i", "--style=file", *files], check=True, cwd=root)
        print(f"Reformatted {len(files)} files ({version}).")
        return 0

    result = subprocess.run([binary, "--dry-run", "--Werror", "--style=file", *files],
                            capture_output=True, text=True, cwd=root)
    if result.returncode != 0:
        print(result.stderr.strip())
        print(f"\nFormat check failed ({version}). Fix with: "
              f"python3 scripts/tools/check_format.py --fix")
        return 1
    print(f"OK: {len(files)} files are formatted ({version}).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
