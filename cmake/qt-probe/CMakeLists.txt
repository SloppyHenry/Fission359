# SPDX-FileCopyrightText: Fission359 contributors
# SPDX-License-Identifier: GPL-3.0-or-later

# Tiny helper project: tells FissionVcpkg.cmake whether a usable system Qt exists.
# It runs in its own build directory because find_package(Qt6) needs an enabled language,
# which is not available yet when the main project decides where Qt comes from.
cmake_minimum_required(VERSION 3.28)
project(FissionQtProbe LANGUAGES CXX)

find_package(Qt6 ${FISSION_QT_MIN_VERSION} CONFIG QUIET COMPONENTS Core Gui Qml Quick)
if(Qt6_FOUND AND Qt6Quick_FOUND)
  message(STATUS "FISSION_QT_PROBE_RESULT=FOUND ${Qt6_VERSION}")
else()
  message(STATUS "FISSION_QT_PROBE_RESULT=MISSING")
endif()
