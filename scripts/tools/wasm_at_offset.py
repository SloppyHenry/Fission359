#!/usr/bin/env python3
# SPDX-FileCopyrightText: Fission359 contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""Names the function of a WebAssembly file that contains a byte offset.

wasm-opt reports parse errors with a byte offset into the file ("popping from empty stack
(at 0:572731)"). This reads the disassembly of `llvm-objdump -d` and prints the function that
contains that offset, with the instructions around it, so that the object file it came from can be
found. Usage: wasm_at_offset.py <llvm-objdump> <file.wasm> <offset>
"""

import re
import subprocess
import sys


def main() -> int:
    objdump, path, offset = sys.argv[1], sys.argv[2], int(sys.argv[3])
    text = subprocess.run([objdump, "-d", path], capture_output=True, text=True).stdout
    header = re.compile(r"^([0-9a-f]+) <(.+)>:$")
    line_at = re.compile(r"^\s*([0-9a-f]+):")
    current = None
    window = []
    for line in text.splitlines():
        found = header.match(line)
        if found:
            current = found.group(2)
            window = []
            continue
        window.append(line)
        window = window[-25:]
        at = line_at.match(line)
        if at and int(at.group(1), 16) >= offset:
            print(f"function: {current}")
            print("\n".join(window))
            return 0
    print("offset not found in the disassembly")
    return 1


if __name__ == "__main__":
    sys.exit(main())
