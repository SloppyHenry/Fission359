# PlaneGCS (vendored)

2D geometric constraint solver used for sketches (see docs/03-architektur.md §1).

| | |
|---|---|
| Project | [FreeCAD](https://github.com/FreeCAD/FreeCAD), Sketcher module |
| Path in origin | `src/Mod/Sketcher/App/planegcs/` |
| Release tag | `1.1.4` (2026-09-28) |
| Commit | `4fd3bf320d9566a27e60069fc8387448aaa3a094` |
| License | LGPL-2.1-or-later (SPDX header in each file; text in `LICENSE`) |
| Copyright | Konstantinos Poulios and FreeCAD contributors |

## What is here

- `App/planegcs/` — the 11 source files, **byte-for-byte unmodified**. (The extra `App/` level
  makes their relative include `../../SketcherGlobal.h` resolve to the stand-in below.)
- `SketcherGlobal.h` — stand-in for FreeCAD's export-macro header (macro is empty).
- `shim/` — replacements for FreeCAD internals PlaneGCS includes:
  - `Base/Console.h`: `Base::Console()` and `Base::TimeElapsed` (written for Fission3D),
  - `FCConfig.h`: empty,
  - `boost_graph_adjacency_list.hpp`: copy of `src/boost_graph_adjacency_list.hpp` from the same tag.
- `CMakeLists.txt` — builds the static library target `planegcs` (Eigen 3 + Boost.Graph headers
  from vcpkg).

## Updating

1. Pick a FreeCAD release tag and download the files listed above from that tag.
2. Replace `App/planegcs/*`, update the table above (tag, commit, date) and
   `THIRD_PARTY_NOTICES.md`.
3. Build and run `scripts/test` (the `[smoke][planegcs]` test solves a small system).

## Notes

- Boost.Graph is only used for splitting the equation system into independent parts.
- FreeCAD's solver logging is silenced; warnings go to stderr.
