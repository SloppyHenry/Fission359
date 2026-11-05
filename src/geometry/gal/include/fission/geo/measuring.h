// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include "fission/geo/solid.h"
#include "fission/geo/types.h"

namespace fission::geo {

/// What can be measured on a body: a corner, an edge or a face. `number` counts like the
/// numbers of the display mesh (`TriangleMesh::cornerId`, `edgeId`, `faceIds`): 1-based, in the
/// order of the body's corners, edges, faces.
enum class ElementKind { corner, edge, face };

struct ElementRef {
    Solid solid;
    ElementKind kind = ElementKind::face;
    std::uint32_t number = 0;
};

/// What measuring one or two elements found. Only what makes sense is set. Millimetres, degrees,
/// square millimetres.
struct Measurement {
    std::optional<Point3d> position; ///< A corner: where it is.
    std::optional<double> length;    ///< An edge: its length (the arc length for an arc).
    std::optional<double> area;      ///< A face: its area.
    std::optional<double> radius;    ///< A circle or arc, a cylinder or sphere: its radius.
    std::optional<double> distance;  ///< Two elements: the shortest distance (0 if they touch).
    /// Two elements: the angle between them, 0 to 180. Two edges: the sharper of the two angles
    /// between the lines. Two flat faces that share an edge: the angle inside the body between
    /// them (a box corner: 90). Two other flat faces: the angle between their planes, 0 to 90. A
    /// straight edge and a flat face: the angle between the line and the plane, 0 to 90.
    std::optional<double> angle;
    /// The line to draw: for two elements the two nearest points, for an edge its two ends.
    std::optional<std::array<Point3d, 2>> span;
};

} // namespace fission::geo
