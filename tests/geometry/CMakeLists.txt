# SPDX-FileCopyrightText: Fission3D contributors
# SPDX-License-Identifier: GPL-3.0-or-later

# Kernel robustness catalog (AP-0.5): typical maker cases run through OpenCASCADE and Manifold.
# Like the smoke tests these use the libraries directly on purpose. Result table:
#   FISSION_GEOMETRY_REPORT=report.csv build/<preset>/tests/geometry/fission_geometry_tests
#   python3 scripts/tools/geometry_report.py report.csv

add_executable(fission_geometry_tests
  support.cpp
  test_booleans.cpp
  test_fillets.cpp
  test_text.cpp
  test_sweeps.cpp
  test_io.cpp
  test_manifold.cpp)

target_link_libraries(fission_geometry_tests PRIVATE
  Catch2::Catch2WithMain
  ${FISSION_OCCT_TARGETS}
  ${FISSION_OCCT_EXTRA_TARGETS}
  manifold::manifold)
target_compile_definitions(fission_geometry_tests PRIVATE
  FISSION_TEST_FIXTURES_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures")
fission_apply_compiler_options(fission_geometry_tests)

catch_discover_tests(fission_geometry_tests TEST_PREFIX "geometry: ")
