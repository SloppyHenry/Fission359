// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QColor>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "fission/geo/mesh.h"

namespace fission::render {

/// A body as the renderer wants it: its tessellation and the colour it is drawn in.
///
/// The renderer keeps the graphics buffers of a body as long as `id`, `meshVersion` and `color`
/// stay the same, so a change to one body uploads that body only. `mesh` is shared: building a
/// scene does not copy any triangles.
struct RenderBody {
    std::string id;                ///< Stable for one body across rebuilds of the scene.
    std::uint64_t meshVersion = 0; ///< Changes whenever the tessellation is a different one.
    std::shared_ptr<const geo::TriangleMesh> mesh;
    QColor color{233, 234, 236};

    /// The tessellation, or an empty one if there is none yet.
    const geo::TriangleMesh& geometry() const;
    bool operator==(const RenderBody& other) const {
        return id == other.id && meshVersion == other.meshVersion && color == other.color;
    }
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
