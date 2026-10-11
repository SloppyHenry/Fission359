# SPDX-FileCopyrightText: Fission3D contributors
# SPDX-License-Identifier: GPL-3.0-or-later

fission_add_library(fission_ui
  SOURCES src/layer.cpp
  PUBLIC fission_interaction fission_services
  PRIVATE Qt6::Qml Qt6::Quick)
