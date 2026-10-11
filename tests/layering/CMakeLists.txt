# SPDX-FileCopyrightText: Fission3D contributors
# SPDX-License-Identifier: GPL-3.0-or-later

# Proof that the layer rule is enforced by the compiler, not only by convention:
#  - the control file uses only what core offers and must compile;
#  - the violation file includes a Qt header while seeing only core's include paths and
#    must be rejected.
# Both targets are excluded from the normal build; the tests below build them on demand.

add_library(fission_layering_control OBJECT EXCLUDE_FROM_ALL control_core_only.cpp)
target_link_libraries(fission_layering_control PRIVATE fission_core)

add_library(fission_layering_violation_qt_in_core OBJECT EXCLUDE_FROM_ALL
  violation_core_includes_qt.cpp)
target_link_libraries(fission_layering_violation_qt_in_core PRIVATE fission_core)

add_test(NAME layering_control_builds
  COMMAND ${CMAKE_COMMAND} --build "${CMAKE_BINARY_DIR}" --target fission_layering_control)

add_test(NAME layering_rejects_qt_in_core
  COMMAND ${CMAKE_COMMAND}
          -DBUILD_DIR=${CMAKE_BINARY_DIR}
          -DTARGET=fission_layering_violation_qt_in_core
          "-DEXPECT_REGEX=QString"
          -P "${CMAKE_CURRENT_SOURCE_DIR}/expect_build_failure.cmake")
