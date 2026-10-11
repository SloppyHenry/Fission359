# SPDX-FileCopyrightText: Fission3D contributors
# SPDX-License-Identifier: GPL-3.0-or-later

fission_add_library(fission_render
  SOURCES src/layer.cpp
  PUBLIC fission_core
  PRIVATE Qt6::Gui)
