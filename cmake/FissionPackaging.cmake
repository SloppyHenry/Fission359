# SPDX-FileCopyrightText: Fission359 contributors
# SPDX-License-Identifier: GPL-3.0-or-later

# Names and metadata that end up in installed packages (macOS .app/.dmg, Linux AppImage and
# Flatpak). Decided in ADR-0006; changing the application ID later orphans users' settings.

set(FISSION_APP_ID "io.github.sloppyhenry.Fission359" CACHE INTERNAL "Reverse-DNS application ID")
set(FISSION_APP_NAME "Fission359" CACHE INTERNAL "Display name")
set(FISSION_MIME_TYPE "application/x-fission359" CACHE INTERNAL "MIME type of .fission files")
# macOS uniform type identifier of project files (UTI).
set(FISSION_PROJECT_UTI "${FISSION_APP_ID}.project" CACHE INTERNAL "")
# Release date in the AppStream metadata; set it when a version is released.
set(FISSION_RELEASE_DATE "2026-10-27" CACHE INTERNAL "")

set(FISSION_PACKAGING_DIR "${CMAKE_SOURCE_DIR}/packaging")
set(FISSION_ICON_SIZES 16 32 48 64 128 256 512 1024)

# fission_make_icns(<output.icns>): builds a macOS icon file from the PNGs in packaging/icons/png
# with iconutil (part of macOS). Adds a custom command; use the output as a target source.
function(fission_make_icns output)
  set(_iconset "${CMAKE_CURRENT_BINARY_DIR}/${FISSION_APP_NAME}.iconset")
  set(_png "${FISSION_PACKAGING_DIR}/icons/png")
  set(_copies "")
  set(_deps "")
  # <iconset file name>=<png size>
  foreach(_pair
      "icon_16x16.png=16" "icon_16x16@2x.png=32" "icon_32x32.png=32" "icon_32x32@2x.png=64"
      "icon_128x128.png=128" "icon_128x128@2x.png=256" "icon_256x256.png=256"
      "icon_256x256@2x.png=512" "icon_512x512.png=512" "icon_512x512@2x.png=1024")
    string(REPLACE "=" ";" _parts "${_pair}")
    list(GET _parts 0 _name)
    list(GET _parts 1 _size)
    list(APPEND _copies COMMAND "${CMAKE_COMMAND}" -E copy
         "${_png}/icon-${_size}.png" "${_iconset}/${_name}")
    list(APPEND _deps "${_png}/icon-${_size}.png")
  endforeach()
  list(REMOVE_DUPLICATES _deps)
  add_custom_command(
    OUTPUT "${output}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_iconset}"
    ${_copies}
    COMMAND iconutil --convert icns --output "${output}" "${_iconset}"
    DEPENDS ${_deps}
    COMMENT "Building ${FISSION_APP_NAME}.icns"
    VERBATIM)
endfunction()
