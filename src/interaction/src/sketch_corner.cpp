// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Rounding and chamfering corners (AP-2.1e2, decision 39): the two elements of a corner are
// shortened, a tangent arc (or a line) is put between them. Exact: the centre of the arc is where
// the two moved carriers meet, the tangent points are the feet of the centre on the elements.

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

#include "fission/interaction/sketch_offset.h"
#include "fission/interaction/sketch_shapes.h"

#include "plane_math.h"

namespace fission::interaction {

namespace {

using core::SketchElement;
using core::SketchElementKind;
using core::SketchPoint;
using namespace detail;

constexpr double kJoinTolerance = 1e-6;
constexpr double kFitTolerance = 1e-9;

// One end of a corner: the element and the way it leaves the corner.
struct Leg {
    const SketchElement* element = nullptr;
    bool atEnd = false;
    bool arc = false;
    Pt corner;
    Pt dir;           ///< unit direction in which it leaves the corner
    double avail = 0; ///< length available (mm, along the element)
    Pt centre;        ///< arc
    double radius = 0.0;
    double turn = 1.0;   ///< arc: +1 counter-clockwise when leaving the corner
    double theta0 = 0.0; ///< arc: the angle of the corner about the centre
};

bool curveKind(const SketchElement& element) {
    return element.kind == SketchElementKind::line || element.kind == SketchElementKind::arc;
}

double arcSweepOf(const SketchElement& element) {
    const double sweep = wrapAngle(angleOf(element.points[0], element.points[2]) -
                                   angleOf(element.points[0], element.points[1]));
    return sweep > 1e-12 ? sweep : kTwoPi;
}

std::optional<Leg> makeLeg(const SketchElement& element, bool atEnd, const Pt& corner) {
    if (!curveKind(element) || !core::sketchPointsUsable(element.kind, element.points)) {
        return std::nullopt;
    }
    Leg leg;
    leg.element = &element;
    leg.atEnd = atEnd;
    leg.corner = corner;
    if (element.kind == SketchElementKind::line) {
        const Pt other = atEnd ? element.points[0] : element.points[1];
        leg.dir = unit(sub(other, corner));
        leg.avail = dist(other, corner);
        return leg;
    }
    leg.arc = true;
    leg.centre = element.points[0];
    leg.radius = dist(element.points[0], element.points[1]);
    leg.turn = atEnd ? -1.0 : 1.0;
    leg.theta0 = angleOf(leg.centre, corner);
    leg.dir = mul(left(unit(sub(corner, leg.centre))), leg.turn);
    leg.avail = leg.radius * arcSweepOf(element);
    return leg;
}

Pt pointAlong(const Leg& leg, double s) {
    if (!leg.arc) {
        return add(leg.corner, mul(leg.dir, s));
    }
    return onCircle(leg.centre, leg.radius, leg.theta0 + leg.turn * s / leg.radius);
}

// How far along the element from the corner a place of its carrier lies.
double distanceAlong(const Leg& leg, const Pt& at) {
    if (!leg.arc) {
        return dot(sub(at, leg.corner), leg.dir);
    }
    return leg.radius * wrapAngle(leg.turn * (angleOf(leg.centre, at) - leg.theta0));
}

struct Legs {
    Leg a;
    Leg b;
};

std::optional<Legs> legsOf(const std::vector<SketchElement>& elements, const SketchCorner& corner) {
    const auto first = std::ranges::find(elements, corner.first, &SketchElement::id);
    const auto second = std::ranges::find(elements, corner.second, &SketchElement::id);
    if (first == elements.end() || second == elements.end()) {
        return std::nullopt;
    }
    const auto a = makeLeg(*first, corner.firstAtEnd, corner.at);
    const auto b = makeLeg(*second, corner.secondAtEnd, corner.at);
    if (!a || !b || std::abs(cross(a->dir, b->dir)) < 1e-9) {
        return std::nullopt; // not a corner: they go on smoothly (or fold back)
    }
    return Legs{*a, *b};
}

// The places the centre of a circle of this radius can be: at that distance from the carrier of
// `leg`, on the side where the other leg goes.
std::optional<Carrier> locus(const Leg& leg, const Pt& otherDir, double radius) {
    Carrier carrier;
    if (!leg.arc) {
        const double side = cross(leg.dir, otherDir) > 0.0 ? 1.0 : -1.0;
        carrier.p = add(leg.corner, mul(left(leg.dir), side * radius));
        carrier.d = leg.dir;
        return carrier;
    }
    const double out = dot(otherDir, unit(sub(leg.corner, leg.centre))) > 0.0 ? 1.0 : -1.0;
    const double r = leg.radius + out * radius;
    if (r < kFitTolerance) {
        return std::nullopt;
    }
    carrier.circle = true;
    carrier.p = leg.centre;
    carrier.r = r;
    return carrier;
}

Pt footOn(const Leg& leg, const Pt& centre) {
    if (!leg.arc) {
        return add(leg.corner, mul(leg.dir, dot(sub(centre, leg.corner), leg.dir)));
    }
    return onCircle(leg.centre, leg.radius, angleOf(leg.centre, centre));
}

struct Fillet {
    Pt centre;
    Pt tangentA;
    Pt tangentB;
};

// The rounding of this radius, when it fits on both elements (leaving at least 0.01 mm).
std::optional<Fillet> tryFillet(const Legs& legs, double radius) {
    const auto la = locus(legs.a, legs.b.dir, radius);
    const auto lb = locus(legs.b, legs.a.dir, radius);
    if (!la || !lb) {
        return std::nullopt;
    }
    std::optional<Fillet> best;
    double bestSum = std::numeric_limits<double>::max();
    for (const Pt& centre : carrierCrossings(*la, *lb)) {
        const Pt ta = footOn(legs.a, centre);
        const Pt tb = footOn(legs.b, centre);
        const double sa = distanceAlong(legs.a, ta);
        const double sb = distanceAlong(legs.b, tb);
        if (sa <= kFitTolerance || sb <= kFitTolerance ||
            sa > legs.a.avail - kSketchMinimumSize + kFitTolerance ||
            sb > legs.b.avail - kSketchMinimumSize + kFitTolerance) {
            continue;
        }
        if (sa + sb < bestSum) {
            bestSum = sa + sb;
            best = Fillet{centre, ta, tb};
        }
    }
    return best;
}

// The new end point of an element whose end at the corner moves to `to`.
SketchEdit shortened(const Legs& legs, const Pt& toA, const Pt& toB) {
    SketchEdit edit;
    const auto move = [&](const Leg& leg, const Pt& to) {
        std::vector<SketchPoint> points = leg.element->points;
        if (leg.element->kind == SketchElementKind::line) {
            points[leg.atEnd ? 1 : 0] = to;
        } else {
            points[leg.atEnd ? 2 : 1] = to;
        }
        edit.moves.push_back({leg.element->id, std::move(points)});
    };
    move(legs.a, toA);
    move(legs.b, toB);
    return edit;
}

double floorToHundredth(double value) {
    return std::max(0.0, std::floor(value * 100.0 + 1e-6) / 100.0);
}

} // namespace

std::optional<SketchCorner> findCorner(const std::vector<SketchElement>& elements,
                                       const SketchPoint& at, double reach) {
    struct End {
        std::size_t index;
        bool atEnd;
        Pt point;
    };
    std::vector<End> ends;
    for (std::size_t i = 0; i < elements.size(); ++i) {
        const SketchElement& element = elements[i];
        if (!core::sketchPointsUsable(element.kind, element.points)) {
            continue;
        }
        if (element.kind == SketchElementKind::line) {
            ends.push_back({i, false, element.points[0]});
            ends.push_back({i, true, element.points[1]});
        } else if (element.kind == SketchElementKind::arc) {
            const double radius = dist(element.points[0], element.points[1]);
            ends.push_back({i, false, element.points[1]});
            ends.push_back({i, true,
                            onCircle(element.points[0], radius,
                                     angleOf(element.points[0], element.points[2]))});
        } else if (element.kind == SketchElementKind::spline) {
            ends.push_back({i, false, element.points.front()});
            ends.push_back({i, true, element.points.back()});
        }
    }
    std::optional<SketchCorner> best;
    double nearest = reach;
    for (const End& end : ends) {
        const double d = dist(end.point, at);
        if (d > nearest) {
            continue;
        }
        std::vector<const End*> here;
        for (const End& other : ends) {
            if (dist(other.point, end.point) <= kJoinTolerance) {
                here.push_back(&other);
            }
        }
        if (here.size() != 2 || here[0]->index == here[1]->index) {
            continue; // an end alone, three ends, or an element that closes on itself
        }
        nearest = d;
        SketchCorner corner;
        corner.at = end.point;
        corner.first = elements[here[0]->index].id;
        corner.second = elements[here[1]->index].id;
        corner.firstAtEnd = here[0]->atEnd;
        corner.secondAtEnd = here[1]->atEnd;
        corner.curve = !curveKind(elements[here[0]->index]) || !curveKind(elements[here[1]->index]);
        best = corner;
    }
    return best;
}

SketchCornerResult roundCorner(const std::vector<SketchElement>& elements,
                               const SketchCorner& corner, double radius) {
    SketchCornerResult result;
    if (corner.curve) {
        result.notice = SketchCornerNotice::curve;
        return result;
    }
    const auto legs = legsOf(elements, corner);
    if (!legs) {
        result.notice = SketchCornerNotice::noCorner;
        return result;
    }
    if (!(radius >= kSketchMinimumSize)) {
        result.notice = SketchCornerNotice::tooSmall;
        return result;
    }
    const auto fitted = tryFillet(*legs, radius);
    if (!fitted) {
        // the biggest radius that still fits: by halving between one that fits and this one
        double fits = kSketchMinimumSize;
        if (!tryFillet(*legs, fits)) {
            result.notice = SketchCornerNotice::tooBig;
            result.limit = 0.0;
            return result;
        }
        double over = radius;
        for (int i = 0; i < 100 && over - fits > 1e-9; ++i) {
            const double middle = (fits + over) / 2.0;
            (tryFillet(*legs, middle) ? fits : over) = middle;
        }
        double limit = floorToHundredth(fits);
        if (limit >= kSketchMinimumSize && !tryFillet(*legs, limit)) {
            limit = floorToHundredth(limit - 0.01);
        }
        result.notice = SketchCornerNotice::tooBig;
        result.limit = limit;
        return result;
    }
    result.edit = shortened(*legs, fitted->tangentA, fitted->tangentB);
    SketchElement arc;
    arc.kind = SketchElementKind::arc;
    arc.construction = legs->a.element->construction && legs->b.element->construction;
    const bool counterClockwise =
        cross(sub(fitted->tangentA, fitted->centre), sub(fitted->tangentB, fitted->centre)) > 0.0;
    arc.points = counterClockwise
                     ? std::vector<SketchPoint>{fitted->centre, fitted->tangentA, fitted->tangentB}
                     : std::vector<SketchPoint>{fitted->centre, fitted->tangentB, fitted->tangentA};
    result.edit.added.push_back(std::move(arc));
    return result;
}

SketchCornerResult chamferCorner(const std::vector<SketchElement>& elements,
                                 const SketchCorner& corner, double distance) {
    SketchCornerResult result;
    if (corner.curve) {
        result.notice = SketchCornerNotice::curve;
        return result;
    }
    const auto legs = legsOf(elements, corner);
    if (!legs) {
        result.notice = SketchCornerNotice::noCorner;
        return result;
    }
    if (!(distance >= kSketchMinimumSize)) {
        result.notice = SketchCornerNotice::tooSmall;
        return result;
    }
    const double room = std::min(legs->a.avail, legs->b.avail) - kSketchMinimumSize;
    if (distance > room + kFitTolerance) {
        result.notice = SketchCornerNotice::tooBig;
        result.limit = floorToHundredth(room);
        return result;
    }
    const Pt toA = pointAlong(legs->a, distance);
    const Pt toB = pointAlong(legs->b, distance);
    result.edit = shortened(*legs, toA, toB);
    SketchElement line;
    line.kind = SketchElementKind::line;
    line.construction = legs->a.element->construction && legs->b.element->construction;
    line.points = {toA, toB};
    result.edit.added.push_back(std::move(line));
    return result;
}

double cornerRadiusAt(const std::vector<SketchElement>& elements, const SketchCorner& corner,
                      const SketchPoint& at) {
    const auto legs = legsOf(elements, corner);
    if (!legs) {
        return 0.0;
    }
    const auto from = [&](const Leg& leg) {
        return leg.arc ? std::abs(dist(at, leg.centre) - leg.radius)
                       : std::abs(cross(leg.dir, sub(at, leg.corner)));
    };
    return std::min(from(legs->a), from(legs->b));
}

double cornerDistanceAt(const std::vector<SketchElement>& elements, const SketchCorner& corner,
                        const SketchPoint& at) {
    const auto legs = legsOf(elements, corner);
    if (!legs) {
        return 0.0;
    }
    const Pt sum = add(legs->a.dir, legs->b.dir);
    if (norm(sum) < 1e-9) {
        return 0.0;
    }
    const double cosHalf = std::sqrt((1.0 + dot(legs->a.dir, legs->b.dir)) / 2.0);
    return std::max(0.0, dot(sub(at, corner.at), unit(sum)) / cosHalf);
}

} // namespace fission::interaction
