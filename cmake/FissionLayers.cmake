# SPDX-FileCopyrightText: Fission3D contributors
# SPDX-License-Identifier: GPL-3.0-or-later

# Layer rules (docs/03-architektur.md, section 2): dependencies point only downwards.
#
# Every library is created with fission_add_library(). The allow-list below is the single
# place that says which target may link which other targets. fission_verify_layering()
# checks it at the end of the configure step, so a wrong dependency stops the build
# immediately instead of showing up as an architecture problem months later.
#
# Entries are target names; "*" is a wildcard (for example "Qt6::*").
#
#   fission_geometry          public interface of the geometry layer (GAL), no dependencies
#   fission_geometry_occt     OpenCASCADE implementation of the GAL (only place for TopoDS_*)
#   fission_geometry_mesh     Manifold / signed-distance implementation of the GAL
#   fission_geometry_sketch   PlaneGCS binding
#   fission_core              document, features graph, commands (no Qt, talks to the GAL only)
#
# Third-party libraries are listed per layer on purpose: for example OpenCASCADE (TK*) may only
# be linked by fission_geometry_occt, Manifold only by fission_geometry_mesh.

set(FISSION_ALLOWED_DEPS_fission_geometry "")
set(FISSION_ALLOWED_DEPS_fission_geometry_occt fission_geometry TK.*)
set(FISSION_ALLOWED_DEPS_fission_geometry_mesh fission_geometry manifold::manifold)
set(FISSION_ALLOWED_DEPS_fission_geometry_sketch fission_geometry planegcs Eigen3::Eigen)
set(FISSION_ALLOWED_DEPS_fission_core fission_geometry nlohmann_json::nlohmann_json)
set(FISSION_ALLOWED_DEPS_fission_features fission_core fission_geometry Eigen3::Eigen)
set(FISSION_ALLOWED_DEPS_fission_io
  fission_core fission_geometry
  lib3mf::lib3mf MINIZIP::minizip-ng NanoSVG::nanosvg nlohmann_json::nlohmann_json)
set(FISSION_ALLOWED_DEPS_fission_analysis fission_core fission_geometry Eigen3::Eigen)
set(FISSION_ALLOWED_DEPS_fission_services
  fission_core spdlog::spdlog nlohmann_json::nlohmann_json Qt6::Core)
set(FISSION_ALLOWED_DEPS_fission_render
  fission_core fission_geometry Eigen3::Eigen Qt6::Core Qt6::Gui Qt6::GuiPrivate)
set(FISSION_ALLOWED_DEPS_fission_interaction
  fission_core fission_geometry fission_features fission_render Eigen3::Eigen
  Qt6::Core Qt6::Gui)
set(FISSION_ALLOWED_DEPS_fission_ui
  fission_core fission_features fission_interaction fission_render fission_services
  Qt6::Core Qt6::Gui Qt6::GuiPrivate Qt6::Qml Qt6::Quick)

# fission_add_library(<name> SOURCES <files...> [PUBLIC <deps...>] [PRIVATE <deps...>])
# Public headers live in <dir>/include/fission/<layer>/.
function(fission_add_library name)
  cmake_parse_arguments(ARG "" "" "SOURCES;PUBLIC;PRIVATE" ${ARGN})
  if(NOT DEFINED FISSION_ALLOWED_DEPS_${name})
    message(FATAL_ERROR
      "Layer rule: '${name}' has no allow-list in cmake/FissionLayers.cmake. "
      "Add the layer there first.")
  endif()

  add_library(${name} STATIC ${ARG_SOURCES})
  target_include_directories(${name} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
  target_compile_features(${name} PUBLIC cxx_std_20)
  if(ARG_PUBLIC)
    target_link_libraries(${name} PUBLIC ${ARG_PUBLIC})
  endif()
  if(ARG_PRIVATE)
    target_link_libraries(${name} PRIVATE ${ARG_PRIVATE})
  endif()
  fission_apply_compiler_options(${name})

  set_property(GLOBAL APPEND PROPERTY FISSION_LIBRARIES ${name})
endfunction()

# Fails configuration if any registered library links something outside its allow-list.
function(fission_verify_layering)
  get_property(_libraries GLOBAL PROPERTY FISSION_LIBRARIES)
  set(_violations "")
  foreach(_lib IN LISTS _libraries)
    get_target_property(_linked ${_lib} LINK_LIBRARIES)
    if(NOT _linked)
      continue()
    endif()
    foreach(_dep IN LISTS _linked)
      set(_ok FALSE)
      foreach(_pattern IN LISTS FISSION_ALLOWED_DEPS_${_lib})
        string(REPLACE "*" ".*" _regex "${_pattern}")
        if(_dep MATCHES "^${_regex}$")
          set(_ok TRUE)
          break()
        endif()
      endforeach()
      if(NOT _ok)
        list(APPEND _violations "  ${_lib} must not link ${_dep}")
      endif()
    endforeach()
  endforeach()

  if(_violations)
    list(JOIN _violations "\n" _text)
    message(FATAL_ERROR
      "Layer rule violated (see cmake/FissionLayers.cmake and docs/03-architektur.md):\n"
      "${_text}")
  endif()
endfunction()
