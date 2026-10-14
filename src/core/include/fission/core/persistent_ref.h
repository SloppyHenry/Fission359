// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>
#include <vector>

#include "fission/geo/types.h"

namespace fission::core {

enum class ElementKind { face, edge };

/// What kind of surface or curve an element lies on (kernel independent).
enum class ElementShape { unknown, plane, cylinder, cone, sphere, torus, freeform, line, circle, curve };

/// What a neighbouring face looked like. Its name may be gone after a feature was replaced; the
/// look still lets the face be recognised.
struct NeighbourLook {
    std::string name;
    ElementShape shape = ElementShape::unknown;
    geo::Point3d centroid;
    geo::Point3d direction{0.0, 0.0, 1.0};
    bool hasDirection = false;
    bool directed = false;

    bool operator==(const NeighbourLook&) const = default;
};

/// What a referenced face or edge looked like when it was picked and when it was last resolved
/// (ADR-0003). Millimetres, no kernel types.
struct ElementLook {
    ElementKind kind = ElementKind::face;
    ElementShape shape = ElementShape::unknown;
    geo::Point3d centroid;                          ///< Centre of mass.
    geo::Point3d direction{0.0, 0.0, 1.0};          ///< Plane normal, axis or line direction.
    bool hasDirection = false;
    bool directed = false;                          ///< Plane normals have a sign, axes do not.
    double size = 0.0;                              ///< Area or length.
    double radius = 0.0;                            ///< Cylinder or circle radius, otherwise 0.
    std::vector<std::string> neighbours;            ///< Sorted names of the adjacent faces.
    std::vector<NeighbourLook> neighbourLooks;      ///< The same faces with their look.

    bool operator==(const ElementLook&) const = default;
};

/// A reference from one feature to a face or edge of another (docs/03-architektur.md §3.4):
/// the name the creating feature gave it plus what it looked like. Resolving it after a change
/// is the job of the naming layer (AP-2.10); this type only carries the data and is saved in
/// the file. A reference that cannot be resolved safely is "needs help", never guessed.
struct PersistentRef {
    ElementKind kind = ElementKind::face;
    std::string name; ///< "F5/hole/wall": feature number, feature type, role.
    ElementLook look;

    bool operator==(const PersistentRef&) const = default;
};

} // namespace fission::core
