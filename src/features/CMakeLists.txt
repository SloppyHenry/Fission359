# SPDX-FileCopyrightText: Fission3D contributors
# SPDX-License-Identifier: GPL-3.0-or-later

# Feature types talk to the geometry kernel only through the GAL interface (GeometryKernel).
fission_add_library(fission_features
  SOURCES src/layer.cpp src/builtin.cpp src/box.cpp src/cylinder.cpp src/shapes.cpp src/text.cpp
  PUBLIC fission_core)
