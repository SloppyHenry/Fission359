// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/interaction/sketch_pattern.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <numbers>

#include "fission/interaction/sketch_offset.h"

namespace fission::interaction {

using core::SketchElement;
using core::SketchElementKind;
using core::SketchPoint;

namespace {

double distance(const SketchPoint& a, const SketchPoint& b) {
    return std::hypot(a.u - b.u, a.v - b.v);
}

bool near(const SketchPoint& a, const SketchPoint& b) {
    return distance(a, b) <= kSketchCopyTolerance;
}

// An arc as {middle, start, end on the circle}: the stored end only gives the direction.
bool arcEndOnCircle(const SketchElement& arc, SketchPoint& out) {
    const SketchPoint& m = arc.points[0];
    const double radius = distance(m, arc.points[1]);
    const double toEnd = distance(m, arc.points[2]);
    if (radius <= 0.0 || toEnd <= 0.0) {
        return false;
    }
    out = {m.u + (arc.points[2].u - m.u) * radius / toEnd,
           m.v + (arc.points[2].v - m.v) * radius / toEnd};
    return true;
}

// True when both are the same thing drawn the same way (within the tolerance).
bool sameThing(const SketchElement& a, const SketchElement& b) {
    if (a.kind != b.kind || a.construction != b.construction ||
        a.points.size() != b.points.size()) {
        return false;
    }
    switch (a.kind) {
    case SketchElementKind::point:
        return near(a.points[0], b.points[0]);
    case SketchElementKind::line:
        return (near(a.points[0], b.points[0]) && near(a.points[1], b.points[1])) ||
               (near(a.points[0], b.points[1]) && near(a.points[1], b.points[0]));
    case SketchElementKind::circle:
        return near(a.points[0], b.points[0]) &&
               std::abs(distance(a.points[0], a.points[1]) - distance(b.points[0], b.points[1])) <=
                   kSketchCopyTolerance;
    case SketchElementKind::arc: {
        SketchPoint endA;
        SketchPoint endB;
        return near(a.points[0], b.points[0]) && near(a.points[1], b.points[1]) &&
               arcEndOnCircle(a, endA) && arcEndOnCircle(b, endB) && near(endA, endB);
    }
    case SketchElementKind::spline: {
        const auto same = [&](bool reversed) {
            for (std::size_t i = 0; i < a.points.size(); ++i) {
                const SketchPoint& other = b.points[reversed ? a.points.size() - 1 - i : i];
                if (!near(a.points[i], other)) {
                    return false;
                }
            }
            return true;
        };
        return same(false) || same(true);
    }
    }
    return false;
}

bool wellFormed(const SketchElement& element) {
    return core::sketchPointsUsable(element.kind, element.points);
}

using Map = std::function<SketchPoint(const SketchPoint&)>;

// The copy of an element under a map that keeps lengths (a move, a turn, a mirroring). `mirrored`
// turns the way round, which an arc must make up for by swapping its start and end.
std::optional<SketchElement> transformed(const SketchElement& element, const Map& map,
                                         bool mirrored) {
    SketchElement copy;
    copy.kind = element.kind;
    copy.construction = element.construction;
    if (element.kind == SketchElementKind::arc && mirrored) {
        SketchPoint end;
        if (!arcEndOnCircle(element, end)) {
            return std::nullopt;
        }
        copy.points = {map(element.points[0]), map(end), map(element.points[1])};
        return copy;
    }
    copy.points.reserve(element.points.size());
    for (const SketchPoint& point : element.points) {
        copy.points.push_back(map(point));
    }
    return copy;
}

std::vector<const SketchElement*> sourcesOf(const std::vector<SketchElement>& elements,
                                            const std::vector<std::string>& ids,
                                            const std::string& except) {
    std::vector<const SketchElement*> found;
    for (const SketchElement& element : elements) {
        if (element.id != except && wellFormed(element) &&
            std::ranges::find(ids, element.id) != ids.end()) {
            found.push_back(&element);
        }
    }
    return found;
}

// Adds the copy unless it lies on an element of the sketch or on one that is added already.
void addIfNew(SketchCopyResult& result, const std::vector<SketchElement>& elements,
              SketchElement copy) {
    const auto same = [&](const SketchElement& other) { return sameThing(copy, other); };
    if (std::ranges::any_of(elements, same) || std::ranges::any_of(result.edit.added, same)) {
        return;
    }
    result.edit.added.push_back(std::move(copy));
}

SketchCopyResult finished(SketchCopyResult result) {
    if (result.edit.added.empty() && result.notice == SketchCopyNotice::none) {
        result.notice = SketchCopyNotice::nothingNew;
    }
    return result;
}

SketchCopyResult copies(const std::vector<SketchElement>& elements,
                        const std::vector<const SketchElement*>& sources,
                        const std::vector<Map>& maps) {
    SketchCopyResult result;
    for (const Map& map : maps) {
        for (const SketchElement* source : sources) {
            if (auto copy = transformed(*source, map, false)) {
                addIfNew(result, elements, std::move(*copy));
            }
        }
    }
    return finished(std::move(result));
}

} // namespace

std::optional<std::string> pickElementNear(const std::vector<SketchElement>& elements,
                                           const SketchPoint& at, double reach) {
    const SketchElement* point = nullptr;
    double pointDistance = reach;
    for (const SketchElement& element : elements) {
        if (element.kind == SketchElementKind::point && wellFormed(element)) {
            const double d = distance(at, element.points[0]);
            if (d <= pointDistance) {
                pointDistance = d;
                point = &element;
            }
        }
    }
    if (point == nullptr) {
        return elementNear(elements, at, reach);
    }
    if (auto nearer = elementNear(elements, at, pointDistance)) {
        return nearer; // a line, circle or curve at least as near as the point
    }
    return point->id;
}

SketchCopyResult mirrorElements(const std::vector<SketchElement>& elements,
                                const std::vector<std::string>& ids, const std::string& axis) {
    SketchCopyResult result;
    const auto line = std::ranges::find_if(
        elements, [&](const SketchElement& element) { return element.id == axis; });
    if (line == elements.end() || line->kind != SketchElementKind::line || !wellFormed(*line)) {
        result.notice = SketchCopyNotice::noAxis;
        return result;
    }
    const SketchPoint a = line->points[0];
    const double length = distance(a, line->points[1]);
    if (length <= kSketchCopyTolerance) {
        result.notice = SketchCopyNotice::noAxis;
        return result;
    }
    const double du = (line->points[1].u - a.u) / length;
    const double dv = (line->points[1].v - a.v) / length;
    const Map reflect = [=](const SketchPoint& p) {
        const double along = (p.u - a.u) * du + (p.v - a.v) * dv;
        return SketchPoint{2.0 * (a.u + along * du) - p.u, 2.0 * (a.v + along * dv) - p.v};
    };
    const auto sources = sourcesOf(elements, ids, axis);
    if (sources.empty()) {
        result.notice = SketchCopyNotice::noSelection;
        return result;
    }
    for (const SketchElement* source : sources) {
        if (auto copy = transformed(*source, reflect, true)) {
            addIfNew(result, elements, std::move(*copy));
        }
    }
    return finished(std::move(result));
}

SketchCopyResult linearPattern(const std::vector<SketchElement>& elements,
                               const std::vector<std::string>& ids, const SketchPoint& step,
                               int count) {
    SketchCopyResult result;
    if (count > kSketchMaxPatternCount) {
        result.notice = SketchCopyNotice::tooMany;
        return result;
    }
    if (count < 2 || !std::isfinite(step.u) || !std::isfinite(step.v) ||
        std::hypot(step.u, step.v) <= kSketchCopyTolerance) {
        result.notice = SketchCopyNotice::nothing;
        return result;
    }
    const auto sources = sourcesOf(elements, ids, {});
    if (sources.empty()) {
        result.notice = SketchCopyNotice::noSelection;
        return result;
    }
    std::vector<Map> maps;
    for (int k = 1; k < count; ++k) {
        maps.push_back(
            [=](const SketchPoint& p) { return SketchPoint{p.u + k * step.u, p.v + k * step.v}; });
    }
    return copies(elements, sources, maps);
}

SketchCopyResult circularPattern(const std::vector<SketchElement>& elements,
                                 const std::vector<std::string>& ids, const SketchPoint& centre,
                                 int count, double angleDegrees) {
    SketchCopyResult result;
    if (count > kSketchMaxPatternCount) {
        result.notice = SketchCopyNotice::tooMany;
        return result;
    }
    if (count < 2 || !std::isfinite(angleDegrees) || std::abs(angleDegrees) <= 1e-9 ||
        std::abs(angleDegrees) > 360.0) {
        result.notice = SketchCopyNotice::nothing;
        return result;
    }
    const auto sources = sourcesOf(elements, ids, {});
    if (sources.empty()) {
        result.notice = SketchCopyNotice::noSelection;
        return result;
    }
    const bool whole = std::abs(angleDegrees) >= 360.0 - 1e-9;
    const double each = whole ? angleDegrees / count : angleDegrees / (count - 1);
    std::vector<Map> maps;
    for (int k = 1; k < count; ++k) {
        const double turn = each * k * std::numbers::pi / 180.0;
        const double c = std::cos(turn);
        const double s = std::sin(turn);
        maps.push_back([=](const SketchPoint& p) {
            const double x = p.u - centre.u;
            const double y = p.v - centre.v;
            return SketchPoint{centre.u + c * x - s * y, centre.v + s * x + c * y};
        });
    }
    return copies(elements, sources, maps);
}

} // namespace fission::interaction
