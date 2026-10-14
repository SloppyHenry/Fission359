# SPDX-FileCopyrightText: Fission3D contributors
# SPDX-License-Identifier: GPL-3.0-or-later

fission_add_library(fission_ui
  SOURCES
    src/layer.cpp src/viewport_3d.cpp src/theme_tokens.cpp src/theme.cpp src/language.cpp src/numbers.cpp src/document_model.cpp
    include/fission/ui/viewport_3d.h include/fission/ui/theme.h include/fission/ui/language.h
    include/fission/ui/app_info.h include/fission/ui/numbers.h include/fission/ui/app_options.h
    include/fission/ui/document_model.h
  PUBLIC fission_interaction fission_services fission_render Qt6::Quick
  PRIVATE fission_core fission_features Qt6::Qml Qt6::GuiPrivate)
