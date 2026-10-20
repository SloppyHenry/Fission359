# SPDX-FileCopyrightText: Fission3D contributors
# SPDX-License-Identifier: GPL-3.0-or-later

fission_add_library(fission_geometry_occt
  SOURCES src/layer.cpp src/solids.cpp src/primitives.cpp src/booleans.cpp src/fonts.cpp src/text.cpp src/kernel.cpp
  PUBLIC fission_geometry
  PRIVATE ${FISSION_OCCT_TARGETS} TKService TKV3d) # fonts and text outlines
