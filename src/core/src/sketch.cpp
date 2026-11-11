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

/// The number of a rule or measure id ("R12" -> 12); nullopt when it is not of that form.
std::optional<int> parseRuleNumber(std::string_view id) noexcept {
    if (id.size() < 2 || id.front() != 'R') {
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
    return static_cast<int>(number);
}

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

SketchPlane planeOnFace(const Point3d& centre, const Point3d& normal,
                        const std::optional<Point3d>& longestEdge) {
    const double normalLength = length(normal);
    if (!(normalLength > 0.0) || !std::isfinite(normalLength)) {
        return SketchPlane(); // there is no face to lie on
    }
    const Point3d n = scaled(normal, 1.0 / normalLength);
    // The direction, made perpendicular to the normal; nothing if it lies along it.
    const auto across = [&](const Point3d& direction) -> std::optional<Point3d> {
        const double along = dot(direction, n);
        const Point3d rest{direction.x - along * n.x, direction.y - along * n.y,
                           direction.z - along * n.z};
        const double restLength = length(rest);
        if (!(restLength > 1e-9) || !std::isfinite(restLength)) {
            return std::nullopt;
        }
        return scaled(rest, 1.0 / restLength);
    };
    std::optional<Point3d> u;
    if (longestEdge) {
        u = across(*longestEdge);
    }
    if (!u) {
        u = across({1.0, 0.0, 0.0});
    }
    if (!u) {
        u = across({0.0, 1.0, 0.0});
    }
    return SketchPlane(centre, *u, cross(n, *u));
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

// --- rules and measures --------------------------------------------------------------------

std::string_view sketchRuleKindWord(geo::SketchRuleKind kind) {
    switch (kind) {
    case geo::SketchRuleKind::horizontal:
        return "horizontal";
    case geo::SketchRuleKind::vertical:
        return "vertical";
    case geo::SketchRuleKind::parallel:
        return "parallel";
    case geo::SketchRuleKind::perpendicular:
        return "perpendicular";
    case geo::SketchRuleKind::tangent:
        return "tangent";
    case geo::SketchRuleKind::equal:
        return "equal";
    case geo::SketchRuleKind::symmetric:
        return "symmetric";
    case geo::SketchRuleKind::midpoint:
        return "midpoint";
    case geo::SketchRuleKind::fixed:
        return "fixed";
    case geo::SketchRuleKind::pointOnCurve:
        return "pointOnCurve";
    case geo::SketchRuleKind::concentric:
        return "concentric";
    case geo::SketchRuleKind::coincident:
        return "coincident";
    }
    return "horizontal";
}

std::optional<geo::SketchRuleKind> sketchRuleKindFromWord(std::string_view word) {
    constexpr std::array kinds{geo::SketchRuleKind::horizontal, geo::SketchRuleKind::vertical,
                               geo::SketchRuleKind::parallel,   geo::SketchRuleKind::perpendicular,
                               geo::SketchRuleKind::tangent,    geo::SketchRuleKind::equal,
                               geo::SketchRuleKind::symmetric,  geo::SketchRuleKind::midpoint,
                               geo::SketchRuleKind::fixed,      geo::SketchRuleKind::pointOnCurve,
                               geo::SketchRuleKind::concentric, geo::SketchRuleKind::coincident};
    for (const geo::SketchRuleKind kind : kinds) {
        if (word == sketchRuleKindWord(kind)) {
            return kind;
        }
    }
    return std::nullopt;
}

std::string_view sketchMeasureKindWord(geo::SketchMeasureKind kind) {
    switch (kind) {
    case geo::SketchMeasureKind::length:
        return "length";
    case geo::SketchMeasureKind::distance:
        return "distance";
    case geo::SketchMeasureKind::horizontalDistance:
        return "horizontalDistance";
    case geo::SketchMeasureKind::verticalDistance:
        return "verticalDistance";
    case geo::SketchMeasureKind::radius:
        return "radius";
    case geo::SketchMeasureKind::diameter:
        return "diameter";
    case geo::SketchMeasureKind::angle:
        return "angle";
    }
    return "length";
}

std::optional<geo::SketchMeasureKind> sketchMeasureKindFromWord(std::string_view word) {
    constexpr std::array kinds{geo::SketchMeasureKind::length,
                               geo::SketchMeasureKind::distance,
                               geo::SketchMeasureKind::horizontalDistance,
                               geo::SketchMeasureKind::verticalDistance,
                               geo::SketchMeasureKind::radius,
                               geo::SketchMeasureKind::diameter,
                               geo::SketchMeasureKind::angle};
    for (const geo::SketchMeasureKind kind : kinds) {
        if (word == sketchMeasureKindWord(kind)) {
            return kind;
        }
    }
    return std::nullopt;
}

bool sketchReferenceExists(const SketchData& data, std::string_view ref) noexcept {
    const std::size_t dot = ref.find('.');
    if (dot == std::string_view::npos) {
        return data.find(ref) != nullptr;
    }
    const std::string_view id = ref.substr(0, dot);
    const std::string_view indexText = ref.substr(dot + 1);
    if (indexText.empty()) {
        return false;
    }
    std::size_t index = 0;
    for (const char c : indexText) {
        if (c < '0' || c > '9' || index > 100000000) {
            return false;
        }
        index = index * 10 + static_cast<std::size_t>(c - '0');
    }
    const SketchElement* element = data.find(id);
    return element != nullptr && index < element->points.size();
}

// --- data ----------------------------------------------------------------------------------

std::string SketchData::allocateId(SketchElementKind kind) {
    int& last = lastNumber[static_cast<std::size_t>(kind)];
    ++last;
    return std::string(1, sketchIdLetter(kind)) + std::to_string(last);
}

std::string SketchData::allocateRuleId() {
    ++lastRuleNumber;
    return "R" + std::to_string(lastRuleNumber);
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
    // Rules and measures share the "R" ids. A rule's kind was free text before the solver read
    // it, so only its references are checked; a measure must also name a known kind.
    const auto referencesExist = [this](const std::vector<std::string>& refs) {
        return std::ranges::all_of(
            refs, [this](const std::string& ref) { return sketchReferenceExists(*this, ref); });
    };
    std::set<std::string> ruleIds;
    for (const SketchRule& rule : rules) {
        const auto number = parseRuleNumber(rule.id);
        if (!number || *number > lastRuleNumber || !ruleIds.insert(rule.id).second ||
            !referencesExist(rule.elements)) {
            return false;
        }
    }
    for (const SketchMeasure& measure : measures) {
        const auto number = parseRuleNumber(measure.id);
        if (!number || *number > lastRuleNumber || !ruleIds.insert(measure.id).second ||
            !sketchMeasureKindFromWord(measure.kind) || !referencesExist(measure.elements)) {
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
        if (element.link) {
            text +=
                "&" + std::to_string(element.link->input) + "." + std::to_string(element.link->ref);
        }
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
    for (const SketchMeasure& measure : measures) {
        text += "%" + measure.id + ":" + measure.kind;
        for (const std::string& id : measure.elements) {
            text += "," + id;
        }
        text += ";" + measure.value.expression + ";" + number(measure.lastValue);
    }
    return text;
}

} // namespace fission::core
