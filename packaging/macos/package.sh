#!/usr/bin/env bash
# SPDX-FileCopyrightText: Fission3D contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Builds the macOS disk image from a finished build:  Fission3D.app -> Fission3D-<version>-macos-arm64.dmg
#
#   packaging/macos/package.sh <build-dir> <output-dir>
#
# Steps: copy the .app, bundle Qt (macdeployqt), check that nothing points outside the bundle,
# sign (ad hoc by default; Developer ID if FISSION_CODESIGN_IDENTITY is set), build the .dmg.
# Notarisation is a separate step: packaging/macos/notarize.sh.
# bash 3.2 compatible (the bash that macOS ships).

set -euo pipefail

BUILD_DIR="$(cd "${1:?build directory}" && pwd)"
OUT_DIR="${2:?output directory}"
HERE="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$HERE/../.." && pwd)"
mkdir -p "$OUT_DIR"
OUT_DIR="$(cd "$OUT_DIR" && pwd)"

# shellcheck source=/dev/null
. "$BUILD_DIR/package-env.sh"

say()  { printf '%s\n' "$*"; }
step() { printf '\n==> %s\n' "$*"; }
die()  { printf 'Fehler / error: %s\n' "$*" >&2; exit 1; }

SOURCE_APP="$BUILD_DIR/apps/fission3d/fission3d.app"
[ -d "$SOURCE_APP" ] || die "App fehlt / missing: $SOURCE_APP (zuerst bauen / build first)"
[ -x "$FISSION_MACDEPLOYQT" ] || die "macdeployqt nicht gefunden / not found: '$FISSION_MACDEPLOYQT'"

STAGE="$OUT_DIR/stage"
APP="$STAGE/$FISSION_APP_NAME.app"
DMG="$OUT_DIR/$FISSION_APP_NAME-$FISSION_VERSION-macos-arm64.dmg"

step "Kopiere die App / copy the app"
rm -rf "$STAGE" "$DMG"
mkdir -p "$STAGE"
ditto "$SOURCE_APP" "$APP"

step "Bündle Qt und Bibliotheken (macdeployqt) / bundle Qt and libraries"
# Homebrew splits Qt into one package per module, each with its own lib directory.
libpaths=(-libpath="$FISSION_VCPKG_LIB_DIR")
for dir in "$FISSION_QT_PREFIX"/opt/qt*/lib; do
    [ -d "$dir" ] && libpaths+=(-libpath="$dir")
done
# macdeployqt prints harmless "ERROR" lines (an unresolved optional plugin dependency, and a
# signature check that fails because it modified the files); the checks below decide.
DEPLOY_LOG="$OUT_DIR/macdeployqt.log"
"$FISSION_MACDEPLOYQT" "$APP" -qmldir="$FISSION_QML_DIR" "${libpaths[@]}" >"$DEPLOY_LOG" 2>&1 \
    || { tail -n 20 "$DEPLOY_LOG"; die "macdeployqt ist fehlgeschlagen / failed"; }
say "  Protokoll / log: $DEPLOY_LOG"

# The offscreen platform plugin lets the packaged app run without a display (CI, tests).
mkdir -p "$APP/Contents/PlugIns/platforms"
if [ -f "$FISSION_QT_PLUGINS/platforms/libqoffscreen.dylib" ]; then
    cp -f "$FISSION_QT_PLUGINS/platforms/libqoffscreen.dylib" "$APP/Contents/PlugIns/platforms/"
fi

# All Mach-O files of the bundle (executables, dylibs, plugins).
macho_files() {
    find "$APP" -type f -print0 | while IFS= read -r -d '' file; do
        if file -b "$file" | grep -q 'Mach-O'; then
            printf '%s\n' "$file"
        fi
    done
}

step "Mache die App eigenständig und prüfe sie / make the bundle self-contained and check it"
python3 "$HERE/relocate.py" "$APP" || die "Die App zeigt noch auf Dateien außerhalb / still references outside files"

step "Ermittle die kleinste unterstützte macOS-Version / minimum macOS version"
min_os="$FISSION_MACOS_MIN_VERSION"
while IFS= read -r file; do
    v="$(otool -l "$file" | awk '/LC_BUILD_VERSION/ {f=1} f && /minos/ {print $2; exit}')"
    if [ -n "$v" ] && [ "$(printf '%s\n%s\n' "$min_os" "$v" | sort -V | tail -n 1)" = "$v" ]; then
        min_os="$v"
    fi
done < <(macho_files)
if [ "$min_os" != "$FISSION_MACOS_MIN_VERSION" ]; then
    say "  Achtung / note: Bibliotheken verlangen macOS $min_os (Ziel: $FISSION_MACOS_MIN_VERSION)."
    say "  Dieses Paket läuft nur dort. Die CI baut mit vcpkg-Qt für $FISSION_MACOS_MIN_VERSION."
    /usr/libexec/PlistBuddy -c "Set :LSMinimumSystemVersion $min_os" "$APP/Contents/Info.plist"
fi
say "  macOS $min_os"

step "Signiere / sign"
IDENTITY="${FISSION_CODESIGN_IDENTITY:--}"
sign_args=(--force --sign "$IDENTITY")
if [ "$IDENTITY" = "-" ]; then
    say "  Ad-hoc-Signatur (kein Entwicklerkonto nötig) / ad-hoc signature"
else
    say "  Developer-ID-Signatur mit Hardened Runtime / Developer ID signature"
    sign_args+=(--options runtime --timestamp --entitlements "$HERE/entitlements.plist")
fi
# Inside-out: files first, then frameworks, then the app itself.
while IFS= read -r file; do
    codesign "${sign_args[@]}" "$file" >/dev/null
done < <(macho_files | awk '{ print length($0) " " $0 }' | sort -rn | cut -d' ' -f2-)
while IFS= read -r framework; do
    codesign "${sign_args[@]}" "$framework" >/dev/null
done < <(find "$APP" -type d -name '*.framework' | awk '{ print length($0) " " $0 }' | sort -rn | cut -d' ' -f2-)
codesign "${sign_args[@]}" "$APP"
codesign --verify --deep --strict --verbose=1 "$APP"

step "Baue das Disk-Image / build the disk image"
VENV="$OUT_DIR/../tools/dmgbuild-venv"
if [ ! -x "$VENV/bin/dmgbuild" ]; then
    mkdir -p "$(dirname "$VENV")"
    python3 -m venv "$VENV"
    "$VENV/bin/pip" install --quiet --disable-pip-version-check -r "$HERE/requirements.txt"
fi
cp "$HERE/Installationshilfe.txt" "$STAGE/Installationshilfe - Installation help.txt"
"$VENV/bin/dmgbuild" -s "$HERE/dmg-settings.py" \
    -D app="$APP" \
    -D appname="$FISSION_APP_NAME" \
    -D help_file="$STAGE/Installationshilfe - Installation help.txt" \
    -D background="$REPO_ROOT/packaging/icons/dmg-background.png" \
    "$FISSION_APP_NAME" "$DMG"
if [ "$IDENTITY" != "-" ]; then
    codesign --force --sign "$IDENTITY" --timestamp "$DMG"
fi

say ""
say "Fertig / done: $DMG"
