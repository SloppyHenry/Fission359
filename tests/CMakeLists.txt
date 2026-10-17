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

# Start the real app headless, render one frame and keep it as proof. On macOS with vcpkg's
# Qt (CI) the build tree's executable carries a private copy of the Qt libraries and cannot load
# Qt's QML plugins; there the same start is done on the finished package (CI job "Package").
set(_fission_run_app_in_tree ON)
# Sanitizer builds: fontconfig's own leak is ignored (tests/lsan.supp), and the program is much
# slower, so the start-time budget is relaxed there (the 3 s of the milestone are for release builds).
set(_fission_lsan_env "LSAN_OPTIONS=suppressions=${CMAKE_CURRENT_SOURCE_DIR}/lsan.supp")
set(_fission_budget_env "")
if(FISSION_SANITIZE)
  set(_fission_budget_env "FISSION_STARTUP_BUDGET_MS=20000")
endif()
if(APPLE AND FISSION_QT_FROM_VCPKG)
  set(_fission_run_app_in_tree OFF)
endif()
if(_fission_run_app_in_tree)
  # The QML modules of Qt (QtQuick.Controls ...) live next to the Qt libraries; an application
  # that is not deployed yet has to be told where.
  set(_fission_qml_path "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_QML}")
  add_test(NAME app_opens_window
    COMMAND fission3d --smoke-test "${CMAKE_BINARY_DIR}/smoke-test.png")
  set_tests_properties(app_opens_window PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QML_IMPORT_PATH=${_fission_qml_path};${_fission_lsan_env}"
    TIMEOUT 60)
endif()

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
  set(_fission_gui_env "FISSION_GUI_TEST=1")
elseif(FISSION_XVFB_RUN)
  set(_fission_gui_runner ${FISSION_XVFB_RUN} -a -s "-screen 0 1280x800x24")
endif()
set(_fission_native_menu_flag "")
if(APPLE)
  set(_fission_native_menu_flag --native-menu-bar)
endif()
if(_fission_run_app_in_tree AND (APPLE OR FISSION_XVFB_RUN))
  foreach(_theme dark light)
    add_test(NAME app_window_${_theme}
      COMMAND ${_fission_gui_runner} $<TARGET_FILE:fission3d>
              --smoke-test "${CMAKE_BINARY_DIR}/window-${_theme}.png" --theme ${_theme}
              --language de --size 1280x800)
    set_tests_properties(app_window_${_theme} PROPERTIES
      ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 FIXTURES_SETUP window_${_theme})
    add_test(NAME app_window_${_theme}_matches_design
      COMMAND fission_screenshot_check "${CMAKE_BINARY_DIR}/window-${_theme}.png" ${_theme} --expect-3d
              ${_fission_native_menu_flag}
              --reference "${CMAKE_CURRENT_SOURCE_DIR}/reference/mockup-${_theme}.png")
    set_tests_properties(app_window_${_theme}_matches_design PROPERTIES
      ENVIRONMENT "QT_QPA_PLATFORM=offscreen" FIXTURES_REQUIRED window_${_theme} TIMEOUT 60)
  endforeach()
  # AP-1.5: the shape gallery opens, shapes are put down (also as hole, also lettering) through
  # the calls the tiles make, and the window still draws.
  add_test(NAME app_shape_gallery
    COMMAND ${_fission_gui_runner} $<TARGET_FILE:fission3d>
            --smoke-test "${CMAKE_BINARY_DIR}/window-gallery.png" --language en --size 1280x800
            --show-shapes --simulate "put:sphere;drop:box,640,420;puthole:cylinder;put:text;put:star;put:box;type:50,tab,20,tab,4,enter")
  set_tests_properties(app_shape_gallery PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90)
  # A handle can be held and pulled: the real mouse path of the window, in small moves. These two
  # run alone: on a shared screen (macOS) a window of another test that opens meanwhile takes the
  # focus, and a drag whose grab is taken away ends where it is (as it should). Before the
  # fix the dot was rebuilt after every millimetre, the pointer lost it and the width stopped at 23.
  add_test(NAME app_handle_drag
    COMMAND ${_fission_gui_runner} $<TARGET_FILE:fission3d>
            --smoke-test "${CMAKE_BINARY_DIR}/window-handle-drag.png" --language en --size 1280x800
            --camera=0,90,260 --simulate "put:box;grab:width,120,0,12;expect:width,45")
  set_tests_properties(app_handle_drag PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # Pulled far beyond the edge of the view the handle must stay with the pointer: it once vanished
  # under it, the drag was thrown away and the handles were gone.
  add_test(NAME app_handle_drag_far
    COMMAND ${_fission_gui_runner} $<TARGET_FILE:fission3d>
            --smoke-test "${CMAKE_BINARY_DIR}/window-handle-far.png" --language en --size 1280x800
            --camera=-30,36,300 --simulate "put:box;grab:width,900,0,30;expect:width,100")
  set_tests_properties(app_handle_drag_far PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  add_test(NAME app_start_time
    COMMAND ${_fission_gui_runner} ${Python3_EXECUTABLE}
            "${CMAKE_SOURCE_DIR}/scripts/tools/check_startup.py" $<TARGET_FILE:fission3d>)
  set_tests_properties(app_start_time PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env};${_fission_budget_env}" TIMEOUT 180)
endif()
