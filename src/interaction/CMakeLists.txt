# SPDX-FileCopyrightText: Fission3D contributors
# SPDX-License-Identifier: GPL-3.0-or-later

fission_add_library(fission_interaction
  SOURCES src/layer.cpp src/drag.cpp include/fission/interaction/drag.h
  PUBLIC fission_core fission_render fission_features Qt6::Gui)
