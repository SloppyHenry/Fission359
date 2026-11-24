<!-- SPDX-FileCopyrightText: Fission359 contributors -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Old `.fission` files

One folder per format version (`v1/`). The files are written once and **never changed**: every
later program version must still open them (`tests/unit/io/test_fission_file.cpp`). If one of those
tests fails after a format change, write a migration; do not touch the files.

`v1/sketch.fission` is the one file of `v1/` that was not written by a program of format 1: the
step type `sketch` came with format 3 (AP-2.1a). It was written with the hidden test "write the
version 1 fixture files" and then brought into the shape of format 1 (`formatVersion` 1, no
`sketch` field on the step, no `slider` on measures, checksum new). `v3/sketch.fission` is a real
format 3 file with one element of every kind. `v4/extrude.fission` is the first file with a step
`extrude` (AP-2.3b): a 60 × 40 rectangle pulled 25 mm.

`v1/hole.fission` was written by a program of format 4 (the step type `hole` came with AP-2.7b2)
and is kept as it was written; the test expects format 4 for it.

`v5/pattern.fission` (a 10 mm cube in a row of 4, 20 mm apart, the third left out) and
`v5/revolve.fission` (a rectangle turned a full circle) are the first files with a step `pattern`
(AP-2.8b) and `revolve` (AP-2.3c).

`v5/mirror.fission` is the first file with a step `mirror` (AP-2.8c): a 10 mm cube mirrored at x = 30
with the original kept.
