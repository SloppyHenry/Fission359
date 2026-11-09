// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/interaction/sketch_tool.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace fission::interaction {

namespace {

using core::SketchElement;
using core::SketchElementKind;
using core::SketchPoint;

constexpr double kDegrees = 180.0 / std::numbers::pi;
constexpr double kTwoPi = 2.0 * std::numbers::pi;

SketchElement made(SketchElementKind kind, std::vector<SketchPoint> points) {
    SketchElement element;
    element.kind = kind;
    element.points = std::move(points);
    return element;
}

SketchElement lineOf(SketchPoint from, SketchPoint to) {
    return made(SketchElementKind::line, {from, to});
}

SketchElement guideOf(SketchPoint from, SketchPoint to) {
    SketchElement element = lineOf(from, to);
    element.construction = true;
    return element;
}

double distance(const SketchPoint& a, const SketchPoint& b) {
    return std::hypot(b.u - a.u, b.v - a.v);
}

// -1 for a negative number, else 1 (a pointer exactly on the axis counts as on the positive side).
double side(double value) {
    return value < 0.0 ? -1.0 : 1.0;
}

// A rectangle from two opposite corners as four lines, counter-clockwise from the lower left.
SketchBatch rectangleLines(const SketchPoint& a, const SketchPoint& b) {
    const double u0 = std::min(a.u, b.u);
    const double u1 = std::max(a.u, b.u);
    const double v0 = std::min(a.v, b.v);
    const double v1 = std::max(a.v, b.v);
    return {lineOf({u0, v0}, {u1, v0}), lineOf({u1, v0}, {u1, v1}), lineOf({u1, v1}, {u0, v1}),
            lineOf({u0, v1}, {u0, v0})};
}

} // namespace

void SketchTool::select(SketchToolKind kind, int variant) {
    kind_ = kind;
    variant_ = variant == kSketchVariantSecond ? kSketchVariantSecond : kSketchVariantFirst;
    placed_.clear();
    clearTyped();
    lastDirection_.reset();
    armed_ = false;
    dragging_ = false;
    sweep_ = 0.0;
}

void SketchTool::clearTyped() noexcept {
    typed_[0].reset();
    typed_[1].reset();
    typed_[2].reset();
}

void SketchTool::setPolygonSides(int sides) noexcept {
    sides_ = std::clamp(sides, kSketchMinimumSides, kSketchMaximumSides);
}

int SketchTool::fieldCount() const {
    if (!busy()) {
        return 0;
    }
    switch (kind_) {
    case SketchToolKind::line:
        return dragging_ ? 1 : 2;
    case SketchToolKind::rectangle:
        return 2;
    case SketchToolKind::circle:
        return variant_ == kSketchVariantFirst ? 1 : 0;
    case SketchToolKind::arc:
        return placed_.size() == 2 ? 1 : 0;
    case SketchToolKind::slot:
        return placed_.size() == 1 ? 2 : placed_.size() == 2 ? 1 : 0;
    case SketchToolKind::polygon:
        return 3;
    case SketchToolKind::spline:
        return 2;
    case SketchToolKind::point:
    case SketchToolKind::trim:
    case SketchToolKind::extend:
    case SketchToolKind::offset:
    case SketchToolKind::fillet:
    case SketchToolKind::none:
        break;
    }
    return 0;
}

void SketchTool::moveTo(const SketchPoint& position, bool alt) {
    pointer_ = position;
    alt_ = alt;
    if (armed_ && !dragging_ && distance(position, pressAt_) > dragDistance_) {
        dragging_ = true; // an arc is pulled from the end of the line
        clearTyped();
    }
    if (kind_ == SketchToolKind::arc && variant_ == kSketchVariantSecond && placed_.size() == 2 &&
        distance(position, placed_[0]) > 1e-12) {
        // The pointer turns round the middle; the turn is followed bit by bit so that it can go
        // further than a half circle either way.
        const double angle = std::atan2(position.v - placed_[0].v, position.u - placed_[0].u);
        double delta = std::fmod(angle - lastAngle_, kTwoPi);
        if (delta > std::numbers::pi) {
            delta -= kTwoPi;
        } else if (delta <= -std::numbers::pi) {
            delta += kTwoPi;
        }
        const double limit = kTwoPi - 1e-6;
        sweep_ = std::clamp(sweep_ + delta, -limit, limit);
        lastAngle_ = angle;
    }
}

