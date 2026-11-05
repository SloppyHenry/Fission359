# STEP and IGES test files

Small files for `tests/unit/geometry/test_exchange.cpp` (AP-2.13). All were written with
OpenCASCADE by `make_fixtures.cpp` in this folder (not part of the build; it needs the
OpenCASCADE modules of the project). They contain no third-party content and can be used and
changed freely (same licence as the project).

| File | What it is | Expected |
|---|---|---|
| `block-with-hole.step` | Block 20 × 10 × 5 mm with a hole of 4 mm diameter, named "Block with hole", colour (204, 51, 26) | 1 body, 7 faces, volume 1000 − 20π mm³ |
| `block-with-hole.iges` | The same block as IGES (millimetres, as solid) | 1 body, 7 faces |
| `block-faces-only.iges` | The same block as IGES, written as loose surfaces (OCCT's default) | 1 body, sewn together from its surfaces |
| `cube-one-inch.step` | Cube of 1 inch, written with unit inch | 25.4 mm edge |
| `assembly-two-parts.step` | Assembly: cube "Cube" (10 mm) and rod "Rod" (blue, radius 4, height 12) moved 30 mm in X | 2 bodies, assembly |
| `sheet-only.step` | A single flat face, no solid | error: no body |

The files of real programs are in `foreign/`.

The files are never changed: later versions of the program must still read them.
