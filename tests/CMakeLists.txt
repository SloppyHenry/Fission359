# SPDX-FileCopyrightText: Fission359 contributors
# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Catch2 3 REQUIRED)
include(Catch)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

# AP-6.7a: in the WebAssembly build the Catch2 test listing runs at test time through the
# Node emulator (CROSSCOMPILING_EMULATOR), so the discovery mode there is PRE_TEST.
set(_fission_discovery "")
if(FISSION_WEB_CORE)
  set(_fission_discovery "DISCOVERY_MODE;PRE_TEST")
endif()

add_subdirectory(unit)
add_subdirectory(geometry)
add_subdirectory(naming)
add_subdirectory(layering)
if(NOT FISSION_WEB_CORE)
  # smoke and ui need Qt and the app, which the web core build does not have.
  add_subdirectory(smoke)
  add_subdirectory(ui)
endif()

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

# The tests of the built app need the fission359 target and mostly Qt; the web core build
# (FISSION_WEB_CORE) has neither.
if(NOT FISSION_WEB_CORE)

# --version needs no window system and loads OpenCASCADE (proves the library setup).
add_test(NAME app_prints_version COMMAND fission359 --version)
set_tests_properties(app_prints_version PROPERTIES
  PASS_REGULAR_EXPRESSION "Fission359 [0-9]+\\.[0-9]+\\.[0-9]+ \\(OpenCASCADE [0-9]+\\.[0-9]+\\.[0-9]+\\)"
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
    COMMAND fission359 --smoke-test "${CMAKE_BINARY_DIR}/smoke-test.png")
  set_tests_properties(app_opens_window PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QML_IMPORT_PATH=${_fission_qml_path};${_fission_lsan_env}"
    TIMEOUT 60)
endif()

endif()

# Rule 10 (AP-1.12): every tool and command has a hint, a tooltip sentence and synonyms.
add_test(NAME commands_complete
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/scripts/tools/check_commands.py"
          "${CMAKE_SOURCE_DIR}")

# Rule 5 (AP-1.12): every problem code of the core has a message in the catalogue.
add_test(NAME messages_complete
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/scripts/tools/check_messages.py"
          "${CMAKE_SOURCE_DIR}")

# Pflege P15: every kind of step has a sentence in the recipe.
add_test(NAME recipe_sentences_complete
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/scripts/tools/check_sentences.py"
          "${CMAKE_SOURCE_DIR}")

# Texts: English in the source, German translation complete, nothing hard-coded (AP-0.8).
add_test(NAME translations_complete
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/scripts/tools/check_translations.py"
          "${CMAKE_SOURCE_DIR}")

# Everything from here on runs fission359 in a real window (Qt, display, packaged app).
if(NOT FISSION_WEB_CORE)

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
# P22: the window tests also run on macOS in the CI, against the finished package (it carries the
# QML plugins that the build tree lacks). FISSION_APP_UNDER_TEST is the path of the executable
# inside the .app; only the CI sets it, so a local run on a Mac never opens a window.
set(FISSION_APP_UNDER_TEST "" CACHE FILEPATH
  "Executable inside a packaged .app: registers the window tests (app_*) on macOS against it")
set(_fission_app_exe "$<TARGET_FILE:fission359>")
set(_fission_run_window_tests OFF)
if(APPLE AND FISSION_APP_UNDER_TEST)
  if(NOT EXISTS "${FISSION_APP_UNDER_TEST}")
    message(FATAL_ERROR "FISSION_APP_UNDER_TEST does not exist: ${FISSION_APP_UNDER_TEST}")
  endif()
  set(_fission_app_exe "${FISSION_APP_UNDER_TEST}")
  set(_fission_run_window_tests ON)
elseif(_fission_run_app_in_tree AND (APPLE OR FISSION_XVFB_RUN))
  set(_fission_run_window_tests ON)
