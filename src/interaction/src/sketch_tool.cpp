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

SketchElement made(SketchElementKind kind, std::vector<SketchPoint> points) {
    SketchElement element;
    element.kind = kind;
    element.points = std::move(points);
    return element;
}

SketchElement lineOf(SketchPoint from, SketchPoint to) {
    return made(SketchElementKind::line, {from, to});
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

std::optional<SketchElement> circleThroughThreePoints(const SketchPoint& a, const SketchPoint& b,
                                                      const SketchPoint& c) {
    const double abu = b.u - a.u;
    const double abv = b.v - a.v;
    const double acu = c.u - a.u;
    const double acv = c.v - a.v;
    const double cross = abu * acv - abv * acu;
    const double reach = std::max(abu * abu + abv * abv, acu * acu + acv * acv);
    // (nearly) on one line: the middle would be far away or not exist
    if (!(reach > 0.0) || std::abs(cross) <= 1e-9 * reach) {
        return std::nullopt;
    }
    const double ab2 = abu * abu + abv * abv;
    const double ac2 = acu * acu + acv * acv;
    const double du = (acv * ab2 - abv * ac2) / (2.0 * cross);
    const double dv = (abu * ac2 - acu * ab2) / (2.0 * cross);
    const SketchPoint middle{a.u + du, a.v + dv};
    if (!std::isfinite(middle.u) || !std::isfinite(middle.v) || distance(middle, a) > 1e7) {
        return std::nullopt;
    }
    return made(SketchElementKind::circle, {middle, a});
}

void SketchTool::select(SketchToolKind kind, int variant) {
    kind_ = kind;
    variant_ = variant == kSketchVariantSecond ? kSketchVariantSecond : kSketchVariantFirst;
    placed_.clear();
    typed_[0].reset();
    typed_[1].reset();
}

int SketchTool::fieldCount() const {
    if (!busy()) {
        return 0;
    }
    switch (kind_) {
    case SketchToolKind::line:
    case SketchToolKind::rectangle:
        return 2;
    case SketchToolKind::circle:
        return variant_ == kSketchVariantFirst ? 1 : 0;
    case SketchToolKind::point:
    case SketchToolKind::none:
        break;
    }
    return 0;
}

void SketchTool::moveTo(const SketchPoint& position, bool alt) {
    pointer_ = position;
    alt_ = alt;
}

void SketchTool::setTyped(int field, std::optional<double> value) {
    if (field < 0 || field > 1) {
        return;
    }
    if (value && !std::isfinite(*value)) {
        value.reset();
    }
    typed_[field] = value;
}

void SketchTool::markHelper(SketchBatch& batch, bool alt) const {
    const bool aid = helper_ || alt;
    for (SketchElement& element : batch) {
        element.construction = aid;
    }
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
        out.at = end;
        out.fields = {
            {"length", std::abs(length), typed_[0].has_value()},
            {"angle",
             typed_[1] ? *typed_[1] : std::atan2(end.v - first.v, end.u - first.u) * kDegrees,
             typed_[1].has_value()}};
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
            }
        }
        break;
    case SketchToolKind::point:
    case SketchToolKind::none:
        break;
    }
    markHelper(out.elements, alt);
    return out;
}

SketchToolStep SketchTool::finish(const SketchPoint& pointer, bool alt) {
    SketchToolStep step;
    Shape result = shape(pointer, alt);
    const bool threePoints = kind_ == SketchToolKind::circle && variant_ == kSketchVariantSecond;
    if (threePoints && placed_.size() == 2 && result.elements.empty()) {
        step.notice = SketchToolNotice::collinear; // the two points stay: another third one fits
        return step;
    }
    typed_[0].reset();
    typed_[1].reset();
    if (result.elements.empty()) {
        step.notice = SketchToolNotice::tooSmall;
        return step; // the first click stays: the person pulls on
    }
    step.done.push_back(std::move(result.elements));
    if (kind_ == SketchToolKind::line) {
        placed_ = {step.done.back().front().points[1]}; // the chain goes on from the end
    } else {
        placed_.clear();
    }
    return step;
}

SketchToolStep SketchTool::click(const SketchPoint& position, bool alt) {
    moveTo(position, alt);
    SketchToolStep step;
    switch (kind_) {
    case SketchToolKind::none:
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
    }
    return step;
}

SketchToolStep SketchTool::doubleClick() {
    if (kind_ == SketchToolKind::line) {
        cancel();
    }
    return {};
}

SketchToolStep SketchTool::accept(bool alt) {
    if (fieldCount() == 0) {
        return {};
    }
    return finish(pointer_, alt);
}

bool SketchTool::cancel() {
    const bool was = busy();
    placed_.clear();
    typed_[0].reset();
    typed_[1].reset();
    return was;
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
    // what is placed and not yet part of a shape shows as points
    if (kind_ != SketchToolKind::line) {
        for (const SketchPoint& point : placed_) {
            SketchElement dot = made(SketchElementKind::point, {point});
            out.elements.push_back(std::move(dot));
        }
    }
    if (kind_ == SketchToolKind::circle && variant_ == kSketchVariantSecond &&
        out.elements.empty()) {
        out.elements.push_back(lineOf(placed_.front(), placed_.size() > 1 ? placed_[1] : pointer_));
        out.elements.back().construction = true;
    }
    return out;
}

} // namespace fission::interaction
