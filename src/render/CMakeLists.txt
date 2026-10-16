# SPDX-FileCopyrightText: Fission3D contributors
# SPDX-License-Identifier: GPL-3.0-or-later

fission_add_library(fission_render
  SOURCES src/layer.cpp src/benchmark_scene.cpp src/scene.cpp src/detail.cpp src/camera.cpp src/navigation.cpp src/picking.cpp
          src/scene_renderer.cpp
  PUBLIC fission_core fission_geometry Qt6::Gui Qt6::GuiPrivate)

qt_add_shaders(fission_render "fission_render_shaders"
  PREFIX "/fission/render/shaders"
  BASE src/shaders
  FILES
    src/shaders/background.vert
    src/shaders/background.frag
    src/shaders/body.vert
    src/shaders/body.frag
    src/shaders/bed.frag
    src/shaders/flat.vert
    src/shaders/flat.frag
    src/shaders/shadow.vert
    src/shaders/shadow.frag
    src/shaders/fullscreen.vert
    src/shaders/ssao.frag
    src/shaders/normal.frag
    src/shaders/aoblur.frag)
