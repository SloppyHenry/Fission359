#!/usr/bin/env bash
# SPDX-FileCopyrightText: Fission359 contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Installs the newest Fission359 from the GitHub releases, with one command:
#
#   curl -fsSL https://raw.githubusercontent.com/SloppyHenry/Fission359/main/scripts/install.sh | bash
#
# and removes it again:
#
#   curl -fsSL https://raw.githubusercontent.com/SloppyHenry/Fission359/main/scripts/install.sh | bash -s -- --uninstall
#
#   macOS (Apple Silicon): the app goes to /Applications (or ~/Applications if that is not writable).
#   Linux (x86_64): the AppImage is unpacked into ~/.local/share/fission359 (no FUSE and no sudo
#   needed), with the command "fission359", a menu entry, the icon and the file type for .fission.
#
# Projects (.fission files) and settings are never touched, also not by --uninstall.

set -euo pipefail

REPO="SloppyHenry/Fission359"
APP_ID="io.github.sloppyhenry.Fission359"

say()  { printf '==> %s\n' "$*"; }
note() { printf '    %s\n' "$*"; }
die()  { printf 'Fehler / error: %s\n' "$*" >&2; exit 1; }

ACTION=install
for argument in "$@"; do
    case "$argument" in
        --uninstall) ACTION=uninstall ;;
        --help|-h) sed -n '5,17p' "$0" 2>/dev/null | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) die "Unbekannte Option '$argument' / unknown option (--uninstall, --help)" ;;
    esac
done

SYSTEM="$(uname -s)"
MACHINE="$(uname -m)"
case "$SYSTEM/$MACHINE" in
    Darwin/arm64) PLATFORM=macos ;;
    Linux/x86_64) PLATFORM=linux ;;
    *) die "Für $SYSTEM/$MACHINE gibt es kein fertiges Paket / no ready package. Selbst bauen: https://github.com/$REPO#build-from-source" ;;
esac

# --------------------------------------------------------------------------- where things go
mac_app_dirs() { printf '%s\n' "/Applications/Fission359.app" "$HOME/Applications/Fission359.app"; }

LINUX_HOME="${XDG_DATA_HOME:-$HOME/.local/share}"
LINUX_APP="$LINUX_HOME/fission359"
LINUX_BIN="$HOME/.local/bin/fission359"
LINUX_DESKTOP="$LINUX_HOME/applications/$APP_ID.desktop"
LINUX_ICON="$LINUX_HOME/icons/hicolor/scalable/apps/$APP_ID.svg"
LINUX_MIME="$LINUX_HOME/mime/packages/$APP_ID.xml"

# The first file that matches a pattern, empty when there is none.
first_file() {
    local found
    for found in "$@"; do
        [ -e "$found" ] && { printf '%s\n' "$found"; return 0; }
    done
    return 0
}

refresh_linux_menus() {
    command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database -q "$LINUX_HOME/applications" || true
    command -v update-mime-database >/dev/null 2>&1 && update-mime-database "$LINUX_HOME/mime" >/dev/null 2>&1 || true
    command -v gtk-update-icon-cache >/dev/null 2>&1 && gtk-update-icon-cache -q -t "$LINUX_HOME/icons/hicolor" 2>/dev/null || true
}

# ------------------------------------------------------------------------------- uninstall
if [ "$ACTION" = uninstall ]; then
    say "Entferne Fission359 / removing Fission359"
    removed=0
    if [ "$PLATFORM" = macos ]; then
        while read -r app; do
            if [ -d "$app" ]; then
                rm -rf "$app" && note "entfernt / removed: $app" && removed=1
            fi
        done < <(mac_app_dirs)
    else
        for path in "$LINUX_APP" "$LINUX_BIN" "$LINUX_DESKTOP" "$LINUX_ICON" "$LINUX_MIME"; do
            if [ -e "$path" ] || [ -L "$path" ]; then
                rm -rf "$path" && note "entfernt / removed: $path" && removed=1
            fi
        done
        refresh_linux_menus
    fi
    [ "$removed" = 1 ] || note "Nichts gefunden / nothing found: Fission359 war nicht (mit diesem Skript) installiert."
    note "Deine Projekte und Einstellungen bleiben erhalten / your projects and settings stay."
    exit 0
fi

# ---------------------------------------------------------------------------------- install
command -v curl >/dev/null 2>&1 || die "curl fehlt / missing"

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

