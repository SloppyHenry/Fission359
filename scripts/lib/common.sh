# SPDX-FileCopyrightText: Fission3D contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Shared helpers for scripts/setup, scripts/run and scripts/test.
# Written for bash 3.2 (the version macOS ships), so no modern bash features.
# shellcheck shell=bash

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

if [ -t 1 ]; then
    C_RESET=$'\033[0m'; C_BOLD=$'\033[1m'; C_RED=$'\033[31m'
    C_GREEN=$'\033[32m'; C_YELLOW=$'\033[33m'; C_DIM=$'\033[2m'
else
    C_RESET=""; C_BOLD=""; C_RED=""; C_GREEN=""; C_YELLOW=""; C_DIM=""
fi

say()   { printf '%s\n' "$*"; }
step()  { printf '\n%s==> %s%s\n' "$C_BOLD" "$*" "$C_RESET"; }
ok()    { printf '  %s✔%s %s\n' "$C_GREEN" "$C_RESET" "$*"; }
warn()  { printf '  %s!%s %s\n' "$C_YELLOW" "$C_RESET" "$*"; }
fail()  { printf '  %s✘%s %s\n' "$C_RED" "$C_RESET" "$*"; }
note()  { printf '    %s%s%s\n' "$C_DIM" "$*" "$C_RESET"; }
die()   { printf '\n%sAbbruch:%s %s\n' "$C_RED" "$C_RESET" "$*" >&2; exit 1; }

# Prints "macos", "linux" or "other".
detect_os() {
    case "$(uname -s)" in
        Darwin) echo macos ;;
        Linux)  echo linux ;;
        *)      echo other ;;
    esac
}

# Asks a yes/no question (default: yes). Honors FISSION_ASSUME_YES=1 (set by --yes).
ask_yes_no() {
    if [ "${FISSION_ASSUME_YES:-0}" = "1" ]; then
        return 0
    fi
    if [ ! -t 0 ]; then
        return 1
    fi
    local answer
    printf '%s [J/n] ' "$1"
    read -r answer
    case "$answer" in
        ""|j|J|ja|Ja|y|Y|yes) return 0 ;;
        *) return 1 ;;
    esac
}

# preset_name <debug|release|asan>  ->  e.g. "macos-debug"
preset_name() {
    local os
    os="$(detect_os)"
    [ "$os" = "other" ] && die "Dieses Betriebssystem wird nicht unterstützt (nur macOS und Linux)."
    echo "$os-$1"
}

need_command() {
    command -v "$1" >/dev/null 2>&1 || die "'$1' wurde nicht gefunden. Bitte zuerst scripts/setup ausführen."
}

# --------------------------------------------------------------------------- vcpkg

# Prints the vcpkg commit that vcpkg.json pins (builtin-baseline).
vcpkg_baseline() {
    python3 -c "import json,sys; print(json.load(open(sys.argv[1]))['builtin-baseline'])" \
        "$REPO_ROOT/vcpkg.json"
}

# Prints the vcpkg directory in use: $VCPKG_ROOT if set, otherwise <repo>/.vcpkg.
vcpkg_root() {
    if [ -n "${VCPKG_ROOT:-}" ]; then
        echo "$VCPKG_ROOT"
    else
        echo "$REPO_ROOT/.vcpkg"
    fi
}

# Limits parallel work, for small or shared machines:  FISSION_JOBS=2 scripts/run
# Everything else uses all cores.
apply_job_limit() {
    if [ -n "${FISSION_JOBS:-}" ]; then
        export VCPKG_MAX_CONCURRENCY="$FISSION_JOBS"
        export CMAKE_BUILD_PARALLEL_LEVEL="$FISSION_JOBS"
    fi
}
