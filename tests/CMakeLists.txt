# SPDX-FileCopyrightText: Fission3D contributors
# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Catch2 3 REQUIRED)
include(Catch)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_subdirectory(unit)
add_subdirectory(layering)

# Source-level checks (license headers, kernel types and Qt only where allowed).
add_test(NAME license_headers
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/scripts/tools/check_spdx.py"
          "${CMAKE_SOURCE_DIR}")
add_test(NAME layer_includes
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/scripts/tools/check_layers.py"
          "${CMAKE_SOURCE_DIR}")

# Start the real app headless, render one frame and keep it as proof.
add_test(NAME app_opens_window
  COMMAND fission3d --smoke-test "${CMAKE_BINARY_DIR}/smoke-test.png")
set_tests_properties(app_opens_window PROPERTIES
  ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
  TIMEOUT 60)