SketchToolNotice SketchTool::setTyped(int field, std::optional<double> value) {
    if (field < 0 || field > 2) {
        return SketchToolNotice::none;
    }
    if (value && !std::isfinite(*value)) {
        value.reset();
    }
    if (kind_ == SketchToolKind::polygon && field == 1 && value) {
        const double sides = *value;
        if (std::abs(sides - std::round(sides)) > 1e-9 || sides < kSketchMinimumSides ||
            sides > kSketchMaximumSides) {
            typed_[1].reset(); // the old number stays
            return SketchToolNotice::badSides;
        }
    }
    typed_[field] = value;
    return SketchToolNotice::none;
}

void SketchTool::markHelper(SketchBatch& batch, bool alt) const {
    const bool aid = helper_ || alt;
    for (SketchElement& element : batch) {
        element.construction = aid;
    }
}

// The end of a line pulled from `first` to the pointer, with its typed length and angle.
SketchPoint SketchTool::pulledEnd(const SketchPoint& first, const SketchPoint& pointer,
                                  Shape& out) const {
    const double du = pointer.u - first.u;
    const double dv = pointer.v - first.v;
    double length = std::hypot(du, dv);
    double angle = length > 1e-12 ? std::atan2(dv, du) : 0.0;
    if (typed_[1]) {
        angle = *typed_[1] / kDegrees;
    }
    if (typed_[0]) {
        length = *typed_[0];
    }
    SketchPoint end = pointer;
    if (typed_[0] || typed_[1]) {
        end = {first.u + length * std::cos(angle), first.v + length * std::sin(angle)};
    }
    out.fields = {{"length", std::abs(length), typed_[0].has_value()},
                  {"angle",
                   typed_[1] ? *typed_[1] : std::atan2(end.v - first.v, end.u - first.u) * kDegrees,
                   typed_[1].has_value()}};
    return end;
}

// The arc that joins the end of the last line (the chain is `placed_.front()`).
SketchTool::Shape SketchTool::tangentShape(const SketchPoint& pointer) const {
    Shape out;
    out.at = pointer;
    const SketchPoint& first = placed_.front();
    std::optional<double> radius;
    if (typed_[0]) {
        radius = *typed_[0];
    }
    out.tangent =
        lastDirection_ ? tangentArc(first, *lastDirection_, pointer, radius) : std::nullopt;
    out.fields = {{"radius", out.tangent ? out.tangent->radius : std::abs(radius.value_or(0.0)),
                   typed_[0].has_value()}};
    if (distance(first, pointer) < kSketchMinimumSize ||
        (radius && std::abs(*radius) < kSketchMinimumSize)) {
        out.tooSmall = true;
        out.tangent.reset();
    } else if (!out.tangent) {
        out.straight = true;
    } else if (distance(first, out.tangent->end) < kSketchMinimumSize) {
        out.tooSmall = true;
        out.tangent.reset();
    } else {
        out.at = out.tangent->end;
        out.elements = {out.tangent->arc};
    }
    return out;
}

SketchTool::Shape SketchTool::arcShape(const SketchPoint& pointer) const {
    Shape out;
    out.at = pointer;
    const SketchPoint& first = placed_.front();
    if (placed_.size() == 1) {
        out.guides = {guideOf(first, pointer)};
        return out;
    }
    if (variant_ == kSketchVariantFirst) {
        // start, end, a point on the arc (or the radius, and the side the pointer is on)
        const SketchPoint& end = placed_[1];
        std::optional<SketchElement> arc;
        if (typed_[0]) {
            const double cross = (end.u - first.u) * (pointer.v - first.v) -
                                 (end.v - first.v) * (pointer.u - first.u);
            arc = arcOverChord(first, end, cross < 0.0 ? -1 : 1, *typed_[0]);
            out.fields = {{"radius", std::abs(*typed_[0]), true}};
            out.tooSmall = !arc;
        } else {
            arc = arcThroughThreePoints(first, pointer, end);
            out.fields = {{"radius", arc ? distance(arc->points[0], arc->points[1]) : 0.0, false}};
            out.straight = !arc;
        }
        if (arc) {
            out.elements = {std::move(*arc)};
        } else {
            out.guides = {guideOf(first, end)};
        }
        return out;
    }
    // middle, start, and the angle the pointer turns to
    const SketchPoint& start = placed_[1];
    const double sweep = typed_[0] ? *typed_[0] / kDegrees : sweep_;
    const double radius = distance(first, start);
    const double from = std::atan2(start.v - first.v, start.u - first.u);
    out.at = {first.u + radius * std::cos(from + sweep), first.v + radius * std::sin(from + sweep)};
    out.fields = {{"angle", sweep * kDegrees, typed_[0].has_value()}};
    out.guides = {guideOf(first, start)};
    if (auto arc = arcBySweep(first, start, sweep)) {
        out.elements = {std::move(*arc)};
    } else {
        out.tooSmall = true;
    }
    return out;
}

