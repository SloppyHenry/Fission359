// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The sketch solver (AP-2.2a): the reference sketches of tests/fixtures/sketches (JSON), each
// solved twice. Every solved sketch is checked rule by rule with its own geometry (not with the
// solver's), the determined ones against coordinates worked out by hand.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <map>
#include <numbers>
#include <string>
#include <vector>

#include "fission/geo/sketch/planegcs_solver.h"
#include "fission/geo/sketch_solver.h"

#include <nlohmann/json.hpp>

namespace {

using fission::geo::HeldPoint;
using fission::geo::Point2d;
using fission::geo::SketchMeasureKind;
using fission::geo::SketchRuleKind;
using fission::geo::SketchSystem;
using fission::geo::SolveOptions;
using fission::geo::SolverCurve;
using fission::geo::SolverCurveKind;
using fission::geo::SolveResult;
using fission::geo::SolverMeasure;
using fission::geo::SolverPoint;
using fission::geo::SolverRule;
using fission::geo::SolveStatus;
using Json = nlohmann::json;

const std::map<std::string, SketchRuleKind> kRuleKinds = {
    {"horizontal", SketchRuleKind::horizontal}, {"vertical", SketchRuleKind::vertical},
    {"parallel", SketchRuleKind::parallel},     {"perpendicular", SketchRuleKind::perpendicular},
    {"tangent", SketchRuleKind::tangent},       {"equal", SketchRuleKind::equal},
    {"symmetric", SketchRuleKind::symmetric},   {"midpoint", SketchRuleKind::midpoint},
    {"fixed", SketchRuleKind::fixed},           {"pointOnCurve", SketchRuleKind::pointOnCurve},
    {"concentric", SketchRuleKind::concentric}, {"coincident", SketchRuleKind::coincident}};

const std::map<std::string, SketchMeasureKind> kMeasureKinds = {
    {"length", SketchMeasureKind::length},
    {"distance", SketchMeasureKind::distance},
    {"horizontalDistance", SketchMeasureKind::horizontalDistance},
    {"verticalDistance", SketchMeasureKind::verticalDistance},
    {"radius", SketchMeasureKind::radius},
    {"diameter", SketchMeasureKind::diameter},
    {"angle", SketchMeasureKind::angle}};

std::vector<std::string> strings(const Json& list) {
    return list.get<std::vector<std::string>>();
}

SketchSystem systemOf(const Json& json) {
    SketchSystem system;
    for (const Json& p : json.at("points")) {
        system.points.push_back({p.at("id"), {p.at("x"), p.at("y")}});
    }
    for (const Json& c : json.value("curves", Json::array())) {
        SolverCurve curve;
        curve.id = c.at("id");
        const std::string kind = c.at("kind");
        curve.kind = kind == "line"     ? SolverCurveKind::line
                     : kind == "circle" ? SolverCurveKind::circle
                                        : SolverCurveKind::arc;
        curve.points = strings(c.at("points"));
        system.curves.push_back(curve);
    }
    for (const Json& r : json.value("rules", Json::array())) {
        system.rules.push_back(
            {r.at("id"), kRuleKinds.at(r.at("kind")), strings(r.at("elements"))});
    }
    for (const Json& m : json.value("measures", Json::array())) {
        system.measures.push_back(
            {m.at("id"), kMeasureKinds.at(m.at("kind")), strings(m.at("elements")), m.at("value")});
    }
    for (const Json& h : json.value("held", Json::array())) {
        system.held.push_back({h.at("point"), {h.at("x"), h.at("y")}});
    }
    return system;
}

// --- an independent check of every rule and measure ----------------------------------------

constexpr double kCheckTolerance = 1e-7;

class Checker {
public:
    Checker(const SketchSystem& system, const SolveResult& result) : system_(system) {
        for (const SolverPoint& p : result.points) {
            at_[p.id] = p.position;
        }
    }

