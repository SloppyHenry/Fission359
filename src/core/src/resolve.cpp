// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Resolving persistent references (ADR-0003). The algorithm is the one measured in the
// prototype tests/naming/naming.cpp (docs/spikes/persistente-referenzen.md); keep both in step.

#include "fission/core/resolve.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numbers>
#include <set>
#include <utility>

namespace fission::core {

namespace {

constexpr double kDegrees = 180.0 / std::numbers::pi;

geo::Matrix3 transposed(const geo::Matrix3& matrix) {
    geo::Matrix3 out;
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            out.m[r][c] = matrix.m[c][r];
        }
    }
    return out;
}

double distanceBetween(const geo::Point3d& a, const geo::Point3d& b) {
    return std::hypot(a.x - b.x, a.y - b.y, a.z - b.z);
}

double angleBetweenDegrees(const ElementLook& a, const ElementLook& b) {
    if (!a.hasDirection || !b.hasDirection) {
        return 0.0;
    }
    const double cosine = std::clamp(a.direction.x * b.direction.x + a.direction.y * b.direction.y +
                                         a.direction.z * b.direction.z,
                                     -1.0, 1.0);
    double degrees = std::acos(cosine) * kDegrees;
    if (!(a.directed && b.directed)) {
        degrees = std::min(degrees, 180.0 - degrees); // axes have no sign
    }
    return degrees;
}

double ratio(double a, double b) {
    if (a <= 0.0 || b <= 0.0) {
        return 1.0;
    }
    return std::min(a, b) / std::max(a, b);
}

// 1 = same place, 0 = further away than `positionRadiusFactor` of the body diagonal.
double positionScore(const ElementLook& a, const ElementLook& b, double diagonal,
                     const ResolveLimits& limits) {
    const double scale = std::max(limits.positionRadiusFactor * diagonal, 1e-9);
    return std::max(0.0, 1.0 - distanceBetween(a.centroid, b.centroid) / scale);
}

bool looksAlike(const NeighbourLook& a, const NeighbourLook& b, double diagonal,
                const ResolveLimits& limits) {
    if (a.shape != b.shape) {
        return false;
    }
    if (a.hasDirection && b.hasDirection) {
        const double cosine =
            std::clamp(a.direction.x * b.direction.x + a.direction.y * b.direction.y +
                           a.direction.z * b.direction.z,
                       -1.0, 1.0);
        double degrees = std::acos(cosine) * kDegrees;
        if (!(a.directed && b.directed)) {
            degrees = std::min(degrees, 180.0 - degrees);
        }
        if (degrees > limits.directionGateDegrees) {
            return false;
        }
    }
    return distanceBetween(a.centroid, b.centroid) <= limits.positionRadiusFactor * diagonal;
}

struct Similarity {
    bool comparable = false; // same kind and shape, direction within the gate
    double total = 0.0;
};

Similarity similarity(const ElementLook& ref, const ElementLook& cand, double diagonal,
                      const std::set<std::string>& liveNames, const ResolveLimits& limits,
                      const ResolveSwitches& switches) {
    Similarity result;
    if (ref.kind != cand.kind || ref.shape != cand.shape) {
        return result;
    }
    const double angle = angleBetweenDegrees(ref, cand);
    if (angle > limits.directionGateDegrees) {
        return result;
    }
    if (ref.radius > 0.0 && cand.radius > 0.0 && ratio(ref.radius, cand.radius) < 0.8) {
        return result;
    }
    result.comparable = true;

    const double position = positionScore(ref, cand, diagonal, limits);
    const double direction = 1.0 - angle / std::max(limits.directionGateDegrees, 1e-9);
    const double size =
        0.5 * ratio(ref.size, cand.size) + 0.5 * ((ref.radius > 0.0 && cand.radius > 0.0)
                                                      ? ratio(ref.radius, cand.radius)
                                                      : ratio(ref.size, cand.size));

    // Neighbours: a name that still exists must be adjacent to the candidate. A name that is
    // gone (its step was replaced) is recognised by the look of the face instead.
    double neighbours = 0.75; // no information
    if (!ref.neighbourLooks.empty() || !cand.neighbourLooks.empty()) {
        int matched = 0;
        for (const NeighbourLook& expected : ref.neighbourLooks) {
            const bool alive = !expected.name.empty() && liveNames.count(expected.name) != 0;
            if (alive) {
                matched += std::binary_search(cand.neighbours.begin(), cand.neighbours.end(),
                                              expected.name)
                               ? 1
                               : 0;
            } else if (switches.recogniseNeighbours) {
                for (const NeighbourLook& other : cand.neighbourLooks) {
                    if (looksAlike(expected, other, diagonal, limits)) {
                        ++matched;
                        break;
                    }
                }
            }
        }
        const std::size_t total = std::max(ref.neighbourLooks.size(), cand.neighbourLooks.size());
        neighbours = static_cast<double>(matched) / static_cast<double>(total);
        if (!switches.recogniseNeighbours) {
            const bool anyAlive = std::any_of(
                ref.neighbourLooks.begin(), ref.neighbourLooks.end(),
                [&](const NeighbourLook& look) { return liveNames.count(look.name) != 0; });
            if (!anyAlive) {
                neighbours = 0.75;
            }
        }
    }
    result.total = 0.35 * position + 0.15 * direction + 0.20 * size + 0.30 * neighbours;
    return result;
}