SketchTool::Shape SketchTool::slotShape(const SketchPoint& pointer) const {
    Shape out;
    out.at = pointer;
    const SketchPoint& first = placed_.front();
    if (placed_.size() == 1) {
        const SketchPoint end = pulledEnd(first, pointer, out);
        out.at = end;
        out.guides = {guideOf(first, end)};
        out.tooSmall = distance(first, end) < kSketchMinimumSize;
        return out;
    }
    const SketchPoint& second = placed_[1];
    const double length = distance(first, second);
    const double across = length > 0.0 ? ((second.u - first.u) * (pointer.v - first.v) -
                                          (second.v - first.v) * (pointer.u - first.u)) /
                                             length
                                       : 0.0;
    const double width = typed_[0] ? std::abs(*typed_[0]) : 2.0 * std::abs(across);
    out.fields = {{"width", width, typed_[0].has_value()}};
    if (typed_[0] && length > 0.0) { // the number puts the pointer on the side it was on
        const double sign = side(across);
        out.at = {(first.u + second.u) / 2.0 - sign * (second.v - first.v) / length * width / 2.0,
                  (first.v + second.v) / 2.0 + sign * (second.u - first.u) / length * width / 2.0};
    }
    out.elements = slotElements(first, second, width);
    out.tooSmall = out.elements.empty();
    return out;
}

SketchTool::Shape SketchTool::polygonShape(const SketchPoint& pointer) const {
    Shape out;
    const SketchPoint& middle = placed_.front();
    const double du = pointer.u - middle.u;
    const double dv = pointer.v - middle.v;
    const double pulledRadius = std::hypot(du, dv);
    const double radius = typed_[0] ? std::abs(*typed_[0]) : pulledRadius;
    const double angle =
        typed_[2] ? *typed_[2] / kDegrees : (pulledRadius > 1e-12 ? std::atan2(dv, du) : 0.0);
    const int sides = typed_[1] ? static_cast<int>(std::lround(*typed_[1])) : sides_;
    const bool inside = variant_ == kSketchVariantSecond;
    // the corner (or the middle of a side) the numbers and the pointer name
    out.at = {middle.u + radius * std::cos(angle), middle.v + radius * std::sin(angle)};
    out.fields = {{"radius", radius, typed_[0].has_value()},
                  {"sides", static_cast<double>(sides), typed_[1].has_value()},
                  {"angle", angle * kDegrees, typed_[2].has_value()}};
    const double step = std::numbers::pi / static_cast<double>(sides);
    const double circumRadius = inside ? radius / std::cos(step) : radius;
    out.elements = polygonLines(middle, circumRadius, sides, inside ? angle - step : angle);
    out.tooSmall = out.elements.empty();
    return out;
}

// The curve through the points clicked so far and the pointer (or the length and angle typed for
// the next stretch). The pointer on the first point of a curve with three or more points shows
// the curve closed.
SketchTool::Shape SketchTool::splineShape(const SketchPoint& pointer) const {
    Shape out;
    const SketchPoint& last = placed_.back();
    const SketchPoint end = pulledEnd(last, pointer, out);
    out.at = end;
    std::vector<SketchPoint> through = placed_;
    if (!hasTyped() && placed_.size() >= 3 && distance(end, placed_.front()) < kSketchMinimumSize) {
        through.push_back(placed_.front());
        out.at = placed_.front();
    } else if (distance(last, end) >= kSketchMinimumSize) {
        through.push_back(end);
    }
    if (through.size() >= 2) {
        out.elements = {made(SketchElementKind::spline, std::move(through))};
    } else {
        out.tooSmall = true;
    }
    return out;
}

