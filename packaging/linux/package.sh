#!/usr/bin/env bash
# SPDX-FileCopyrightText: Fission359 contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Builds the Linux packages from a finished build:
#   Fission359-<version>-x86_64.AppImage   (always)
#   an AppDir that the Flatpak build reuses (<output-dir>/AppDir)
#
#   packaging/linux/package.sh <build-dir> <output-dir>
#
# The AppImage runs on distributions with a glibc at least as new as the machine it was built
# on, so release builds happen on the oldest supported system (Ubuntu 24.04, see ADR-0006).

set -euo pipefail

BUILD_DIR="$(cd "${1:?build directory}" && pwd)"
OUT_DIR="${2:?output directory}"
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$OUT_DIR"
OUT_DIR="$(cd "$OUT_DIR" && pwd)"

# shellcheck source=/dev/null
. "$BUILD_DIR/package-env.sh"

step() { printf '\n==> %s\n' "$*"; }
die()  { printf 'Fehler / error: %s\n' "$*" >&2; exit 1; }

TOOLS_DIR="$OUT_DIR/../tools"
APPDIR="$OUT_DIR/AppDir"
mkdir -p "$TOOLS_DIR"

step "Hole die Paketierungs-Werkzeuge / fetch packaging tools"
while read -r sum name url; do
    case "$sum" in ''|\#*) continue ;; esac
    if [ ! -f "$TOOLS_DIR/$name" ]; then
        curl --fail --silent --show-error --location --output "$TOOLS_DIR/$name" "$url"
    fi
    echo "$sum  $TOOLS_DIR/$name" | sha256sum --check --quiet - \
        || die "Prüfsumme stimmt nicht / checksum mismatch: $name"
    chmod +x "$TOOLS_DIR/$name"
done < "$HERE/tools.txt"
echo "  OK"

step "Installiere in das AppDir / install into the AppDir"
rm -rf "$APPDIR"
cmake --install "$BUILD_DIR" --prefix "$APPDIR/usr"

# linuxdeploy's Qt plugin asks qmake where Qt lives; vcpkg's Qt has none, so CMake writes a
# shim that answers from the paths it found.
QMAKE="$BUILD_DIR/qmake-shim"
[ -x "$QMAKE" ] || die "qmake-shim fehlt / missing (cmake neu ausführen / reconfigure)"
export QMAKE

# AppImages need FUSE to run; build machines and containers often have none, so unpack instead.
export APPIMAGE_EXTRACT_AND_RUN=1
# Where the bundled libraries come from (vcpkg: OpenCASCADE and friends).
export LD_LIBRARY_PATH="$FISSION_VCPKG_LIB_DIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export QML_SOURCES_PATHS="$FISSION_QML_DIR"
# Wayland and offscreen next to X11 (the Qt plugin only deploys X11 by default). The plugin
# names differ between Qt versions, so deploy whichever exist. "waylandclient" pulls in the
# Wayland shell, graphics and decoration plugins.
QT_PLUGIN_DIR="$("$QMAKE" -query QT_INSTALL_PLUGINS)"
extra_platforms=""
for plugin in libqwayland.so libqwayland-egl.so libqwayland-generic.so libqoffscreen.so; do
    [ -f "$QT_PLUGIN_DIR/platforms/$plugin" ] && extra_platforms="${extra_platforms:+$extra_platforms;}$plugin"
done
export EXTRA_PLATFORM_PLUGINS="$extra_platforms"
export EXTRA_QT_MODULES="waylandclient"

# linuxdeploy's Qt plugin does not deploy the Wayland client plugins of Qt 6 (it only knows the
# Qt 5 file names). Without them the Wayland platform plugin cannot show a window, so copy them
# here, point them to the bundled libraries and let linuxdeploy bundle their dependencies.
command -v patchelf >/dev/null || die "patchelf fehlt / not installed (sudo apt install patchelf)"
DEPLOY_DEPS_ARGS=()
for dir in wayland-decoration-client wayland-graphics-integration-client wayland-shell-integration; do
    if [ -d "$QT_PLUGIN_DIR/$dir" ]; then
        mkdir -p "$APPDIR/usr/plugins"
        cp -a "$QT_PLUGIN_DIR/$dir" "$APPDIR/usr/plugins/"
        for plugin in "$APPDIR/usr/plugins/$dir"/*.so; do
            patchelf --set-rpath '$ORIGIN/../../lib' "$plugin"
        done
        DEPLOY_DEPS_ARGS+=(--deploy-deps-only "$APPDIR/usr/plugins/$dir")
    fi
done
export LINUXDEPLOY_OUTPUT_VERSION="$FISSION_VERSION"
export LINUXDEPLOY_PLUGIN_PATH="$TOOLS_DIR"
export PATH="$TOOLS_DIR:$PATH"

# The OpenGL dispatch library of GLVND is small and stable, but not installed on every desktop
# (Ubuntu: libopengl0), and linuxdeploy leaves it out. Without it the app does not start at all.
EXTRA_LIBRARY_ARGS=()
glvnd_opengl="$(ldconfig -p 2>/dev/null | awk '/libOpenGL\.so\.0 .*x86-64/ {print $NF; exit}')"
[ -n "$glvnd_opengl" ] && EXTRA_LIBRARY_ARGS+=(--library "$glvnd_opengl")

step "Bündle Qt und Bibliotheken, baue das AppImage / bundle libraries, build the AppImage"
ARCH_DIR="$OUT_DIR"
cd "$ARCH_DIR"
rm -f ./*.AppImage
"$TOOLS_DIR/linuxdeploy-x86_64.AppImage" \
    --appdir "$APPDIR" \
    --executable "$APPDIR/usr/bin/fission359" \
    --desktop-file "$APPDIR/usr/share/applications/$FISSION_APP_ID.desktop" \
    --icon-file "$APPDIR/usr/share/icons/hicolor/512x512/apps/$FISSION_APP_ID.png" \
    "${DEPLOY_DEPS_ARGS[@]+"${DEPLOY_DEPS_ARGS[@]}"}" \
    "${EXTRA_LIBRARY_ARGS[@]+"${EXTRA_LIBRARY_ARGS[@]}"}" \
    --plugin qt \
    --output appimage

produced="$(ls -1 ./*.AppImage 2>/dev/null | head -n 1)"
[ -n "$produced" ] || die "linuxdeploy hat kein AppImage erzeugt / no AppImage produced"
final="$OUT_DIR/$FISSION_APP_NAME-$FISSION_VERSION-x86_64.AppImage"
[ "$produced" -ef "$final" ] || mv -f "$produced" "$final"
printf '\nFertig / done: %s\n' "$final"