    /// Empty when everything holds, otherwise the first failing id.
    std::string firstViolation() const {
        for (const SolverRule& rule : system_.rules) {
            if (!ruleHolds(rule)) {
                return rule.id;
            }
        }
        for (const SolverMeasure& measure : system_.measures) {
            if (!measureHolds(measure)) {
                return measure.id;
            }
        }
        for (const SolverCurve& curve : system_.curves) {
            if (curve.kind == SolverCurveKind::arc) {
                const Point2d c = at_.at(curve.points[0]);
                const double r = dist(c, at_.at(curve.points[1]));
                if (std::abs(dist(c, at_.at(curve.points[2])) - r) > kCheckTolerance) {
                    return curve.id;
                }
            }
        }
        return {};
    }

private:
    static double dist(const Point2d& a, const Point2d& b) {
        return std::hypot(b.x - a.x, b.y - a.y);
    }
    static bool near(double a, double b) { return std::abs(a - b) <= kCheckTolerance; }

    const SolverCurve& curve(const std::string& id) const {
        for (const SolverCurve& c : system_.curves) {
            if (c.id == id) {
                return c;
            }
        }
        throw std::runtime_error("no curve " + id);
    }
    bool isCurve(const std::string& id) const {
        return std::ranges::any_of(system_.curves,
                                   [&](const SolverCurve& c) { return c.id == id; });
    }
    Point2d pt(const std::string& id) const { return at_.at(id); }
    Point2d from(const std::string& line) const { return pt(curve(line).points[0]); }
    Point2d to(const std::string& line) const { return pt(curve(line).points[1]); }
    double length(const std::string& line) const { return dist(from(line), to(line)); }
    double radius(const std::string& id) const {
        const SolverCurve& c = curve(id);
        return dist(pt(c.points[0]), pt(c.points[1]));
    }
    Point2d centre(const std::string& id) const { return pt(curve(id).points[0]); }
    bool isLine(const std::string& id) const {
        return isCurve(id) && curve(id).kind == SolverCurveKind::line;
    }
    /// Distance of a point from the infinite line.
    double lineDistance(const Point2d& p, const std::string& line) const {
        const Point2d a = from(line);
        const Point2d b = to(line);
        return std::abs((b.x - a.x) * (a.y - p.y) - (a.x - p.x) * (b.y - a.y)) / dist(a, b);
    }
    static double angleOf(const Point2d& a, const Point2d& b) {
        return std::atan2(b.y - a.y, b.x - a.x);
    }
    static double wrap(double a) { return std::remainder(a, 2.0 * std::numbers::pi); }

