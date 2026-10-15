# SPDX-FileCopyrightText: Fission3D contributors
# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(fission_unit_tests
  core/test_version.cpp
  core/test_expression.cpp
  core/test_measures.cpp
  core/test_document.cpp
  core/test_recompute.cpp
  core/test_history.cpp
  features/test_primitives.cpp
  geometry/test_occt_layer.cpp
  geometry/test_solids.cpp
  render/test_camera.cpp
  render/test_navigation.cpp
  render/test_picking.cpp
  ui/test_theme_tokens.cpp
  ui/test_numbers.cpp
  ui/test_document_model.cpp
  ui/test_viewport_navigation.cpp)

target_link_libraries(fission_unit_tests PRIVATE
  fission_core fission_features fission_geometry_occt fission_render fission_ui Catch2::Catch2WithMain)
fission_apply_compiler_options(fission_unit_tests)

catch_discover_tests(fission_unit_tests)
