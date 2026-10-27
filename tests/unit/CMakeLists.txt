# SPDX-FileCopyrightText: Fission3D contributors
# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(fission_unit_tests
  core/test_version.cpp
  core/test_expression.cpp
  core/test_measures.cpp
  core/test_document.cpp
  core/test_recompute.cpp
  core/test_history.cpp
  core/test_selection.cpp
  core/test_scaling.cpp
  core/test_cuts.cpp
  core/test_serialize.cpp
  features/test_primitives.cpp
  features/test_combine.cpp
  features/test_split.cpp
  features/test_text_feature.cpp
  services/test_settings.cpp
  io/test_fission_file.cpp
  io/test_export.cpp
  geometry/test_occt_layer.cpp
  geometry/test_solids.cpp
  geometry/test_shapes.cpp
  geometry/test_rotation.cpp
  geometry/test_booleans_gal.cpp
  geometry/test_seat.cpp
  geometry/test_text.cpp
  render/test_camera.cpp
  render/test_navigation.cpp
  render/test_picking.cpp
  render/test_pick.cpp
  render/test_scene_renderer.cpp
  render/test_detail.cpp
  ui/test_theme_tokens.cpp
  ui/test_numbers.cpp
  ui/test_document_model.cpp
  ui/test_file_controller.cpp
  ui/test_preferences.cpp
  ui/test_export_controller.cpp
  ui/test_selection_controller.cpp
  ui/test_placement.cpp
  ui/test_handles.cpp
  ui/test_shape_previews.cpp
  ui/test_viewport_navigation.cpp
  ui/test_printer_catalog.cpp
  ui/test_gizmo.cpp
  ui/test_rotate.cpp
  ui/test_scale.cpp
  ui/test_distances.cpp
  ui/test_body_edits.cpp
  ui/test_align.cpp
  interaction/test_drag.cpp
  interaction/test_snap.cpp
  interaction/test_distances.cpp
  interaction/test_align.cpp)

target_link_libraries(fission_unit_tests PRIVATE
  fission_core fission_services fission_io lib3mf::lib3mf fission_features fission_geometry_occt fission_render fission_interaction fission_ui Catch2::Catch2WithMain)
fission_apply_compiler_options(fission_unit_tests)

target_include_directories(fission_unit_tests SYSTEM PRIVATE "${FISSION_LIB3MF_CPP_INCLUDE_DIR}")

target_compile_definitions(fission_unit_tests PRIVATE
  FISSION_TEST_FIXTURES_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures"
  FISSION_APP_FONTS_DIR="${CMAKE_SOURCE_DIR}/apps/fission3d/resources/fonts")

# Under AddressSanitizer, known leaks of third-party libraries are listed in tests/lsan.supp.
if(FISSION_SANITIZE MATCHES "address")
  catch_discover_tests(fission_unit_tests
    PROPERTIES ENVIRONMENT "LSAN_OPTIONS=suppressions=${CMAKE_SOURCE_DIR}/tests/lsan.supp")
else()
  catch_discover_tests(fission_unit_tests)
endif()
