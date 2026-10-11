# SPDX-FileCopyrightText: Fission3D contributors
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.28)

project(Fission3D
  VERSION 0.0.1
  DESCRIPTION "Easy-to-use, precise 3D construction program for makers"
  LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(FISSION_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)
option(FISSION_BUILD_TESTS "Build the test suite" ON)
set(FISSION_SANITIZE "" CACHE STRING "Sanitizers to enable, e.g. \"address;undefined\"")

include(FissionCompilerOptions)
include(FissionLayers)

# Qt is only allowed in the layers listed in cmake/FissionLayers.cmake.
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick)
qt_standard_project_setup(REQUIRES 6.8)

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

if(FISSION_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# Runs after all targets exist: fails the configure step on any layer violation.
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL fission_verify_layering)
