#!/usr/bin/env python3
# SPDX-FileCopyrightText: Fission3D contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""Every kind of step the program registers has a sentence in the recipe.

Reads the ids of the feature types from src/features/src/*.cpp (`id() const override`) and checks
that apps/fission3d/qml/RecipeSentences.qml names each of them. Usage: check_sentences.py <root>
"""

import re
import sys
from pathlib import Path

root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[2]
ids = []
for source in sorted((root / "src/features/src").glob("*.cpp")):
    ids += re.findall(r'id\(\) const override \{ return "([a-zA-Z]+)"; \}',
                      source.read_text(encoding="utf-8"))
sentences = (root / "apps/fission3d/qml/RecipeSentences.qml").read_text(encoding="utf-8")

problems = [f'RecipeSentences.qml: no sentence for the step "{i}"' for i in ids
            if f'step.type === "{i}"' not in sentences]
if problems or not ids:
    print("\n".join(problems) or "no feature types found")
    sys.exit(1)
print(f"OK: all {len(ids)} kinds of step have a sentence in the recipe.")
