#!/usr/bin/env python3
# SPDX-FileCopyrightText: Fission3D contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""Checks that every source file carries the SPDX license header.

Usage: check_spdx.py [repo-root]
Exit code 0 = all files fine, 1 = at least one file is missing the header.
"""

import sys
from pathlib import Path

REQUIRED = "SPDX-License-Identifier: GPL-3.0-or-later"
HEADER_LINES = 10  # the identifier must appear within the first lines

SOURCE_SUFFIXES = {".cpp", ".cc", ".h", ".hpp", ".hxx", ".qml", ".cmake", ".py", ".sh",
                   ".yml", ".yaml"}
SOURCE_NAMES = {"CMakeLists.txt"}
SKIPPED_DIRS = {".git", "build", "out", "third_party", "vcpkg_installed", "fixtures",
                "__pycache__", ".venv"}


def is_script_without_suffix(path: Path) -> bool:
    """Files like scripts/setup have no suffix but start with a shebang."""
    if path.suffix:
        return False
    try:
        with path.open("rb") as handle:
            return handle.read(2) == b"#!"
    except OSError:
        return False


def source_files(root: Path):
    for path in sorted(root.rglob("*")):
        if not path.is_file():
            continue
        if any(part in SKIPPED_DIRS for part in path.relative_to(root).parts):
            continue
        if (path.suffix in SOURCE_SUFFIXES or path.name in SOURCE_NAMES
                or is_script_without_suffix(path)):
            yield path


def has_header(path: Path) -> bool:
    try:
        with path.open("r", encoding="utf-8", errors="replace") as handle:
            for _, line in zip(range(HEADER_LINES), handle):
                if REQUIRED in line:
                    return True
    except OSError:
        pass
    return False


def main() -> int:
    root = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    checked = 0
    missing = []
    for path in source_files(root):
        checked += 1
        if not has_header(path):
            missing.append(path.relative_to(root))

    if missing:
        print(f"{len(missing)} of {checked} files miss the license header "
              f"('{REQUIRED}' in the first {HEADER_LINES} lines):")
        for path in missing:
            print(f"  {path}")
        return 1

    print(f"OK: {checked} files have the SPDX header.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
