// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/core/body_looks.h"

#include <algorithm>
#include <set>
#include <utility>

#include "fission/core/element_names.h"
#include "fission/geo/mesh.h"

namespace fission::core {

namespace {

ElementShape translate(geo::SignatureShape shape) {
    switch (shape) {
    case geo::SignatureShape::plane:
        return ElementShape::plane;
    case geo::SignatureShape::cylinder:
        return ElementShape::cylinder;
    case geo::SignatureShape::cone:
        return ElementShape::cone;
    case geo::SignatureShape::sphere:
        return ElementShape::sphere;
    case geo::SignatureShape::torus:
        return ElementShape::torus;
    case geo::SignatureShape::freeform:
        return ElementShape::freeform;
    case geo::SignatureShape::line:
        return ElementShape::line;
    case geo::SignatureShape::circle:
        return ElementShape::circle;
    case geo::SignatureShape::curve:
        return ElementShape::curve;
    case geo::SignatureShape::unknown:
        break;
    }
    return ElementShape::unknown;
}

ElementLook baseLook(const geo::ElementSignature& sig) {
    ElementLook look;
    look.kind = sig.kind == geo::SignatureKind::face ? ElementKind::face : ElementKind::edge;
    look.shape = translate(sig.shape);
    look.centroid = sig.centroid;
    look.direction = sig.direction;
    look.hasDirection = sig.hasDirection;
    look.directed = sig.directed;
    look.size = sig.size;
    look.radius = sig.radius;
    return look;
}

} // namespace

BodyLooks describeBody(const std::vector<BodyFace>& faces, const geo::BodySignatures& signatures) {
    // The edges, as far as the names need them: which faces they join.
    geo::TriangleMesh skeleton;
    for (const geo::ElementSignature& edge : signatures.edges) {
        geo::EdgePolyline line;
        line.edgeId = edge.id;
        if (!edge.faces.empty()) {
            line.faces[0] = edge.faces.front();
            line.faces[1] = edge.faces.back(); // a seam has its one face on both sides
        }
        skeleton.edges.push_back(std::move(line));
    }
    const ElementNames names = ElementNames::build(faces, skeleton);

    BodyLooks looks;
    looks.diagonal = signatures.diagonal;

    // What a neighbour looked like: its own look without neighbours of its own.
    const auto neighbourOf = [&](std::uint32_t id) {
        NeighbourLook neighbour;
        const geo::ElementSignature& sig = signatures.faces[id - 1];
        neighbour.name = names.faceName(id);
        neighbour.shape = translate(sig.shape);
        neighbour.centroid = sig.centroid;
        neighbour.direction = sig.direction;
        neighbour.hasDirection = sig.hasDirection;
        neighbour.directed = sig.directed;
        return neighbour;
    };
    const auto fill = [&](const geo::ElementSignature& sig, const std::string& name) {
        NamedLook entry;
        entry.id = sig.id;
        entry.name = name;
        entry.look = baseLook(sig);
        std::set<std::string> around;
        for (const std::uint32_t id : sig.faces) {
            if (id == 0 || id > signatures.faces.size()) {
                continue;
            }
            entry.look.neighbourLooks.push_back(neighbourOf(id));
            if (!names.faceName(id).empty()) {
                around.insert(names.faceName(id));
            }
        }
        entry.look.neighbours.assign(around.begin(), around.end());
        return entry;
    };

    for (const geo::ElementSignature& sig : signatures.faces) {
        looks.faces.push_back(fill(sig, names.faceName(sig.id)));
    }
    for (const geo::ElementSignature& sig : signatures.edges) {
        looks.edges.push_back(fill(sig, names.edgeName(sig.id)));
    }
    return looks;
}

std::vector<ConsumedElement> consumedBetween(const BodyLooks& before, const BodyLooks& after,
                                             const FeatureId& step) {
    std::vector<ConsumedElement> gone;
    const auto collect = [&](const std::vector<NamedLook>& was, const std::vector<NamedLook>& is) {
        std::set<std::string> alive;
        for (const NamedLook& entry : is) {
            if (!entry.name.empty()) {
                alive.insert(entry.name);
            }
        }
        for (const NamedLook& entry : was) {
            if (!entry.name.empty() && alive.count(entry.name) == 0) {
                gone.push_back({entry.look.kind, entry.name, entry.look, step});
            }
        }
    };
    collect(before.faces, after.faces);
    collect(before.edges, after.edges);
    return gone;
}

void mergeConsumed(std::vector<ConsumedElement>& list, const std::vector<ConsumedElement>& more) {
    for (const ConsumedElement& element : more) {
        const bool known = std::any_of(list.begin(), list.end(), [&](const ConsumedElement& c) {
            return c.kind == element.kind && c.name == element.name;
        });
        if (!known) {
            list.push_back(element);
        }
    }
}

} // namespace fission::core
