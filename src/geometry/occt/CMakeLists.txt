# SPDX-FileCopyrightText: Fission3D contributors
# SPDX-License-Identifier: GPL-3.0-or-later

fission_add_library(fission_geometry_occt
  SOURCES src/layer.cpp
  PUBLIC fission_geometry
  PRIVATE ${FISSION_OCCT_TARGETS})
