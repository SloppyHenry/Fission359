// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "fission/geo/types.h"

namespace fission::geo {

enum class SignatureKind { face, edge };

/// What kind of surface or curve an element lies on.
enum class SignatureShape {
    unknown,
    plane,
    cylinder,
    cone,
    sphere,
    torus,
    freeform,
    line,
    circle,
    curve
};

/// What one face or edge of a body looks like (ADR-0003): the kernel side of `core::ElementLook`.
/// Numbers are those of the body (as in `TriangleMesh`); names are the business of the core.
/// Millimetres.
struct ElementSignature {
    std::uint32_t id = 0; ///< 1-based number of the face / edge in the body.
    SignatureKind kind = SignatureKind::face;
    SignatureShape shape = SignatureShape::unknown;
    Point3d centroid;                 ///< Centre of mass.
    Point3d direction{0.0, 0.0, 1.0}; ///< Plane normal (out of the body), axis or line direction.
    bool hasDirection = false;
    bool directed = false; ///< Plane normals have a sign, axes and line directions do not.
    double size = 0.0;     ///< Area or length.
    double radius = 0.0;   ///< Cylinder or circle radius, otherwise 0.
    /// The faces beside it, by face number, sorted, without repeats: the neighbours of a face,
    /// the two faces an edge joins.
    std::vector<std::uint32_t> faces;
};

/// The signatures of all faces and edges of one body. Degenerate edges are left out.
struct BodySignatures {
    std::vector<ElementSignature> faces; ///< In face order (`faces[i].id == i + 1`).
    std::vector<ElementSignature> edges; ///< In edge order, gaps where an edge was left out.
    double diagonal = 0.0;               ///< Diagonal of the body's box, for distance scales.
};

/// What an operation did to the names of the faces of its inputs (ADR-0003, "history"). The
/// names are the roles the inputs carried.
struct ElementHistory {
    /// An input face and the faces it became: it was cut into pieces (`#1`, `#2` ...) or got a
    /// new name. A face that simply stays is not listed.
    std::vector<std::pair<std::string, std::vector<std::string>>> modified;
    /// Input faces that are gone: cut away, covered, or joined into a face of another name.
    std::vector<std::string> deleted;

    bool empty() const noexcept { return modified.empty() && deleted.empty(); }
};

} // namespace fission::geo