// How much `cand` looks like the original by place and direction only (used to detect that two
// equal elements swapped places; neighbours and size may change).
double placeLook(const ElementLook& ref, const ElementLook& cand, double diagonal,
                 const ResolveLimits& limits) {
    if (ref.kind != cand.kind || ref.shape != cand.shape) {
        return 0.0;
    }
    if (angleBetweenDegrees(ref, cand) > limits.directionGateDegrees) {
        return 0.0;
    }
    return 0.7 * positionScore(ref, cand, diagonal, limits) +
           0.3 * ((ref.radius > 0.0 && cand.radius > 0.0) ? ratio(ref.radius, cand.radius) : 1.0);
}

ResolveOutcome help(HelpReason reason, std::string detail, FeatureId consumedBy = {}) {
    ResolveOutcome outcome;
    outcome.state = ResolveOutcome::State::needsHelp;
    outcome.reason = reason;
    outcome.consumedBy = std::move(consumedBy);
    outcome.detail = std::move(detail);
    return outcome;
}

ResolveOutcome take(const std::vector<NamedLook>& list, std::size_t index, FoundBy by, double score,
                    std::string detail) {
    ResolveOutcome outcome;
    outcome.state = ResolveOutcome::State::found;
    outcome.by = by;
    outcome.element = list[index].name;
    outcome.id = list[index].id;
    outcome.index = index;
    outcome.score = score;
    outcome.detail = std::move(detail);
    return outcome;
}

const std::vector<NamedLook>& listOf(const BodyLooks& looks, ElementKind kind) {
    return kind == ElementKind::face ? looks.faces : looks.edges;
}

// The faces a name is made of: the name itself for a face, the two faces of "edge[A|B]".
std::vector<std::string> facesInName(const std::string& name) {
    const std::string prefix = "edge[";
    if (name.rfind(prefix, 0) != 0 || name.back() != ']') {
        return {name};
    }
    std::vector<std::string> parts;
    std::string part;
    for (std::size_t i = prefix.size(); i + 1 < name.size(); ++i) {
        if (name[i] == '|') {
            parts.push_back(part);
            part.clear();
        } else {
            part += name[i];
        }
    }
    parts.push_back(part);
    return parts;
}

// A face that was cut into pieces lives on as "name#1", "name#2" ...
bool wasSplit(const std::string& name, const std::set<std::string>& liveFaces) {
    for (const std::string& face : facesInName(name)) {
        const std::string piece = face + "#";
        const auto next = liveFaces.lower_bound(piece);
        if (next != liveFaces.end() && next->rfind(piece, 0) == 0) {
            return true;
        }
    }
    return false;
}

