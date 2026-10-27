# SPDX-FileCopyrightText: Fission359 contributors
# SPDX-License-Identifier: GPL-3.0-or-later

# Compiler warnings and sanitizers shared by all Fission359 targets.

function(fission_apply_compiler_options target)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
    if(FISSION_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
  endif()

  if(FISSION_COVERAGE)
    # Clang: source-based coverage (llvm-cov); GCC: gcov. scripts/tools/coverage.py reads both.
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
      target_compile_options(${target} PRIVATE -fprofile-instr-generate -fcoverage-mapping)
      target_link_options(${target} PRIVATE -fprofile-instr-generate)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      target_compile_options(${target} PRIVATE --coverage -O0)
      target_link_options(${target} PRIVATE --coverage)
    endif()
  endif()

  if(FISSION_SANITIZE)
    list(JOIN FISSION_SANITIZE "," _sanitizers)
    target_compile_options(${target} PRIVATE
      -fsanitize=${_sanitizers} -fno-omit-frame-pointer)
    target_link_options(${target} PRIVATE -fsanitize=${_sanitizers})
  endif()
endfunction()
