# SPDX-FileCopyrightText: Fission3D contributors
# SPDX-License-Identifier: GPL-3.0-or-later

fission_add_library(fission_io
  SOURCES src/layer.cpp src/fission_file.cpp src/atomic_file.cpp src/export.cpp
  PUBLIC fission_core
  PRIVATE MINIZIP::minizip-ng nlohmann_json::nlohmann_json lib3mf::lib3mf)

target_include_directories(fission_io SYSTEM PRIVATE "${FISSION_LIB3MF_CPP_INCLUDE_DIR}")
