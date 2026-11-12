# SPDX-FileCopyrightText: Fission359 contributors
# SPDX-License-Identifier: GPL-3.0-or-later
# WebAssembly (Emscripten) with pthreads, static libraries, release only (ADR-0014, AP-6.7a).
set(VCPKG_ENV_PASSTHROUGH_UNTRACKED EMSCRIPTEN_ROOT EMSDK PATH)

# Locate the Emscripten SDK: $EMSCRIPTEN_ROOT, else emcc on the PATH, else $EMSDK/upstream/emscripten.
if(DEFINED ENV{EMSCRIPTEN_ROOT} AND NOT "$ENV{EMSCRIPTEN_ROOT}" STREQUAL "")
  set(EMSCRIPTEN_ROOT "$ENV{EMSCRIPTEN_ROOT}")
else()
  find_path(EMSCRIPTEN_ROOT NAMES emcc)
  if(NOT EMSCRIPTEN_ROOT)
    # find_path does not search the PATH; an activated emsdk puts emcc there.
    find_program(_fission_emcc NAMES emcc)
    if(_fission_emcc)
      get_filename_component(EMSCRIPTEN_ROOT "${_fission_emcc}" DIRECTORY)
    endif()
  endif()
  if(NOT EMSCRIPTEN_ROOT)
    if(DEFINED ENV{EMSDK} AND NOT "$ENV{EMSDK}" STREQUAL "" AND IS_DIRECTORY "$ENV{EMSDK}/upstream/emscripten")
      set(EMSCRIPTEN_ROOT "$ENV{EMSDK}/upstream/emscripten")
    else()
      message(FATAL_ERROR "Emscripten not found: activate emsdk (source emsdk_env.sh) first")
    endif()
  endif()
endif()
set(ENV{EMSCRIPTEN_ROOT} "${EMSCRIPTEN_ROOT}")

set(VCPKG_TARGET_ARCHITECTURE wasm32)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_BUILD_TYPE release)
set(VCPKG_CMAKE_SYSTEM_NAME Emscripten)
set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${VCPKG_ROOT_DIR}/scripts/toolchains/emscripten.cmake")
# Native WebAssembly exceptions: without them Emscripten cannot catch C++ exceptions, and every
# error the program reports by throwing (OpenCASCADE failures, broken files) ends the program.
# The current exception format (exnref, not the legacy one): Binaryen cannot read the legacy
# format of the large test programs. All libraries and the program must use the same setting.
set(VCPKG_C_FLAGS "-pthread -fwasm-exceptions -sWASM_LEGACY_EXCEPTIONS=0")
set(VCPKG_CXX_FLAGS "-pthread -fwasm-exceptions -sWASM_LEGACY_EXCEPTIONS=0")
set(VCPKG_LINKER_FLAGS "-pthread -fwasm-exceptions -sWASM_LEGACY_EXCEPTIONS=0")
# -O2 instead of the default -O3: clearly less memory in the compiler for OpenCASCADE's largest files.
set(VCPKG_C_FLAGS_RELEASE "-O2 -DNDEBUG")
set(VCPKG_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
