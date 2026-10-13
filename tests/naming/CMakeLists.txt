# SPDX-FileCopyrightText: Fission3D contributors
# SPDX-License-Identifier: GPL-3.0-or-later

# Persistent references prototype (AP-0.6): names for faces and edges that survive changes, and
# a measurement harness that judges every resolved reference against geometric truth.
# This is a spike. Production code arrives with AP-2.10 in src/core and src/geometry/occt.
#   FISSION_NAMING_REPORT=report.csv build/<preset>/tests/naming/fission_naming_tests

add_executable(fission_naming_tests
  naming.cpp
  features.cpp
  truth.cpp
  report.cpp
  test_scenarios.cpp
  test_evil.cpp
  test_random.cpp)

target_link_libraries(fission_naming_tests PRIVATE
  Catch2::Catch2WithMain
  ${FISSION_OCCT_TARGETS})
fission_apply_compiler_options(fission_naming_tests)

# The sensitivity runs (same random chains with parts of the design switched off) take minutes;
# run them by hand: fission_naming_tests "[sensitivity]"
catch_discover_tests(fission_naming_tests TEST_PREFIX "naming: " TEST_SPEC "~[sensitivity]")
