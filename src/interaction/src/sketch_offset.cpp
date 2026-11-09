// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/interaction/sketch_offset.h"

#include <algorithm>
#include <cmath>
#include <deque>
#include <limits>

#include "fission/geo/error.h"
#include "fission/interaction/sketch_shapes.h"

#include "plane_math.h"

namespace fission::interaction {

namespace {

using core::SketchElement;
using core::SketchElementKind;
using core::SketchPoint;
using namespace detail;

constexpr double kJoinTolerance = 1e-6; // ends this near are one point
constexpr double kSameTolerance = 1e-7; // moved ends this near are one point

bool usable(const SketchElement& element) {
    return core::sketchPointsUsable(element.kind, element.points);
}

// A line or an arc that is long enough to count.
struct Item {
    const SketchElement* element = nullptr;
    Pt a; ///< the start as stored
    Pt b; ///< the end as stored (an arc: where its end direction meets the circle)
};

double arcRadius(const SketchElement& element) {
    return dist(element.points[0], element.points[1]);
}

double arcSweep(const SketchElement& element) {
    const double sweep = wrapAngle(angleOf(element.points[0], element.points[2]) -
                                   angleOf(element.points[0], element.points[1]));
    return sweep > 1e-12 ? sweep : kTwoPi;
}

Pt arcEnd(const SketchElement& element) {
    return onCircle(element.points[0], arcRadius(element),
                    angleOf(element.points[0], element.points[2]));
}

bool makeItem(const SketchElement& element, Item& item) {
    if (!usable(element)) {
        return false;
    }
    item.element = &element;
    if (element.kind == SketchElementKind::line) {
        item.a = element.points[0];
        item.b = element.points[1];
        return dist(item.a, item.b) >= kSketchMinimumSize;
    }
    if (element.kind == SketchElementKind::arc) {
        item.a = element.points[1];
        item.b = arcEnd(element);
        return arcRadius(element) >= kSketchMinimumSize;
    }
    return false;
}

SketchChainSegment orient(const Item& item, bool reversed) {
    const SketchElement& element = *item.element;
    SketchChainSegment seg;
    seg.element = element.id;
    seg.reversed = reversed;
    seg.start = reversed ? item.b : item.a;
    seg.end = reversed ? item.a : item.b;
    if (element.kind == SketchElementKind::line) {
        seg.span = dist(item.a, item.b);
    } else {
        seg.arc = true;
        seg.centre = element.points[0];
        seg.radius = arcRadius(element);
        seg.ccw = !reversed;
        seg.span = arcSweep(element);
    }
    return seg;
}

// The direction of travel at a place of the segment (the place only matters for arcs).
Pt tangentAt(const SketchChainSegment& seg, const Pt& at) {
    if (!seg.arc) {
        return unit(sub(seg.end, seg.start));
    }
    const Pt t = left(unit(sub(at, seg.centre)));
    return seg.ccw ? t : mul(t, -1.0);
}

} // namespace

std::optional<SketchChain> findChain(const std::vector<SketchElement>& elements,
                                     const std::string& id) {
    const auto found = std::ranges::find(elements, id, &SketchElement::id);
    if (found == elements.end() || found->kind == SketchElementKind::point || !usable(*found)) {
        return std::nullopt;
    }
    SketchChain chain;
    chain.construction = found->construction;
    if (found->kind == SketchElementKind::spline) {
        chain.curve = true;
        return chain;
    }
    if (found->kind == SketchElementKind::circle) {
        SketchChainSegment seg;
        seg.element = found->id;
        seg.arc = seg.circle = true;
        seg.centre = found->points[0];
        seg.radius = arcRadius(*found);
        seg.start = seg.end = found->points[1];
        seg.span = kTwoPi;
        if (seg.radius < kSketchMinimumSize) {
            return std::nullopt;
        }
        chain.segments.push_back(seg);
        chain.closed = true;
        return chain;
    }
    std::vector<Item> items;
    std::size_t first = elements.size();
    for (const SketchElement& element : elements) {
        Item item;
        if (element.construction == found->construction && makeItem(element, item)) {
            if (element.id == found->id) {
                first = items.size();
            }
            items.push_back(item);
        }
    }
    if (first >= items.size()) {
        return std::nullopt;
    }
    std::vector<bool> used(items.size(), false);
    std::deque<SketchChainSegment> ring;
    ring.push_back(orient(items[first], false));
    used[first] = true;

    // the unused items with an end at `at`: index and whether it is their stored start
    const auto candidatesAt = [&](const Pt& at) {
        std::vector<std::pair<std::size_t, bool>> out;
        for (std::size_t i = 0; i < items.size(); ++i) {
            if (used[i]) {
                continue;
            }
            if (dist(items[i].a, at) <= kJoinTolerance) {
                out.emplace_back(i, true);
            } else if (dist(items[i].b, at) <= kJoinTolerance) {
                out.emplace_back(i, false);
            }
        }
        return out;
    };
    for (std::size_t guard = 0; guard <= items.size() && !chain.closed; ++guard) {
        const auto next = candidatesAt(ring.back().end);
        if (next.empty()) {
            chain.closed =
                ring.size() > 1 && dist(ring.back().end, ring.front().start) <= kJoinTolerance;
            break;
        }
        if (next.size() > 1 || dist(ring.back().end, ring.front().start) <= kJoinTolerance) {
            break; // three ends meet
        }
        used[next[0].first] = true;
        ring.push_back(orient(items[next[0].first], !next[0].second));
    }
    for (std::size_t guard = 0; guard <= items.size() && !chain.closed; ++guard) {
        const auto before = candidatesAt(ring.front().start);
        if (before.empty()) {
            chain.closed =
                ring.size() > 1 && dist(ring.back().end, ring.front().start) <= kJoinTolerance;
            break;
        }
        if (before.size() > 1 || dist(ring.back().end, ring.front().start) <= kJoinTolerance) {
            break;
        }
        used[before[0].first] = true;
        // its end must lie on our start: stored end (second) = not reversed
        ring.push_front(orient(items[before[0].first], before[0].second));
    }
    chain.segments.assign(ring.begin(), ring.end());
    return chain;
}

namespace {

// How far `at` is from one segment, on which side, and the signed distance.
struct Side {
    double distance = std::numeric_limits<double>::max();
    double sign = 1.0;
};

Side sideOf(const SketchChainSegment& seg, const Pt& at) {
    Side best;
    if (!seg.arc) {
        const Pt t = tangentAt(seg, seg.start);
        const double along = std::clamp(dot(sub(at, seg.start), t), 0.0, seg.span);
        const Pt foot = add(seg.start, mul(t, along));
        best.distance = dist(at, foot);
        best.sign = cross(t, sub(at, foot)) >= 0.0 ? 1.0 : -1.0;
        return best;
    }
    const double angleStart = angleOf(seg.centre, seg.start);
    const double swept = seg.circle || seg.ccw ? wrapAngle(angleOf(seg.centre, at) - angleStart)
                                               : wrapAngle(angleStart - angleOf(seg.centre, at));
    const double radial = dist(at, seg.centre);
    if (seg.circle || swept <= seg.span) {
        best.distance = std::abs(radial - seg.radius);
        const bool inside = radial < seg.radius;
        best.sign = (inside == seg.ccw) ? 1.0 : -1.0;
        return best;
    }
    for (const Pt& end : {seg.start, seg.end}) {
        const double d = dist(at, end);
        if (d < best.distance) {
            best.distance = d;
            best.sign = cross(tangentAt(seg, end), sub(at, end)) >= 0.0 ? 1.0 : -1.0;
        }
    }
    return best;
}

double distanceToChords(const Pt& at, const std::vector<Pt>& chain) {
    double best = std::numeric_limits<double>::max();
    for (std::size_t i = 1; i < chain.size(); ++i) {
        const Pt d = sub(chain[i], chain[i - 1]);
        const double length2 = dot(d, d);
        const double t =
            length2 > 0.0 ? std::clamp(dot(sub(at, chain[i - 1]), d) / length2, 0.0, 1.0) : 0.0;
        best = std::min(best, dist(at, add(chain[i - 1], mul(d, t))));
    }
    return best;
}

} // namespace

double chainSignedDistance(const SketchChain& chain, const SketchPoint& at) {
    Side best;
    for (const SketchChainSegment& seg : chain.segments) {
        const Side side = sideOf(seg, at);
        if (side.distance < best.distance) {
            best = side;
        }
    }
    return best.distance == std::numeric_limits<double>::max() ? 0.0 : best.sign * best.distance;
}

std::optional<std::string> elementNear(const std::vector<SketchElement>& elements,
                                       const SketchPoint& at, double reach) {
    std::optional<std::string> found;
    double nearest = reach;
    for (const SketchElement& element : elements) {
        if (element.kind == SketchElementKind::point || !usable(element)) {
            continue;
        }
        double d = std::numeric_limits<double>::max();
        Item item;
        if (element.kind == SketchElementKind::spline) {
            d = distanceToChords(at, element.points);
        } else if (element.kind == SketchElementKind::circle) {
            d = std::abs(dist(at, element.points[0]) - arcRadius(element));
        } else if (makeItem(element, item)) {
            d = sideOf(orient(item, false), at).distance;
        }
        if (d <= nearest) {
            nearest = d;
            found = element.id;
        }
    }
    return found;
}

namespace {

struct Joint {
    Pt outEnd;  ///< where the moved segment before ends
    Pt inStart; ///< where the moved segment after starts
    bool connector = false;
    Pt centre; ///< the old corner (of a connecting arc)
    double turn = 0.0;
};

// The places the moved carriers of two neighbours meet, the one nearest to the old corner.
std::optional<Pt> innerMeeting(const Carrier& a, const Carrier& b, const Pt& corner) {
    std::optional<Pt> best;
    for (const Pt& point : carrierCrossings(a, b)) {
        if (!best || dist(point, corner) < dist(*best, corner)) {
            best = point;
        }
    }
    return best;
}

Carrier movedCarrier(const SketchChainSegment& seg, double offset, const Pt& through) {
    Carrier carrier;
    if (seg.arc) {
        carrier.circle = true;
        carrier.p = seg.centre;
        carrier.r = seg.radius - offset * (seg.ccw ? 1.0 : -1.0);
    } else {
        carrier.p = through;
        carrier.d = tangentAt(seg, seg.start);
    }
    return carrier;
}

} // namespace

SketchOffsetResult offsetChain(const geo::GeometryKernel& kernel, const SketchChain& chain,
                               double signedDistance) {
    SketchOffsetResult result;
    if (chain.curve) {
        result.notice = SketchOffsetNotice::curve;
        return result;
    }
    const double o = signedDistance;
    const auto& segs = chain.segments;
    const std::size_t n = segs.size();
    if (n == 0 || !(std::abs(o) >= kSketchMinimumSize)) {
        return result;
    }
    // the radius of every arc after the move
    std::vector<double> radii(n, 0.0);
    for (std::size_t i = 0; i < n; ++i) {
        if (segs[i].arc) {
            radii[i] = segs[i].radius - o * (segs[i].ccw ? 1.0 : -1.0);
            if (radii[i] < kSketchMinimumSize) {
                result.notice = SketchOffsetNotice::tooBig;
                return result;
            }
        }
    }
    const auto element = [&](SketchElementKind kind, std::vector<SketchPoint> points) {
        SketchElement made;
        made.kind = kind;
        made.points = std::move(points);
        made.construction = chain.construction;
        return made;
    };
    std::vector<SketchElement> made;
    if (n == 1 && segs[0].circle) { // a circle only changes its radius
        const Pt direction = unit(sub(segs[0].start, segs[0].centre));
        made.push_back(element(SketchElementKind::circle,
                               {segs[0].centre, add(segs[0].centre, mul(direction, radii[0]))}));
        result.edit.added = std::move(made);
        return result;
    }

    // --- the places where neighbours meet
    const std::size_t jointCount = chain.closed ? n : n - 1;
    std::vector<Joint> joints(jointCount);
    for (std::size_t k = 0; k < jointCount; ++k) {
        const SketchChainSegment& a = segs[k];
        const SketchChainSegment& b = segs[(k + 1) % n];
        const Pt corner = a.end;
        const Pt ta = tangentAt(a, a.end);
        const Pt tb = tangentAt(b, b.start);
        const Pt out = add(a.end, mul(left(ta), o));
        const Pt in = add(b.start, mul(left(tb), o));
        Joint& joint = joints[k];
        if (dist(out, in) <= kSameTolerance) { // smooth: the moved ends are the same place
            joint.outEnd = joint.inStart = mul(add(out, in), 0.5);
            continue;
        }
        const double turn = cross(ta, tb);
        if (std::abs(turn) < 1e-9) { // the chain turns back on itself
            result.notice = SketchOffsetNotice::failed;
            return result;
        }
        const Carrier moved = movedCarrier(a, o, out);
        const Carrier movedNext = movedCarrier(b, o, in);
        if (o * turn < 0.0) { // the outer side of the corner: the moved ends lie apart
            if (!a.arc && !b.arc) {
                const auto meet = innerMeeting(moved, movedNext, corner);
                if (!meet) {
                    result.notice = SketchOffsetNotice::failed;
                    return result;
                }
                joint.outEnd = joint.inStart = *meet;
            } else {
                joint.outEnd = out;
                joint.inStart = in;
                joint.connector = true;
                joint.centre = corner;
                joint.turn = turn;
            }
        } else { // the inner side: the moved elements cross, both are cut there
            const auto meet = innerMeeting(moved, movedNext, corner);
            if (!meet) {
                result.notice =
                    (a.arc || b.arc) ? SketchOffsetNotice::tooBig : SketchOffsetNotice::crossing;
                return result;
            }
            joint.outEnd = joint.inStart = *meet;
        }
    }

    // --- the moved elements, with the connecting arcs between them
    for (std::size_t i = 0; i < n; ++i) {
        const SketchChainSegment& seg = segs[i];
        Pt start;
        Pt end;
        if (i == 0 && !chain.closed) {
            start = add(seg.start, mul(left(tangentAt(seg, seg.start)), o));
        } else {
            start = joints[(i + n - 1) % n].inStart;
        }
        if (i + 1 == n && !chain.closed) {
            end = add(seg.end, mul(left(tangentAt(seg, seg.end)), o));
        } else {
            end = joints[i].outEnd;
        }
        if (!seg.arc) {
            const Pt t = tangentAt(seg, seg.start);
            if (dot(sub(end, start), t) < kSketchMinimumSize) { // gone, or turned round
                result.notice = SketchOffsetNotice::crossing;
                return result;
            }
            made.push_back(element(SketchElementKind::line,
                                   seg.reversed ? std::vector<SketchPoint>{end, start}
                                                : std::vector<SketchPoint>{start, end}));
        } else {
            const double radius = radii[i];
            const double a0 = angleOf(seg.centre, start);
            const double a1 = angleOf(seg.centre, end);
            const double sweep = seg.ccw ? wrapAngle(a1 - a0) : wrapAngle(a0 - a1);
            // an arc only gets shorter (or stays): a longer one has turned round
            if (sweep * radius < kSketchMinimumSize || sweep > seg.span + 1e-7) {
                result.notice = SketchOffsetNotice::crossing;
                return result;
            }
            const Pt s = onCircle(seg.centre, radius, a0);
            const Pt e = onCircle(seg.centre, radius, a1);
            made.push_back(element(SketchElementKind::arc,
                                   seg.ccw ? std::vector<SketchPoint>{seg.centre, s, e}
                                           : std::vector<SketchPoint>{seg.centre, e, s}));
        }
        if (i < jointCount && joints[i].connector) {
            const Joint& joint = joints[i];
            made.push_back(
                element(SketchElementKind::arc,
                        joint.turn > 0.0
                            ? std::vector<SketchPoint>{joint.centre, joint.outEnd, joint.inStart}
                            : std::vector<SketchPoint>{joint.centre, joint.inStart, joint.outEnd}));
        }
    }

    // --- the result must not cross itself (neighbours meet only where they are joined)
    if (made.size() > 1) {
        std::vector<geo::SketchCurve> curves;
        for (const SketchElement& piece : made) {
            geo::SketchCurve curve;
            curve.kind = piece.kind == SketchElementKind::line ? geo::SketchCurveKind::line
                                                               : geo::SketchCurveKind::arc;
            for (const SketchPoint& point : piece.points) {
                curve.points.push_back({point.u, point.v});
            }
            curves.push_back(std::move(curve));
        }
        std::vector<std::vector<geo::SketchCrossing>> crossings;
        try {
            crossings = kernel.sketchIntersections(curves);
        } catch (const geo::GeometryError&) {
            result.notice = SketchOffsetNotice::failed;
            return result;
        }
        const auto endsOf = [](const SketchElement& piece) {
            return piece.kind == SketchElementKind::line
                       ? std::vector<Pt>{piece.points[0], piece.points[1]}
                       : std::vector<Pt>{piece.points[1], arcEnd(piece)};
        };
        const std::size_t m = made.size();
        constexpr double kEndNear = 1e-5;
        for (std::size_t i = 0; i < m; ++i) {
            for (const geo::SketchCrossing& crossing : crossings[i]) {
                const std::size_t j = crossing.other;
                const bool neighbours =
                    j + 1 == i || i + 1 == j ||
                    (chain.closed && ((i == 0 && j == m - 1) || (j == 0 && i == m - 1)));
                const Pt at{crossing.point.x, crossing.point.y};
                const auto nearEnd = [&](const SketchElement& piece) {
                    const auto ends = endsOf(piece);
                    return std::ranges::any_of(
                        ends, [&](const Pt& end) { return dist(end, at) <= kEndNear; });
                };
                if (!(neighbours && nearEnd(made[i]) && nearEnd(made[j]))) {
                    result.notice = SketchOffsetNotice::crossing;
                    return result;
                }
            }
        }
    }
    result.edit.added = std::move(made);
    return result;
}

} // namespace fission::interaction
