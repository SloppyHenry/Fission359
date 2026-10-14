# SPDX-FileCopyrightText: Fission3D contributors
# SPDX-License-Identifier: GPL-3.0-or-later

# Checks screenshots of the real window against the design tokens (used by tests/CMakeLists.txt).
add_executable(fission_screenshot_check screenshot_check.cpp)
target_link_libraries(fission_screenshot_check PRIVATE fission_ui Qt6::Gui)
fission_apply_compiler_options(fission_screenshot_check)