    bool ruleHolds(const SolverRule& rule) const {
        const std::vector<std::string>& e = rule.elements;
        switch (rule.kind) {
        case SketchRuleKind::horizontal:
            return e.size() == 1 ? near(from(e[0]).y, to(e[0]).y) : near(pt(e[0]).y, pt(e[1]).y);
        case SketchRuleKind::vertical:
            return e.size() == 1 ? near(from(e[0]).x, to(e[0]).x) : near(pt(e[0]).x, pt(e[1]).x);
        case SketchRuleKind::parallel:
        case SketchRuleKind::perpendicular: {
            const double a = wrap(angleOf(from(e[1]), to(e[1])) - angleOf(from(e[0]), to(e[0])));
            return near(rule.kind == SketchRuleKind::parallel ? std::sin(a) : std::cos(a), 0.0);
        }
        case SketchRuleKind::tangent: {
            const bool firstLine = isLine(e[0]);
            const bool secondLine = isLine(e[1]);
            if (firstLine || secondLine) {
                const std::string& line = firstLine ? e[0] : e[1];
                const std::string& round = firstLine ? e[1] : e[0];
                return near(lineDistance(centre(round), line), radius(round));
            }
            const double d = dist(centre(e[0]), centre(e[1]));
            return near(d, radius(e[0]) + radius(e[1])) ||
                   near(d, std::abs(radius(e[0]) - radius(e[1])));
        }
        case SketchRuleKind::equal:
            return isLine(e[0]) ? near(length(e[0]), length(e[1]))
                                : near(radius(e[0]), radius(e[1]));
        case SketchRuleKind::symmetric: {
            const Point2d a = pt(e[0]);
            const Point2d b = pt(e[1]);
            if (isLine(e[2])) {
                const Point2d mid{(a.x + b.x) / 2, (a.y + b.y) / 2};
                const Point2d l1 = from(e[2]);
                const Point2d l2 = to(e[2]);
                const double dot = (b.x - a.x) * (l2.x - l1.x) + (b.y - a.y) * (l2.y - l1.y);
                return near(lineDistance(mid, e[2]), 0.0) && near(dot, 0.0);
            }
            const Point2d c = pt(e[2]);
            return near(c.x, (a.x + b.x) / 2) && near(c.y, (a.y + b.y) / 2);
        }
        case SketchRuleKind::midpoint: {
            const Point2d p = pt(e[0]);
            return near(p.x, (from(e[1]).x + to(e[1]).x) / 2) &&
                   near(p.y, (from(e[1]).y + to(e[1]).y) / 2);
        }
        case SketchRuleKind::fixed:
            return true; // checked through the expected coordinates of the sketch
        case SketchRuleKind::pointOnCurve: {
            const Point2d p = pt(e[0]);
            if (isLine(e[1])) {
                return near(lineDistance(p, e[1]), 0.0);
            }
            return near(dist(p, centre(e[1])), radius(e[1]));
        }
        case SketchRuleKind::concentric:
            return near(centre(e[0]).x, centre(e[1]).x) && near(centre(e[0]).y, centre(e[1]).y);
        case SketchRuleKind::coincident:
            return near(pt(e[0]).x, pt(e[1]).x) && near(pt(e[0]).y, pt(e[1]).y);
        }
        return false;
    }

    bool measureHolds(const SolverMeasure& m) const {
        const std::vector<std::string>& e = m.elements;
        switch (m.kind) {
        case SketchMeasureKind::length:
            if (curve(e[0]).kind == SolverCurveKind::line) {
                return near(length(e[0]), m.value);
            } else {
                const SolverCurve& arc = curve(e[0]);
                const double r = radius(e[0]);
                const double sweep = wrap(angleOf(pt(arc.points[0]), pt(arc.points[2])) -
                                          angleOf(pt(arc.points[0]), pt(arc.points[1])));
                const double positive = sweep <= 0.0 ? sweep + 2.0 * std::numbers::pi : sweep;
                return near(r * positive, m.value);
            }
        case SketchMeasureKind::distance:
            if (!isCurve(e[0]) && !isCurve(e[1])) {
                return near(dist(pt(e[0]), pt(e[1])), m.value);
            }
            if (!isCurve(e[0])) {
                return near(lineDistance(pt(e[0]), e[1]), m.value);
            }
            return near(lineDistance(from(e[1]), e[0]), m.value);
        case SketchMeasureKind::horizontalDistance:
            return near(pt(e[1]).x - pt(e[0]).x, m.value);
        case SketchMeasureKind::verticalDistance:
            return near(pt(e[1]).y - pt(e[0]).y, m.value);
        case SketchMeasureKind::radius:
            return near(radius(e[0]), m.value);
        case SketchMeasureKind::diameter:
            return near(2.0 * radius(e[0]), m.value);
        case SketchMeasureKind::angle: {
            const double target = m.value * std::numbers::pi / 180.0;
            double actual = angleOf(from(e[0]), to(e[0]));
            if (e.size() == 2) {
                actual = angleOf(from(e[1]), to(e[1])) - actual;
            }
            return near(std::remainder(actual - target, 2.0 * std::numbers::pi), 0.0);
        }
        }
        return false;
    }

