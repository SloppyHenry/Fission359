# SPDX-FileCopyrightText: Fission359 contributors
# SPDX-License-Identifier: GPL-3.0-or-later

# Persistent references prototype (AP-0.6): names for faces and edges that survive changes, and
# a measurement harness that judges every resolved reference against geometric truth.
# This is a spike. Production code arrives with AP-2.10 in src/core and src/geometry/occt.
#   FISSION_NAMING_REPORT=report.csv build/<preset>/tests/naming/fission_naming_tests
#   build/<preset>/tests/naming/fission_naming_tests "[sensitivity]"   (minutes, not in ctest)

add_library(fission_naming_proto OBJECT
  naming.cpp
  recipe.cpp
  truth.cpp
  report.cpp)
target_link_libraries(fission_naming_proto PUBLIC
  Catch2::Catch2
  ${FISSION_OCCT_TARGETS})
fission_apply_compiler_options(fission_naming_proto)

add_executable(fission_naming_tests
  test_scenarios.cpp
  test_evil.cpp
  test_random.cpp)
target_link_libraries(fission_naming_tests PRIVATE fission_naming_proto Catch2::Catch2WithMain)
fission_apply_compiler_options(fission_naming_tests)

# The sensitivity runs (same random chains with parts of the design switched off) take minutes;
# run them by hand.
catch_discover_tests(fission_naming_tests TEST_PREFIX "naming: " TEST_SPEC "~[sensitivity]")

# OpenCASCADE's own naming framework (OCAF / TNaming), linked only here. The executable also
# carries the LeakSanitizer exception for a leak inside OpenCASCADE (see test_ocaf.cpp).
add_executable(fission_ocaf_tests test_ocaf.cpp)
target_link_libraries(fission_ocaf_tests PRIVATE
  fission_naming_proto
  Catch2::Catch2WithMain
  ${FISSION_OCCT_OCAF_TARGETS})
fission_apply_compiler_options(fission_ocaf_tests)
catch_discover_tests(fission_ocaf_tests TEST_PREFIX "naming: ")
