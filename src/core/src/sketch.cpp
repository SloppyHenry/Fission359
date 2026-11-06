// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/core/sketch.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <set>

namespace fission::core {

namespace {

using geo::Point3d;

double dot(const Point3d& a, const Point3d& b) noexcept {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Point3d cross(const Point3d& a, const Point3d& b) noexcept {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

double length(const Point3d& a) noexcept {
    return std::sqrt(dot(a, a));
}

Point3d scaled(const Point3d& a, double factor) noexcept {
    return {a.x * factor, a.y * factor, a.z * factor};
}

constexpr std::array<char, sketchElementKindCount> idLetters{'P', 'L', 'C', 'A', 'S'};

} // namespace

// --- plane ---------------------------------------------------------------------------------

SketchPlane::SketchPlane(const Point3d& origin, const Point3d& uAxis, const Point3d& vAxis)
    : origin_(origin) {
    const double uLength = length(uAxis);
    if (!(uLength > 0.0) || !std::isfinite(uLength)) {
        return; // the ground plane's axes
    }
    const Point3d u = scaled(uAxis, 1.0 / uLength);
    // The part of v that is perpendicular to u.
    const double along = dot(vAxis, u);
    const Point3d rest{vAxis.x - along * u.x, vAxis.y - along * u.y, vAxis.z - along * u.z};
    const double restLength = length(rest);
    if (!(restLength > 1e-12) || !std::isfinite(restLength)) {
        return;
    }
    u_ = u;
    v_ = scaled(rest, 1.0 / restLength);
    n_ = cross(u_, v_);
}

Point3d SketchPlane::toWorld(const SketchPoint& point) const noexcept {
    return {origin_.x + point.u * u_.x + point.v * v_.x,
            origin_.y + point.u * u_.y + point.v * v_.y,
            origin_.z + point.u * u_.z + point.v * v_.z};
}

Point3d SketchPlane::directionToWorld(const SketchPoint& direction) const noexcept {
    return {direction.u * u_.x + direction.v * v_.x, direction.u * u_.y + direction.v * v_.y,
            direction.u * u_.z + direction.v * v_.z};
}

SketchPoint SketchPlane::toPlane(const Point3d& world) const noexcept {
    const Point3d d{world.x - origin_.x, world.y - origin_.y, world.z - origin_.z};
    return {dot(d, u_), dot(d, v_)};
}

double SketchPlane::heightOf(const Point3d& world) const noexcept {
    const Point3d d{world.x - origin_.x, world.y - origin_.y, world.z - origin_.z};
    return dot(d, n_);
}

std::string_view basePlaneWord(BasePlane plane) {
    switch (plane) {
    case BasePlane::xy:
        return "xy";
    case BasePlane::xz:
        return "xz";
    case BasePlane::yz:
        return "yz";
    }
    return "xy";
}

std::optional<BasePlane> basePlaneFromWord(std::string_view word) {
    for (const BasePlane plane : {BasePlane::xy, BasePlane::xz, BasePlane::yz}) {
        if (word == basePlaneWord(plane)) {
            return plane;
        }
    }
    return std::nullopt;
}

SketchPlane placedBasePlane(BasePlane plane, const Point3d& origin, const geo::Angles& turn) {
    Point3d u{1.0, 0.0, 0.0};
    Point3d v{0.0, 1.0, 0.0};
    switch (plane) {
    case BasePlane::xy:
        break;
    case BasePlane::xz:
        v = {0.0, 0.0, 1.0};
        break;
    case BasePlane::yz:
        u = {0.0, 1.0, 0.0};
        v = {0.0, 0.0, 1.0};
        break;
    }
    return SketchPlane(origin, geo::rotatedDirection(u, turn), geo::rotatedDirection(v, turn));
}

// --- elements ------------------------------------------------------------------------------

char sketchIdLetter(SketchElementKind kind) noexcept {
    return idLetters[static_cast<std::size_t>(kind)];
}

std::size_t sketchMinPoints(SketchElementKind kind) noexcept {
    switch (kind) {
    case SketchElementKind::point:
        return 1;
    case SketchElementKind::line:
    case SketchElementKind::circle:
    case SketchElementKind::spline:
        return 2;
    case SketchElementKind::arc:
        return 3;
    }
    return 1;
}

bool sketchTakesMorePoints(SketchElementKind kind) noexcept {
    return kind == SketchElementKind::spline;
}

bool sketchPointsUsable(SketchElementKind kind, const std::vector<SketchPoint>& points) noexcept {
    const std::size_t least = sketchMinPoints(kind);
    if (points.size() < least || (points.size() > least && !sketchTakesMorePoints(kind))) {
        return false;
    }
    return std::ranges::all_of(
        points, [](const SketchPoint& p) { return std::isfinite(p.u) && std::isfinite(p.v); });
}

std::optional<ParsedSketchId> parseSketchId(std::string_view id) noexcept {
    if (id.size() < 2) {
        return std::nullopt;
    }
    const auto letter = std::ranges::find(idLetters, id.front());
    if (letter == idLetters.end()) {
        return std::nullopt;
    }
    long long number = 0;
    for (const char c : id.substr(1)) {
        if (c < '0' || c > '9' || number > 100000000) {
            return std::nullopt;
        }
        number = number * 10 + (c - '0');
    }
    if (number < 1 || id[1] == '0') {
        return std::nullopt;
    }
    return ParsedSketchId{static_cast<SketchElementKind>(letter - idLetters.begin()),
                          static_cast<int>(number)};
}

// --- data ----------------------------------------------------------------------------------

std::string SketchData::allocateId(SketchElementKind kind) {
    int& last = lastNumber[static_cast<std::size_t>(kind)];
    ++last;
    return std::string(1, sketchIdLetter(kind)) + std::to_string(last);
}

std::optional<std::size_t> SketchData::indexOf(std::string_view id) const noexcept {
    for (std::size_t i = 0; i < elements.size(); ++i) {
        if (elements[i].id == id) {
            return i;
        }
    }
    return std::nullopt;
}

const SketchElement* SketchData::find(std::string_view id) const noexcept {
    const auto index = indexOf(id);
    return index ? &elements[*index] : nullptr;
}

SketchElement* SketchData::find(std::string_view id) noexcept {
    const auto index = indexOf(id);
    return index ? &elements[*index] : nullptr;
}

bool SketchData::consistent() const {
    std::set<std::string> seen;
    for (const SketchElement& element : elements) {
        const auto parsed = parseSketchId(element.id);
        if (!parsed || parsed->kind != element.kind ||
            parsed->number > lastNumber[static_cast<std::size_t>(element.kind)] ||
            !seen.insert(element.id).second || !sketchPointsUsable(element.kind, element.points)) {
            return false;
        }
    }
    if (std::ranges::any_of(lastNumber, [](int n) { return n < 0; }) || lastRuleNumber < 0) {
        return false;
    }
    std::set<std::string> ruleIds;
    for (const SketchRule& rule : rules) {
        if (rule.id.empty() || !ruleIds.insert(rule.id).second) {
            return false;
        }
    }
    return true;
}

std::string SketchData::fingerprint() const {
    const auto number = [](double value) {
        return std::to_string(std::bit_cast<std::uint64_t>(value));
    };
    std::string text;
    for (const SketchElement& element : elements) {
        text += element.id;
        text += element.construction ? "~" : "=";
        for (const SketchPoint& point : element.points) {
            text += number(point.u);
            text += ',';
            text += number(point.v);
            text += ';';
        }
        text += '|';
    }
    for (const SketchRule& rule : rules) {
        text += "#" + rule.id + ":" + rule.kind;
        for (const std::string& id : rule.elements) {
            text += "," + id;
        }
    }
    return text;
}

} // namespace fission::core
