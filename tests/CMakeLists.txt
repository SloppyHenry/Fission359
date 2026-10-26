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
            --camera=0,90,260 --simulate "tool:size;put:box;grab:width,120,0,12;expect:width,45")
  set_tests_properties(app_handle_drag PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # Pulled far beyond the edge of the view the handle must stay with the pointer: it once vanished
  # under it, the drag was thrown away and the handles were gone.
  add_test(NAME app_handle_drag_far
    COMMAND ${_fission_gui_runner} $<TARGET_FILE:fission3d>
            --smoke-test "${CMAKE_BINARY_DIR}/window-handle-far.png" --language en --size 1280x800
            --camera=-30,36,300 --simulate "tool:size;put:box;grab:width,900,0,30;expect:width,100")
  set_tests_properties(app_handle_drag_far PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # AP-1.6a: the gizmo of the tool "Move" is pulled along X and across the bed with the real mouse
  # path of the window. The new box lands at (-30, -30); both pulls go towards plus.
  add_test(NAME app_gizmo_move
    COMMAND ${_fission_gui_runner} $<TARGET_FILE:fission3d>
            --smoke-test "${CMAKE_BINARY_DIR}/window-gizmo-move.png" --language en --size 1280x800
            --camera=0,90,260 --simulate "put:box;gizmo:x,100,0,10;expect:x,-20;gizmo:free,0,-60,8;expect:y,-20")
  set_tests_properties(app_gizmo_move PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # Feedback 2026-10-18: the depth could not be dragged when looking at the front, and a size
  # pulled smaller than possible stayed big. Seen nearly from the front, the depth handle points
  # at the viewer and is dragged on the screen; pulled far below zero, the width stops at the
  # smallest it can be.
  add_test(NAME app_handle_drag_front
    COMMAND ${_fission_gui_runner} $<TARGET_FILE:fission3d>
            --smoke-test "${CMAKE_BINARY_DIR}/window-handle-front.png" --language en --size 1280x800
            --camera=0,8,220 --simulate "tool:size;put:box;grab:depth,0,60,10;expect:depth,30;grab:width,-400,0,10;near:width,1")
  set_tests_properties(app_handle_drag_front PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # AP-1.6b: a ring of the tool "Rotate" is taken and turned with the real mouse path of the
  # window, in steps of 15 degrees: once round Z and then round X seen from the side (a body that
  # was spun 90 degrees round Z and is then tipped about the X axis of the bed ends with the tip
  # in its Y value), once round X seen from straight above, where the ring is edge on and goes by
  # the pointer along it on screen.
  add_test(NAME app_ring_turn
    COMMAND ${_fission_gui_runner} $<TARGET_FILE:fission3d>
            --smoke-test "${CMAKE_BINARY_DIR}/window-ring-turn.png" --language en --size 1280x800
            --camera=35,35,200 --simulate "tool:rotate;put:box;ring:z,90,12;near:rotZ,90;ring:x,-45,12;least:rotY,45")
  set_tests_properties(app_ring_turn PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  add_test(NAME app_ring_turn_edge_on
    COMMAND ${_fission_gui_runner} $<TARGET_FILE:fission3d>
            --smoke-test "${CMAKE_BINARY_DIR}/window-ring-edge.png" --language en --size 1280x800
            --camera=0,90,260 --simulate "tool:rotate;put:box;ring:x,60,10;least:rotX,15")
  set_tests_properties(app_ring_turn_edge_on PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # The size handles of a turned body go with it and still change their value.
  add_test(NAME app_turned_body_handles
    COMMAND ${_fission_gui_runner} $<TARGET_FILE:fission3d>
            --smoke-test "${CMAKE_BINARY_DIR}/window-turned-handles.png" --language en --size 1280x800
            --camera=35,35,200 --simulate "tool:size;put:box;type:20,tab,20,tab,50,enter;tool:rotate;ring:y,45,8;ring:z,30,8;tool:size;grab:height,0,-60,8;least:height,56")
  set_tests_properties(app_turned_body_handles PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # The corner squares of the tool Size (AP-1.6b2) make a body larger in all directions alike:
  # pulled with a real mouse, then pulled again and 200 typed instead.
  add_test(NAME app_corner_scale
    COMMAND ${_fission_gui_runner} $<TARGET_FILE:fission3d>
            --smoke-test "${CMAKE_BINARY_DIR}/window-corner-scale.png" --language en --size 1280x800
            --camera=35,35,200 --simulate "tool:size;put:box;corner:1,60,40,10;least:width,22;least:height,22;corner:1,30,20,6,hold;type:200,enter;least:height,44")
  set_tests_properties(app_corner_scale PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # A sphere (the corner floats beside it), a turned box, and a box seen from the front and from
  # straight above: the corner nearest the viewer points almost at the eye, it still pulls.
  add_test(NAME app_corner_scale_round_turned
    COMMAND ${_fission_gui_runner} $<TARGET_FILE:fission3d>
            --smoke-test "${CMAKE_BINARY_DIR}/window-corner-round.png" --language en --size 1280x800
            --camera=35,35,200 --simulate "tool:size;put:sphere;corner:1,50,30,10;least:radius,11;tool:rotate;put:box;ring:z,30,8;tool:size;corner:1,40,25,8;least:width,22;near:rotZ,30")
  set_tests_properties(app_corner_scale_round_turned PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  add_test(NAME app_corner_scale_front
    COMMAND ${_fission_gui_runner} $<TARGET_FILE:fission3d>
            --smoke-test "${CMAKE_BINARY_DIR}/window-corner-front.png" --language en --size 1280x800
            --camera=0,5,200 --simulate "tool:size;put:box;corner:1,40,-20,8;least:width,21")
  set_tests_properties(app_corner_scale_front PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  add_test(NAME app_corner_scale_top
    COMMAND ${_fission_gui_runner} $<TARGET_FILE:fission3d>
            --smoke-test "${CMAKE_BINARY_DIR}/window-corner-top.png" --language en --size 1280x800
            --camera=0,90,260 --simulate "tool:size;put:box;corner:1,40,30,8;least:width,21")
  set_tests_properties(app_corner_scale_top PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # AP-1.7b, Ablauf D step 2 (rule 13): a cylinder "as hole" put down on a box cuts it at once,
  # without anything being grouped: the recipe has exactly box, cylinder and the cut.
  add_test(NAME app_flow_d_hole
    COMMAND ${_fission_gui_runner} $<TARGET_FILE:fission3d>
            --smoke-test "${CMAKE_BINARY_DIR}/window-flow-d-hole.png" --language en --size 1280x800
            --camera=0,90,260 --simulate "hover:10,10;hover:10,10;drophole:cylinder,366,341;recipe:box,cylinder,combine")
  set_tests_properties(app_flow_d_hole PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # AP-1.7b, Ablauf D step 3: the ghost of the hole is pulled along X with the real mouse path;
  # the cut stays (the hole is still in the box), and it is no new step "group".
  add_test(NAME app_flow_d_hole_move
    COMMAND ${_fission_gui_runner} $<TARGET_FILE:fission3d>
            --smoke-test "${CMAKE_BINARY_DIR}/window-flow-d-hole-move.png" --language en --size 1280x800
            --camera=0,90,260 --simulate "hover:10,10;hover:10,10;drophole:cylinder,366,341;step:1;gizmo:x,12,0,8;recipe:box,cylinder,combine")
  set_tests_properties(app_flow_d_hole_move PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # AP-1.7d: two chosen bodies are merged through the menu Body; the recipe gets one step.
  add_test(NAME app_combine_menu
    COMMAND ${_fission_gui_runner} $<TARGET_FILE:fission3d>
            --smoke-test "${CMAKE_BINARY_DIR}/window-combine-menu.png" --language en --size 1280x800
            --camera=0,90,260 --simulate "put:cylinder;selectall;menu:merge;recipe:box,cylinder,combine")
  set_tests_properties(app_combine_menu PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # AP-1.7e: one body is split through the menu Body: the cut, and one placing step per part.
  add_test(NAME app_split_menu
    COMMAND ${_fission_gui_runner} $<TARGET_FILE:fission3d>
            --smoke-test "${CMAKE_BINARY_DIR}/window-split-menu.png" --language en --size 1280x800
            --camera=20,35,160 --simulate "selectall;menu:split;recipe:box,split,place,place")
  set_tests_properties(app_split_menu PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # AP-1.8a: the arrow keys in the field "width": +1, with Shift +10 (a new box is 20 mm wide).
  # On macOS in a German environment, as started from a German terminal: numbers must not
  # follow its decimal comma (20 - 0,1 once came out as 19).
  set(_fission_german_env "")
  if(APPLE)
    set(_fission_german_env "LANG=de_DE.UTF-8;LC_ALL=de_DE.UTF-8")
  endif()
  add_test(NAME app_number_field_arrows
    COMMAND ${_fission_gui_runner} $<TARGET_FILE:fission3d>
            --smoke-test "${CMAKE_BINARY_DIR}/window-number-field-arrows.png" --language en --size 1280x800
            --camera=0,90,260 --simulate "put:box;focus:width;key:Up;near:width,21;key:Shift+Up;near:width,31;key:Alt+Down;near:width,30.9;key:Ctrl+Down;near:width,30.8")
  set_tests_properties(app_number_field_arrows PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env};${_fission_german_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # AP-1.6b3: a shape dropped on a wall lies on it (a quarter turn about X); with Alt it stays up.
  add_test(NAME app_wall_drop
    COMMAND ${_fission_gui_runner} $<TARGET_FILE:fission3d>
            --smoke-test "${CMAKE_BINARY_DIR}/window-wall-drop.png" --language en --size 1280x800
            --camera=20,25,160 --simulate "drop:cylinder,382,274;near:rotX,90;near:rotY,0;near:rotZ,0")
  set_tests_properties(app_wall_drop PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  add_test(NAME app_wall_drop_upright
    COMMAND ${_fission_gui_runner} $<TARGET_FILE:fission3d>
            --smoke-test "${CMAKE_BINARY_DIR}/window-wall-upright.png" --language en --size 1280x800
            --camera=20,25,160 --simulate "drop:cylinder,382,274,1;near:rotX,0")
  set_tests_properties(app_wall_drop_upright PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # AP-1.6c: the arrow pulled up to 1.6 mm short of another body goes all the way: the side of
  # the 23.3 mm wide box (-18.35 + 8.35) touches the other, at a place no grid has (x = -21.65).
  # With snapping off (as with Shift held) it stays where the pointer put it, so the same check
  # must fail.
  add_test(NAME app_snap_catch
    COMMAND ${_fission_gui_runner} $<TARGET_FILE:fission3d>
            --smoke-test "${CMAKE_BINARY_DIR}/window-snap-catch.png" --language en --size 1280x800
            --camera=0,30,160 --simulate "put:box;type:23.3,enter;gizmo:x,48,0,8;near:x,-21.65")
  set_tests_properties(app_snap_catch PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  add_test(NAME app_snap_off_does_not_catch
    COMMAND ${_fission_gui_runner} $<TARGET_FILE:fission3d>
            --smoke-test "${CMAKE_BINARY_DIR}/window-snap-off.png" --language en --size 1280x800
            --camera=0,30,160 --simulate "put:box;type:23.3,enter;snap:off;gizmo:x,48,0,8;near:x,-21.65")
  set_tests_properties(app_snap_off_does_not_catch PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE WILL_FAIL TRUE)
  # AP-1.6c2: the same for the size handle of a box that is pulled towards another one: with a grid
  # of 3 mm the width would be 39 or 42, the sides touch at 40. The pull ends 0.4 mm short of it.
  add_test(NAME app_snap_size_handle
    COMMAND ${_fission_gui_runner} $<TARGET_FILE:fission3d>
            --smoke-test "${CMAKE_BINARY_DIR}/window-snap-size-handle.png" --language en --size 1280x800
            --camera=0,30,160 --simulate "put:box;tool:size;grid:3;grab:width,57,0,8;near:width,40")
  set_tests_properties(app_snap_size_handle PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  add_test(NAME app_snap_size_handle_off
    COMMAND ${_fission_gui_runner} $<TARGET_FILE:fission3d>
            --smoke-test "${CMAKE_BINARY_DIR}/window-snap-size-handle-off.png" --language en --size 1280x800
            --camera=0,30,160 --simulate "put:box;tool:size;grid:3;snap:off;grab:width,57,0,8;near:width,40")
  set_tests_properties(app_snap_size_handle_off PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE WILL_FAIL TRUE)
  # The corner of a 7 mm cube pulled until its top is level with the 20 mm box: 285.7 %, no whole
  # percent (the pull alone ends at about 276 %).
  add_test(NAME app_snap_corner_handle
    COMMAND ${_fission_gui_runner} $<TARGET_FILE:fission3d>
            --smoke-test "${CMAKE_BINARY_DIR}/window-snap-corner.png" --language en --size 1280x800
            --camera=0,30,160 --simulate "put:box;type:20,tab,20,tab,7,enter;tool:size;corner:1,220,220,8;near:height,20")
  set_tests_properties(app_snap_corner_handle PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  add_test(NAME app_snap_corner_handle_off
    COMMAND ${_fission_gui_runner} $<TARGET_FILE:fission3d>
            --smoke-test "${CMAKE_BINARY_DIR}/window-snap-corner-off.png" --language en --size 1280x800
            --camera=0,30,160 --simulate "put:box;type:20,tab,20,tab,7,enter;tool:size;snap:off;corner:1,220,220,8;near:height,20")
  set_tests_properties(app_snap_corner_handle_off PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE WILL_FAIL TRUE)
  # A box dropped from the gallery 0.4 mm beside the first one: with a grid of 3 mm its middle would
  # be at 21, the sides touch at 20.
  add_test(NAME app_snap_drop
    COMMAND ${_fission_gui_runner} $<TARGET_FILE:fission3d>
            --smoke-test "${CMAKE_BINARY_DIR}/window-snap-drop.png" --language en --size 1280x800
            --camera=0,30,160 --simulate "grid:3;drop:box,462,312;near:x,20")
  set_tests_properties(app_snap_drop PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  add_test(NAME app_snap_drop_off
    COMMAND ${_fission_gui_runner} $<TARGET_FILE:fission3d>
            --smoke-test "${CMAKE_BINARY_DIR}/window-snap-drop-off.png" --language en --size 1280x800
            --camera=0,30,160 --simulate "grid:3;snap:off;drop:box,462,312;near:x,20")
  set_tests_properties(app_snap_drop_off PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE WILL_FAIL TRUE)
  # AP-1.6d, acceptance criterion 1 of AP-1.6: a shape stands exactly 5 mm from the edge of the bed
  # by its distance measure. The box put down stands at x = -30 (20 mm wide): its left side is
  # then 5 mm from the edge at x = -128 + 5 + 10 = -113.
  add_test(NAME app_distance_five_from_edge
    COMMAND ${_fission_gui_runner} $<TARGET_FILE:fission3d>
            --smoke-test "${CMAKE_BINARY_DIR}/window-distance.png" --language en --size 1280x800
            --camera=20,35,170 --simulate "put:box;tool:move;distance:left,5;near:x,-113")
  set_tests_properties(app_distance_five_from_edge PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # The same from the other side: 5 mm from the right edge, the middle at x = 128 - 5 - 10 = 113.
  # (The box put down is beside the first one, not in line with it: no body is in the way.)
  add_test(NAME app_distance_other_side
    COMMAND ${_fission_gui_runner} $<TARGET_FILE:fission3d>
            --smoke-test "${CMAKE_BINARY_DIR}/window-distance-right.png" --language en --size 1280x800
            --camera=20,35,170 --simulate "put:box;tool:move;distance:right,5;near:x,113")
  set_tests_properties(app_distance_other_side PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # AP-1.6e: three boxes lined up on the left along X. The demo box is at x = 0, the first box put
  # down at x = -30 (its left side at -40), the second at x = 0: all three end at x = -30.
  add_test(NAME app_align_left
    COMMAND ${_fission_gui_runner} $<TARGET_FILE:fission3d>
            --smoke-test "${CMAKE_BINARY_DIR}/window-align.png" --language en --size 1280x800
            --camera=20,35,200 --simulate "put:box;put:box;selectall;align:x,min;step:0;near:x,-30;step:2;near:x,-30")
  set_tests_properties(app_align_left PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # A box lifted with the Z arrow falls back on the bed with "Drop to bed" (the key D).
  add_test(NAME app_drop_to_bed
    COMMAND ${_fission_gui_runner} $<TARGET_FILE:fission3d>
            --smoke-test "${CMAKE_BINARY_DIR}/window-drop.png" --language en --size 1280x800
            --camera=20,35,200 --simulate "put:box;gizmo:z,0,-60,8;least:z,5;menu:dropToBed;near:z,0")
  set_tests_properties(app_drop_to_bed PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # Duplicate: the copy lies 10 mm to the side and to the back of the box put down at (-30, -30),
  # and is the chosen one.
  add_test(NAME app_duplicate
    COMMAND ${_fission_gui_runner} $<TARGET_FILE:fission3d>
            --smoke-test "${CMAKE_BINARY_DIR}/window-duplicate.png" --language en --size 1280x800
            --camera=20,35,200 --simulate "put:box;menu:duplicate;near:x,-20;near:y,-20")
  set_tests_properties(app_duplicate PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # Delete: the chosen shape goes (its step with it); Undo brings the same step back.
  add_test(NAME app_delete_step
    COMMAND ${_fission_gui_runner} $<TARGET_FILE:fission3d>
            --smoke-test "${CMAKE_BINARY_DIR}/window-delete.png" --language en --size 1280x800
            --camera=20,35,200 --simulate "put:cylinder;menu:delete;recipe:box;menu:undo;recipe:box,cylinder")
  set_tests_properties(app_delete_step PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # Time marker: pulled back behind two steps, a new step goes in at it; put back, nothing is lost.
  add_test(NAME app_time_marker
    COMMAND ${_fission_gui_runner} $<TARGET_FILE:fission3d>
            --smoke-test "${CMAKE_BINARY_DIR}/window-marker.png" --language de --size 1280x800
            --camera=20,35,200
            --simulate "put:cylinder;put:sphere;marker:2;put:cone;recipe:box,cylinder,cone,sphere;marker:-1;recipe:box,cylinder,cone,sphere")
  set_tests_properties(app_time_marker PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # The time marker is pulled up with the mouse (real mouse events through the window).
  add_test(NAME app_time_marker_drag
    COMMAND ${_fission_gui_runner} $<TARGET_FILE:fission3d>
            --smoke-test "${CMAKE_BINARY_DIR}/window-marker-drag.png" --language de --size 1280x800
            --camera=20,35,200
            --simulate "put:cylinder;put:sphere;markerdrag:-150,8;markerset;recipe:box,cylinder,sphere")
  set_tests_properties(app_time_marker_drag PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # A double click on a row of the recipe renames the step (real mouse events).
  add_test(NAME app_recipe_rename
    COMMAND ${_fission_gui_runner} $<TARGET_FILE:fission3d>
            --smoke-test "${CMAKE_BINARY_DIR}/window-recipe-rename.png" --language de --size 1280x800
            --camera=20,35,200 --simulate "put:cylinder;rowdouble:1;rowediting:1")
  set_tests_properties(app_recipe_rename PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # The same for a row of the body list.
  add_test(NAME app_body_rename
    COMMAND ${_fission_gui_runner} $<TARGET_FILE:fission3d>
            --smoke-test "${CMAKE_BINARY_DIR}/window-body-rename.png" --language de --size 1280x800
            --camera=20,35,200 --simulate "put:cylinder;rowdouble:b1;rowediting:b1")
  set_tests_properties(app_body_rename PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # Right click on a row of the recipe, "Rename" in its menu: the row is being renamed.
  add_test(NAME app_recipe_menu_rename
    COMMAND ${_fission_gui_runner} $<TARGET_FILE:fission3d>
            --smoke-test "${CMAKE_BINARY_DIR}/window-recipe-menu.png" --language de --size 1280x800
            --camera=20,35,200
            --simulate "put:cylinder;rightclickitem:recipeRow_1;clickitem:menu_rename;rowediting:1")
  set_tests_properties(app_recipe_menu_rename PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # ... and "Hide" in the same menu hides the step (the body list loses the body).
  add_test(NAME app_recipe_menu_hide
    COMMAND ${_fission_gui_runner} $<TARGET_FILE:fission3d>
            --smoke-test "${CMAKE_BINARY_DIR}/window-recipe-hide.png" --language de --size 1280x800
            --camera=20,35,200
            --simulate "put:cylinder;rightclickitem:recipeRow_1;clickitem:menu_hide;recipe:box,cylinder")
  set_tests_properties(app_recipe_menu_hide PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  add_test(NAME app_start_time
    COMMAND ${_fission_gui_runner} ${Python3_EXECUTABLE}
            "${CMAKE_SOURCE_DIR}/scripts/tools/check_startup.py" $<TARGET_FILE:fission3d>)
  set_tests_properties(app_start_time PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env};${_fission_budget_env}" TIMEOUT 180)
endif()
