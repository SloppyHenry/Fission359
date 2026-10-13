#!/usr/bin/env python3
# SPDX-FileCopyrightText: Fission3D contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""Makes an application bundle self-contained after macdeployqt.

macdeployqt copies Qt into the bundle, but with Qt from Homebrew some copied libraries keep
absolute install names and dependencies (/opt/homebrew/...), so the app would load a second copy
from the system or fail on another Mac. This script rewrites, for every Mach-O file in the bundle:

* the install name of each library to @rpath/<path inside Contents/Frameworks>,
* every dependency that exists inside the bundle to the same @rpath form,
* and removes run-path entries that point outside the bundle.

The main executable has the run path @executable_path/../Frameworks, which dyld also applies to
plugins and their dependencies. Anything still pointing outside the bundle afterwards is reported
(and makes the exit code non-zero): that is a library macdeployqt could not find.

Usage: relocate.py Fission3D.app
"""

import subprocess
import sys
from pathlib import Path

SYSTEM_PREFIXES = ("/System/Library/", "/usr/lib/")


def run(*args: str) -> str:
    return subprocess.run(args, check=True, capture_output=True, text=True).stdout


def is_macho(path: Path) -> bool:
    if path.is_symlink() or not path.is_file():
        return False
    with path.open("rb") as handle:
        magic = handle.read(4)
    return magic in (b"\xcf\xfa\xed\xfe", b"\xca\xfe\xba\xbe", b"\xbe\xba\xfe\xca")


def dependencies(path: Path) -> list[str]:
    """Install names listed by otool -L, without the file's own id."""
    lines = run("otool", "-L", str(path)).splitlines()[1:]
    names = [line.strip().split(" (")[0] for line in lines]
    own_id = install_id(path)
    return [n for n in names if n != own_id]


def install_id(path: Path) -> str | None:
    lines = run("otool", "-D", str(path)).splitlines()
    return lines[1].strip() if len(lines) > 1 else None


def rpaths(path: Path) -> list[str]:
    result = []
    lines = run("otool", "-l", str(path)).splitlines()
    for index, line in enumerate(lines):
        if line.strip() == "cmd LC_RPATH":
            result.append(lines[index + 2].strip().split(" (")[0].removeprefix("path "))
    return result


def main() -> int:
    app = Path(sys.argv[1]).resolve()
    frameworks = app / "Contents" / "Frameworks"
    files = [p for p in sorted(app.rglob("*")) if is_macho(p)]

    # Index of libraries inside the bundle by file name: "QtCore" -> .../QtCore.framework/Versions/A/QtCore.
    index: dict[str, str] = {}
    for path in frameworks.rglob("*"):
        if is_macho(path):
            index[path.name] = "@rpath/" + str(path.relative_to(frameworks))

    for path in files:
        args: list[str] = []
        own_id = install_id(path)
        if own_id is not None and path.is_relative_to(frameworks) and not own_id.startswith("@rpath/"):
            args += ["-id", "@rpath/" + str(path.relative_to(frameworks))]
        for dep in dependencies(path):
            if dep.startswith(("@", *SYSTEM_PREFIXES)):
                continue
            target = index.get(Path(dep).name)
            if target is not None:
                args += ["-change", dep, target]
        for rpath in rpaths(path):
            if not rpath.startswith("@"):
                args += ["-delete_rpath", rpath]
        if args:
            subprocess.run(["install_name_tool", *args, str(path)], check=True,
                           capture_output=True)

    # Report what is still not self-contained.
    problems = 0
    for path in files:
        for dep in dependencies(path):
            if not dep.startswith(("@", *SYSTEM_PREFIXES)):
                print(f"verweist nach außen / external reference: {dep}  "
                      f"(in {path.relative_to(app)})")
                problems += 1
        for rpath in rpaths(path):
            if not rpath.startswith("@"):
                print(f"absoluter Suchpfad / absolute rpath: {rpath}  (in {path.relative_to(app)})")
                problems += 1
        # A relative reference must point to something that exists in the bundle.
        for dep in dependencies(path):
            if dep.startswith("@rpath/") and not (frameworks / dep.removeprefix("@rpath/")).exists():
                # Qt plugins may reference libraries via the main executable's other run paths.
                print(f"fehlende Bibliothek / missing library: {dep}  (in {path.relative_to(app)})")
                problems += 1
    print(f"relocate: {len(files)} Mach-O files, {problems} problems")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
