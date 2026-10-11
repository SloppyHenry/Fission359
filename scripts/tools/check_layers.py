#!/usr/bin/env python3
# SPDX-FileCopyrightText: Fission3D contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""Checks the layer rules at source level (docs/03-architektur.md, section 2).

 - Qt may only be used in services, render, interaction, ui and the app.
 - OpenCASCADE types only in src/geometry/occt, Manifold only in src/geometry/mesh,
   PlaneGCS only in src/geometry/sketch.

This complements the CMake allow-list (cmake/FissionLayers.cmake): CMake stops wrong
link dependencies, this script stops wrong #includes and kernel types in comments-free code.

Usage: check_layers.py [repo-root]
"""

import re
import sys
from pathlib import Path

QT_ALLOWED = ("src/services/", "src/render/", "src/interaction/", "src/ui/", "apps/")

QT_INCLUDE = re.compile(r'#\s*include\s*[<"](Q[A-Za-z0-9_]*|Qt[A-Za-z0-9_]*/[^>"]*)[>"]')

# kernel name -> (directory that may use it, patterns)
KERNELS = {
    "OpenCASCADE": (
        "src/geometry/occt/",
        re.compile(r'#\s*include\s*[<"][^>"]*\.hxx[>"]'
                   r'|\b(TopoDS|BRepBuilderAPI|BRepPrimAPI|BRepAlgoAPI|BRepFilletAPI)_\w+'
                   r'|\bgp_\w+|\bStandard_\w+')),
    "Manifold": (
        "src/geometry/mesh/",
        re.compile(r'#\s*include\s*[<"]manifold[/.][^>"]*[>"]|\bmanifold::')),
    "PlaneGCS": (
        "src/geometry/sketch/",
        re.compile(r'#\s*include\s*[<"][^>"]*(planegcs|GCS\.h)[^>"]*[>"]|\bGCS::')),
}

CODE_SUFFIXES = {".cpp", ".cc", ".h", ".hpp", ".hxx"}


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    return re.sub(r"//[^\n]*", "", text)


def main() -> int:
    root = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    problems = []
    checked = 0

    for top in ("src", "apps"):
        for path in sorted((root / top).rglob("*")):
            if path.suffix not in CODE_SUFFIXES or not path.is_file():
                continue
            checked += 1
            rel = path.relative_to(root).as_posix()
            code = strip_comments(path.read_text(encoding="utf-8", errors="replace"))

            if not rel.startswith(QT_ALLOWED) and QT_INCLUDE.search(code):
                problems.append(f"{rel}: Qt is not allowed in this layer")

            for name, (allowed_dir, pattern) in KERNELS.items():
                if not rel.startswith(allowed_dir) and pattern.search(code):
                    problems.append(f"{rel}: {name} types belong into {allowed_dir} only")

    if problems:
        print("Layer rule violations:")
        for problem in problems:
            print(f"  {problem}")
        return 1

    print(f"OK: {checked} source files respect the layer rules.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
