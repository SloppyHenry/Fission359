# SPDX-FileCopyrightText: Fission359 contributors
# SPDX-License-Identifier: GPL-3.0-or-later

# Feature types talk to the geometry kernel only through the GAL interface (GeometryKernel).
fission_add_library(fission_features
  SOURCES src/layer.cpp src/builtin.cpp src/box.cpp src/cylinder.cpp src/shapes.cpp src/text.cpp src/combine.cpp src/place.cpp src/pattern.cpp src/split.cpp src/blend.cpp src/face_offset.cpp src/face_ops.cpp src/shell_draft.cpp src/import.cpp src/sketch.cpp
  PUBLIC fission_core)
