#!/usr/bin/env python3
# SPDX-FileCopyrightText: Fission3D contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""Checks that the interface is translated and that no text is hard-coded.

 1. Every text in the QML files that a person reads (text, title, placeholder ...) goes through
    qsTr(): no string literal with letters stands there on its own.
 2. Every qsTr("...") source text of the QML files exists in the German and in the English
    translation file (so the files were updated), and
 3. every message of both files has a finished translation (also all plural forms).

Usage: check_translations.py [repo-root]
"""

import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

# Properties whose value a person reads.
TEXT_PROPERTIES = ("text", "title", "placeholderText", "label", "tip", "hint", "heading", "aside",
                   "subtitle", "description", "toolTip", "iconText")
LITERAL = re.compile(
    r'^\s*(?:' + "|".join(TEXT_PROPERTIES) + r')\s*:\s*"((?:[^"\\]|\\.)*)"\s*$', re.M)
# Texts that are the same in every language (names, key labels, units).
ALLOWED_LITERALS = {"Fission3D", "X", "Y", "Z", "⌘K", "⌘Z", "⌘", "–", "mm"}
QSTR = re.compile(r'qsTr\(\s*"((?:[^"\\]|\\.)*)"')


def unescape(text: str) -> str:
    return text.replace('\\"', '"').replace("\\\\", "\\").replace("\\n", "\n")


def ts_messages(path: Path):
    root = ET.parse(path).getroot()
    for message in root.iter("message"):
        source = message.find("source").text or ""
        translation = message.find("translation")
        plural = message.get("numerus") == "yes"
        yield source, translation, plural


def main() -> int:
    root = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    qml_dir = root / "apps" / "fission3d" / "qml"
    i18n_dir = root / "apps" / "fission3d" / "i18n"
    problems = []

    sources = set()
    for qml in sorted(qml_dir.glob("*.qml")):
        text = qml.read_text(encoding="utf-8")
        for match in LITERAL.finditer(text):
            literal = unescape(match.group(1))
            if re.search(r"[A-Za-z]{2,}", literal) and literal not in ALLOWED_LITERALS:
                line = text.count("\n", 0, match.start()) + 1
                problems.append(f"{qml.name}:{line}: hard-coded text \"{literal}\" (use qsTr)")
        for match in QSTR.finditer(text):
            sources.add(unescape(match.group(1)))

    for language in ("de", "en"):
        path = i18n_dir / f"fission3d_{language}.ts"
        if not path.exists():
            problems.append(f"{path.name}: missing")
            continue
        known = set()
        for source, translation, plural in ts_messages(path):
            known.add(source)
            if translation is None:
                problems.append(f"{path.name}: no translation element for \"{source}\"")
                continue
            if translation.get("type") in ("unfinished", "vanished", "obsolete"):
                problems.append(f"{path.name}: unfinished translation of \"{source}\"")
            if plural:
                forms = translation.findall("numerusform")
                if not forms or any(not (f.text or "").strip() for f in forms):
                    problems.append(f"{path.name}: plural forms of \"{source}\" are incomplete")
            elif not (translation.text or "").strip():
                problems.append(f"{path.name}: empty translation of \"{source}\"")
        for source in sorted(sources - known):
            problems.append(f"{path.name}: \"{source}\" is not in the file (run update_translations)")

    if problems:
        print("Translation problems:")
        for problem in problems:
            print("  " + problem)
        return 1
    print(f"OK: {len(sources)} texts, German and English complete, no hard-coded texts.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
