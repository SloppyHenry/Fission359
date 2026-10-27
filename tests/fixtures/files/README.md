<!-- SPDX-FileCopyrightText: Fission3D contributors -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Old `.fission` files

One folder per format version (`v1/`). The files are written once and **never changed**: every
later program version must still open them (`tests/unit/io/test_fission_file.cpp`). If one of those
tests fails after a format change, write a migration; do not touch the files.
