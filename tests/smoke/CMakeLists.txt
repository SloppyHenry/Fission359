# SPDX-FileCopyrightText: Fission359 contributors
# SPDX-License-Identifier: GPL-3.0-or-later

# One small test per third-party library: proves that it is available and usable.
# These tests use the libraries directly on purpose (see docs/arbeitspakete, AP-0.2).
# Product code goes through the layers instead.

add_executable(fission_smoke_tests
  smoke_occt.cpp
  smoke_manifold.cpp
  smoke_planegcs.cpp
  smoke_eigen.cpp
  smoke_lib3mf.cpp
  smoke_minizip.cpp
  smoke_nanosvg.cpp
  smoke_stb.cpp
  smoke_json.cpp
  smoke_spdlog.cpp)

target_link_libraries(fission_smoke_tests PRIVATE
  Catch2::Catch2WithMain
  ${FISSION_OCCT_TARGETS}
  manifold::manifold
  planegcs
  Eigen3::Eigen
  lib3mf::lib3mf
  MINIZIP::minizip-ng
  NanoSVG::nanosvg
  nlohmann_json::nlohmann_json
  spdlog::spdlog)
target_include_directories(fission_smoke_tests SYSTEM PRIVATE
  "${STB_INCLUDE_DIR}"
  "${FISSION_LIB3MF_CPP_INCLUDE_DIR}")
fission_apply_compiler_options(fission_smoke_tests)

# GCC 15 reports a false positive for Eigen's fixed-size 2x2 solves (-Warray-bounds inside
# Eigen's own SSE code, e.g. on Ubuntu 26.04). Not our code, so only this file is exempt.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set_source_files_properties(smoke_eigen.cpp PROPERTIES COMPILE_OPTIONS "-Wno-array-bounds")
endif()

catch_discover_tests(fission_smoke_tests TEST_PREFIX "smoke: ")
