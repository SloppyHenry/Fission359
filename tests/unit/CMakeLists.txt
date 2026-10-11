# SPDX-FileCopyrightText: Fission3D contributors
# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(fission_unit_tests
  core/test_version.cpp)

target_link_libraries(fission_unit_tests PRIVATE fission_core Catch2::Catch2WithMain)
fission_apply_compiler_options(fission_unit_tests)

catch_discover_tests(fission_unit_tests)
