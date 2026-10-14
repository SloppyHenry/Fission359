# SPDX-FileCopyrightText: Fission3D contributors
# SPDX-License-Identifier: GPL-3.0-or-later

fission_add_library(fission_services
  SOURCES src/layer.cpp src/log.cpp src/startup_clock.cpp
  PUBLIC fission_core
  PRIVATE spdlog::spdlog Qt6::Core)
