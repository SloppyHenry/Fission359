# Third-Party Notices

Fission3D is licensed under **GPL-3.0-or-later** (see [LICENSE](LICENSE)). It uses the libraries
below. Only GPL-3-compatible licenses are allowed (docs/03-architektur.md §8). Every new
dependency needs an entry here and a justification in the pull request.

> Status: AP-0.1. The complete inventory, generated from vcpkg, comes with AP-0.2.

## Linked into the application

| Library | Version tested | Purpose | License | Linking |
|---|---|---|---|---|
| [Qt 6](https://www.qt.io/) (Core, Gui, Qml, Quick) | 6.10.2 (Linux), 6.11.2 (macOS) | UI, rendering abstraction, platform | LGPL-3.0 | dynamic |

## Used for building and testing only

| Tool / library | Purpose | License |
|---|---|---|
| [Catch2](https://github.com/catchorg/Catch2) v3 | unit tests | BSL-1.0 |
| CMake, Ninja | build system | BSD-3-Clause, Apache-2.0 |
| clang-format, clang-tidy | formatting and static analysis | Apache-2.0 with LLVM exception |

## Reused texts

| File | Source | License |
|---|---|---|
| `LICENSE` | [GNU GPL v3](https://www.gnu.org/licenses/gpl-3.0.txt) | verbatim, not modifiable |
| `CODE_OF_CONDUCT.md` | [Contributor Covenant 2.1](https://www.contributor-covenant.org/version/2/1/code_of_conduct/) | CC BY 4.0 |
