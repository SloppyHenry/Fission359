#!/usr/bin/env python3
# SPDX-FileCopyrightText: Fission359 contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""Rule 5: every problem the program can report has an entry in the message catalogue.

Reads the codes of `FeatureIssueCode` (src/core/include/fission/core/feature_type.h) and checks
that apps/fission359/qml/MessageCatalog.qml has an entry for each and that every entry lists its
actions. The codes of problems with files (`FileController::fileProblem`) and with exporting
(`ExportController::finished`) are read from the C++ sources and must be in `fileEntries` and
`exportEntries` and have a sentence in `fileText` / `exportText`.
Usage: check_messages.py <repository root>
"""

import re
import sys
from pathlib import Path

root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[2]
header = (root / "src/core/include/fission/core/feature_type.h").read_text(encoding="utf-8")
enum = re.search(r"enum class FeatureIssueCode \{(.*?)\};", header, re.S)
codes = re.findall(r"^\s*([a-zA-Z]+),", enum.group(1), re.M)
catalogue = (root / "apps/fission359/qml/MessageCatalog.qml").read_text(encoding="utf-8")
entries = dict(re.findall(r'"([a-zA-Z]+)":\s*\{\s*actions:\s*\[([^\]]*)\]', catalogue))

problems = []
for code in codes:
    if code not in entries:
        problems.append(f"MessageCatalog.qml: no entry for {code}")
    if f'case "{code}"' not in catalogue and code not in ("namingContractViolated", "kernelFailed"):
        problems.append(f"MessageCatalog.qml: no sentence for {code}")
for code in entries:
    if code not in codes:
        problems.append(f"MessageCatalog.qml: {code} is not a code of the core")



def function_body(source, name):
    start = source.find(f"function {name}(")
    if start < 0:
        return ""
    end = source.find("\n    function ", start + 1)
    end = source.find("\n    /// ", start + 1) if end < 0 else end
    return source[start:end if end > 0 else len(source)]


def table_keys(source, name):
    block = re.search(rf"readonly property var {name}: \(\{{(.*?)\n    \}}\)", source, re.S)
    return re.findall(r'^\s*"([a-zA-Z]+)":', block.group(1), re.M) if block else []


file_source = (root / "src/ui/src/file_controller.cpp").read_text(encoding="utf-8")
code_of = re.search(r"QString codeOf\(.*?\n\}", file_source, re.S)
file_codes = set(re.findall(r'return QStringLiteral\("([a-zA-Z]+)"\)', code_of.group(0) if code_of else ""))
file_codes |= set(re.findall(r'fileProblem\(QStringLiteral\("([a-zA-Z]+)"\)', file_source))
export_source = (root / "src/ui/src/export_controller.cpp").read_text(encoding="utf-8")
export_codes = set(re.findall(r'(?:finish|finished)\([^;]*?QStringLiteral\("([a-zA-Z]+)"\)', export_source))

for label, found, table, text in (
    ("file", file_codes, "fileEntries", "fileText"),
    ("export", export_codes, "exportEntries", "exportText"),
):
    keys = table_keys(catalogue, table)
    body = function_body(catalogue, text)
    if not found:
        problems.append(f"no {label} codes found in the C++ sources")
    for code in sorted(found):
        if code not in keys:
            problems.append(f"MessageCatalog.qml: {table} has no entry for {code}")
        if f'case "{code}"' not in body:
            problems.append(f"MessageCatalog.qml: {text} has no sentence for {code}")
    for code in keys:
        if code not in found:
            problems.append(f"MessageCatalog.qml: {code} in {table} is not a {label} code of the program")

if problems or not codes:
    print("\n".join(problems) or "no codes found")
    sys.exit(1)
print(f"OK: all {len(codes)} problem codes, {len(file_codes)} file codes and {len(export_codes)} export codes have a message.")
