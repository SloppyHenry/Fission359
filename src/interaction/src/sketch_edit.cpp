// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/interaction/sketch_edit.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

#include "fission/geo/error.h"
#include "fission/interaction/sketch_shapes.h"

namespace fission::interaction {

namespace {

using core::SketchElement;
using core::SketchElementKind;
using core::SketchPoint;

constexpr double kTwoPi = 2.0 * std::numbers::pi;
constexpr double kSplineTolerance = 0.005; // mm, as the sketch draws curves

double distance(const SketchPoint& a, const SketchPoint& b) {
    return std::hypot(b.u - a.u, b.v - a.v);
}

bool isCurve(const SketchElement& element) {
    return element.kind != SketchElementKind::point &&
           core::sketchPointsUsable(element.kind, element.points);
}

SketchShape shapeOf(const SketchElement& element) {
    SketchShape shape;
    shape.kind = element.kind;
    const auto& p = element.points;
    switch (element.kind) {
    case SketchElementKind::line:
        shape.first = p[0];
        shape.last = p[1];
        shape.span = distance(p[0], p[1]);
        break;
    case SketchElementKind::circle:
    case SketchElementKind::arc: {
        shape.first = p[0];
        shape.last = p[1];
        shape.radius = distance(p[0], p[1]);
        shape.start = std::atan2(p[1].v - p[0].v, p[1].u - p[0].u);
        shape.span = kTwoPi;
        if (element.kind == SketchElementKind::arc) {
            shape.finish = p[2];
            double sweep = std::atan2(p[2].v - p[0].v, p[2].u - p[0].u) - shape.start;
            sweep = std::fmod(sweep, kTwoPi);
            sweep = sweep < 0.0 ? sweep + kTwoPi : sweep;
            shape.span = sweep > 1e-12 ? sweep : kTwoPi;
        }
        break;
    }
    default:
        break;
    }
    return shape;
}

// The place on the shape at a parameter (line: distance from start; circle, arc: angle).
SketchPoint pointAtParameter(const SketchShape& shape, double parameter) {
    if (shape.kind == SketchElementKind::line) {
        const double t = shape.span > 0.0 ? parameter / shape.span : 0.0;
        return {shape.first.u + (shape.last.u - shape.first.u) * t,
                shape.first.v + (shape.last.v - shape.first.v) * t};
    }
    return {shape.first.u + shape.radius * std::cos(shape.start + parameter),
            shape.first.v + shape.radius * std::sin(shape.start + parameter)};
}

double angleOf(const SketchShape& shape, const SketchPoint& at) {
    double a = std::atan2(at.v - shape.first.v, at.u - shape.first.u) - shape.start;
    a = std::fmod(a, kTwoPi);
    return a < 0.0 ? a + kTwoPi : a;
}

struct Nearest {
    double distance = std::numeric_limits<double>::max();
    double parameter = 0.0;
};

// How far `at` is from the drawn shape, and the parameter of the nearest place on it.
Nearest nearestOn(const SketchShape& shape, const SketchPoint& at) {
    Nearest best;
    if (shape.kind == SketchElementKind::line) {
        const double dx = shape.last.u - shape.first.u;
        const double dy = shape.last.v - shape.first.v;
        const double length = shape.span;
        const double t = std::clamp(((at.u - shape.first.u) * dx + (at.v - shape.first.v) * dy) /
                                        (length * length),
                                    0.0, 1.0);
        best.parameter = t * length;
        best.distance = distance(at, pointAtParameter(shape, best.parameter));
        return best;
    }
    const double angle = angleOf(shape, at);
    if (shape.kind == SketchElementKind::circle || angle <= shape.span) {
        best.parameter = angle;
        best.distance = std::abs(distance(at, shape.first) - shape.radius);
        return best;
    }
    const SketchPoint startPoint = pointAtParameter(shape, 0.0);
    const SketchPoint endPoint = pointAtParameter(shape, shape.span);
    const double toStart = distance(at, startPoint);
    const double toEnd = distance(at, endPoint);
    best.parameter = toStart <= toEnd ? 0.0 : shape.span;
    best.distance = std::min(toStart, toEnd);
    return best;
}

double distanceToChain(const SketchPoint& at, const std::vector<SketchPoint>& chain) {
    double best = std::numeric_limits<double>::max();
    for (std::size_t i = 1; i < chain.size(); ++i) {
        const double dx = chain[i].u - chain[i - 1].u;
        const double dy = chain[i].v - chain[i - 1].v;
        const double length2 = dx * dx + dy * dy;
        double t = length2 > 0.0
                       ? ((at.u - chain[i - 1].u) * dx + (at.v - chain[i - 1].v) * dy) / length2
                       : 0.0;
        t = std::clamp(t, 0.0, 1.0);
        best = std::min(best, distance(at, {chain[i - 1].u + t * dx, chain[i - 1].v + t * dy}));
    }
    return best;
}

geo::SketchCurve curveOf(const SketchElement& element) {
    geo::SketchCurve curve;
    switch (element.kind) {
    case SketchElementKind::line:
        curve.kind = geo::SketchCurveKind::line;
        break;
    case SketchElementKind::circle:
        curve.kind = geo::SketchCurveKind::circle;
        break;
    case SketchElementKind::arc:
        curve.kind = geo::SketchCurveKind::arc;
        break;
    default:
        curve.kind = geo::SketchCurveKind::spline;
        break;
    }
    for (const SketchPoint& point : element.points) {
        curve.points.push_back({point.u, point.v});
    }
    return curve;
}

// Points along a shape from parameter `a` to `b` (b > a), the first and last given exactly.
std::vector<SketchPoint> sample(const SketchShape& shape, double a, double b,
                                const SketchPoint& from, const SketchPoint& to) {
    if (shape.kind == SketchElementKind::line) {
        return {from, to};
    }
    const double step = std::numbers::pi / 90.0; // two degrees
    const int segments = std::max(2, static_cast<int>(std::ceil((b - a) / step)));
    std::vector<SketchPoint> out{from};
    for (int i = 1; i < segments; ++i) {
        out.push_back(pointAtParameter(shape, a + (b - a) * i / segments));
    }
    out.push_back(to);
    return out;
}

} // namespace

// --- trimming ------------------------------------------------------------------------------

SketchTrimmer::SketchTrimmer(const geo::GeometryKernel& kernel,
                             const std::vector<SketchElement>& elements) {
    std::vector<geo::SketchCurve> curves;
    for (const SketchElement& element : elements) {
        if (!isCurve(element)) {
            continue;
        }
        Part part;
        part.element = element;
        part.shape = shapeOf(element);
        parts_.push_back(std::move(part));
        curves.push_back(curveOf(element));
    }
    std::vector<std::vector<geo::SketchCrossing>> crossings;
    try {
        crossings = kernel.sketchIntersections(curves);
    } catch (const geo::GeometryError&) {
        usable_ = false;
        parts_.clear();
        return;
    }
    for (std::size_t i = 0; i < parts_.size(); ++i) {
        Part& part = parts_[i];
        part.crossed = !crossings[i].empty();
        // arc length of one unit of the parameter
        const double scale = part.element.kind == SketchElementKind::line ? 1.0 : part.shape.radius;
        double lastLength = 0.0;
        const double fullLength = part.shape.span * scale;
        for (const geo::SketchCrossing& crossing : crossings[i]) {
            const double length = crossing.parameter * scale;
            if (part.element.kind == SketchElementKind::spline) {
                break; // a curve is never cut
            }
            const bool ring = part.element.kind == SketchElementKind::circle;
            if (!ring &&
                (length < kSketchMinimumSize || length > fullLength - kSketchMinimumSize)) {
                continue; // at an end: no cut
            }
            if (!part.cuts.empty() && length - lastLength < kSketchMinimumSize) {
                continue;
            }
            part.cuts.push_back({crossing.parameter, {crossing.point.x, crossing.point.y}});
            lastLength = length;
        }
        if (part.element.kind == SketchElementKind::circle && part.cuts.size() >= 2 &&
            fullLength - (part.cuts.back().parameter - part.cuts.front().parameter) * scale <
                kSketchMinimumSize) {
            part.cuts.pop_back(); // the last cut is on the first, round the circle
        }
        if (part.element.kind == SketchElementKind::spline) {
            try {
                std::vector<geo::Point2d> through;
                for (const SketchPoint& point : part.element.points) {
                    through.push_back({point.u, point.v});
                }
                for (const geo::Point2d& point :
                     kernel.sketchSpline(through, kSplineTolerance).polyline) {
                    part.curve.push_back({point.x, point.y});
                }
            } catch (const geo::GeometryError&) {
                part.curve = part.element.points; // the points themselves, to find it at least
            }
        }
    }
}

const SketchTrimmer::Part* SketchTrimmer::find(const std::string& element) const {
    for (const Part& part : parts_) {
        if (part.element.id == element) {
            return &part;
        }
    }
    return nullptr;
}

std::size_t SketchTrimmer::pieceCount(const Part& part) const {
    switch (part.element.kind) {
    case SketchElementKind::circle:
        return part.cuts.size() >= 2 ? part.cuts.size() : 1;
    case SketchElementKind::line:
    case SketchElementKind::arc:
        return part.cuts.size() + 1;
    default:
        return 1;
    }
}

std::optional<std::size_t> SketchTrimmer::pieceOf(const Part& part, double parameter) const {
    const std::size_t count = pieceCount(part);
    if (count == 1) {
        return 0;
    }
    if (part.element.kind == SketchElementKind::circle) {
        std::size_t index = count - 1; // before the first cut: the piece that wraps round
        for (std::size_t i = 0; i < part.cuts.size(); ++i) {
            if (part.cuts[i].parameter <= parameter) {
                index = i;
            }
        }
        return index;
    }
    std::size_t index = 0;
    while (index < part.cuts.size() && part.cuts[index].parameter < parameter) {
        ++index;
    }
    return index;
}

SketchTrimmer::Cut SketchTrimmer::pieceStart(const Part& part, std::size_t index) const {
    if (part.element.kind == SketchElementKind::circle) {
        return part.cuts[index];
    }
    if (index == 0) {
        return {0.0, part.element.kind == SketchElementKind::line ? part.element.points[0]
                                                                  : part.element.points[1]};
    }
    return part.cuts[index - 1];
}

SketchTrimmer::Cut SketchTrimmer::pieceEnd(const Part& part, std::size_t index) const {
    if (part.element.kind == SketchElementKind::circle) {
        const std::size_t next = (index + 1) % part.cuts.size();
        Cut cut = part.cuts[next];
        if (next <= index) {
            cut.parameter += kTwoPi; // round the circle past its direction 0
        }
        return cut;
    }
    if (index == part.cuts.size()) {
        return {part.shape.span, part.element.kind == SketchElementKind::line
                                     ? part.element.points[1]
                                     : part.element.points[2]};
    }
    return part.cuts[index];
}

std::optional<SketchPiece> SketchTrimmer::pieceAt(const SketchPoint& at, double reach,
                                                  const std::string& prefer) const {
    constexpr double kTie = 1e-9;
    const Part* best = nullptr;
    double bestDistance = reach;
    double bestParameter = 0.0;
    for (const Part& part : parts_) {
        Nearest near;
        if (part.element.kind == SketchElementKind::spline) {
            near.distance = distanceToChain(at, part.curve);
        } else {
            near = nearestOn(part.shape, at);
        }
        const bool better =
            best == nullptr ? near.distance <= reach : near.distance < bestDistance - kTie;
        const bool tiePreferred =
            best != nullptr && near.distance <= bestDistance + kTie && part.element.id == prefer;
        if (better || tiePreferred) {
            best = &part;
            bestDistance = near.distance;
            bestParameter = near.parameter;
        }
    }
    if (best == nullptr) {
        return std::nullopt;
    }
    const auto index = pieceOf(*best, bestParameter);
    if (!index) {
        return std::nullopt;
    }
    return SketchPiece{best->element.id, *index};
}

std::vector<SketchPoint> SketchTrimmer::outline(const SketchPiece& piece) const {
    const Part* part = find(piece.element);
    if (part == nullptr || piece.index >= pieceCount(*part)) {
        return {};
    }
    if (part->element.kind == SketchElementKind::spline) {
        return part->curve;
    }
    if (part->element.kind == SketchElementKind::circle && part->cuts.size() < 2) {
        std::vector<SketchPoint> ring =
            sample(part->shape, 0.0, kTwoPi, part->element.points[1], part->element.points[1]);
        return ring;
    }
    const Cut from = pieceStart(*part, piece.index);
    const Cut to = pieceEnd(*part, piece.index);
    return sample(part->shape, from.parameter, to.parameter, from.point, to.point);
}

SketchEdit SketchTrimmer::trim(const std::vector<SketchPiece>& marked) const {
    SketchEdit edit;
    for (const Part& part : parts_) {
        const std::size_t count = pieceCount(part);
        std::vector<bool> gone(count, false);
        bool any = false;
        for (const SketchPiece& piece : marked) {
            if (piece.element == part.element.id && piece.index < count) {
                gone[piece.index] = true;
                any = true;
            }
        }
        if (!any) {
            continue;
        }
        const auto& id = part.element.id;
        if (part.element.kind == SketchElementKind::spline) {
            if (part.crossed) {
                edit.notice = SketchEditNotice::curvePartial;
            } else {
                edit.removed.push_back(id);
            }
            continue;
        }
        const bool ring = part.element.kind == SketchElementKind::circle;
        if (std::ranges::all_of(gone, [](bool value) { return value; })) {
            edit.removed.push_back(id);
            continue;
        }
        // the runs of pieces that stay, each from the start of its first to the end of its last
        std::vector<std::pair<std::size_t, std::size_t>> runs;
        if (ring) {
            std::size_t first = 0;
            while (!gone[first]) {
                ++first;
            }
            std::optional<std::size_t> runStart;
            for (std::size_t step = 1; step <= count; ++step) {
                const std::size_t i = (first + step) % count;
                if (!gone[i]) {
                    if (!runStart) {
                        runStart = i;
                    }
                } else if (runStart) {
                    runs.emplace_back(*runStart, (i + count - 1) % count);
                    runStart.reset();
                }
            }
        } else {
            std::optional<std::size_t> runStart;
            for (std::size_t i = 0; i <= count; ++i) {
                if (i < count && !gone[i]) {
                    if (!runStart) {
                        runStart = i;
                    }
                } else if (runStart) {
                    runs.emplace_back(*runStart, i - 1);
                    runStart.reset();
                }
            }
        }
        const SketchPoint middle = part.element.points[0];
        bool keepsId = !ring; // a circle becomes arcs: they are new elements
        for (const auto& [firstPiece, lastPiece] : runs) {
            const SketchPoint from = pieceStart(part, firstPiece).point;
            const SketchPoint to = pieceEnd(part, lastPiece).point;
            std::vector<SketchPoint> points;
            if (part.element.kind == SketchElementKind::line) {
                points = {from, to};
            } else {
                points = {middle, from, to};
            }
            if (keepsId) {
                keepsId = false;
                edit.moves.push_back({id, points});
            } else {
                SketchElement piece;
                piece.kind = ring ? SketchElementKind::arc : part.element.kind;
                piece.points = points;
                piece.construction = part.element.construction;
                edit.added.push_back(std::move(piece));
            }
        }
        if (ring) {
            edit.removed.push_back(id);
        }
    }
    return edit;
}

// --- extending -----------------------------------------------------------------------------

std::optional<SketchExtension> findExtension(const geo::GeometryKernel& kernel,
                                             const std::vector<SketchElement>& elements,
                                             const SketchPoint& at, double reach, bool* usable) {
    if (usable != nullptr) {
        *usable = true;
    }
    // the line or arc nearest to the pointer
    const SketchElement* chosen = nullptr;
    Nearest nearest;
    nearest.distance = reach;
    for (const SketchElement& element : elements) {
        if ((element.kind != SketchElementKind::line && element.kind != SketchElementKind::arc) ||
            !core::sketchPointsUsable(element.kind, element.points)) {
            continue;
        }
        const Nearest near = nearestOn(shapeOf(element), at);
        if (near.distance <= nearest.distance) {
            chosen = &element;
            nearest = near;
        }
    }
    if (chosen == nullptr) {
        return std::nullopt;
    }
    const SketchShape shape = shapeOf(*chosen);
    const bool atEnd = nearest.parameter >= shape.span / 2.0;
    std::vector<geo::SketchCurve> others;
    for (const SketchElement& element : elements) {
        if (&element != chosen && isCurve(element)) {
            others.push_back(curveOf(element));
        }
    }
    std::vector<geo::SketchCrossing> crossings;
    try {
        crossings = kernel.sketchCarrierCrossings(curveOf(*chosen), others);
    } catch (const geo::GeometryError&) {
        if (usable != nullptr) {
            *usable = false;
        }
        return std::nullopt;
    }
    // the crossing that lies beyond the end, nearest first
    const bool line = chosen->kind == SketchElementKind::line;
    const double scale = line ? 1.0 : shape.radius;
    std::optional<double> found;
    SketchPoint target;
    for (const geo::SketchCrossing& crossing : crossings) {
        double beyond = 0.0; // how far beyond the end, in mm
        if (line) {
            beyond = atEnd ? crossing.parameter - shape.span : -crossing.parameter;
        } else {
            beyond =
                (atEnd ? crossing.parameter - shape.span : kTwoPi - crossing.parameter) * scale;
            if (crossing.parameter < shape.span) {
                continue; // on the arc itself
            }
            if ((kTwoPi - crossing.parameter) * scale < kSketchMinimumSize) {
                continue; // at the start of the arc, round the circle
            }
        }
        if (beyond < kSketchMinimumSize) {
            continue; // not beyond the end: it is there already, or behind it
        }
        if (!found || beyond < *found) {
            found = beyond;
            target = {crossing.point.x, crossing.point.y};
        }
    }
    if (!found) {
        return std::nullopt;
    }
    SketchExtension extension;
    extension.element = chosen->id;
    extension.atEnd = atEnd;
    extension.target = target;
    if (line) {
        extension.from = atEnd ? chosen->points[1] : chosen->points[0];
        extension.preview = {extension.from, target};
    } else {
        // the end of an arc is where its end direction meets the circle
        const double endAngle = shape.span;
        extension.from = atEnd ? pointAtParameter(shape, endAngle) : chosen->points[1];
        const double targetAngle = angleOf(shape, target);
        if (atEnd) {
            extension.preview = sample(shape, endAngle, targetAngle, extension.from, target);
        } else {
            // backwards from the start: from the target (angle targetAngle) up to the start
            // (angle 2π), drawn from the start to the target
            std::vector<SketchPoint> back =
                sample(shape, targetAngle, kTwoPi, target, extension.from);
            std::ranges::reverse(back);
            extension.preview = std::move(back);
        }
    }
    return extension;
}

SketchEdit extensionEdit(const std::vector<SketchElement>& elements,
                         const SketchExtension& extension) {
    SketchEdit edit;
    const auto found = std::ranges::find(elements, extension.element, &SketchElement::id);
    if (found == elements.end()) {
        return edit;
    }
    std::vector<SketchPoint> points = found->points;
    if (found->kind == SketchElementKind::line) {
        points[extension.atEnd ? 1 : 0] = extension.target;
    } else {
        points[extension.atEnd ? 2 : 1] = extension.target;
    }
    edit.moves.push_back({extension.element, std::move(points)});
    return edit;
}

} // namespace fission::interaction
