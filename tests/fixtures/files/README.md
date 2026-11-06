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
format 3 file with one element of every kind.
