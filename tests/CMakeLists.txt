# SPDX-FileCopyrightText: Fission3D contributors
# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Catch2 3 REQUIRED)
include(Catch)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_subdirectory(unit)
add_subdirectory(smoke)
add_subdirectory(layering)

# Source-level checks (license headers, kernel types and Qt only where allowed).
add_test(NAME license_headers
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/scripts/tools/check_spdx.py"
          "${CMAKE_SOURCE_DIR}")
add_test(NAME layer_includes
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/scripts/tools/check_layers.py"
          "${CMAKE_SOURCE_DIR}")

# License inventory of all third-party libraries (resolved from vcpkg.json without building).
get_filename_component(FISSION_VCPKG_ROOT "${CMAKE_TOOLCHAIN_FILE}/../../.." ABSOLUTE)
set(_licenses_py "${CMAKE_SOURCE_DIR}/scripts/tools/licenses.py")
add_test(NAME third_party_licenses
  COMMAND ${Python3_EXECUTABLE} "${_licenses_py}" check --vcpkg-root "${FISSION_VCPKG_ROOT}")
add_test(NAME third_party_notices_current
  COMMAND ${Python3_EXECUTABLE} "${_licenses_py}" notices --verify
          --vcpkg-root "${FISSION_VCPKG_ROOT}")
# Control: a GPL-2.0-only license in the inventory must be rejected.
add_test(NAME third_party_rejects_forbidden_license
  COMMAND ${Python3_EXECUTABLE} "${_licenses_py}" check --vcpkg-root "${FISSION_VCPKG_ROOT}"
          --inventory "${CMAKE_CURRENT_SOURCE_DIR}/licenses/forbidden-inventory.json"
          --expect-problem stb)

# --version needs no window system and loads OpenCASCADE (proves the library setup).
add_test(NAME app_prints_version COMMAND fission3d --version)
set_tests_properties(app_prints_version PROPERTIES
  PASS_REGULAR_EXPRESSION "Fission3D [0-9]+\\.[0-9]+\\.[0-9]+ \\(OpenCASCADE [0-9]+\\.[0-9]+\\.[0-9]+\\)"
  TIMEOUT 30)

# Start the real app headless, render one frame and keep it as proof.
add_test(NAME app_opens_window
  COMMAND fission3d --smoke-test "${CMAKE_BINARY_DIR}/smoke-test.png")
set_tests_properties(app_opens_window PROPERTIES
  ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
  TIMEOUT 60)
