#!/usr/bin/env python3
# SPDX-FileCopyrightText: Fission3D contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""Rule 5: every problem the core can report has an entry in the message catalogue.

Reads the codes of `FeatureIssueCode` (src/core/include/fission/core/feature_type.h) and checks
that apps/fission3d/qml/MessageCatalog.qml has an entry for each and that every entry lists its
actions. Usage: check_messages.py <repository root>
"""

import re
import sys
from pathlib import Path

root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[2]
header = (root / "src/core/include/fission/core/feature_type.h").read_text(encoding="utf-8")
enum = re.search(r"enum class FeatureIssueCode \{(.*?)\};", header, re.S)
codes = re.findall(r"^\s*([a-zA-Z]+),", enum.group(1), re.M)
catalogue = (root / "apps/fission3d/qml/MessageCatalog.qml").read_text(encoding="utf-8")
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

if problems or not codes:
    print("\n".join(problems) or "no codes found")
    sys.exit(1)
print(f"OK: all {len(codes)} problem codes have a message.")
