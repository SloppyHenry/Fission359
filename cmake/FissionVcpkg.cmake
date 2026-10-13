# SPDX-FileCopyrightText: Fission3D contributors
# SPDX-License-Identifier: GPL-3.0-or-later

# Must be included BEFORE project(): it selects the vcpkg toolchain, the triplet and the
# manifest features (whether Qt comes from vcpkg or from the system).
#
#   FISSION_QT_SOURCE = auto (default) | system | vcpkg
#     auto   : use a system Qt >= FISSION_QT_MIN_VERSION if one is found, else build Qt with vcpkg
#     system : require the system Qt
#     vcpkg  : always build Qt with vcpkg (what CI uses; reproducible)
#
#   vcpkg itself: $VCPKG_ROOT if set, otherwise <repo>/.vcpkg (created by scripts/setup).

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
if(NOT DEFINED VCPKG_TARGET_TRIPLET)
  cmake_host_system_information(RESULT _fission_arch QUERY OS_PLATFORM)
  if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin" AND _fission_arch STREQUAL "arm64")
    set(VCPKG_TARGET_TRIPLET "fission-arm64-osx" CACHE STRING "")
  elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux" AND _fission_arch STREQUAL "x86_64")
    set(VCPKG_TARGET_TRIPLET "fission-x64-linux" CACHE STRING "")
  else()
    message(FATAL_ERROR
      "Nicht unterstützte Plattform: ${CMAKE_HOST_SYSTEM_NAME}/${_fission_arch} "
      "(unterstützt: macOS arm64, Linux x86_64).")
  endif()
endif()

# Binaries must run on the oldest macOS we support. The vcpkg triplet builds the libraries
# for the same version (cmake/vcpkg-triplets/fission-arm64-osx.cmake).
set(FISSION_MACOS_MIN_VERSION "14.0" CACHE INTERNAL "Minimum macOS version of the app")
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin" AND NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "${FISSION_MACOS_MIN_VERSION}" CACHE STRING
      "Minimum macOS version")
endif()

# We never cross-compile, so build tools for the host with the same triplet. Without this vcpkg
# builds a second, full-featured copy of Qt (ICU, OpenSSL, PostgreSQL, Widgets, debug and
# release) just to run moc and friends, which costs hours and extra system packages.
if(NOT DEFINED VCPKG_HOST_TRIPLET)
  set(VCPKG_HOST_TRIPLET "${VCPKG_TARGET_TRIPLET}" CACHE STRING "")
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
if(FISSION_QT_SOURCE STREQUAL "vcpkg")
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