SketchTool::Shape SketchTool::shape(const SketchPoint& pointer, bool alt) const {
    Shape out;
    out.at = pointer;
    if (placed_.empty()) {
        return out;
    }
    const SketchPoint& first = placed_.front();
    switch (kind_) {
    case SketchToolKind::line: {
        if (dragging_) {
            out = tangentShape(pointer);
            break;
        }
        const SketchPoint end = pulledEnd(first, pointer, out);
        out.at = end;
        if (distance(first, end) < kSketchMinimumSize) {
            out.tooSmall = true;
        } else {
            out.elements = {lineOf(first, end)};
        }
        break;
    }
    case SketchToolKind::rectangle: {
        double du = pointer.u - first.u;
        double dv = pointer.v - first.v;
        const double sideU = side(du);
        const double sideV = side(dv);
        const bool middle = variant_ == kSketchVariantSecond;
        // typed sizes are whole sizes; the middle variant spreads them to both sides
        const double share = middle ? 0.5 : 1.0;
        if (typed_[0]) {
            du = sideU * *typed_[0] * share;
        }
        if (typed_[1]) {
            dv = sideV * *typed_[1] * share;
        }
        const SketchPoint corner{first.u + du, first.v + dv};
        const SketchPoint other = middle ? SketchPoint{first.u - du, first.v - dv} : first;
        out.at = corner;
        out.fields = {{"width", std::abs(du) / share, typed_[0].has_value()},
                      {"height", std::abs(dv) / share, typed_[1].has_value()}};
        if (std::abs(du) / share < kSketchMinimumSize ||
            std::abs(dv) / share < kSketchMinimumSize) {
            out.tooSmall = true;
        } else {
            out.elements = rectangleLines(other, corner);
        }
        break;
    }
    case SketchToolKind::circle:
        if (variant_ == kSketchVariantFirst) {
            const double du = pointer.u - first.u;
            const double dv = pointer.v - first.v;
            double radius = std::hypot(du, dv);
            SketchPoint rim = pointer;
            if (typed_[0]) {
                const double angle = radius > 1e-12 ? std::atan2(dv, du) : 0.0;
                radius = *typed_[0];
                rim = {first.u + radius * std::cos(angle), first.v + radius * std::sin(angle)};
                radius = std::abs(radius);
            }
            out.at = rim;
            out.fields = {{"radius", radius, typed_[0].has_value()}};
            if (radius < kSketchMinimumSize) {
                out.tooSmall = true;
            } else {
                out.elements = {made(SketchElementKind::circle, {first, rim})};
            }
        } else if (placed_.size() == 2) {
            if (auto circle = circleThroughThreePoints(placed_[0], placed_[1], pointer)) {
                out.fields = {{"radius", distance(circle->points[0], circle->points[1]), false}};
                out.elements = {std::move(*circle)};
            } else {
                out.straight = true;
            }
        }
        break;
    case SketchToolKind::arc:
        out = arcShape(pointer);
        break;
    case SketchToolKind::slot:
        out = slotShape(pointer);
        break;
    case SketchToolKind::polygon:
        out = polygonShape(pointer);
        break;
    case SketchToolKind::spline:
        out = splineShape(pointer);
        break;
    case SketchToolKind::point:
    case SketchToolKind::trim:
    case SketchToolKind::extend:
    case SketchToolKind::offset:
    case SketchToolKind::fillet:
    case SketchToolKind::none:
        break;
    }
    markHelper(out.elements, alt);
    return out;
}

