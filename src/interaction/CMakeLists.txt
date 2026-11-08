# SPDX-FileCopyrightText: Fission359 contributors
# SPDX-License-Identifier: GPL-3.0-or-later

fission_add_library(fission_interaction
  SOURCES src/layer.cpp src/drag.cpp src/snap.cpp src/distances.cpp src/align.cpp src/sketch_snap.cpp src/sketch_tool.cpp
    include/fission/interaction/drag.h include/fission/interaction/snap.h
    include/fission/interaction/distances.h include/fission/interaction/align.h
    include/fission/interaction/sketch_snap.h include/fission/interaction/sketch_tool.h
  PUBLIC fission_core fission_render fission_features Qt6::Gui)
