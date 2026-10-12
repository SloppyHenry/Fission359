// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: LGPL-2.1-or-later

// Stand-in for FreeCAD's src/Mod/Sketcher/SketcherGlobal.h, which PlaneGCS includes as
// "../../SketcherGlobal.h". FreeCAD uses it for DLL export macros; here the solver is a plain
// static library, so the macro is empty.
#pragma once

#define SketcherExport
