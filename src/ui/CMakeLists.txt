# SPDX-FileCopyrightText: Fission3D contributors
# SPDX-License-Identifier: GPL-3.0-or-later

fission_add_library(fission_ui
  SOURCES
    src/layer.cpp src/viewport_3d.cpp src/theme_tokens.cpp src/theme.cpp src/language.cpp src/numbers.cpp src/document_model.cpp src/printer_catalog.cpp src/selection_controller.cpp src/placement_controller.cpp src/shape_previews.cpp src/handle_controller.cpp src/gizmo_controller.cpp src/rotate_controller.cpp src/scale_controller.cpp src/snap_session.cpp
    include/fission/ui/viewport_3d.h include/fission/ui/theme.h include/fission/ui/language.h
    include/fission/ui/app_info.h include/fission/ui/numbers.h include/fission/ui/app_options.h
    include/fission/ui/document_model.h include/fission/ui/printer_catalog.h
    include/fission/ui/selection_controller.h include/fission/ui/placement_controller.h include/fission/ui/shape_previews.h include/fission/ui/handle_controller.h include/fission/ui/gizmo_controller.h include/fission/ui/rotate_controller.h include/fission/ui/scale_controller.h include/fission/ui/snap_session.h
  PUBLIC fission_interaction fission_services fission_render Qt6::Quick
  PRIVATE fission_core fission_features Qt6::Qml Qt6::GuiPrivate)
