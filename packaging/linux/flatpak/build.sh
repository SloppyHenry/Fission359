#!/usr/bin/env bash
# SPDX-FileCopyrightText: Fission3D contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Builds the Flatpak from the AppDir of packaging/linux/package.sh and installs it for the
# current user (no root needed):  packaging/linux/flatpak/build.sh <output-dir>

set -euo pipefail

OUT_DIR="$(cd "${1:?output directory (build/package)}" && pwd)"
HERE="$(cd "$(dirname "$0")" && pwd)"
APP_ID="io.github.sloppyhenry.Fission3D"

die() { printf 'Fehler / error: %s\n' "$*" >&2; exit 1; }

[ -d "$OUT_DIR/AppDir/usr" ] || die "AppDir fehlt / missing: erst packaging/linux/package.sh ausführen"
command -v flatpak >/dev/null || die "flatpak fehlt / not installed (sudo apt install flatpak)"
command -v flatpak-builder >/dev/null || die "flatpak-builder fehlt / not installed (sudo apt install flatpak-builder)"

# Runtime and SDK come from Flathub; installed for the user only.
flatpak remote-add --user --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo
RUNTIME_VERSION="$(sed -n "s/^runtime-version: '\(.*\)'/\1/p" "$HERE/$APP_ID.yml")"
flatpak install --user --noninteractive --or-update flathub \
    "org.freedesktop.Platform//$RUNTIME_VERSION" "org.freedesktop.Sdk//$RUNTIME_VERSION"

mkdir -p "$OUT_DIR/flatpak"
cp "$HERE/$APP_ID.yml" "$OUT_DIR/flatpak/"
cd "$OUT_DIR/flatpak"
flatpak-builder --user --force-clean --install --disable-rofiles-fuse \
    --repo=repo --state-dir=.flatpak-builder build "$APP_ID.yml"
flatpak build-bundle repo "$OUT_DIR/Fission3D.flatpak" "$APP_ID"
echo "Fertig / done: $OUT_DIR/Fission3D.flatpak"
