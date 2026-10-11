# SPDX-FileCopyrightText: Fission3D contributors
# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(fission3d src/main.cpp)

qt_add_qml_module(fission3d
  URI Fission3D
  VERSION 1.0
  QML_FILES qml/Main.qml
  # The module directory "Fission3D" would collide with the executable "fission3d" on
  # case-insensitive file systems (macOS), so it gets its own parent directory.
  OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/qml-modules/Fission3D")

# The app wires the layers together. Kernel implementations are linked here only,
# so that no other layer can depend on them.
target_link_libraries(fission3d PRIVATE
  Qt6::Core Qt6::Gui Qt6::Quick
  fission_core fission_ui
  fission_geometry_occt fission_geometry_mesh fission_geometry_sketch
  fission_features fission_io fission_analysis)

fission_apply_compiler_options(fission3d)
