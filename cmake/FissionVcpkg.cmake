# SPDX-FileCopyrightText: Fission359 contributors
# SPDX-License-Identifier: GPL-3.0-or-later

# Must be included BEFORE project(): it selects the vcpkg toolchain, the triplet and the
# manifest features (whether Qt comes from vcpkg or from the system).
#
#   FISSION_QT_SOURCE = auto (default) | system | vcpkg
#     auto   : use a system Qt >= FISSION_QT_MIN_VERSION if one is found, else build Qt with vcpkg
#     system : require the system Qt
#     vcpkg  : always build Qt with vcpkg (what CI uses; reproducible)
#
#   vcpkg itself: $VCPKG_ROOT if set, otherwise <repo>/.vcpkg (created by scripts/setup). A
#   second working tree of the repository (git worktree) has no .vcpkg of its own: it uses the
#   one of the main working tree, so that the libraries are not built twice.

# WebAssembly build of the Qt-free core (AP-6.7a, ADR-0014): ON builds only the Qt-free
# libraries and their tests with the Emscripten triplet; Qt is neither probed nor built.
option(FISSION_WEB_CORE "Build only the Qt-free libraries and their tests for WebAssembly (AP-6.7a)" OFF)

set(FISSION_QT_MIN_VERSION 6.8)
# The environment variable of the same name sets the default (used by CI: FISSION_QT_SOURCE=vcpkg).
set(_fission_qt_source_default "auto")
if(DEFINED ENV{FISSION_QT_SOURCE} AND NOT "$ENV{FISSION_QT_SOURCE}" STREQUAL "")
  set(_fission_qt_source_default "$ENV{FISSION_QT_SOURCE}")
endif()
set(FISSION_QT_SOURCE "${_fission_qt_source_default}" CACHE STRING
    "Where Qt comes from: auto, system or vcpkg")
set_property(CACHE FISSION_QT_SOURCE PROPERTY STRINGS auto system vcpkg)

# --- vcpkg toolchain ---------------------------------------------------------------
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VCPKG_ROOT} AND NOT "$ENV{VCPKG_ROOT}" STREQUAL "")
    set(_fission_vcpkg_root "$ENV{VCPKG_ROOT}")
  else()
    set(_fission_vcpkg_root "${CMAKE_SOURCE_DIR}/.vcpkg")
    if(NOT EXISTS "${_fission_vcpkg_root}/scripts/buildsystems/vcpkg.cmake")
      # A git worktree: the main working tree is the folder that holds the common .git.
      find_package(Git QUIET)
      if(GIT_FOUND)
        execute_process(
          COMMAND "${GIT_EXECUTABLE}" rev-parse --path-format=absolute --git-common-dir
          WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
          OUTPUT_VARIABLE _fission_git_common
          OUTPUT_STRIP_TRAILING_WHITESPACE
          RESULT_VARIABLE _fission_git_result
          ERROR_QUIET)
        if(_fission_git_result EQUAL 0 AND NOT _fission_git_common STREQUAL "")
          get_filename_component(_fission_main_tree "${_fission_git_common}" DIRECTORY)
          if(EXISTS "${_fission_main_tree}/.vcpkg/scripts/buildsystems/vcpkg.cmake")
            set(_fission_vcpkg_root "${_fission_main_tree}/.vcpkg")
            message(STATUS "vcpkg aus dem Hauptordner: ${_fission_vcpkg_root}")
          endif()
        endif()
      endif()
    endif()
  endif()
  set(_fission_toolchain "${_fission_vcpkg_root}/scripts/buildsystems/vcpkg.cmake")
  if(NOT EXISTS "${_fission_toolchain}")
    message(FATAL_ERROR
      "vcpkg wurde nicht gefunden (${_fission_vcpkg_root}).\n"
      "Bitte zuerst  scripts/setup  ausführen. / Please run scripts/setup first.")
  endif()
  set(CMAKE_TOOLCHAIN_FILE "${_fission_toolchain}" CACHE FILEPATH "vcpkg toolchain" FORCE)
endif()

# --- triplet ------------------------------------------------------------------------
set(VCPKG_OVERLAY_TRIPLETS "${CMAKE_SOURCE_DIR}/cmake/vcpkg-triplets" CACHE STRING "")
# The native triplet of the build host, computed once. The target uses it directly, except
# for the WebAssembly build (FISSION_WEB_CORE), which cross-compiles with its own triplet
# while the host tools stay on the native one.
cmake_host_system_information(RESULT _fission_arch QUERY OS_PLATFORM)
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin" AND _fission_arch STREQUAL "arm64")
  set(_fission_native_triplet "fission-arm64-osx")
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux" AND _fission_arch STREQUAL "x86_64")
  set(_fission_native_triplet "fission-x64-linux")
else()
  message(FATAL_ERROR
    "Nicht unterstützte Plattform: ${CMAKE_HOST_SYSTEM_NAME}/${_fission_arch} "
    "(unterstützt: macOS arm64, Linux x86_64).")
endif()