if [ -n "${FISSION_INSTALL_FROM:-}" ]; then
    # For testing the script with a package from a CI run: a local .dmg or .AppImage.
    [ -f "$FISSION_INSTALL_FROM" ] || die "Datei fehlt / file missing: $FISSION_INSTALL_FROM"
    file="$(basename "$FISSION_INSTALL_FROM")"
    cp "$FISSION_INSTALL_FROM" "$work/$file"
else
say "Suche die neueste Version / looking for the newest release"
# All releases, newest first (the alphas are pre-releases, so /releases/latest would not see them).
releases="$(curl -fsSL "https://api.github.com/repos/$REPO/releases?per_page=10")" \
    || die "GitHub ist nicht erreichbar / cannot reach GitHub"
case "$PLATFORM" in
    macos) pattern='-macos-arm64\.dmg$' ;;
    linux) pattern='-x86_64\.AppImage$' ;;
esac
url="$(printf '%s\n' "$releases" | grep -o '"browser_download_url": *"[^"]*"' \
    | sed 's/.*"\(https[^"]*\)"$/\1/' | grep -E -- "$pattern" | head -n 1 || true)"
[ -n "$url" ] || die "Noch keine veröffentlichte Version für dieses System / no release for this system yet: https://github.com/$REPO/releases"
file="${url##*/}"
note "$file"
say "Lade herunter / downloading"
curl -fL --progress-bar -o "$work/$file" "$url" || die "Download fehlgeschlagen / download failed"
fi

if [ "$PLATFORM" = macos ]; then
    target="/Applications"
    [ -w "$target" ] || { target="$HOME/Applications"; mkdir -p "$target"; }
    say "Installiere nach / installing to $target"
    mount_point="$work/volume"
    hdiutil attach "$work/$file" -mountpoint "$mount_point" -nobrowse -readonly -quiet \
        || die "Das Disk-Image lässt sich nicht öffnen / cannot open the disk image"
    rm -rf "$target/Fission359.app"
    cp -R "$mount_point/Fission359.app" "$target/" || { hdiutil detach "$mount_point" -quiet; die "Kopieren fehlgeschlagen / copy failed"; }
    hdiutil detach "$mount_point" -quiet || true
    say "Fertig / done: $target/Fission359.app"
    note "Starten / start: im Launchpad oder im Ordner Programme auf Fission359 klicken, oder: open \"$target/Fission359.app\""
else
    say "Installiere nach / installing to $LINUX_APP"
    chmod +x "$work/$file"
    # Unpacked instead of run as AppImage: needs no FUSE (Ubuntu 24.04 has none by default).
    (cd "$work" && "./$file" --appimage-extract >/dev/null) || die "Das AppImage lässt sich nicht entpacken / cannot unpack the AppImage"
    rm -rf "$LINUX_APP"
    mkdir -p "$(dirname "$LINUX_APP")" "$(dirname "$LINUX_BIN")"
    mv "$work/squashfs-root" "$LINUX_APP"
    ln -sf "$LINUX_APP/AppRun" "$LINUX_BIN"

    desktop="$(first_file "$LINUX_APP"/usr/share/applications/*.desktop)"
    if [ -n "$desktop" ]; then
        mkdir -p "$(dirname "$LINUX_DESKTOP")"
        sed "s#^Exec=.*#Exec=\"$LINUX_BIN\" %f#" "$desktop" > "$LINUX_DESKTOP"
    fi
    icon="$(first_file "$LINUX_APP"/usr/share/icons/hicolor/scalable/apps/*.svg)"
    if [ -n "$icon" ]; then
        mkdir -p "$(dirname "$LINUX_ICON")"
        cp "$icon" "$LINUX_ICON"
    fi
    mime="$(first_file "$LINUX_APP"/usr/share/mime/packages/*.xml)"
    if [ -n "$mime" ]; then
        mkdir -p "$(dirname "$LINUX_MIME")"
        cp "$mime" "$LINUX_MIME"
    fi
    refresh_linux_menus
    say "Fertig / done"
    note "Starten / start: im Programm-Menü Fission359, oder im Terminal: fission359"
    case ":$PATH:" in
        *":$HOME/.local/bin:"*) ;;
        *) note "Hinweis / note: ~/.local/bin steht nicht im PATH; dann so starten: $LINUX_BIN" ;;
    esac
fi
note "Entfernen / uninstall: curl -fsSL https://raw.githubusercontent.com/$REPO/main/scripts/install.sh | bash -s -- --uninstall"
