# STEP and IGES files from other programs

Real files made by other CAD programs, to check that they import (`tests/unit/geometry/test_exchange.cpp`).
They are test data only; none is linked into the program. Never change them.

| File | Source | Licence of the source | What it is |
|---|---|---|---|
| `as1-ac-214.stp` | [FreeCAD](https://github.com/FreeCAD/FreeCAD) `data/tests/Step/` (the well known "as1" assembly of the CAx-IF, written by AutoCAD) | LGPL-2.1 | Assembly of 18 parts (plate, brackets, bolts, nuts) with names and colours |
| `screw.step` | [Open CASCADE](https://github.com/Open-Cascade-SAS/OCCT) `data/step/` | LGPL-2.1 with the Open CASCADE exception | One screw |
| `eight_cyl.stp` | [pythonocc-core](https://github.com/tpaviot/pythonocc-core) `test/test_io/` | LGPL-3.0 | Eight cylinders, placed by an assembly |
| `io1-ug-214.stp` | pythonocc-core `test/test_io/` (written by NX) | LGPL-3.0 | One part with a colour |
| `nema-17-bracket.step` | [build123d](https://github.com/gumyr/build123d) `docs/topology_selection/examples/` | Apache-2.0 | A motor bracket |
| `red_cube_blue_cylinder.step` | [CadQuery](https://github.com/CadQuery/cadquery) `tests/testdata/` | Apache-2.0 | A red cube and a blue cylinder |
| `testfile.igs` | pythonocc-core `test/test_io/` (written by Rhino) | LGPL-3.0 | IGES, one open surface: no solid body |
| `example_45_faces.iges` | pythonocc-core `test/test_io/` | LGPL-3.0 | IGES, 45 surfaces that do not close up: no solid body |

All of these licences allow passing the files on with the project (GPL-3.0-or-later) as test data.
Expected values (number of bodies, volume) were recorded from the first import and checked by
hand against the bodies' sizes (box 10 × 10 × 10 mm = 1000 mm³, cylinder radius 5, height 10 =
785.398 mm³); the colours of `red_cube_blue_cylinder.step` stand in the file as `COLOUR_RGB`.
