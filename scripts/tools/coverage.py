#!/usr/bin/env python3
# SPDX-FileCopyrightText: Fission3D contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""Runs the unit tests of a coverage build and reports line coverage for one source folder.

Works with Clang (source-based coverage, llvm-cov) and GCC (gcov). Used by scripts/coverage;
needs nothing beyond the compiler's own tools.

    coverage.py --build-dir build/macos-coverage --source-root . --prefix src/core/ --minimum 85
"""

import argparse
import glob
import json
import os
import shutil
import subprocess
import sys
import tempfile


def run(command, **kwargs):
    return subprocess.run(command, check=True, text=True, capture_output=True, **kwargs)


def compiler_family(build_dir):
    cache = os.path.join(build_dir, "CMakeCache.txt")
    with open(cache, encoding="utf-8") as handle:
        for line in handle:
            if line.startswith("CMAKE_CXX_COMPILER:"):
                compiler = line.split("=", 1)[1].strip()
                version = run([compiler, "--version"]).stdout.lower()
                return "clang" if "clang" in version else "gcc"
    sys.exit("CMakeCache.txt nennt keinen C++-Compiler")


def find_tool(name):
    """llvm-cov and llvm-profdata: next to Xcode's Clang on macOS, otherwise on the PATH."""
    if sys.platform == "darwin":
        found = run(["xcrun", "--find", name]).stdout.strip()
        if found:
            return found
    found = shutil.which(name)
    if not found:
        sys.exit(f"'{name}' wurde nicht gefunden")
    return found


def clang_lines(build_dir, executable, test_args, prefix):
    """Returns {file: (covered, total)} from llvm-cov."""
    with tempfile.TemporaryDirectory() as work:
        env = dict(os.environ, LLVM_PROFILE_FILE=os.path.join(work, "run-%p.profraw"))
        subprocess.run([executable, *test_args], check=True, env=env, capture_output=True)
        raw = glob.glob(os.path.join(work, "*.profraw"))
        merged = os.path.join(work, "merged.profdata")
        run([find_tool("llvm-profdata"), "merge", "-sparse", *raw, "-o", merged])
        report = run([find_tool("llvm-cov"), "export", "-format=text",
                      f"-instr-profile={merged}", executable]).stdout
    lines = {}
    for entry in json.loads(report)["data"][0]["files"]:
        name = entry["filename"]
        if prefix in name:
            summary = entry["summary"]["lines"]
            lines[name] = (summary["covered"], summary["count"])
    return lines


def gcc_lines(build_dir, executable, test_args, prefix):
    """Returns {file: (covered, total)} from gcov's JSON output."""
    for old in glob.glob(os.path.join(build_dir, "**", "*.gcda"), recursive=True):
        os.remove(old)
    subprocess.run([executable, *test_args], check=True, capture_output=True)
    counts = {}  # file -> {line: max count}
    for gcda in glob.glob(os.path.join(build_dir, "**", "*.gcda"), recursive=True):
        result = subprocess.run(["gcov", "--json-format", "--stdout", gcda], text=True,
                                capture_output=True, cwd=os.path.dirname(gcda))
        for chunk in result.stdout.splitlines():
            if not chunk.strip():
                continue
            for entry in json.loads(chunk).get("files", []):
                name = os.path.normpath(os.path.join(os.path.dirname(gcda), entry["file"]))
                if prefix not in name:
                    continue
                per_file = counts.setdefault(name, {})
                for line in entry["lines"]:
                    number = line["line_number"]
                    per_file[number] = max(per_file.get(number, 0), line["count"])
    return {name: (sum(1 for c in per_line.values() if c > 0), len(per_line))
            for name, per_line in counts.items()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", required=True)
    parser.add_argument("--source-root", required=True)
    parser.add_argument("--prefix", default="src/core/", help="only files whose path contains this")
    parser.add_argument("--minimum", type=float, default=0.0, help="fail below this percent")
    parser.add_argument("--executable", default="tests/unit/fission_unit_tests")
    parser.add_argument("test_args", nargs="*", help="passed to the test program (Catch2 filter)")
    args = parser.parse_args()

    build_dir = os.path.abspath(args.build_dir)
    executable = os.path.join(build_dir, args.executable)
    family = compiler_family(build_dir)
    collect = clang_lines if family == "clang" else gcc_lines
    lines = collect(build_dir, executable, args.test_args, args.prefix)
    if not lines:
        sys.exit(f"Keine Messwerte für '{args.prefix}'. Wurde mit FISSION_COVERAGE=ON gebaut?")

    source_root = os.path.abspath(args.source_root)
    covered_all = total_all = 0
    print(f"{'Datei':48} {'Zeilen':>7} {'abgedeckt':>10}")
    for name in sorted(lines):
        covered, total = lines[name]
        covered_all += covered
        total_all += total
        percent = 100.0 * covered / total if total else 100.0
        shown = os.path.relpath(name, source_root) if name.startswith(source_root) else name
        print(f"{shown:48} {total:7d} {percent:9.1f}%")
    overall = 100.0 * covered_all / total_all if total_all else 100.0
    print(f"{'Gesamt':48} {total_all:7d} {overall:9.1f}%")
    if overall < args.minimum:
        print(f"Unter dem Mindestwert von {args.minimum:.0f} %.", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
