#!/usr/bin/env python3
# SPDX-FileCopyrightText: Fission359 contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""License inventory of all third-party libraries.

Reads vcpkg.json and the port definitions of the pinned vcpkg checkout, resolves the full
dependency closure for macOS (arm64) and Linux (x86_64) WITHOUT building anything, and
checks every license against the GPL-3.0-or-later compatibility policy below.

  licenses.py check    [--vcpkg-root DIR]   exit 1 on forbidden/unknown/missing licenses
  licenses.py notices  [--vcpkg-root DIR] [--write | --verify]
                                            render the generated part of THIRD_PARTY_NOTICES.md

Non-vcpkg components (vendored PlaneGCS, Qt as system package) and license overrides for ports
that carry no license field live in third_party/inventory.json.
"""

import argparse
import fnmatch
import json
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
NOTICES = REPO / "THIRD_PARTY_NOTICES.md"
INVENTORY = REPO / "third_party" / "inventory.json"
BEGIN = "<!-- BEGIN GENERATED: scripts/tools/licenses.py notices --write -->"
END = "<!-- END GENERATED -->"

# --- license policy ---------------------------------------------------------------------
# Compatible with distributing a combined work under GPL-3.0-or-later.
ALLOWED = {
    "MIT", "MIT-0", "MIT-open-group", "X11", "ISC", "0BSD", "BSD-1-Clause", "BSD-2-Clause",
    "BSD-3-Clause", "BSD-2-Clause-Patent", "Apache-2.0", "BSL-1.0", "Zlib", "libpng-2.0",
    "Libpng", "FTL", "IJG", "bzip2-1.0.6", "curl", "Unlicense", "CC0-1.0", "blessing",
    "Bitstream-Vera", "OFL-1.1", "Unicode-3.0", "Unicode-DFS-2016", "MIT-Modern-Variant", "MPL-2.0", "PSF-2.0", "Python-2.0", "NCSA", "HPND",
    "LGPL-2.1-only", "LGPL-2.1-or-later", "LGPL-3.0-only", "LGPL-3.0-or-later",
    "GPL-3.0-only", "GPL-3.0-or-later", "GPL-2.0-or-later", "Public-Domain", "PD",
}
# Known to be incompatible (or not acceptable for this project).
FORBIDDEN = {
    "GPL-2.0-only", "GPL-2.0", "BSD-4-Clause", "OpenSSL", "SSPL-1.0", "BUSL-1.1",
    "CC-BY-NC-4.0", "CC-BY-NC-SA-4.0", "Proprietary", "LicenseRef-Proprietary",
}

# Platform identifiers that are true for our two supported targets (dynamic triplets).
PLATFORMS = {
    "macos": {"osx", "arm64", "arm", "native"},
    "linux": {"linux", "x64", "native"},
}


# --- vcpkg platform expressions --------------------------------------------------------
def eval_platform(expression, truthy):
    """Evaluates expressions like '!(android | ios) & linux'. None means 'always'."""
    if not expression:
        return True
    tokens = re.findall(r"[A-Za-z0-9_+\-.]+|[!&|(),]", expression)
    position = 0

    def peek():
        return tokens[position] if position < len(tokens) else None

    def take():
        nonlocal position
        token = tokens[position]
        position += 1
        return token

    def parse_or():
        value = parse_and()
        while peek() in ("|", ","):
            take()
            right = parse_and()
            value = value or right
        return value

    def parse_and():
        value = parse_not()
        while peek() == "&":
            take()
            right = parse_not()
            value = value and right
        return value

    def parse_not():
        if peek() == "!":
            take()
            return not parse_not()
        if peek() == "(":
            take()
            value = parse_or()
            if peek() == ")":
                take()
            return value
        return take() in truthy

    return parse_or()


# --- dependency resolution -------------------------------------------------------------
class Ports:
    def __init__(self, vcpkg_root):
        self.root = Path(vcpkg_root)
        self.cache = {}

    def get(self, name):
        if name not in self.cache:
            path = self.root / "ports" / name / "vcpkg.json"
            self.cache[name] = json.loads(path.read_text()) if path.exists() else None
        return self.cache[name]


def _feature_names(entries, truthy):
    names = []
    for entry in entries or []:
        if isinstance(entry, str):
            names.append(entry)
        elif eval_platform(entry.get("platform"), truthy):
            names.append(entry["name"])
    return names


def resolve(manifest, ports, truthy, extra_features=("qt",)):
    """Returns {port name: {"host": bool}} for the whole closure."""
    result = {}
    seen = set()
    work = []

    def add(name, features, defaults, host):
        key = (name, frozenset(features), defaults)
        if key in seen:
            return
        seen.add(key)
        work.append((name, set(features), defaults, host))

    def add_dependency(entry, host):
        if isinstance(entry, str):
            add(entry, [], True, host)
            return
        if not eval_platform(entry.get("platform"), truthy):
            return
        add(entry["name"], _feature_names(entry.get("features"), truthy),
            entry.get("default-features", True), host or entry.get("host", False))

    for entry in manifest.get("dependencies", []):
        add_dependency(entry, False)
    for feature in extra_features:
        for entry in manifest.get("features", {}).get(feature, {}).get("dependencies", []):
            add_dependency(entry, False)

    while work:
        name, features, defaults, host = work.pop()
        port = ports.get(name)
        info = result.setdefault(name, {"host": True, "missing": port is None})
        info["host"] = info["host"] and host
        if port is None:
            continue
        enabled = set(features)
        if defaults:
            enabled.update(_feature_names(port.get("default-features"), truthy))
        for entry in port.get("dependencies", []):
            add_dependency(entry, host)
        for feature in enabled:
            for entry in port.get("features", {}).get(feature, {}).get("dependencies", []):
                add_dependency(entry, host)
    return result


# --- licenses ---------------------------------------------------------------------------
def classify(expression):
    """Returns 'allowed', 'forbidden' or 'unknown' for an SPDX license expression."""
    tokens = re.findall(r"\(|\)|[A-Za-z0-9.+\-:]+", expression)
    position = 0

    def peek():
        return tokens[position] if position < len(tokens) else None

    def take():
        nonlocal position
        token = tokens[position]
        position += 1
        return token

    order = {"allowed": 0, "unknown": 1, "forbidden": 2}

    def parse_or():
        results = [parse_and()]
        while peek() == "OR":
            take()
            results.append(parse_and())
        return min(results, key=order.get)  # best alternative wins

    def parse_and():
        results = [parse_atom()]
        while peek() == "AND":
            take()
            results.append(parse_atom())
        return max(results, key=order.get)  # every part must be fine

    def parse_atom():
        if peek() == "(":
            take()
            value = parse_or()
            if peek() == ")":
                take()
            return value
        license_id = take().rstrip("+")
        if peek() == "WITH":
            take()
            take()  # the exception id does not change compatibility
        if license_id in FORBIDDEN:
            return "forbidden"
        if license_id in ALLOWED or license_id + "-or-later" in ALLOWED:
            return "allowed"
        return "unknown"

    return parse_or() if tokens else "unknown"


def port_version(port):
    for key in ("version", "version-semver", "version-date", "version-string"):
        if key in port:
            version = str(port[key])
            if port.get("port-version"):
                version += f"#{port['port-version']}"
            return version
    return "?"


def load_inventory():
    return json.loads((INVENTORY_OVERRIDE or INVENTORY).read_text())


INVENTORY_OVERRIDE = None  # set from --inventory (used by the negative test)


def override_for(name, overrides):
    for pattern, value in overrides.items():
        if fnmatch.fnmatch(name, pattern):
            return value
    return None


def collect(vcpkg_root):
    """Returns (components, problems). components: list of dicts, one per library."""
    manifest = json.loads((REPO / "vcpkg.json").read_text())
    inventory = load_inventory()
    ports = Ports(vcpkg_root)
    overrides = inventory.get("overrides", {})
    direct_names = {
        entry if isinstance(entry, str) else entry["name"]
        for entry in manifest.get("dependencies", [])
    }
    for entry in manifest.get("features", {}).get("qt", {}).get("dependencies", []):
        direct_names.add(entry if isinstance(entry, str) else entry["name"])

    closure = {}
    for platform, truthy in PLATFORMS.items():
        for name, info in resolve(manifest, ports, truthy).items():
            entry = closure.setdefault(name, {"host": True, "platforms": set(), "missing": False})
            entry["host"] = entry["host"] and info["host"]
            entry["platforms"].add(platform)
            entry["missing"] = entry["missing"] or info["missing"]

    components, problems = [], []
    for name in sorted(closure):
        info = closure[name]
        port = ports.get(name)
        if port is None:
            problems.append(f"{name}: port definition not found in vcpkg checkout")
            continue
        declared = port.get("license")
        override = override_for(name, overrides)
        license_expression = declared or (override or {}).get("license")
        source = "vcpkg" if declared else ("override" if override else "missing")
        if not declared and (override or {}).get("system"):
            # Nothing is shipped: the port only refers to a library of the operating system.
            components.append({
                "name": name, "version": port_version(port), "license": "system library",
                "verdict": "allowed", "source": "override", "host": info["host"],
                "direct": False, "platforms": sorted(info["platforms"]), "system": True,
            })
            continue
        if not license_expression:
            problems.append(f"{name}: no license information (add it to third_party/inventory.json)")
            license_expression, verdict = "UNKNOWN", "unknown"
        else:
            verdict = classify(license_expression)
            if verdict != "allowed":
                problems.append(f"{name}: license '{license_expression}' is {verdict} "
                                f"(GPL-3.0-or-later compatibility)")
        components.append({
            "name": name, "version": port_version(port), "license": license_expression,
            "verdict": verdict, "source": source, "host": info["host"],
            "direct": name in direct_names, "platforms": sorted(info["platforms"]),
        })

    for extra in inventory.get("extra", []):
        verdict = classify(extra["license"])
        if verdict != "allowed":
            problems.append(f"{extra['name']}: license '{extra['license']}' is {verdict}")
        components.append({
            "name": extra["name"], "version": extra.get("version", "?"),
            "license": extra["license"], "verdict": verdict, "source": "inventory",
            "host": False, "direct": True, "platforms": ["macos", "linux"],
            "purpose": extra.get("purpose", ""),
        })
    return components, problems, inventory


# --- notices ----------------------------------------------------------------------------
def render(components, inventory):
    purposes = inventory.get("purposes", {})
    lines = []

    def table(rows, header):
        lines.append("| " + " | ".join(header) + " |")
        lines.append("|" + "|".join("---" for _ in header) + "|")
        for row in rows:
            lines.append("| " + " | ".join(row) + " |")
        lines.append("")

    runtime = [c for c in components if not c["host"]]
    direct = [c for c in runtime if c["direct"] and not c["name"].startswith("qt")]
    direct = [c for c in direct if c["name"] != "boost-graph"]
    lines.append("### Direct dependencies\n")
    rows = []
    for c in sorted(direct, key=lambda c: c["name"].lower()):
        purpose = c.get("purpose") or purposes.get(c["name"], "")
        rows.append([c["name"], c["version"], purpose, c["license"]])
    rows.append(["Qt 6 (qtbase, qtdeclarative, qtshadertools, qtsvg; qttools for lrelease at build time)", "≥ 6.8",
                 purposes.get("qt", "UI and rendering abstraction"),
                 "LGPL-3.0-only OR GPL-3.0-only (we use it as LGPL, dynamically linked)"])
    rows.append(["Boost.Graph (header only)", "", purposes.get("boost-graph", ""), "BSL-1.0"])
    table(rows, ["Library", "Version", "Purpose", "License"])

    boost = [c for c in runtime if c["name"].startswith("boost") and not c["direct"]]
    transitive = [c for c in runtime if not c["direct"] and not c["name"].startswith("qt")
                  and not c["name"].startswith("boost")]
    lines.append("### Transitive dependencies\n")
    rows = [[c["name"], c["version"], c["license"]]
            for c in sorted(transitive, key=lambda c: c["name"].lower())]
    if boost:
        rows.append([f"boost-* ({len(boost)} header-only modules)", boost[0]["version"], "BSL-1.0"])
    table(rows, ["Library", "Version", "License"])

    build_tools = [c for c in components if c["host"]]
    lines.append("### Build tools only (not part of the program)\n")
    table([[c["name"], c["version"], c["license"]]
           for c in sorted(build_tools, key=lambda c: c["name"].lower())],
          ["Tool", "Version", "License"])
    lines.append("Versions are those of the vcpkg baseline pinned in `vcpkg.json`.")
    return "\n".join(lines).rstrip() + "\n"


def merged_notices(generated):
    text = NOTICES.read_text()
    if BEGIN not in text or END not in text:
        raise SystemExit(f"{NOTICES.name}: markers '{BEGIN}' / '{END}' not found")
    head, rest = text.split(BEGIN, 1)
    _, tail = rest.split(END, 1)
    return f"{head}{BEGIN}\n\n{generated}\n{END}{tail}"


# --- main -------------------------------------------------------------------------------
def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("command", choices=["check", "notices"])
    parser.add_argument("--vcpkg-root", default=str(REPO / ".vcpkg"))
    parser.add_argument("--write", action="store_true")
    parser.add_argument("--verify", action="store_true")
    parser.add_argument("--inventory", help="use another inventory file (tests)")
    parser.add_argument("--expect-problem", metavar="TEXT",
                        help="succeed only if check reports a problem containing TEXT (tests)")
    args = parser.parse_args()
    global INVENTORY_OVERRIDE
    if args.inventory:
        INVENTORY_OVERRIDE = Path(args.inventory)

    if not (Path(args.vcpkg_root) / "ports").is_dir():
        print(f"vcpkg checkout not found: {args.vcpkg_root} (run scripts/setup)", file=sys.stderr)
        return 2

    components, problems, inventory = collect(args.vcpkg_root)

    if args.expect_problem is not None:
        matching = [p for p in problems if args.expect_problem in p]
        if matching:
            print("OK: the check rejected the forbidden license as intended:")
            print("  " + "\n  ".join(matching))
            return 0
        print(f"Expected a license problem containing '{args.expect_problem}', got: {problems}")
        return 1

    if args.command == "check":
        runtime = [c for c in components if not c["host"]]
        print(f"{len(components)} components checked "
              f"({len(runtime)} shipped, {len(components) - len(runtime)} build tools).")
        if problems:
            print("License problems:")
            for problem in problems:
                print(f"  {problem}")
            return 1
        print("OK: all licenses are compatible with GPL-3.0-or-later.")
        return 0

    if problems:
        print("Cannot write notices, fix license problems first:\n  " + "\n  ".join(problems))
        return 1
    new_text = merged_notices(render(components, inventory))
    if args.write:
        NOTICES.write_text(new_text)
        print(f"{NOTICES.name} updated.")
        return 0
    if args.verify:
        if NOTICES.read_text() != new_text:
            print(f"{NOTICES.name} is out of date. Run: scripts/tools/licenses.py notices --write")
            return 1
        print(f"OK: {NOTICES.name} is up to date.")
        return 0
    print(render(components, inventory))
    return 0


if __name__ == "__main__":
    sys.exit(main())
