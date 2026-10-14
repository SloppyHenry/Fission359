// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QColor>

#include <vector>

#include "fission/geo/mesh.h"

namespace fission::render {

/// A body as the renderer wants it: its tessellation and the colour it is drawn in.
struct RenderBody {
    geo::TriangleMesh mesh;
    QColor color{233, 234, 236};
};

/// The print bed (Druckbett): a flat plate with rounded corners, origin in the middle.
struct BedSpec {
    float sizeX = 256.0F; ///< mm
    float sizeY = 256.0F; ///< mm
    float cornerRadius = 6.0F;
    float thickness = 5.0F;
};

struct RenderScene {
    std::vector<RenderBody> bodies;
    BedSpec bed;
};

/// Colours of the 3D view; they come from the theme tokens (docs/08-gestaltung.md §2).
struct ViewPalette {
    QColor viewCenter{0x2B, 0x2D, 0x31};
    QColor viewEdge{0x23, 0x25, 0x28};
    QColor bed{0x25, 0x27, 0x2B};
    QColor bedEdge{0x44, 0x47, 0x4E};
    QColor grid{0x30, 0x32, 0x37};
    QColor gridMajor{0x3A, 0x3D, 0x43};
    QColor axisX{0xE5, 0x53, 0x4B};
    QColor axisY{0x57, 0xAB, 0x5A};
    QColor origin{0xA4, 0xA8, 0xB0};
    QColor edgeLine{10, 11, 13, 140};
    QColor shadow{0, 0, 0, 150};

    bool operator==(const ViewPalette&) const = default;
};

} // namespace fission::render
