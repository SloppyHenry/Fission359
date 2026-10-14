# SPDX-FileCopyrightText: Fission3D contributors
# SPDX-License-Identifier: GPL-3.0-or-later

# Qt private headers. The renderer talks to the graphics card through QRhi, whose headers Qt
# ships as "GuiPrivate". Some distributions (Debian 13) install the headers but no CMake
# package for them; then the include directories next to the public ones are used.
find_package(Qt6 6.8 QUIET OPTIONAL_COMPONENTS GuiPrivate)
if(NOT TARGET Qt6::GuiPrivate)
  get_target_property(_qt_gui_includes Qt6::Gui INTERFACE_INCLUDE_DIRECTORIES)
  set(_qt_private_dirs "")
  foreach(_dir IN LISTS _qt_gui_includes)
    if(_dir MATCHES "/(QtGui)$")
      get_filename_component(_root "${_dir}" DIRECTORY)
      list(APPEND _qt_private_dirs
        "${_root}/QtGui/${Qt6Gui_VERSION}" "${_root}/QtGui/${Qt6Gui_VERSION}/QtGui"
        "${_root}/QtCore/${Qt6Gui_VERSION}" "${_root}/QtCore/${Qt6Gui_VERSION}/QtCore")
    endif()
  endforeach()
  if(NOT _qt_private_dirs)
    message(FATAL_ERROR "Qt private headers not found: install qt6-base-private-dev (or use vcpkg Qt)")
  endif()
  add_library(Qt6::GuiPrivate INTERFACE IMPORTED)
  set_target_properties(Qt6::GuiPrivate PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${_qt_private_dirs}"
    INTERFACE_LINK_LIBRARIES Qt6::Gui)
  message(STATUS "Qt6::GuiPrivate: using private header directories ${_qt_private_dirs}")
endif()
