#!/usr/bin/env python3
# SPDX-FileCopyrightText: Fission359 contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""Turns the CSV written by the geometry tests into a Markdown table.

    FISSION_GEOMETRY_REPORT=build/geometry-report.csv \
        build/<preset>/tests/geometry/fission_geometry_tests
    python3 scripts/tools/geometry_report.py build/geometry-report.csv

Every test case appends one row (tags, name, outcome, kernel seconds, notes). When a case
appears several times (repeated runs), the last row wins.
"""

import csv
import re
import sys
from pathlib import Path

GROUPS = {
    "boolean": "A. Booleans (union, cut)",
    "fillet": "B. Rounding and bevelling",
    "hollow": "C. Hollowing",
    "text": "D. Text",
    "thread": "E. Threads and transitions",
    "loft": "E. Threads and transitions",
    "io": "F. Files and export tessellation",
    "manifold": "G. Mesh booleans (Manifold)",
}
OUTCOMES = {"ok": "ok", "failed": "FAILED", "known-problem": "known problem"}


def case_key(row):
    match = re.match(r"([A-Z])(\d+)([a-z]?)", row["name"])
    return (match.group(1), int(match.group(2)), match.group(3)) if match else ("~", 0, row["name"])


def main(argv):
    if len(argv) != 2:
        print(__doc__)
        return 2
    rows = {}
    with open(argv[1], newline="", encoding="utf-8") as handle:
        for tags, name, outcome, seconds, notes in csv.reader(handle):
            group = next((GROUPS[t] for t in tags.split() if t in GROUPS), "?")
            rows[name] = {"group": group, "name": name, "outcome": outcome,
                          "seconds": float(seconds), "notes": notes.strip().rstrip(";")}
    current = None
    for row in sorted(rows.values(), key=case_key):
        if row["group"] != current:
            current = row["group"]
            print(f"\n### {current}\n")
            print("| Case | Result | Kernel time | Notes |")
            print("|---|---|---|---|")
        seconds = f"{row['seconds'] * 1000:.0f} ms" if row["seconds"] < 1 else f"{row['seconds']:.1f} s"
        print(f"| {row['name']} | {OUTCOMES.get(row['outcome'], row['outcome'])} | {seconds} | {row['notes']} |")
    failed = [r["name"] for r in rows.values() if r["outcome"] == "failed"]
    print(f"\n{len(rows)} cases, {len(failed)} failed.")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