ResolveOutcome bySimilarity(const PersistentRef& ref, const BodyLooks& now,
                            const std::vector<ConsumedElement>& consumed,
                            const std::set<std::string>& live, const ResolveLimits& limits,
                            const ResolveSwitches& switches) {
    const std::vector<NamedLook>& list = listOf(now, ref.kind);

    struct Ranked {
        std::size_t index = 0;
        bool any = false;
        double score = 0.0;
    };
    Ranked best;
    Ranked second;
    for (std::size_t i = 0; i < list.size(); ++i) {
        const Similarity sim =
            similarity(ref.look, list[i].look, now.diagonal, live, limits, switches);
        if (!sim.comparable) {
            continue;
        }
        if (!best.any || sim.total > best.score) {
            second = best;
            best = {i, true, sim.total};
        } else if (!second.any || sim.total > second.score) {
            second = {i, true, sim.total};
        }
    }
    if (!best.any) {
        return help(HelpReason::notFound, "name not found: nothing similar enough");
    }

    double dead = 0.0;
    const ConsumedElement* deadElement = nullptr;
    if (switches.useConsumed) {
        for (const ConsumedElement& gone : consumed) {
            if (gone.kind != ref.kind) {
                continue;
            }
            const Similarity sim =
                similarity(ref.look, gone.look, now.diagonal, live, limits, switches);
            if (sim.comparable && sim.total > dead) {
                dead = sim.total;
                deadElement = &gone;
            }
        }
    }
    char scores[96];
    std::snprintf(scores, sizeof scores, " [best %.2f, second %.2f, consumed %.2f]", best.score,
                  second.any ? second.score : 0.0, dead);

    // Consumed elements compete too: a reference that looks at least as much like something a
    // step used up as like any live element has lost its target.
    if (deadElement != nullptr && dead >= limits.consumedFloor &&
        dead >= best.score - limits.consumedSlack) {
        return help(HelpReason::consumed,
                    "name not found: looks like an element that was used up (" + deadElement->name +
                        ")" + scores);
    }

    // The winner may also be clear by place: it sits exactly where the original was and the
    // runner-up is much further away (typical when a step was replaced by an equal one).
    bool nearest = !second.any;
    if (!nearest) {
        const double d1 = distanceBetween(list[best.index].look.centroid, ref.look.centroid);
        const double d2 = distanceBetween(list[second.index].look.centroid, ref.look.centroid);
        nearest = d1 <= limits.nearestAbsolute * now.diagonal && d1 <= limits.nearestRatio * d2;
    }
    const double liveRival = nearest || !second.any ? 0.0 : second.score;
    const double rival = std::max(liveRival, dead);
    if (best.score >= limits.minScore && best.score - rival >= limits.minMargin) {
        return take(list, best.index, FoundBy::similarity, best.score,
                    std::string("name not found: clear winner by similarity") + scores);
    }
    return help(HelpReason::notFound, std::string("name not found: no clear winner") + scores);
}

} // namespace

namespace {

// The number of the step in the first face of a name: 5 for "F5/box/top" and
// "edge[F5/box/top|...]".
std::optional<int> stepNumberOf(const std::string& name) {
    const std::vector<std::string> faces = facesInName(name);
    if (faces.empty() || faces.front().size() < 3 || faces.front()[0] != 'F') {
        return std::nullopt;
    }
    const std::string& face = faces.front();
    std::size_t end = 1;
    int number = 0;
    while (end < face.size() && face[end] >= '0' && face[end] <= '9' && number < 100000000) {
        number = number * 10 + (face[end] - '0');
        ++end;
    }
    if (end == 1 || end >= face.size() || face[end] != '/') {
        return std::nullopt;
    }
    return number;
}

} // namespace

Placement placementOf(const Document& document, const FeatureRegistry& registry,
                      const std::vector<FeatureState>& states, const std::string& elementName) {
    const std::optional<int> number = stepNumberOf(elementName);
    if (!number) {
        return {};
    }
    for (const Feature& feature : document.features()) {
        if (feature.number != *number) {
            continue;
        }
        const FeatureType* type = registry.find(feature.type);
        const auto state = std::find_if(states.begin(), states.end(),
                                        [&](const FeatureState& s) { return s.id == feature.id; });
        if (type == nullptr || state == states.end() || !state->description) {
            return {};
        }
        Placement placement;
        placement.angles = FeatureType::turnOf(state->params);
        if (!placement.angles.isZero()) {
            placement.centre = type->bodyCentre(state->params);
        }
        return placement;
    }
    return {};
}