endif()
if(_fission_run_window_tests)
  foreach(_theme dark light)
    add_test(NAME app_window_${_theme}
      COMMAND ${_fission_gui_runner} ${_fission_app_exe}
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
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-gallery.png" --language en --size 1280x800
            --show-shapes --simulate "put:sphere;drop:box,640,420;puthole:cylinder;put:text;put:star;put:box;type:50,tab,20,tab,4,enter")
  set_tests_properties(app_shape_gallery PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90)
  # A handle can be held and pulled: the real mouse path of the window, in small moves. These two
  # run alone: on a shared screen (macOS) a window of another test that opens meanwhile takes the
  # focus, and a drag whose grab is taken away ends where it is (as it should). Before the
  # fix the dot was rebuilt after every millimetre, the pointer lost it and the width stopped at 23.
  add_test(NAME app_handle_drag
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-handle-drag.png" --language en --size 1280x800
            --camera=0,90,260 --simulate "tool:size;put:box;grab:width,120,0,12;expect:width,45")
  set_tests_properties(app_handle_drag PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # AP-1.11f: the same drag with the classic style of handles (squares); same values, same commands.
  add_test(NAME app_handle_drag_classic
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-handle-drag-classic.png" --language en --size 1280x800
            --camera=0,90,260 --simulate "setpref:handleStyle,classic;tool:size;put:box;grab:width,120,0,12;expect:width,45")
  set_tests_properties(app_handle_drag_classic PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # Pulled far beyond the edge of the view the handle must stay with the pointer: it once vanished
  # under it, the drag was thrown away and the handles were gone.
  add_test(NAME app_handle_drag_far
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-handle-far.png" --language en --size 1280x800
            --camera=-30,36,300 --simulate "tool:size;put:box;grab:width,900,0,30;expect:width,100")
  set_tests_properties(app_handle_drag_far PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # AP-1.6a: the gizmo of the tool "Move" is pulled along X and across the bed with the real mouse
  # path of the window. The new box lands at (-30, -30); both pulls go towards plus.
  add_test(NAME app_gizmo_move
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-gizmo-move.png" --language en --size 1280x800
            --camera=0,90,260 --simulate "put:box;gizmo:x,100,0,10;expect:x,-20;gizmo:free,0,-60,8;expect:y,-20")
  set_tests_properties(app_gizmo_move PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # Feedback 2026-10-18: the depth could not be dragged when looking at the front, and a size
  # pulled smaller than possible stayed big. Seen nearly from the front, the depth handle points
  # at the viewer and is dragged on the screen; pulled far below zero, the width stops at the
  # smallest it can be.
  add_test(NAME app_handle_drag_front
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
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
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-ring-turn.png" --language en --size 1280x800
            --camera=35,35,200 --simulate "tool:rotate;put:box;ring:z,90,12;near:rotZ,90;ring:x,-45,12;least:rotY,45")
  set_tests_properties(app_ring_turn PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # P2: a click on empty ground closes the open shape gallery.
  add_test(NAME app_gallery_closes_on_empty_click
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-gallery-closes.png" --language en
            --size 1280x800 --simulate "flyout:shapes;expectflyout:shapes;click:900,650;expectflyout:")
  set_tests_properties(app_gallery_closes_on_empty_click PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  add_test(NAME app_ring_turn_edge_on
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-ring-edge.png" --language en --size 1280x800
            --camera=0,90,260 --simulate "tool:rotate;put:box;ring:x,60,10;least:rotX,15")
  set_tests_properties(app_ring_turn_edge_on PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # The size handles of a turned body go with it and still change their value.
  add_test(NAME app_turned_body_handles
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-turned-handles.png" --language en --size 1280x800
            --camera=35,35,200 --simulate "tool:size;put:box;type:20,tab,20,tab,50,enter;tool:rotate;ring:y,45,8;ring:z,30,8;tool:size;grab:height,0,-60,8;least:height,56")
  set_tests_properties(app_turned_body_handles PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # The corner squares of the tool Size (AP-1.6b2) make a body larger in all directions alike:
  # pulled with a real mouse, then pulled again and 200 typed instead.
  add_test(NAME app_corner_scale
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-corner-scale.png" --language en --size 1280x800
            --camera=35,35,200 --simulate "tool:size;put:box;corner:1,60,40,10;least:width,22;least:height,22;corner:1,30,20,6,hold;type:200,enter;least:height,44")
  set_tests_properties(app_corner_scale PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # A sphere (the corner floats beside it), a turned box, and a box seen from the front and from
  # straight above: the corner nearest the viewer points almost at the eye, it still pulls.
  add_test(NAME app_corner_scale_round_turned
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-corner-round.png" --language en --size 1280x800
            --camera=35,35,200 --simulate "tool:size;put:sphere;corner:1,50,30,10;least:radius,11;tool:rotate;put:box;ring:z,30,8;tool:size;corner:1,40,25,8;least:width,22;near:rotZ,30")
  set_tests_properties(app_corner_scale_round_turned PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # Pflege T7: two tabs; the keys for the next and the previous tab change the tab shown. (The
  # keys Cmd+Shift+] and [ cannot be played back reliably as synthetic key events.)
  if(APPLE)
    set(_fission_next_tab_key "Meta+Tab")
    set(_fission_prev_tab_key "Meta+Shift+Tab")
  else()
    set(_fission_next_tab_key "Ctrl+PgDown")
    set(_fission_prev_tab_key "Ctrl+PgUp")
  endif()
  add_test(NAME app_tab_keys
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-tab-keys.png" --language en --size 1280x800
            --camera=35,35,200 --simulate "put:box;menu:new;expecttab:1,2;key:${_fission_prev_tab_key};expecttab:0,2;key:${_fission_next_tab_key};expecttab:1,2;menu:prevtab;expecttab:0,2")
  set_tests_properties(app_tab_keys PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # AP-1.8 criterion 1, rule 2: a wrong text in the value bubble is not taken, the bubble stays
  # open and says why; Esc takes everything back: the value stays and no new step appears.
  add_test(NAME app_bubble_wrong_input
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-bubble-wrong-input.png" --language en --size 1280x800
            --camera=35,35,200 --simulate "put:box;tool:size;expectundo:addFeature;type:Breitee,enter;expectbubble:Breitee;type:esc;near:width,20;expectundo:addFeature")
  set_tests_properties(app_bubble_wrong_input PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # pflege.md P1: pull the radius of a sphere, do other work, undo twice (the menu entry Undo):
  # the radius is 10 again. A wrong value in `near` must turn this red.
  add_test(NAME app_sphere_radius_undo
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-sphere-radius-undo.png" --language en --size 1280x800
            --camera=35,35,200 --simulate "tool:size;put:sphere;grab:radius,60,0,30;least:radius,11;put:box;menu:undo;menu:undo;step:1;near:radius,10")
  set_tests_properties(app_sphere_radius_undo PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  add_test(NAME app_corner_scale_front
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-corner-front.png" --language en --size 1280x800
            --camera=0,5,200 --simulate "tool:size;put:box;corner:1,40,-20,8;least:width,21")
  set_tests_properties(app_corner_scale_front PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  add_test(NAME app_corner_scale_top
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-corner-top.png" --language en --size 1280x800
            --camera=0,90,260 --simulate "tool:size;put:box;corner:1,40,30,8;least:width,21")
  set_tests_properties(app_corner_scale_top PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # AP-1.7b, Ablauf D step 2 (rule 13): a cylinder "as hole" put down on a box cuts it at once,
  # without anything being grouped: the recipe has exactly box, cylinder and the cut.
  add_test(NAME app_flow_d_hole
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-flow-d-hole.png" --language en --size 1280x800
            --camera=0,90,260 --simulate "hover:10,10;hover:10,10;drophole:cylinder,366,341;recipe:box,cylinder,combine")
  set_tests_properties(app_flow_d_hole PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # AP-1.7b, Ablauf D step 3: the ghost of the hole is pulled along X with the real mouse path;
  # the cut stays (the hole is still in the box), and it is no new step "group".
  add_test(NAME app_flow_d_hole_move
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-flow-d-hole-move.png" --language en --size 1280x800
            --camera=0,90,260 --simulate "hover:10,10;hover:10,10;drophole:cylinder,366,341;step:1;gizmo:x,12,0,8;recipe:box,cylinder,combine")
  set_tests_properties(app_flow_d_hole_move PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # AP-1.7d: two chosen bodies are merged through the menu Body; the recipe gets one step.
  add_test(NAME app_combine_menu
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-combine-menu.png" --language en --size 1280x800
            --camera=0,90,260 --simulate "put:cylinder;selectall;menu:merge;recipe:box,cylinder,combine")
  set_tests_properties(app_combine_menu PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # AP-2.1b: the command search finds the sketch ("zeichnen", "2D", "outline"), "Sketch" waits for
  # the click on a plane, a click on the ground makes a sketch step that is the active one, "Done"
  # takes an empty sketch away again (one undo step), and Esc does the same.
  add_test(NAME app_sketch_ground
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-sketch-ground.png" --language en --size 1280x800
            --camera=0,90,260 --simulate "search:zeichnen,menu.sketch;search:2D,menu.sketch;search:outline,menu.sketch;runcommand:menu.sketch;expectsketch:picking;expecthint:flat face;click:60,60;expectsketch:active;expectsketchplane:ground;recipe:box,sketch;menu:sketchdone;expectsketch:idle;recipe:box;expectundo:removeFeature;menu:undo;recipe:box,sketch;menu:redo;recipe:box")
  set_tests_properties(app_sketch_ground PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # Esc ends the sketch like Done does (an empty sketch goes).
  add_test(NAME app_sketch_escape
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-sketch-escape.png" --language en --size 1280x800
            --camera=0,90,260 --simulate "menu:sketch;expectsketch:picking;click:60,60;expectsketch:active;key:Escape;expectsketch:idle;recipe:box")
  set_tests_properties(app_sketch_escape PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # The ground again after Done, undo and redo.
  add_test(NAME app_sketch_ground_again
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-sketch-again.png" --language en --size 1280x800
            --camera=0,90,260 --simulate "menu:sketch;click:60,60;expectsketch:active;menu:sketchdone;menu:undo;menu:redo;recipe:box;menu:sketch;expectsketch:picking;click:60,60;expectsketch:active;recipe:box,sketch")
  set_tests_properties(app_sketch_ground_again PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # AP-2.1b: a click on the top of the box makes a sketch on that face (it reads the box).
  add_test(NAME app_sketch_face
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-sketch-face.png" --language en --size 1280x800
            --camera=0,90,260 --simulate "menu:sketch;expectsketch:picking;click:366,341;expectsketch:active;expectsketchplane:face;recipe:box,sketch;menu:sketchdone;expectsketch:idle;recipe:box")
  set_tests_properties(app_sketch_face PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # AP-2.1c1: a rectangle in a sketch on the ground: tool chosen by a word of the command search,
  # first corner clicked, width and height typed, Enter sets four lines (one undo step); Done keeps
  # the sketch with its lines in the recipe.
  add_test(NAME app_sketch_rectangle
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-sketch-rectangle.png" --language en --size 1280x800
            --camera=0,90,260 --simulate "menu:sketch;click:60,60;expectsketch:active;search:Viereck,sketch.rectangle;search:Strich,sketch.line;search:Hilfslinie,sketch.helper;search:Markierung,sketch.point;menu:sketchtool.rectangle;expectsketchtool:rectangle;sketchmove:520,330;sketchclick:520,330;sketchmove:700,420;type:40,tab,25,enter;expectsketchelements:line=4;expectsketchsize:40,25;expectundo:addSketchElement;menu:undo;expectsketchelements:;menu:redo;expectsketchelements:line=4;menu:sketchdone;expectsketch:idle;recipe:box,sketch;menu:undo;expectsketchelements:")
  set_tests_properties(app_sketch_rectangle PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # AP-2.1c1: a chain of lines ended by a double click, a circle with a typed radius, and Esc ends
  # the tool first and the sketch second.
  add_test(NAME app_sketch_line_circle
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-sketch-line-circle.png" --language en --size 1280x800
            --camera=0,90,260 --simulate "menu:sketch;click:60,60;expectsketch:active;menu:sketchtool.line;expectsketchtool:line;sketchmove:400,300;sketchclick:400,300;sketchclick:600,300;sketchdouble:600,450;expectsketchelements:line=2;menu:sketchtool.circle;sketchmove:800,350;sketchclick:800,350;sketchmove:850,350;type:12.5,enter;expectsketchelements:line=2,circle=1;expectsketchtool:circle;key:Escape;expectsketchtool:;expectsketch:active;key:Escape;expectsketch:idle;recipe:box,sketch")
  set_tests_properties(app_sketch_line_circle PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # AP-2.1c1: the helper line switch makes dashed aids, the next line is a normal one again.
  add_test(NAME app_sketch_helper_line
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-sketch-helper.png" --language en --size 1280x800
            --camera=0,90,260 --simulate "menu:sketch;click:60,60;expectsketch:active;menu:sketchtool.helper;menu:sketchtool.line;sketchmove:400,300;sketchclick:400,300;sketchclick:600,300;expectsketchelements:line=1,helper=1;menu:sketchtool.helper;sketchclick:600,450;expectsketchelements:line=2,helper=1;menu:sketchtool.point;sketchclick:300,200;expectsketchelements:line=2,point=1,helper=1")
  set_tests_properties(app_sketch_helper_line PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # AP-2.1c1 decision 17: with a drawing tool on, a digit opens the number field (it is not the
  # view "front" of the key 1); Esc closes the field, the element goes on.
  add_test(NAME app_sketch_digits
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-sketch-digits.png" --language en --size 1280x800
            --camera=0,90,260 --simulate "menu:sketch;click:60,60;expectsketch:active;menu:sketchtool.line;sketchmove:400,300;sketchclick:400,300;sketchmove:500,300;key:1;expectsketchtyping:yes;key:Escape;expectsketchtyping:no;expectsketchtool:line;expectsketchelements:")
  set_tests_properties(app_sketch_digits PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # AP-2.1c2: slot and polygon with typed numbers. The keys S and G choose the tools, the command
  # search knows their words (Schlitz, Sechseck, Kreisbogen); the slot takes its length (Enter sets
  # the second middle) and its width, the polygon its radius and number of sides (a wrong number is
  # refused, the last good one stays); one undo step takes a whole slot.
  add_test(NAME app_sketch_slot_polygon
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-sketch-slot-polygon.png" --language en --size 1280x800
            --camera=0,90,260 --simulate "menu:sketch;click:60,60;expectsketch:active;search:Schlitz,sketch.slot;search:Sechseck,sketch.polygon;search:Kreisbogen,sketch.arc;key:A;expectsketchtool:arc;key:S;expectsketchtool:slot;sketchmove:400,300;sketchclick:400,300;sketchmove:600,300;type:40,enter;sketchmove:500,330;type:10,enter;expectsketchelements:line=2,arc=2;expectundo:addSketchElement;menu:undo;expectsketchelements:;menu:redo;expectsketchelements:line=2,arc=2;key:G;expectsketchtool:polygon;sketchmove:800,500;sketchclick:800,500;sketchmove:900,500;type:15,tab,8,enter;expectsketchelements:line=10,arc=2;sketchmove:800,500;sketchclick:800,500;sketchmove:900,500;type:15,tab,2,enter;expectsketchelements:line=18,arc=2;menu:sketchdone;expectsketch:idle;recipe:box,sketch")
  set_tests_properties(app_sketch_slot_polygon PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 120 RUN_SERIAL TRUE)
  # AP-2.1c2: pulling on the end of a line makes an arc that joins it (the chain goes on from its
  # end); a second pull with the button held takes a typed radius.
  add_test(NAME app_sketch_tangent_arc
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-sketch-tangent-arc.png" --language en --size 1280x800
            --camera=0,90,260 --simulate "menu:sketch;click:60,60;expectsketch:active;menu:sketchtool.line;sketchmove:400,300;sketchclick:400,300;sketchclick:600,300;expectsketchelements:line=1;sketchdrag:600,300,700,200;expectsketchelements:line=1,arc=1;sketchclick:700,100;expectsketchelements:line=2,arc=1;sketchdrag:700,100,800,50,hold;type:20,enter;expectsketchelements:line=2,arc=2;expectundo:addSketchElement;key:Escape;expectsketchtool:line;key:Escape;expectsketchtool:;menu:sketchtool.arc;sketchmove:300,500;sketchclick:300,500;sketchclick:500,500;sketchmove:400,430;sketchclick:400,430;expectsketchelements:line=2,arc=3;menu:sketchdone;expectsketch:idle;recipe:box,sketch")
  set_tests_properties(app_sketch_tangent_arc PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 120 RUN_SERIAL TRUE)
  # AP-2.1d: a curve through clicked points. The key K and the words Kurve / Freiform choose the tool;
  # Backspace takes the last point back; a click on the first point closes the curve (one element);
  # a double click sets an open one; Esc throws an unfinished curve away; one undo step per curve.
  add_test(NAME app_sketch_curve_close
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-sketch-curve-close.png" --language en --size 1280x800
            --camera=0,90,260 --simulate "menu:sketch;click:60,60;expectsketch:active;search:Kurve,sketch.spline;search:Freiform,sketch.spline;key:K;expectsketchtool:spline;sketchmove:300,300;sketchclick:300,300;sketchmove:400,200;sketchclick:400,200;sketchmove:500,300;sketchclick:500,300;sketchmove:450,420;sketchclick:450,420;key:Backspace;sketchmove:400,400;sketchclick:400,400;sketchmove:300,300;sketchclick:300,300;expectsketchelements:spline=1;expectundo:addSketchElement;menu:undo;expectsketchelements:;menu:redo;expectsketchelements:spline=1;sketchmove:600,250;sketchclick:600,250;sketchmove:700,350;sketchclick:700,350;sketchmove:800,250;sketchdouble:800,250;expectsketchelements:spline=2;sketchmove:600,500;sketchclick:600,500;sketchmove:700,550;sketchclick:700,550;key:Escape;expectsketchelements:spline=2;expectsketchtool:spline;key:Escape;expectsketchtool:;menu:sketchdone;expectsketch:idle;recipe:box,sketch")
  set_tests_properties(app_sketch_curve_close PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 120 RUN_SERIAL TRUE)
  # AP-2.1e1: trimming. The keys T and E choose the tools (no other shortcut has them), the command
  # search knows their words. A rectangle with a circle on its right edge: trimming the circle's
  # outer half leaves an arc (the box round everything gets smaller), trimming the piece of the
  # edge inside the circle gives two lines; each is one undo step. A stroke over the three pieces of
  # the edge takes them all in one step. Places are given in millimetres on the ground plane, on
  # whole tens: clicks snap to the grid.
  add_test(NAME app_sketch_trim
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-sketch-trim.png" --language en --size 1280x800
            --camera=0,90,260 --simulate "menu:sketch;click:60,60;expectsketch:active;search:abschneiden,sketch.trim;search:trim,sketch.trim;search:anschließen,sketch.extend;search:extend,sketch.extend;menu:sketchtool.rectangle;sketchclickmm:-40,-30;sketchclickmm:40,30;menu:sketchtool.circle;sketchclickmm:40,0;sketchclickmm:60,0;expectsketchelements:line=4,circle=1;expectsketchsize:100,60;key:T;expectsketchtool:trim;sketchmovemm:60,0;sketchclickmm:60,0;expectsketchelements:line=4,arc=1;expectsketchsize:80,60;expectundo:trimSketch;sketchclickmm:40,0;expectsketchelements:line=5,arc=1;expectundo:trimSketch;menu:undo;expectsketchelements:line=4,arc=1;menu:undo;expectsketchelements:line=4,circle=1;menu:redo;expectsketchelements:line=4,arc=1;sketchdragmm:40,-28,40,28;expectsketchelements:line=3,arc=1;expectundo:trimSketch;menu:undo;expectsketchelements:line=4,arc=1;key:Escape;expectsketchtool:;menu:sketchdone;expectsketch:idle;recipe:box,sketch")
  set_tests_properties(app_sketch_trim PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 120 RUN_SERIAL TRUE)
  # AP-2.1e1: extending. A line goes on up to a wall (one undo step); at the wall there is
  # nothing further, so a second click changes nothing.
  add_test(NAME app_sketch_extend
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-sketch-extend.png" --language en --size 1280x800
            --camera=0,90,260 --simulate "menu:sketch;click:60,60;expectsketch:active;key:L;sketchclickmm:-40,0;sketchclickmm:0,0;key:Escape;sketchclickmm:30,-20;sketchclickmm:30,20;key:Escape;expectsketchelements:line=2;key:E;expectsketchtool:extend;sketchmovemm:-3,0;sketchclickmm:-3,0;expectundo:extendSketch;expectsketchelements:line=2;sketchclickmm:-3,0;expectundo:extendSketch;menu:undo;expectundo:addSketchElement;menu:redo;expectundo:extendSketch;key:Escape;expectsketchtool:;menu:sketchdone;expectsketch:idle;recipe:box,sketch")
  set_tests_properties(app_sketch_extend PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 120 RUN_SERIAL TRUE)
  # AP-2.1e2: offsetting and rounding a corner. A rectangle 80 x 40 is offset outwards by a typed
  # 10 mm (a second rectangle, 100 x 60, one undo step), then a corner of the outer one is rounded
  # with a typed 5 mm. The keys O and F choose the tools (F is "frame" outside the sketch).
  add_test(NAME app_sketch_offset_round
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-sketch-offset-round.png" --language en --size 1280x800
            --camera=0,90,260 --simulate "menu:sketch;click:60,60;expectsketch:active;search:wandstärke,sketch.offset;search:verrunden,sketch.fillet;search:bevel corner,sketch.chamfer;menu:sketchtool.rectangle;sketchclickmm:-40,-20;sketchclickmm:40,20;expectsketchelements:line=4;expectsketchsize:80,40;key:O;expectsketchtool:offset;sketchclickmm:0,-20;sketchmovemm:0,-30;type:10,enter;expectsketchelements:line=8;expectsketchsize:100,60;expectundo:offsetSketch;key:F;expectsketchtool:fillet;sketchmovemm:50,30;type:5,enter;expectsketchelements:line=8,arc=1;expectundo:roundCorner;menu:undo;expectsketchelements:line=8;menu:undo;expectsketchelements:line=4;menu:redo;menu:redo;expectsketchelements:line=8,arc=1;key:Escape;expectsketchtool:;menu:sketchdone;expectsketch:idle;recipe:box,sketch")
  set_tests_properties(app_sketch_offset_round PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 120 RUN_SERIAL TRUE)
  # AP-2.1e3: mirror and pattern. A circle is mirrored at a helper line (the keys M and W choose the
  # tools, Enter goes on from choosing), then the circle is repeated in a row of three with a typed
  # distance. Each is one undo step. Points sit on whole tens: clicks catch on the grid.
  add_test(NAME app_sketch_mirror_pattern
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-sketch-mirror-pattern.png" --language en --size 1280x800
            --camera=0,90,260 --simulate "menu:sketch;click:60,60;expectsketch:active;search:umklappen,sketch.mirror;search:mirror image,sketch.mirror;search:kreismuster,sketch.pattern;search:circular pattern,sketch.pattern;menu:sketchtool.helper;menu:sketchtool.line;sketchclickmm:0,-40;sketchclickmm:0,40;key:Escape;menu:sketchtool.helper;menu:sketchtool.circle;sketchclickmm:30,0;sketchclickmm:40,0;expectsketchelements:line=1,circle=1,helper=1;key:M;expectsketchtool:mirror;sketchclickmm:40,0;key:Return;sketchmovemm:0,10;sketchclickmm:0,10;expectsketchelements:line=1,circle=2,helper=1;expectundo:mirrorSketch;key:W;expectsketchtool:pattern;sketchclickmm:40,0;key:Return;sketchclickmm:60,20;sketchmovemm:60,50;type:3,tab,30,enter;expectsketchelements:line=1,circle=4,helper=1;expectundo:patternSketch;menu:undo;expectsketchelements:line=1,circle=2,helper=1;menu:undo;expectsketchelements:line=1,circle=1,helper=1;menu:redo;menu:redo;expectsketchelements:line=1,circle=4,helper=1;key:Escape;expectsketchtool:;menu:sketchdone;expectsketch:idle;recipe:box,sketch")
  set_tests_properties(app_sketch_mirror_pattern PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 120 RUN_SERIAL TRUE)
  # AP-2.1g: a rectangle 80 x 40 and a circle of radius 10 over the middle of its right side: three
  # closed areas (the hint line counts them), a click chooses one, Esc lets it go; undoing the
  # circle leaves one area, redoing it brings three back. The areas are worked out in the
  # background: the check waits for them. Points sit on whole tens (clicks catch on the grid).
  add_test(NAME app_sketch_regions
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-sketch-regions.png" --language en --size 1280x800
            --camera=0,90,260 --simulate "menu:sketch;click:60,60;expectsketch:active;menu:sketchtool.rectangle;sketchclickmm:-40,-20;sketchclickmm:40,20;expectsketchregions:1,none;menu:sketchtool.circle;sketchclickmm:40,0;sketchclickmm:50,0;key:Escape;expectsketchtool:;expectsketchregions:3,none;expecthint:3 closed areas;sketchclickmm:-20,0;expectsketchregions:3,selected;expecthint:One is chosen;key:Escape;expectsketch:active;expectsketchregions:3,none;menu:undo;expectsketchregions:1,none;menu:redo;expectsketchregions:3,none;menu:sketchdone;expectsketch:idle;recipe:box,sketch")
  set_tests_properties(app_sketch_regions PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 120 RUN_SERIAL TRUE)
  # AP-2.1f: a sketch on the top of the box, the border taken over with the tool U (found by words
  # only this tool carries), one undo step; the box is made wider by a typed value and the border
  # follows (20 x 20 becomes 30 x 20). Nothing is clicked on a grid: the edges come by name.
  add_test(NAME app_sketch_take_over
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-sketch-take-over.png" --language en --size 1280x800
            --camera=0,90,260 --simulate "menu:sketch;expectsketch:picking;click:366,341;expectsketch:active;expectsketchplane:face;search:körperkante,sketch.takeover;search:include edge,sketch.takeover;key:U;expectsketchtool:takeover;sketchtake:F1/box/top;expectsketchlinked:4;expectundo:takeOverEdges;menu:undo;expectsketchlinked:0;menu:redo;expectsketchlinked:4;key:Escape;expectsketchtool:;menu:sketchdone;expectsketch:idle;step:0;value:width,30;expectsketchfollow:30,20;recipe:box,sketch")
  set_tests_properties(app_sketch_take_over PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 120 RUN_SERIAL TRUE)
  # AP-1.7e: one body is split through the menu Body: the cut, and one placing step per part.
  add_test(NAME app_split_menu
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
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
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-number-field-arrows.png" --language en --size 1280x800
            --camera=0,90,260 --simulate "put:box;focus:width;key:Up;near:width,21;key:Shift+Up;near:width,31;key:Alt+Down;near:width,30.9;key:Ctrl+Down;near:width,30.8")
  set_tests_properties(app_number_field_arrows PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env};${_fission_german_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # AP-1.6b3: a shape dropped on a wall lies on it (a quarter turn about X); with Alt it stays up.
  add_test(NAME app_wall_drop
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-wall-drop.png" --language en --size 1280x800
            --camera=20,25,160 --simulate "drop:cylinder,382,274;near:rotX,90;near:rotY,0;near:rotZ,0")
  set_tests_properties(app_wall_drop PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  add_test(NAME app_wall_drop_upright
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-wall-upright.png" --language en --size 1280x800
            --camera=20,25,160 --simulate "drop:cylinder,382,274,1;near:rotX,0")
  set_tests_properties(app_wall_drop_upright PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # AP-1.6c: the arrow pulled up to 1.6 mm short of another body goes all the way: the side of
  # the 23.3 mm wide box (-18.35 + 8.35) touches the other, at a place no grid has (x = -21.65).
  # With snapping off (as with Shift held) it stays where the pointer put it, so the same check
  # must fail.
  add_test(NAME app_snap_catch
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-snap-catch.png" --language en --size 1280x800
            --camera=0,30,160 --simulate "put:box;type:23.3,enter;gizmo:x,48,0,8;near:x,-21.65")
  set_tests_properties(app_snap_catch PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  add_test(NAME app_snap_off_does_not_catch
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-snap-off.png" --language en --size 1280x800
            --camera=0,30,160 --simulate "put:box;type:23.3,enter;snap:off;gizmo:x,48,0,8;near:x,-21.65")
  set_tests_properties(app_snap_off_does_not_catch PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE WILL_FAIL TRUE)
  # AP-1.6c2: the same for the size handle of a box that is pulled towards another one: with a grid
  # of 3 mm the width would be 39 or 42, the sides touch at 40. The pull ends 0.4 mm short of it.
  add_test(NAME app_snap_size_handle
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-snap-size-handle.png" --language en --size 1280x800
            --camera=0,30,160 --simulate "put:box;tool:size;grid:3;grab:width,57,0,8;near:width,40")
  set_tests_properties(app_snap_size_handle PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  add_test(NAME app_snap_size_handle_off
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-snap-size-handle-off.png" --language en --size 1280x800
            --camera=0,30,160 --simulate "put:box;tool:size;grid:3;snap:off;grab:width,57,0,8;near:width,40")
  set_tests_properties(app_snap_size_handle_off PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE WILL_FAIL TRUE)
  # The corner of a 7 mm cube pulled until its top is level with the 20 mm box: 285.7 %, no whole
  # percent (the pull alone ends at about 276 %).
  add_test(NAME app_snap_corner_handle
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-snap-corner.png" --language en --size 1280x800
            --camera=0,30,160 --simulate "put:box;type:20,tab,20,tab,7,enter;tool:size;corner:1,220,220,8;near:height,20")
  set_tests_properties(app_snap_corner_handle PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  add_test(NAME app_snap_corner_handle_off
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-snap-corner-off.png" --language en --size 1280x800
            --camera=0,30,160 --simulate "put:box;type:20,tab,20,tab,7,enter;tool:size;snap:off;corner:1,220,220,8;near:height,20")
  set_tests_properties(app_snap_corner_handle_off PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE WILL_FAIL TRUE)
  # A box dropped from the gallery 0.4 mm beside the first one: with a grid of 3 mm its middle would
  # be at 21, the sides touch at 20.
  add_test(NAME app_snap_drop
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-snap-drop.png" --language en --size 1280x800
            --camera=0,30,160 --simulate "grid:3;drop:box,462,312;near:x,20")
  set_tests_properties(app_snap_drop PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  add_test(NAME app_snap_drop_off
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-snap-drop-off.png" --language en --size 1280x800
            --camera=0,30,160 --simulate "grid:3;snap:off;drop:box,462,312;near:x,20")
  set_tests_properties(app_snap_drop_off PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE WILL_FAIL TRUE)
  # AP-1.6d, acceptance criterion 1 of AP-1.6: a shape stands exactly 5 mm from the edge of the bed
  # by its distance measure. The box put down stands at x = -30 (20 mm wide): its left side is
  # then 5 mm from the edge at x = -128 + 5 + 10 = -113.
  add_test(NAME app_distance_five_from_edge
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-distance.png" --language en --size 1280x800
            --camera=20,35,170 --simulate "put:box;tool:move;distance:left,5;near:x,-113")
  set_tests_properties(app_distance_five_from_edge PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # The same from the other side: 5 mm from the right edge, the middle at x = 128 - 5 - 10 = 113.
  # (The box put down is beside the first one, not in line with it: no body is in the way.)
  add_test(NAME app_distance_other_side
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-distance-right.png" --language en --size 1280x800
            --camera=20,35,170 --simulate "put:box;tool:move;distance:right,5;near:x,113")
  set_tests_properties(app_distance_other_side PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # AP-1.6e: three boxes lined up on the left along X. The demo box is at x = 0, the first box put
  # down at x = -30 (its left side at -40), the second at x = 0: all three end at x = -30.
  add_test(NAME app_align_left
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-align.png" --language en --size 1280x800
            --camera=20,35,200 --simulate "put:box;put:box;selectall;align:x,min;step:0;near:x,-30;step:2;near:x,-30")
  set_tests_properties(app_align_left PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # A box lifted with the Z arrow falls back on the bed with "Drop to bed" (the key D).
  add_test(NAME app_drop_to_bed
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-drop.png" --language en --size 1280x800
            --camera=20,35,200 --simulate "put:box;gizmo:z,0,-60,8;least:z,5;menu:dropToBed;near:z,0")
  set_tests_properties(app_drop_to_bed PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # Duplicate: the copy lies 10 mm to the side and to the back of the box put down at (-30, -30),
  # and is the chosen one.
  add_test(NAME app_duplicate
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-duplicate.png" --language en --size 1280x800
            --camera=20,35,200 --simulate "put:box;menu:duplicate;near:x,-20;near:y,-20")
  set_tests_properties(app_duplicate PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # Delete: the chosen shape goes (its step with it); Undo brings the same step back.
  add_test(NAME app_delete_step
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-delete.png" --language en --size 1280x800
            --camera=20,35,200 --simulate "put:cylinder;menu:delete;recipe:box;menu:undo;recipe:box,cylinder")
  set_tests_properties(app_delete_step PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # Time marker: pulled back behind two steps, a new step goes in at it; put back, nothing is lost.
  add_test(NAME app_time_marker
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-marker.png" --language de --size 1280x800
            --camera=20,35,200
            --simulate "put:cylinder;put:sphere;marker:2;put:cone;recipe:box,cylinder,cone,sphere;marker:-1;recipe:box,cylinder,cone,sphere")
  set_tests_properties(app_time_marker PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # The time marker is pulled up with the mouse (real mouse events through the window).
  add_test(NAME app_time_marker_drag
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-marker-drag.png" --language de --size 1280x800
            --camera=20,35,200
            --simulate "put:cylinder;put:sphere;markerdrag:-150,8;markerset;recipe:box,cylinder,sphere")
  set_tests_properties(app_time_marker_drag PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # A double click on a row of the recipe renames the step (real mouse events).
  add_test(NAME app_recipe_rename
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-recipe-rename.png" --language de --size 1280x800
            --camera=20,35,200 --simulate "put:cylinder;rowdouble:1;rowediting:1")
  set_tests_properties(app_recipe_rename PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # The same for a row of the body list.
  add_test(NAME app_body_rename
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-body-rename.png" --language de --size 1280x800
            --camera=20,35,200 --simulate "put:cylinder;rowdouble:b1;rowediting:b1")
  set_tests_properties(app_body_rename PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # Right click on a row of the recipe, "Rename" in its menu: the row is being renamed.
  add_test(NAME app_recipe_menu_rename
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-recipe-menu.png" --language de --size 1280x800
            --camera=20,35,200
            --simulate "put:cylinder;rightclickitem:recipeRow_1;clickitem:menu_rename;rowediting:1")
  set_tests_properties(app_recipe_menu_rename PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # ... and "Hide" in the same menu hides the step (the body list loses the body).
  add_test(NAME app_recipe_menu_hide
    COMMAND ${_fission_gui_runner} ${_fission_app_exe}
            --smoke-test "${CMAKE_BINARY_DIR}/window-recipe-hide.png" --language de --size 1280x800
            --camera=20,35,200
            --simulate "put:cylinder;rightclickitem:recipeRow_1;clickitem:menu_hide;recipe:box,cylinder")
  set_tests_properties(app_recipe_menu_hide PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
  # AP-1.10: save and open, a crash (kill -9) and the rescue card, quitting without saving, and the
  # export files, all through the real program (the steps the person would try by hand).
  add_test(NAME app_files_end_to_end
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/ui/e2e_files.py"
            ${_fission_app_exe} ${_fission_gui_runner})
  set_tests_properties(app_files_end_to_end PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 600 RUN_SERIAL TRUE)
  # Meilenstein M1, flow A (the keychain): plate, text, hole with a distance, red text, 3MF.
  add_test(NAME app_flow_a_keychain
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/ui/e2e_keychain.py"
            ${_fission_app_exe} ${_fission_gui_runner})
  set_tests_properties(app_flow_a_keychain PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 240 RUN_SERIAL TRUE)
  add_test(NAME app_start_time
    COMMAND ${_fission_gui_runner} ${Python3_EXECUTABLE}
            "${CMAKE_SOURCE_DIR}/scripts/tools/check_startup.py" ${_fission_app_exe})
  set_tests_properties(app_start_time PROPERTIES
    ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env};${_fission_budget_env}" TIMEOUT 180)

  # Window tests that do not fit the packaged program on macOS, each with its reason. They are
  # switched off visibly ("Not Run (Disabled)" in the ctest list), never weakened. Empty so far:
  # the first CI runs show which ones need an entry (see docs/arbeitspakete/anleitungen/pflege.md, P22).
  set(_fission_app_tests_not_on_macos "")
  if(APPLE AND FISSION_APP_UNDER_TEST)
    foreach(_test IN LISTS _fission_app_tests_not_on_macos)
      set_tests_properties(${_test} PROPERTIES DISABLED TRUE)
    endforeach()
  endif()
endif()

endif()
