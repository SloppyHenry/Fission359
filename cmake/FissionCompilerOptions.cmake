# SPDX-FileCopyrightText: Fission359 contributors
# SPDX-License-Identifier: GPL-3.0-or-later

# Compiler warnings and sanitizers shared by all Fission359 targets.
# emcc is Clang, so the warning flags below are also accepted by the WebAssembly build.

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

# AP-6.7a: link options for the test executables of the WebAssembly build, which run under
# Node. NODERAWFS lets the tests read the fixtures from the real disk; the pthread pool has
# 4 workers (ADR-0014). No-op when not cross-compiling with Emscripten.
function(fission_web_test_options target)
  if(EMSCRIPTEN)
    target_link_options(${target} PRIVATE
      -sPTHREAD_POOL_SIZE=4 -sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=4GB -sNODERAWFS=1
      -sEXIT_RUNTIME=1 -sSTACK_SIZE=5MB)
    # The emulator that runs the tests: the Node of emsdk, else any node on the PATH.
    if(DEFINED ENV{EMSDK_NODE} AND NOT "$ENV{EMSDK_NODE}" STREQUAL "")
      set(_fission_node "$ENV{EMSDK_NODE}")
    else()
      find_program(FISSION_NODE node REQUIRED)
      set(_fission_node "${FISSION_NODE}")
    endif()
    # Node before 24 knows the current WebAssembly exception format only behind a flag.
    set_target_properties(${target} PROPERTIES CROSSCOMPILING_EMULATOR
      "${_fission_node};--experimental-wasm-exnref")
  endif()
endfunction()
