# SPDX-FileCopyrightText: Fission359 contributors
# SPDX-License-Identifier: GPL-3.0-or-later

# Builds TARGET in BUILD_DIR and succeeds only if the build FAILS and the compiler output
# matches EXPECT_REGEX (so an unrelated typo cannot make the test pass by accident).

execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${BUILD_DIR}" --target "${TARGET}"
  RESULT_VARIABLE _result
  OUTPUT_VARIABLE _output
  ERROR_VARIABLE _error)

if(_result EQUAL 0)
  message(FATAL_ERROR
    "Layer rule not enforced: ${TARGET} compiled although it must be rejected.")
endif()

if(NOT "${_output}${_error}" MATCHES "${EXPECT_REGEX}")
  message(FATAL_ERROR
    "${TARGET} failed, but not for the expected reason (${EXPECT_REGEX}):\n${_output}\n${_error}")
endif()

message(STATUS "OK: ${TARGET} was rejected as intended.")
