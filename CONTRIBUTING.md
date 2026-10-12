# Contributing to Fission3D

*Kurz auf Deutsch: Code und Kommentare sind Englisch, UI-Texte sind Deutsch UND Englisch.
Jede Änderung braucht Tests. Die Schichtregeln werden vom Build erzwungen.*

Thank you for helping! Fission3D aims to be **as easy as Tinkercad and as precise as Fusion**.
Please read [docs/01-vision.md](docs/01-vision.md) (design principles) before larger changes.

## Getting started

```sh
scripts/setup      # checks and installs prerequisites
scripts/run        # builds and starts the app
scripts/test       # builds and runs all tests, with a plain-language summary
```

Or use CMake directly: `cmake --preset <macos|linux>-<debug|release|asan>`, then
`cmake --build --preset ...` and `ctest --preset ...`. Requirements: C++20 compiler,
CMake >= 3.28, Ninja, pkg-config, git. Libraries are fetched and built by vcpkg (pinned in
`vcpkg.json`); Qt >= 6.8 comes from the system if available, otherwise vcpkg builds it.
The first build takes a while; later builds use the binary cache. Useful switches:
`FISSION_JOBS=2` (limit parallel jobs on small machines) and `-DFISSION_QT_SOURCE=vcpkg|system|auto`.

## Where work is planned

- Work packages (AP) with acceptance criteria: [docs/arbeitspakete/](docs/arbeitspakete/)
- Definition of Done: [docs/04-roadmap.md](docs/04-roadmap.md)
- Decisions: [docs/adr/](docs/adr/). Do not silently deviate from an accepted ADR; write a new one.

## Architecture rules (enforced by the build)

Layers and their allowed dependencies are listed in [cmake/FissionLayers.cmake](cmake/FissionLayers.cmake).

1. `src/core` knows **no Qt** and no kernel types (OpenCASCADE, Manifold, PlaneGCS).
2. Kernel types appear only inside their implementation folder (`src/geometry/occt`, `mesh`, `sketch`).
3. Dependencies point downwards only. A new library needs an entry in `FissionLayers.cmake`.
4. The UI changes the document only through commands (undo/redo).
5. Long computations never run on the UI thread.

A violation makes the build or the tests fail (`tests/layering`, `scripts/tools/check_layers.py`).

## Code conventions

- **Language:** C++20. Code, comments and commit messages are English.
- **Formatting:** `clang-format` with the repo's `.clang-format` (4 spaces, 100 columns).
  `.editorconfig` covers other file types.
- **Naming:** types `PascalCase`, functions and variables `camelCase`, constants `kPascalCase`,
  namespaces lower case: `fission::<layer>` (for example `fission::core`, `fission::geo::occt`).
- **Headers:** public headers live in `<layer>/include/fission/<layer>/`.
- **Ownership:** RAII, no naked `new`/`delete`, no global mutable state.
- **License header:** every source file starts with

  ```cpp
  // SPDX-FileCopyrightText: Fission3D contributors
  // SPDX-License-Identifier: GPL-3.0-or-later
  ```

  (use `#` instead of `//` in CMake, shell and Python files). `scripts/test` checks this.
- **Dependencies:** only GPL-3-compatible licenses (no GPL-2.0-only, nothing proprietary).
  To add one: put it into `vcpkg.json`, allow it for the right layer in
  `cmake/FissionLayers.cmake`, run `python3 scripts/tools/licenses.py check` and
  `... notices --write` (updates `THIRD_PARTY_NOTICES.md`), and explain the need in the pull request.
  Libraries come from vcpkg; see [ADR-0004](docs/adr/0004-abhaengigkeiten-vcpkg.md).

## UI texts

- Every user-visible string exists in **German and English** and goes through `qsTr()` / `tr()`.
  No hard-coded strings.
- Use only terms from the glossary in [docs/02-ux-konzept.md](docs/02-ux-konzept.md) (section 12).
- Error messages come from the message catalog. No raw kernel errors reach the UI.

## Tests

Framework: Catch2. New logic needs unit tests; new geometry operations need golden tests
(volume, surface, bounding box, topology counts, validity); new file-format fields need
round-trip and migration tests. Large test models (> 1 MB) go under `tests/fixtures/` and are
stored with Git LFS.

## Commits and pull requests

- One topic per commit. Message style: `AP-1.5: Add cylinder primitive with handles`.
- Fill in the pull request template. Describe the effect for users, not only the code.
- Be kind: see the [Code of Conduct](CODE_OF_CONDUCT.md).
