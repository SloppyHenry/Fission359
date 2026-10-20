# SPDX-FileCopyrightText: Fission3D contributors
# SPDX-License-Identifier: GPL-3.0-or-later

fission_add_library(fission_interaction
  SOURCES src/layer.cpp src/drag.cpp src/snap.cpp src/distances.cpp src/align.cpp
    include/fission/interaction/drag.h include/fission/interaction/snap.h
    include/fission/interaction/distances.h include/fission/interaction/align.h
  PUBLIC fission_core fission_render fission_features Qt6::Gui)
