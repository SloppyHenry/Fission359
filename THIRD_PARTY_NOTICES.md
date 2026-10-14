# Third-Party Notices

Fission3D is licensed under **GPL-3.0-or-later** (see [LICENSE](LICENSE)). It uses the libraries
below. Only GPL-3-compatible licenses are allowed (docs/03-architektur.md §8). Every new
dependency needs a justification in the pull request; the inventory is generated and checked:

```sh
python3 scripts/tools/licenses.py check            # fails on a forbidden or unknown license
python3 scripts/tools/licenses.py notices --write  # refreshes the tables below
```

The check runs as test `third_party_licenses` in `scripts/test`, another test fails when the
tables below are out of date. License data comes from the pinned vcpkg checkout; ports without a
license field are covered by `third_party/inventory.json` (with the reason for each entry).

## Libraries

<!-- BEGIN GENERATED: scripts/tools/licenses.py notices --write -->

### Direct dependencies

| Library | Version | Purpose | License |
|---|---|---|---|
| catch2 | 3.16.0 | Unit tests (test builds only) | BSL-1.0 |
| dejavu-sans | 2.37 | Test font for the text cases of the geometry tests (test data, not shipped; tests/fixtures/fonts) | Bitstream-Vera |
| eigen3 | 5.0.1 | Linear algebra (also used by PlaneGCS) | MPL-2.0 |
| lib3mf | 2.5.0#1 | 3MF import and export | BSD-2-Clause |
| manifold | 3.5.4 | Robust mesh booleans, level sets | Apache-2.0 |
| minizip-ng | 4.1.0 | ZIP container of the .fission file format | Zlib |
| nanosvg | 2023-12-29 | SVG import | Zlib |
| nlohmann-json | 3.12.0#2 | JSON (document format) | MIT |
| opencascade | 8.0.1 | Precise B-Rep geometry kernel (booleans, fillets, STEP) | LGPL-2.1-only |
| planegcs | FreeCAD 1.1.4 | 2D sketch constraint solver (vendored in third_party/planegcs) | LGPL-2.1-or-later |
| spdlog | 1.17.0#1 | Logging | MIT |
| stb | 2026-08-02 | Image loading and writing | MIT OR Unlicense |
| Qt 6 (qtbase, qtdeclarative, qtshadertools, qtsvg; qttools for lrelease at build time) | ≥ 6.8 | UI (Qt Quick) and rendering abstraction (RHI) | LGPL-3.0-only OR GPL-3.0-only (we use it as LGPL, dynamically linked) |
| Boost.Graph (header only) |  | Graph algorithms used by PlaneGCS | BSL-1.0 |

### Transitive dependencies

| Library | Version | License |
|---|---|---|
| bzip2 | 1.0.8#6 | bzip2-1.0.6 |
| clipper2 | 2.0.1 | BSL-1.0 |
| cpp-base64 | V2.rc.08 | Zlib |
| dbus | 1.16.2#5 | AFL-2.1 OR GPL-2.0-or-later |
| dirent | 1.26 | MIT |
| double-conversion | 3.4.0 | BSD-3-Clause |
| egl | 2022-12-04 | system library |
| egl-registry | 2025-05-27 | Apache-2.0 |
| expat | 2.8.5 | MIT AND CC0-1.0 |
| fast-float | 8.3.1 | Apache-2.0 OR BSL-1.0 OR MIT |
| fmt | 12.2.0#1 | MIT |
| fontconfig | 2.17.1#2 | MIT |
| freetype | 2.14.3 | FTL OR GPL-2.0-or-later |
| harfbuzz | 14.5.1 | MIT-Modern-Variant AND MIT |
| libb2 | 0.98.1#7 | CC0-1.0 OR OpenSSL OR Apache-2.0 |
| libiconv | 1.19 | LGPL-2.1-or-later |
| libjpeg-turbo | 3.2.0#1 | BSD-3-Clause AND IJG |
| liblzma | 5.8.4 | 0BSD |
| libpng | 1.6.59 | libpng-2.0 |
| libspng | 0.7.4#1 | BSD-2-Clause AND libpng-2.0 |
| libuuid | 1.0.3#17 | BSD-3-Clause |
| libzip | 1.11.4 | BSD-3-Clause |
| md4c | 0.6.0 | MIT |
| opengl | 2022-12-04#3 | system library |
| opengl-registry | 2026-08-03 | MIT AND Apache-2.0 |
| openssl | 3.6.5#1 | Apache-2.0 |
| pcre2 | 10.49 | BSD-3-Clause |
| pthread | 3.0.0#2 | system library |
| pthreads | 3.0.0#14 | Apache-2.0 |
| zlib | 1.3.2#2 | Zlib |
| zstd | 1.5.7 | BSD-3-Clause OR GPL-2.0-only |
| boost-* (74 header-only modules) | 1.92.0 | BSL-1.0 |

### Build tools only (not part of the program)

| Tool | Version | License |
|---|---|---|
| gperf | 3.3#1 | GPL-3.0-or-later |
| vcpkg-boost | 2026-08-21 | MIT |
| vcpkg-cmake | 2025-08-07 | MIT |
| vcpkg-cmake-config | 2026-07-21 | MIT |
| vcpkg-cmake-get-vars | 2025-05-29 | MIT |
| vcpkg-make | 2026-07-09#1 | MIT |
| vcpkg-tool-meson | 1.9.0#10 | Apache-2.0 |

Versions are those of the vcpkg baseline pinned in `vcpkg.json`.

<!-- END GENERATED -->

## Reused texts

| File | Source | License |
|---|---|---|
| `LICENSE` | [GNU GPL v3](https://www.gnu.org/licenses/gpl-3.0.txt) | verbatim, not modifiable |
| `CODE_OF_CONDUCT.md` | [Contributor Covenant 2.1](https://www.contributor-covenant.org/version/2/1/code_of_conduct/) | CC BY 4.0 |
| `third_party/planegcs/LICENSE` | [FreeCAD](https://github.com/FreeCAD/FreeCAD) (LGPL-2.1 text) | LGPL-2.1 |

## Build and test tools (not shipped)

CMake, Ninja (BSD-3-Clause / Apache-2.0), clang-format and clang-tidy (Apache-2.0 with LLVM
exception), Catch2 (BSL-1.0).
