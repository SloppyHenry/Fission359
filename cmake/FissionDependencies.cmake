# SPDX-FileCopyrightText: Fission3D contributors
# SPDX-License-Identifier: GPL-3.0-or-later

# All third-party libraries come from vcpkg (vcpkg.json); Qt is found in FissionQt/find_package
# in the root CMakeLists. Which layer may use which library is decided in FissionLayers.cmake.

# OpenCASCADE: link only the modules Fission3D needs (modeling), not the whole kit.
find_package(OpenCASCADE CONFIG REQUIRED)
set(FISSION_OCCT_TARGETS
  ${OpenCASCADE_FoundationClasses_LIBRARIES}
  ${OpenCASCADE_ModelingData_LIBRARIES}
  ${OpenCASCADE_ModelingAlgorithms_LIBRARIES})
# Further modules, linked only where needed (so far the geometry tests): STEP / mesh file
# exchange and the font renderer for text outlines.
set(FISSION_OCCT_EXTRA_TARGETS
  ${OpenCASCADE_DataExchange_LIBRARIES}
  TKService
  TKV3d)

# OCAF (TKCAF / TKLCAF with TNaming): linked only by the persistent-reference spike tests
# (AP-0.6), to measure OCCT's own naming against ours. The product does not use it.
set(FISSION_OCCT_OCAF_TARGETS TKCDF TKLCAF TKCAF)

find_package(manifold CONFIG REQUIRED)
find_package(Eigen3 CONFIG REQUIRED)
find_package(lib3mf CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(minizip-ng CONFIG REQUIRED)
find_package(NanoSVG CONFIG REQUIRED)
find_package(Boost REQUIRED)           # headers only (Boost.Graph, used by PlaneGCS)

# Header-only libraries without CMake package.
find_path(STB_INCLUDE_DIR stb_image.h REQUIRED)
find_path(FISSION_LIB3MF_CPP_INCLUDE_DIR lib3mf_implicit.hpp
          PATH_SUFFIXES Bindings/Cpp REQUIRED)
