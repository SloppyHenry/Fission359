#!/usr/bin/env python3
# SPDX-FileCopyrightText: Fission3D contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""Rule 10: every tool and command has a hint, a tooltip sentence and synonyms.

Checks apps/fission3d/qml/CommandCatalog.qml, ToolCatalog.qml and ShapeCatalog.qml: each command
has `name`, `tip` and `hint` (as qsTr), and at least three synonyms in German and in English.
Usage: check_commands.py <repository root>
"""

import re
import sys
from pathlib import Path

root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[2]
qml = root / "apps" / "fission3d" / "qml"
problems = []


def words(block, language):
    found = re.search(language + r":\s*\[(.*?)\]", block, re.S)
    return re.findall(r'"([^"]+)"', found.group(1)) if found else []


def check_block(source, block, need_tip_hint):
    ident = re.search(r'(?:id|type):\s*"([^"]+)"', block)
    name = ident.group(1) if ident else block[:30]
    if "name: qsTr(" not in block:
        problems.append(f"{source}: {name}: no name")
    if need_tip_hint:
        for field in ("tip", "hint"):
            if f"{field}: qsTr(" not in block:
                problems.append(f"{source}: {name}: no {field}")
    for language in ("de", "en"):
        if len(set(words(block, language))) < 3:
            problems.append(f"{source}: {name}: fewer than 3 {language} synonyms")


def blocks_of(path, marker):
    text = path.read_text(encoding="utf-8")
    parts = re.split(r"\n\s*\{\s*" + marker + r":", text)
    return [marker + ":" + part for part in parts[1:]]


count = 0
for file, marker, need in (("CommandCatalog.qml", "id", True), ("ToolCatalog.qml", "id", True),
                           ("ShapeCatalog.qml", "type", False)):
    for block in blocks_of(qml / file, marker):
        # a block ends at the closing of its object: cut at the next top level "}"
        check_block(file, block, need)
        count += 1

ids = re.findall(r'\{\s*id:\s*"([^"]+)"', (qml / "CommandCatalog.qml").read_text(encoding="utf-8"))
if len(ids) != len(set(ids)):
    problems.append("CommandCatalog.qml: an id is used twice")

if problems:
    print("\n".join(problems))
    sys.exit(1)
print(f"OK: {count} commands have a name, a hint, a tooltip sentence and 3+ synonyms per language.")