if(FISSION_WEB_CORE)
  # WebAssembly (AP-6.7a): Emscripten target triplet plus the overlay ports (e.g. lib3mf
  # with Emscripten support, cmake/ports).
  set(VCPKG_TARGET_TRIPLET "fission-wasm32-emscripten" CACHE STRING "")
  set(VCPKG_OVERLAY_PORTS "${CMAKE_SOURCE_DIR}/cmake/ports" CACHE STRING "")
  # The project itself is compiled with emcc: chainload the Emscripten toolchain.
  if(DEFINED ENV{EMSCRIPTEN_ROOT} AND NOT "$ENV{EMSCRIPTEN_ROOT}" STREQUAL "")
    set(_fission_emscripten_root "$ENV{EMSCRIPTEN_ROOT}")
  elseif(DEFINED ENV{EMSDK} AND NOT "$ENV{EMSDK}" STREQUAL "")
    set(_fission_emscripten_root "$ENV{EMSDK}/upstream/emscripten")
  else()
    message(FATAL_ERROR
      "Emscripten wurde nicht gefunden (weder EMSCRIPTEN_ROOT noch EMSDK ist gesetzt). "
      "Bitte zuerst emsdk aktivieren (source emsdk_env.sh). / "
      "Emscripten not found: activate emsdk first (source emsdk_env.sh).")
  endif()
  if(NOT DEFINED VCPKG_CHAINLOAD_TOOLCHAIN_FILE)
    set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE
        "${_fission_emscripten_root}/cmake/Modules/Platform/Emscripten.cmake"
        CACHE FILEPATH "")
  endif()
endif()
if(NOT DEFINED VCPKG_TARGET_TRIPLET)
  set(VCPKG_TARGET_TRIPLET "${_fission_native_triplet}" CACHE STRING "")
endif()

# Binaries must run on the oldest macOS we support. The vcpkg triplet builds the libraries
# for the same version (cmake/vcpkg-triplets/fission-arm64-osx.cmake).
set(FISSION_MACOS_MIN_VERSION "14.0" CACHE INTERNAL "Minimum macOS version of the app")
if(NOT FISSION_WEB_CORE AND CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin" AND NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "${FISSION_MACOS_MIN_VERSION}" CACHE STRING
      "Minimum macOS version")
endif()

# Build tools run on the host, so they always use the native triplet -- also for the
# WebAssembly build (FISSION_WEB_CORE), where only the target is cross-compiled. Without
# this vcpkg builds a second, full-featured copy of Qt (ICU, OpenSSL, PostgreSQL, Widgets,
# debug and release) just to run moc and friends, which costs hours and extra system packages.
if(NOT DEFINED VCPKG_HOST_TRIPLET)
  set(VCPKG_HOST_TRIPLET "${_fission_native_triplet}" CACHE STRING "")
endif()

# --- vcpkg behaviour ------------------------------------------------------------------
set(ENV{VCPKG_DISABLE_METRICS} "1")
# Remove build trees after each port; they are huge (Qt: many GB) and not needed afterwards.
set(VCPKG_INSTALL_OPTIONS "--clean-after-build" CACHE STRING "")

# --- Qt: system or vcpkg? -------------------------------------------------------------
if(NOT FISSION_QT_SOURCE MATCHES "^(auto|system|vcpkg)$")
  message(FATAL_ERROR "FISSION_QT_SOURCE must be auto, system or vcpkg (is '${FISSION_QT_SOURCE}')")
endif()

set(_fission_qt_from_vcpkg OFF)
if(FISSION_WEB_CORE)
  # WebAssembly core (AP-6.7a): the browser build has no Qt -- skip the system-Qt probe
  # completely and never build Qt with vcpkg.
elseif(FISSION_QT_SOURCE STREQUAL "vcpkg")
  set(_fission_qt_from_vcpkg ON)
elseif(FISSION_QT_SOURCE STREQUAL "auto")
  set(_probe_args "-DFISSION_QT_MIN_VERSION=${FISSION_QT_MIN_VERSION}")
  foreach(_var CMAKE_PREFIX_PATH Qt6_DIR)
    if(DEFINED ${_var})
      list(APPEND _probe_args "-D${_var}=${${_var}}")
    endif()
  endforeach()
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -S "${CMAKE_SOURCE_DIR}/cmake/qt-probe"
            -B "${CMAKE_BINARY_DIR}/qt-probe" ${_probe_args}
    OUTPUT_VARIABLE _probe_output ERROR_VARIABLE _probe_output
    RESULT_VARIABLE _probe_result)
  if(_probe_result EQUAL 0 AND _probe_output MATCHES "FISSION_QT_PROBE_RESULT=FOUND ([0-9.]+)")
    message(STATUS "Qt: using system Qt ${CMAKE_MATCH_1} (FISSION_QT_SOURCE=auto)")
  else()
    set(_fission_qt_from_vcpkg ON)
    message(STATUS "Qt: no system Qt >= ${FISSION_QT_MIN_VERSION} found, building Qt with vcpkg "
                   "(first run takes a long time)")
  endif()
endif()

if(_fission_qt_from_vcpkg)
  list(APPEND VCPKG_MANIFEST_FEATURES qt)
endif()
set(FISSION_QT_FROM_VCPKG ${_fission_qt_from_vcpkg} CACHE INTERNAL "Qt is built by vcpkg")
