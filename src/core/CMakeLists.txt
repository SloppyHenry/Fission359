# SPDX-FileCopyrightText: Fission359 contributors
# SPDX-License-Identifier: GPL-3.0-or-later

# Core must not know Qt. Layer rules: cmake/FissionLayers.cmake.
fission_add_library(fission_core
  SOURCES src/layer.cpp src/version.cpp src/expression.cpp src/measures.cpp src/id.cpp
          src/document.cpp src/feature_type.cpp src/recompute.cpp
          src/commands.cpp src/history.cpp src/element_names.cpp src/selection.cpp src/scaling.cpp src/cuts.cpp src/serialize.cpp
  PUBLIC fission_geometry nlohmann_json::nlohmann_json)

target_compile_definitions(fission_core PRIVATE FISSION_VERSION_STRING="${PROJECT_VERSION}")
