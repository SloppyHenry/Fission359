# SPDX-FileCopyrightText: Fission3D contributors
# SPDX-License-Identifier: GPL-3.0-or-later

# Spike AP-0.4: viewport prototypes. Not part of the product; build with
#   cmake -DFISSION_BUILD_SPIKES=ON ...
# Both prototypes show the same OpenCASCADE scenes (common/) and print comparable measurements.

find_package(Qt6 6.8 REQUIRED COMPONENTS ShaderTools OpenGL GuiPrivate)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  find_package(OpenGL REQUIRED COMPONENTS GLX) # glXGetCurrentDisplay for prototype B
endif()

add_library(viewport_common STATIC common/scene.cpp)
target_include_directories(viewport_common PUBLIC common)
target_link_libraries(viewport_common PUBLIC ${FISSION_OCCT_TARGETS} Qt6::Core Qt6::Gui)
fission_apply_compiler_options(viewport_common)

# --- prototype A: Qt RHI -------------------------------------------------------------------
qt_add_executable(viewport_a a_rhi/main.cpp)
qt_add_qml_module(viewport_a
  URI ViewportA
  VERSION 1.0
  QML_FILES a_rhi/Main.qml
  SOURCES a_rhi/rhi_viewport.h a_rhi/rhi_viewport.cpp a_rhi/mesh_pass.h a_rhi/mesh_pass.cpp
  OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/qml-modules/ViewportA")
qt_add_shaders(viewport_a "viewport_a_shaders"
  PREFIX "/shaders"
  BASE a_rhi/shaders
  FILES
    a_rhi/shaders/mesh.vert
    a_rhi/shaders/mesh.frag
    a_rhi/shaders/pick.frag
    a_rhi/shaders/line.vert
    a_rhi/shaders/line.frag
    a_rhi/shaders/shadow.vert
    a_rhi/shaders/shadow.frag)
target_link_libraries(viewport_a PRIVATE
  viewport_common Qt6::Core Qt6::Gui Qt6::GuiPrivate Qt6::Quick)
# The generated QML registration includes rhi_viewport.h by its plain name.
target_include_directories(viewport_a PRIVATE a_rhi)
fission_apply_compiler_options(viewport_a)

# Headless benchmark of prototype A (same drawing code, no window).
qt_add_executable(viewport_a_bench a_rhi/bench_main.cpp a_rhi/mesh_pass.h a_rhi/mesh_pass.cpp)
qt_add_shaders(viewport_a_bench "viewport_a_bench_shaders"
  PREFIX "/shaders"
  BASE a_rhi/shaders
  FILES
    a_rhi/shaders/mesh.vert
    a_rhi/shaders/mesh.frag
    a_rhi/shaders/pick.frag
    a_rhi/shaders/line.vert
    a_rhi/shaders/line.frag
    a_rhi/shaders/shadow.vert
    a_rhi/shaders/shadow.frag)
target_link_libraries(viewport_a_bench PRIVATE viewport_common Qt6::Core Qt6::Gui Qt6::GuiPrivate)
fission_apply_compiler_options(viewport_a_bench)

# --- prototype B: OpenCASCADE AIS / V3d ----------------------------------------------------
# Headless benchmark: OpenCASCADE's viewer renders into a Qt-created OpenGL framebuffer.
qt_add_executable(viewport_b_bench b_ais/bench_main.cpp)
target_link_libraries(viewport_b_bench PRIVATE
  viewport_common ${FISSION_OCCT_EXTRA_TARGETS} ${OpenCASCADE_Visualization_LIBRARIES}
  Qt6::Core Qt6::Gui Qt6::OpenGL)
fission_apply_compiler_options(viewport_b_bench)

# Interactive prototype B (Qt Quick + OpenCASCADE viewer in a framebuffer object).
qt_add_executable(viewport_b b_ais/main.cpp)
qt_add_qml_module(viewport_b
  URI ViewportB
  VERSION 1.0
  QML_FILES b_ais/Main.qml
  SOURCES b_ais/ais_viewport.h b_ais/ais_viewport.cpp
  OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/qml-modules/ViewportB")
target_include_directories(viewport_b PRIVATE b_ais)
target_link_libraries(viewport_b PRIVATE
  viewport_common ${FISSION_OCCT_EXTRA_TARGETS} ${OpenCASCADE_Visualization_LIBRARIES}
  Qt6::Core Qt6::Gui Qt6::Quick Qt6::OpenGL)
if(TARGET OpenGL::GLX)
  target_link_libraries(viewport_b PRIVATE OpenGL::GLX)
  target_link_libraries(viewport_b_bench PRIVATE OpenGL::GLX)
endif()
fission_apply_compiler_options(viewport_b)