SketchToolStep SketchTool::finish(const SketchPoint& pointer, bool alt) {
    SketchToolStep step;
    Shape result = shape(pointer, alt);
    if (dragging_) { // the arc from the end of the last line
        armed_ = false;
        dragging_ = false;
        clearTyped();
        if (result.elements.empty()) {
            step.notice =
                result.tooSmall ? SketchToolNotice::tooSmall : SketchToolNotice::collinear;
            return step; // the chain goes on from where it was
        }
        placed_ = {result.tangent->end};
        lastDirection_ = result.tangent->endDirection;
        step.done.push_back(std::move(result.elements));
        return step;
    }
    if (result.elements.empty() && result.straight) {
        step.notice = SketchToolNotice::collinear; // the points stay: another last one fits
        return step;
    }
    if (kind_ == SketchToolKind::polygon && !result.elements.empty()) {
        sides_ =
            static_cast<int>(std::lround(result.fields[1].value)); // kept until the sketch ends
    }
    clearTyped();
    if (result.elements.empty()) {
        step.notice = SketchToolNotice::tooSmall;
        return step; // the first click stays: the person pulls on
    }
    if (kind_ == SketchToolKind::line) {
        const SketchPoint from = result.elements.front().points[0];
        const SketchPoint to = result.elements.front().points[1];
        const double length = distance(from, to);
        lastDirection_ = SketchPoint{(to.u - from.u) / length, (to.v - from.v) / length};
        placed_ = {to}; // the chain goes on from the end
    } else {
        placed_.clear();
    }
    step.done.push_back(std::move(result.elements));
    return step;
}

// The second click of the slot: its length and direction are fixed, the width follows.
SketchToolStep SketchTool::advanceSlot(const SketchPoint& pointer, bool) {
    SketchToolStep step;
    const Shape result = slotShape(pointer);
    clearTyped();
    if (result.tooSmall) {
        step.notice = SketchToolNotice::tooSmall;
        return step;
    }
    placed_.push_back(result.at);
    return step;
}

// Sets the curve through `points` (the clicks of the curve, the first again at the end when it
// closes). Following points that are one are taken once; fewer than two different points make
// nothing (`tooSmall`), and so does a closed curve through only two. Either way the curve is over.
SketchToolStep SketchTool::finishSpline(std::vector<SketchPoint> points, bool alt) {
    SketchToolStep step;
    std::vector<SketchPoint> through;
    for (const SketchPoint& point : points) {
        if (through.empty() || distance(through.back(), point) >= kSketchMinimumSize) {
            through.push_back(point);
        }
    }
    const bool closed =
        through.size() >= 3 && distance(through.front(), through.back()) < kSketchMinimumSize;
    if (closed) {
        through.back() = through.front(); // exactly: the data says "closed" by equal ends
    }
    placed_.clear();
    clearTyped();
    lastDirection_.reset();
    if (through.size() < 2 || (closed && through.size() < 4)) {
        step.notice = SketchToolNotice::tooSmall;
        return step;
    }
    SketchBatch batch{made(SketchElementKind::spline, std::move(through))};
    markHelper(batch, alt);
    step.done.push_back(std::move(batch));
    return step;
}

SketchToolStep SketchTool::click(const SketchPoint& position, bool alt) {
    moveTo(position, alt);
    SketchToolStep step;
    switch (kind_) {
    case SketchToolKind::none:
    case SketchToolKind::trim: // these two edit what is drawn: the controller does that
    case SketchToolKind::extend:
    case SketchToolKind::offset:
    case SketchToolKind::fillet:
        break;
    case SketchToolKind::point: {
        SketchBatch batch{made(SketchElementKind::point, {position})};
        markHelper(batch, alt);
        step.done.push_back(std::move(batch));
        break;
    }
    case SketchToolKind::line:
    case SketchToolKind::rectangle:
        if (!busy()) {
            placed_ = {position};
            lastDirection_.reset();
        } else {
            step = finish(position, alt);
        }
        break;
    case SketchToolKind::circle:
        if (variant_ == kSketchVariantFirst) {
            if (!busy()) {
                placed_ = {position};
            } else {
                step = finish(position, alt);
            }
        } else if (placed_.size() < 2) {
            placed_.push_back(position);
        } else {
            step = finish(position, alt);
        }
        break;
    case SketchToolKind::arc:
        if (placed_.size() < 2) {
            if (placed_.size() == 1 && distance(placed_[0], position) < kSketchMinimumSize) {
                step.notice = SketchToolNotice::tooSmall;
                break;
            }
            placed_.push_back(position);
            if (placed_.size() == 2 && variant_ == kSketchVariantSecond) {
                sweep_ = 0.0;
                lastAngle_ = std::atan2(position.v - placed_[0].v, position.u - placed_[0].u);
            }
        } else {
            step = finish(position, alt);
        }
        break;
    case SketchToolKind::slot:
        if (!busy()) {
            placed_ = {position};
        } else if (placed_.size() == 1) {
            step = advanceSlot(position, alt);
        } else {
            step = finish(position, alt);
        }
        break;
    case SketchToolKind::polygon:
        if (!busy()) {
            placed_ = {position};
        } else {
            step = finish(position, alt);
        }
        break;
    case SketchToolKind::spline:
        if (!busy()) {
            placed_ = {position};
        } else if (distance(position, placed_.back()) < kSketchMinimumSize) {
            break; // the same point twice (the click of a double click): one is enough
        } else if (distance(position, placed_.front()) < kSketchMinimumSize) {
            if (placed_.size() >= 3) { // back on the first point: the curve closes
                std::vector<SketchPoint> through = placed_;
                through.push_back(placed_.front());
                step = finishSpline(std::move(through), alt);
            } else {
                step.notice = SketchToolNotice::tooSmall; // a closed curve needs three points
            }
        } else {
            placed_.push_back(position);
            clearTyped();
        }
        break;
    }
    return step;
}