namespace {

geo::Point3d toWorldPoint(const geo::Point3d& local, const Placement& placement) {
    const geo::Point3d turned = geo::rotatedAbout(local, placement.centre, placement.angles);
    return {turned.x + placement.offset.x, turned.y + placement.offset.y,
            turned.z + placement.offset.z};
}

geo::Point3d unit(const geo::Point3d& v) {
    const double length = std::hypot(v.x, v.y, v.z);
    return length < 1e-12 ? geo::Point3d{0, 0, 1}
                          : geo::Point3d{v.x / length, v.y / length, v.z / length};
}

geo::Point3d cross(const geo::Point3d& a, const geo::Point3d& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

// Any unit vector at right angles to `n` (unit).
geo::Point3d anyPerpendicular(const geo::Point3d& n) {
    const geo::Point3d helper = std::abs(n.x) < 0.9 ? geo::Point3d{1, 0, 0} : geo::Point3d{0, 1, 0};
    return unit(cross(n, helper));
}

} // namespace

RefGhost ghostOf(const PersistentRef& ref, const Placement& placement) {
    const ElementLook& look = ref.look;
    RefGhost ghost;
    ghost.kind = ref.kind;
    const geo::Point3d direction = look.hasDirection ? unit(look.direction) : geo::Point3d{0, 0, 1};
    if (ref.kind == ElementKind::edge) {
        const double half = std::max(look.size, 1.0) / 2.0;
        // An edge without a direction (a free-form curve) is shown as a short upright mark.
        for (const double side : {-1.0, 1.0}) {
            ghost.points.push_back(toWorldPoint({look.centroid.x + side * half * direction.x,
                                                 look.centroid.y + side * half * direction.y,
                                                 look.centroid.z + side * half * direction.z},
                                                placement));
        }
        return ghost;
    }
    // A face: a square of the same area. In its plane for a flat face, along its axis for a
    // round one (the axis is one of the square's sides).
    const double half = std::sqrt(std::max(look.size, 1.0)) / 2.0;
    geo::Point3d u;
    geo::Point3d v;
    if (look.shape == ElementShape::plane) {
        u = anyPerpendicular(direction);
        v = unit(cross(direction, u));
    } else {
        v = direction;
        u = anyPerpendicular(direction);
    }
    for (const auto& [a, b] :
         {std::pair{-1.0, -1.0}, std::pair{1.0, -1.0}, std::pair{1.0, 1.0}, std::pair{-1.0, 1.0}}) {
        ghost.points.push_back(toWorldPoint({look.centroid.x + half * (a * u.x + b * v.x),
                                             look.centroid.y + half * (a * u.y + b * v.y),
                                             look.centroid.z + half * (a * u.z + b * v.z)},
                                            placement));
    }
    return ghost;
}

std::optional<PersistentRef> makeRef(const BodyLooks& looks, ElementKind kind, std::uint32_t id,
                                     const Placement& placement) {
    for (const NamedLook& entry : listOf(looks, kind)) {
        if (entry.id == id) {
            if (entry.name.empty()) {
                return std::nullopt;
            }
            return PersistentRef{kind, entry.name, toBodyFrame(entry.look, placement)};
        }
    }
    return std::nullopt;
}

ElementLook toBodyFrame(const ElementLook& look, const Placement& placement) {
    const geo::Matrix3 back = transposed(geo::rotationOf(placement.angles));
    const auto point = [&](const geo::Point3d& world) {
        const geo::Point3d local = back.apply({world.x - placement.offset.x - placement.centre.x,
                                               world.y - placement.offset.y - placement.centre.y,
                                               world.z - placement.offset.z - placement.centre.z});
        return geo::Point3d{local.x + placement.centre.x, local.y + placement.centre.y,
                            local.z + placement.centre.z};
    };
    ElementLook out = look;
    out.centroid = point(look.centroid);
    if (look.hasDirection) {
        out.direction = back.apply(look.direction);
    }
    for (NeighbourLook& neighbour : out.neighbourLooks) {
        neighbour.centroid = point(neighbour.centroid);
        if (neighbour.hasDirection) {
            neighbour.direction = back.apply(neighbour.direction);
        }
    }
    return out;
}

BodyLooks toBodyFrame(const BodyLooks& looks, const PlacementFn& placementOf) {
    BodyLooks out = looks;
    for (NamedLook& entry : out.faces) {
        entry.look = toBodyFrame(entry.look, placementOf(entry.name));
    }
    for (NamedLook& entry : out.edges) {
        entry.look = toBodyFrame(entry.look, placementOf(entry.name));
    }
    return out;
}

BodyLooks toBodyFrame(const BodyLooks& looks, const Placement& placement) {
    return toBodyFrame(looks, PlacementFn([&](const std::string&) { return placement; }));
}

ResolveOutcome resolve(const PersistentRef& ref, const BodyLooks& worldNow,
                       const std::vector<ConsumedElement>& worldConsumed,
                       const Placement& placement, const ResolveLimits& limits,
                       const ResolveSwitches& switches) {
    return resolve(ref, worldNow, worldConsumed,
                   PlacementFn([&](const std::string&) { return placement; }), limits, switches);
}

ResolveOutcome resolve(const PersistentRef& ref, const BodyLooks& worldNow,
                       const std::vector<ConsumedElement>& worldConsumed,
                       const PlacementFn& placement, const ResolveLimits& limits,
                       const ResolveSwitches& switches) {
    const BodyLooks now = toBodyFrame(worldNow, placement);
    // The memory of used-up elements is saved in the coordinates of their steps already.
    const std::vector<ConsumedElement>& consumed = worldConsumed;
    const std::vector<NamedLook>& list = listOf(now, ref.kind);
    if (list.empty()) {
        return help(HelpReason::notFound, "no body");
    }

    std::set<std::string> live;
    for (const NamedLook& face : now.faces) {
        if (!face.name.empty()) {
            live.insert(face.name);
        }
    }

    std::vector<std::size_t> hits;
    if (!ref.name.empty()) {
        for (std::size_t i = 0; i < list.size(); ++i) {
            if (list[i].name == ref.name) {
                hits.push_back(i);
            }
        }
    }

    if (hits.size() == 1) {
        const std::size_t index = hits.front();
        const ElementLook& cand = list[index].look;
        if (cand.shape != ref.look.shape) {
            return help(HelpReason::notFound,
                        "the name now points to a different kind of surface or curve");
        }
        if (switches.checkTurn && angleBetweenDegrees(ref.look, cand) > limits.plausibleDegrees) {
            return help(HelpReason::turned, "the name points to an element that turned around");
        }
        // Name and look must not contradict each other: if another equal element sits where the
        // original was and the named one moved away, the two probably changed places.
        if (switches.checkSwap) {
            const double named = placeLook(ref.look, cand, now.diagonal, limits);
            for (std::size_t i = 0; i < list.size(); ++i) {
                if (i == index) {
                    continue;
                }
                const double other = placeLook(ref.look, list[i].look, now.diagonal, limits);
                if (other >= limits.swapOther && named <= limits.swapNamed &&
                    other - named >= limits.swapGap) {
                    return help(HelpReason::swapped,
                                "the name points away from where the element used to be, and an "
                                "equal element sits there now (places swapped?)");
                }
            }
        }
        return take(list, index, FoundBy::name, 1.0, "by name");
    }

    // A name that exists twice or more cannot happen (such names belong to nobody), but if it
    // does, it is not a safe name.
    if (hits.size() > 1) {
        return help(HelpReason::split, "the name now matches several elements");
    }

    if (wasSplit(ref.name, live)) {
        return help(HelpReason::split, "the element was cut into pieces");
    }

    return bySimilarity(ref, now, consumed, live, limits, switches);
}

PersistentRef refreshed(const PersistentRef& ref, const BodyLooks& now,
                        const ResolveOutcome& outcome, const Placement& placement) {
    return refreshed(ref, now, outcome, PlacementFn([&](const std::string&) { return placement; }));
}

PersistentRef refreshed(const PersistentRef& ref, const BodyLooks& now,
                        const ResolveOutcome& outcome, const PlacementFn& placement) {
    if (outcome.state != ResolveOutcome::State::found) {
        return ref;
    }
    const std::vector<NamedLook>& list = listOf(now, ref.kind);
    if (outcome.index >= list.size()) {
        return ref;
    }
    PersistentRef out = ref;
    out.name = list[outcome.index].name;
    out.look = toBodyFrame(list[outcome.index].look, placement(out.name));
    return out;
}

} // namespace fission::core