    const SketchSystem& system_;
    std::map<std::string, Point2d> at_;
};

std::vector<std::filesystem::path> fixtureFiles() {
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(
             std::filesystem::path(FISSION_TEST_FIXTURES_DIR) / "sketches")) {
        if (entry.path().extension() == ".json") {
            files.push_back(entry.path());
        }
    }
    std::ranges::sort(files);
    return files;
}

Json readJson(const std::filesystem::path& path) {
    std::ifstream in(path);
    REQUIRE(in.good());
    return Json::parse(in);
}

/// `expected` names exactly the ids, or (`...Within`) a non-empty selection of a group that cannot
/// be told apart (which of three dependent rules is "the" redundant one is the solver's choice).
void checkIds(const Json& expect, const std::string& key, const std::vector<std::string>& actual) {
    if (expect.contains(key)) {
        CHECK(actual == strings(expect.at(key)));
    }
    if (expect.contains(key + "Within")) {
        const std::vector<std::string> group = strings(expect.at(key + "Within"));
        CHECK_FALSE(actual.empty());
        for (const std::string& id : actual) {
            CHECK(std::ranges::find(group, id) != group.end());
        }
    }
}

} // namespace

TEST_CASE("Reference sketches solve the same way every time", "[sketch][solver]") {
    const auto solver = fission::geo::sketch::makePlaneGcsSolver();
    const std::vector<std::filesystem::path> files = fixtureFiles();
    REQUIRE(files.size() >= 50);

    for (const std::filesystem::path& file : files) {
        DYNAMIC_SECTION(file.filename().string()) {
            const Json json = readJson(file);
            const SketchSystem system = systemOf(json);
            const Json& expect = json.at("expect");
            const SolveOptions options;

            const SolveResult first = solver->solve(system, options);
            const SolveResult second = solver->solve(system, options);

            // Bit for bit the same.
            REQUIRE(first.points.size() == second.points.size());
            for (std::size_t i = 0; i < first.points.size(); ++i) {
                CHECK(first.points[i].position.x == second.points[i].position.x);
                CHECK(first.points[i].position.y == second.points[i].position.y);
            }
            CHECK(first.conflicting == second.conflicting);
            CHECK(first.redundant == second.redundant);
            CHECK(first.dof == second.dof);

            const std::string status = expect.at("status");
            const SolveStatus wanted = status == "ok"         ? SolveStatus::ok
                                       : status == "conflict" ? SolveStatus::conflict
                                                              : SolveStatus::failed;
            INFO(first.detail);
            REQUIRE(first.status == wanted);
            if (expect.contains("dof")) {
                CHECK(first.dof == expect.at("dof").get<int>());
            }
            checkIds(expect, "conflicting", first.conflicting);
            checkIds(expect, "redundant", first.redundant);
            if (!expect.contains("conflicting") && !expect.contains("conflictingWithin")) {
                CHECK(first.conflicting.empty());
            }
            if (!expect.contains("redundant") && !expect.contains("redundantWithin")) {
                CHECK(first.redundant.empty());
            }

            if (wanted != SolveStatus::ok) {
                CHECK(first.points == system.points); // nothing moves
                continue;
            }
            if (system.held.empty()) {
                const std::string violation = Checker(system, first).firstViolation();
                INFO("violation: " << violation);
                CHECK(violation.empty());
            }
            if (expect.contains("points")) {
                std::map<std::string, Point2d> got;
                for (const SolverPoint& p : first.points) {
                    got[p.id] = p.position;
                }
                for (const auto& [id, xy] : expect.at("points").items()) {
                    INFO("point " << id);
                    REQUIRE(got.contains(id));
                    CHECK(got[id].x == Catch::Approx(xy.at(0).get<double>()).margin(1e-9));
                    CHECK(got[id].y == Catch::Approx(xy.at(1).get<double>()).margin(1e-9));
                }
            }
        }
    }
}