SketchToolStep SketchTool::doubleClick() {
    if (kind_ == SketchToolKind::line) {
        cancel();
    } else if (kind_ == SketchToolKind::spline && busy()) {
        return finishSpline(placed_, alt_);
    }
    return {};
}

bool SketchTool::undoPoint() {
    if (kind_ != SketchToolKind::spline || !busy()) {
        return false;
    }
    placed_.pop_back();
    clearTyped();
    return true;
}

SketchToolStep SketchTool::accept(bool alt) {
    if (fieldCount() == 0) {
        return {};
    }
    if (kind_ == SketchToolKind::slot && placed_.size() == 1) {
        return advanceSlot(pointer_, alt);
    }
    if (kind_ == SketchToolKind::spline) {
        // Numbers typed: they make the next point (Enter again sets the curve); else Enter sets it.
        if (!hasTyped()) {
            return finishSpline(placed_, alt);
        }
        SketchToolStep step;
        const Shape next = splineShape(pointer_);
        clearTyped();
        if (distance(placed_.back(), next.at) < kSketchMinimumSize) {
            step.notice = SketchToolNotice::tooSmall;
        } else {
            placed_.push_back(next.at);
        }
        return step;
    }
    return finish(pointer_, alt);
}

bool SketchTool::cancel() {
    if (armed_ || dragging_) {
        const bool was = dragging_;
        armed_ = false;
        dragging_ = false;
        clearTyped();
        if (was) {
            return true; // only the arc is dropped, the chain stays
        }
    }
    const bool was = busy();
    placed_.clear();
    lastDirection_.reset();
    clearTyped();
    return was;
}

bool SketchTool::press(const SketchPoint& position, double reach, double dragDistance) {
    if (kind_ != SketchToolKind::line || !busy() || !lastDirection_ || dragging_ ||
        distance(position, placed_.front()) > reach) {
        return false;
    }
    armed_ = true;
    pressAt_ = position;
    dragDistance_ = dragDistance;
    return true;
}

SketchToolStep SketchTool::release(const SketchPoint& position, bool alt) {
    if (!armed_) {
        return {};
    }
    if (!dragging_) {
        armed_ = false; // a press and a release without pulling: the click makes what it makes
        return {};
    }
    moveTo(position, alt);
    return finish(position, alt);
}

SketchDraft SketchTool::draft() const {
    SketchDraft out;
    out.at = pointer_;
    if (!busy()) {
        return out;
    }
    Shape now = shape(pointer_, alt_);
    out.elements = std::move(now.elements);
    out.fields = std::move(now.fields);
    out.at = now.at;
    for (SketchElement& guide : now.guides) {
        out.elements.push_back(std::move(guide));
    }
    // what is placed and not yet part of a shape shows as points
    if (kind_ != SketchToolKind::line) {
        for (const SketchPoint& point : placed_) {
            SketchElement dot = made(SketchElementKind::point, {point});
            out.elements.push_back(std::move(dot));
        }
    }
    if (kind_ == SketchToolKind::circle && variant_ == kSketchVariantSecond &&
        out.elements.size() == placed_.size()) {
        out.elements.push_back(
            guideOf(placed_.front(), placed_.size() > 1 ? placed_[1] : pointer_));
    }
    return out;
}

} // namespace fission::interaction
