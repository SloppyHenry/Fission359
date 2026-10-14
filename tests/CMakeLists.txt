# SPDX-FileCopyrightText: Fission3D contributors
# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Catch2 3 REQUIRED)
include(Catch)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_subdirectory(unit)
add_subdirectory(smoke)
add_subdirectory(geometry)
add_subdirectory(naming)
add_subdirectory(layering)
add_subdirectory(ui)

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

# Texts: English in the source, German translation complete, nothing hard-coded (AP-0.8).
add_test(NAME translations_complete
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/scripts/tools/check_translations.py"
          "${CMAKE_SOURCE_DIR}")

# The real window with a graphics device: needs a display, so on Linux it runs inside Xvfb with
# software OpenGL; on macOS the window system of the machine is used. Skipped without either.
set(_fission_gui_env "QSG_RHI_BACKEND=opengl;QT_QPA_PLATFORM=xcb;LIBGL_ALWAYS_SOFTWARE=1")
find_program(FISSION_XVFB_RUN xvfb-run)
set(_fission_gui_runner "")
if(APPLE)
  set(_fission_gui_env "")
elseif(FISSION_XVFB_RUN)
  set(_fission_gui_runner ${FISSION_XVFB_RUN} -a -s "-screen 0 1280x800x24")
endif()
if(APPLE OR FISSION_XVFB_RUN)
  foreach(_theme dark light)
    add_test(NAME app_window_${_theme}
      COMMAND ${_fission_gui_runner} $<TARGET_FILE:fission3d>
              --smoke-test "${CMAKE_BINARY_DIR}/window-${_theme}.png" --theme ${_theme}
              --language en --size 1280x800)
    set_tests_properties(app_window_${_theme} PROPERTIES
      ENVIRONMENT "${_fission_gui_env}" TIMEOUT 90 FIXTURES_SETUP window_${_theme})
    add_test(NAME app_window_${_theme}_matches_design
      COMMAND fission_screenshot_check "${CMAKE_BINARY_DIR}/window-${_theme}.png" ${_theme} --expect-3d)
    set_tests_properties(app_window_${_theme}_matches_design PROPERTIES
      ENVIRONMENT "QT_QPA_PLATFORM=offscreen" FIXTURES_REQUIRED window_${_theme} TIMEOUT 60)
  endforeach()
  add_test(NAME app_start_time
    COMMAND ${_fission_gui_runner} ${Python3_EXECUTABLE}
            "${CMAKE_SOURCE_DIR}/scripts/tools/check_startup.py" $<TARGET_FILE:fission3d>)
  set_tests_properties(app_start_time PROPERTIES ENVIRONMENT "${_fission_gui_env}" TIMEOUT 180)
endif()
