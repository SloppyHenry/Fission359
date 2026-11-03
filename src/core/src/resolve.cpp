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

ResolveOutcome help(HelpReason reason, std::string detail) {
    ResolveOutcome outcome;
    outcome.state = ResolveOutcome::State::needsHelp;
    outcome.reason = reason;
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

BodyLooks toBodyFrame(const BodyLooks& looks, const Placement& placement) {
    BodyLooks out = looks;
    for (NamedLook& entry : out.faces) {
        entry.look = toBodyFrame(entry.look, placement);
    }
    for (NamedLook& entry : out.edges) {
        entry.look = toBodyFrame(entry.look, placement);
    }
    return out;
}

ResolveOutcome resolve(const PersistentRef& ref, const BodyLooks& worldNow,
                       const std::vector<ConsumedElement>& worldConsumed,
                       const Placement& placement, const ResolveLimits& limits,
                       const ResolveSwitches& switches) {
    const BodyLooks now = toBodyFrame(worldNow, placement);
    std::vector<ConsumedElement> consumed = worldConsumed;
    for (ConsumedElement& gone : consumed) {
        gone.look = toBodyFrame(gone.look, placement);
    }
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
    if (outcome.state != ResolveOutcome::State::found) {
        return ref;
    }
    const std::vector<NamedLook>& list = listOf(now, ref.kind);
    if (outcome.index >= list.size()) {
        return ref;
    }
    PersistentRef out = ref;
    out.name = list[outcome.index].name;
    out.look = toBodyFrame(list[outcome.index].look, placement);
    return out;
}

} // namespace fission::core
