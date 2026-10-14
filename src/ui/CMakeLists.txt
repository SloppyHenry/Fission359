# SPDX-FileCopyrightText: Fission3D contributors
# SPDX-License-Identifier: GPL-3.0-or-later

fission_add_library(fission_ui
  SOURCES src/layer.cpp src/viewport_3d.cpp include/fission/ui/viewport_3d.h
  PUBLIC fission_interaction fission_services fission_render Qt6::Quick
  PRIVATE Qt6::Qml Qt6::GuiPrivate)
