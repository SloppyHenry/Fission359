# SPDX-FileCopyrightText: Fission359 contributors
# SPDX-License-Identifier: GPL-3.0-or-later

fission_add_library(fission_services
  SOURCES src/layer.cpp src/log.cpp src/startup_clock.cpp src/recent_files.cpp src/recovery_store.cpp src/autosave.cpp src/settings.cpp
    include/fission/services/autosave.h
  PUBLIC fission_core Qt6::Core
  PRIVATE spdlog::spdlog)
