# SPDX-FileCopyrightText: Fission3D contributors
# SPDX-License-Identifier: GPL-3.0-or-later

fission_add_library(fission_io
  SOURCES src/layer.cpp src/fission_file.cpp
  PUBLIC fission_core
  PRIVATE MINIZIP::minizip-ng nlohmann_json::nlohmann_json)
