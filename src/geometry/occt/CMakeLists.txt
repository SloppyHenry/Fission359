# SPDX-FileCopyrightText: Fission359 contributors
# SPDX-License-Identifier: GPL-3.0-or-later

fission_add_library(fission_geometry_occt
  SOURCES src/layer.cpp src/solids.cpp src/primitives.cpp src/booleans.cpp src/blends.cpp src/faces.cpp src/stretch.cpp src/shell.cpp src/draft.cpp src/signatures.cpp src/fonts.cpp src/text.cpp src/exchange.cpp src/measuring.cpp src/sketch_curve.cpp src/kernel.cpp
  PUBLIC fission_geometry
  PRIVATE ${FISSION_OCCT_TARGETS} TKService TKV3d # fonts and text outlines
          TKDESTEP TKDEIGES TKXCAF TKXSBase TKDE) # STEP / IGES with colours and names (AP-2.13)
