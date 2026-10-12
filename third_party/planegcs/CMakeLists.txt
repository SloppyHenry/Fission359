# SPDX-FileCopyrightText: Fission3D contributors
# SPDX-License-Identifier: GPL-3.0-or-later

# PlaneGCS: FreeCAD's 2D geometric constraint solver, vendored from FreeCAD 1.1.4.
# Origin and local changes: see README.md. The sources are used unmodified; the few FreeCAD
# internals they need are replaced by small stand-ins in shim/.

find_package(Eigen3 CONFIG REQUIRED)
find_package(Boost REQUIRED)

add_library(planegcs STATIC
  App/planegcs/Constraints.cpp
  App/planegcs/GCS.cpp
  App/planegcs/Geo.cpp
  App/planegcs/SubSystem.cpp
  App/planegcs/qp_eq.cpp)

# The sources live in App/planegcs/ so that their relative include "../../SketcherGlobal.h"
# finds the stand-in next to this file (same shape as FreeCAD's src/Mod/Sketcher/App/planegcs).
# shim/ provides <Base/Console.h>, <FCConfig.h> and <boost_graph_adjacency_list.hpp>.
# Public include style is <planegcs/GCS.h>.
# SYSTEM: our strict warning flags (-Wshadow -Werror) must not apply to these headers.
target_include_directories(planegcs SYSTEM
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/App"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/shim" "${CMAKE_CURRENT_SOURCE_DIR}/App/planegcs")
target_link_libraries(planegcs PUBLIC Eigen3::Eigen PRIVATE Boost::boost)
target_compile_features(planegcs PUBLIC cxx_std_20)

# Third-party code: no -Werror, and no warnings noise in our own build output.
target_compile_options(planegcs PRIVATE -w)
set_property(TARGET planegcs PROPERTY POSITION_INDEPENDENT_CODE ON)
