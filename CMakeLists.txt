# SPDX-FileCopyrightText: Fission3D contributors
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.28)

# vcpkg toolchain, triplet and Qt source must be decided before project().
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(FissionVcpkg)

project(Fission3D
  VERSION 0.0.1
  DESCRIPTION "Easy-to-use, precise 3D construction program for makers"
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(FISSION_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)
option(FISSION_BUILD_TESTS "Build the test suite" ON)
set(FISSION_SANITIZE "" CACHE STRING "Sanitizers to enable, e.g. \"address;undefined\"")
option(FISSION_COVERAGE "Instrument the code for test coverage (scripts/coverage)" OFF)

include(FissionCompilerOptions)
include(FissionLayers)

# Qt is only allowed in the layers listed in cmake/FissionLayers.cmake.
# (Where it comes from - system or vcpkg - is decided in cmake/FissionVcpkg.cmake.)
# LinguistTools provides lrelease/lupdate (translations: qsTr + .ts files, German and English).
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick ShaderTools LinguistTools)
include(FissionQt)
qt_standard_project_setup(REQUIRES 6.8)

# Libraries from vcpkg win over same-named libraries elsewhere on the machine. Homebrew's Qt 6.12
# keeps its headers in /opt/homebrew/include, where Homebrew's own Catch2 (another version) is
# found as well and would otherwise shadow the one the tests are linked against.
include_directories(BEFORE SYSTEM "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include")

include(FissionDependencies)

# Vendored third-party code (not available in vcpkg).
add_subdirectory(third_party/planegcs)

# Layers, ordered from the bottom (no dependencies) to the top (the app).
add_subdirectory(src/geometry/gal)
add_subdirectory(src/geometry/occt)
add_subdirectory(src/geometry/mesh)
add_subdirectory(src/geometry/sketch)
add_subdirectory(src/core)
add_subdirectory(src/features)
add_subdirectory(src/io)
add_subdirectory(src/analysis)
add_subdirectory(src/services)
add_subdirectory(src/render)
add_subdirectory(src/interaction)
add_subdirectory(src/ui)
add_subdirectory(apps/fission3d)

# Throw-away prototypes of the spike work packages (AP-0.4). Off by default.
option(FISSION_BUILD_SPIKES "Build the spike prototypes in spikes/" OFF)
if(FISSION_BUILD_SPIKES)
  add_subdirectory(spikes/viewport)
endif()

if(FISSION_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# Runs after all targets exist: fails the configure step on any layer violation.
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL fission_verify_layering)
