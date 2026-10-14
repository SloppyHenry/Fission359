# SPDX-FileCopyrightText: Fission3D contributors
# SPDX-License-Identifier: GPL-3.0-or-later

# Core must not know Qt. Layer rules: cmake/FissionLayers.cmake.
fission_add_library(fission_core
  SOURCES src/layer.cpp src/version.cpp src/expression.cpp
  PUBLIC fission_geometry)

target_compile_definitions(fission_core PRIVATE FISSION_VERSION_STRING="${PROJECT_VERSION}")
