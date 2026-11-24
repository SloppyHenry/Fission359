// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The sketch solver on PlaneGCS (AP-2.2a). Every call builds a fresh `GCS::System` from the
// `SketchSystem` (a live drag builds it once and keeps it, AP-2.2c4): all numbers PlaneGCS points
// to live in a `std::deque` that is never resized while the system exists, rules and measures are
// added in id order, angles go in as radians.

#include "fission/geo/sketch/planegcs_solver.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <deque>
#include <exception>
#include <initializer_list>
#include <map>
#include <memory>
#include <numbers>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <planegcs/GCS.h>

namespace fission::geo::sketch {
namespace {

/// Bad input: the system cannot be built.
class InvalidSystem : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

bool isDigit(char c) {
    return c >= '0' && c <= '9';
}

/// "R2" before "R10": digit runs compare as numbers.
bool naturalLess(const std::string& a, const std::string& b) {
    std::size_t i = 0;
    std::size_t j = 0;
    while (i < a.size() && j < b.size()) {
        if (isDigit(a[i]) && isDigit(b[j])) {
            std::size_t endA = i;
            std::size_t endB = j;
            while (endA < a.size() && isDigit(a[endA])) {
                ++endA;
            }
            while (endB < b.size() && isDigit(b[endB])) {
                ++endB;
            }
            std::string_view numA(a.data() + i, endA - i);
            std::string_view numB(b.data() + j, endB - j);
            while (numA.size() > 1 && numA.front() == '0') {
                numA.remove_prefix(1);
            }
            while (numB.size() > 1 && numB.front() == '0') {
                numB.remove_prefix(1);
            }
            if (numA.size() != numB.size()) {
                return numA.size() < numB.size();
            }
            if (numA != numB) {
                return numA < numB;
            }
            i = endA;
            j = endB;
        } else {
            if (a[i] != b[j]) {
                return a[i] < b[j];
            }
            ++i;
            ++j;
        }
    }
    if (a.size() - i != b.size() - j) {
        return a.size() - i < b.size() - j;
    }
    return a < b;
}

bool finite(const Point2d& p) {
    return std::isfinite(p.x) && std::isfinite(p.y);
}

constexpr double kMinRadius = 1e-9;
constexpr double kTwoPi = 2.0 * std::numbers::pi;
constexpr double kDegreesToRadians = std::numbers::pi / 180.0;

/// What a rule or measure names.
struct Arg {
    GCS::Point* point = nullptr;
    GCS::Line* line = nullptr;
    GCS::Circle* circle = nullptr; ///< a circle, or the circle of an arc
    GCS::Arc* arc = nullptr;       ///< set for an arc
    /// The points that lie on the curve by construction (ends of a line, the point on a circle,
    /// start and end of an arc), not the centre.
    std::vector<GCS::Point*> onCurve;

    GCS::Curve* curve() const {
        if (line != nullptr) {
            return line;
        }
        return circle;
    }
};

/// What an element of a rule has to be.
enum class Shape { point, line, circle, arc, round /* circle or arc */ };

bool fits(const Arg& arg, Shape shape) {
    switch (shape) {
    case Shape::point:
        return arg.point != nullptr;
    case Shape::line:
        return arg.line != nullptr;
    case Shape::circle:
        return arg.circle != nullptr && arg.arc == nullptr;
    case Shape::arc:
        return arg.arc != nullptr;
    case Shape::round:
        return arg.circle != nullptr;
    }
    return false;
}

bool fits(const std::vector<Arg>& args, std::initializer_list<Shape> shapes) {
    if (args.size() != shapes.size()) {
        return false;
    }
    std::size_t at = 0;
    for (const Shape shape : shapes) {
        if (!fits(args[at++], shape)) {
            return false;
        }
    }
    return true;
}

/// Everything PlaneGCS needs, kept alive for the whole solve.
class Model {
public:
    explicit Model(const SketchSystem& system);

    /// Runs the analysis and the solve. Fills `result`.
    void run(const SolveOptions& options, SolveResult& result);
    /// Analyses the system once when a drag starts (`initSolution` stores the reference
    /// configuration every later `solve` starts from). Returns false when it fails.
    bool startDrag();
    /// One move of a drag: new targets for the held points, then `solve` again without a new
    /// analysis. Fills `result` (`dof` stays -1).
    void dragTo(const std::vector<Point2d>& targets, SolveResult& result);

private:
    struct Item {
        const std::string* id = nullptr;
        const SolverRule* rule = nullptr;
        const SolverMeasure* measure = nullptr;
    };

    double* constant(double value) {
        numbers_.push_back(value);
        return &numbers_.back();
    }
    double* unknown(double value) {
        double* number = constant(value);
        unknowns_.push_back(number);
        return number;
    }

    void addPoints();
    void addCurves();
    void addItems();
    void addHeld();
    std::vector<Arg> resolve(const std::string& itemId, const std::vector<std::string>& ids) const;
    void applyRule(const SolverRule& rule, int tag);
    void applyMeasure(const SolverMeasure& measure, int tag);
    void tangent(Arg& first, Arg& second, int tag);
    void fixPoint(GCS::Point& point, int tag);
    /// The tail after `gcs_.solve`: applies the solution, copies the coordinates into `positions_`
    /// and `result.points`. `convergedCounts` says whether `GCS::Converged` counts as reached.
    /// Returns true when the status of `result` is ok.
    bool finishSolve(int status, bool convergedCounts, SolveResult& result);
    [[noreturn]] static void invalid(const std::string& itemId, const std::string& why);

    const SketchSystem& system_;
    std::deque<double> numbers_; ///< Stable addresses: PlaneGCS keeps pointers into it.
    GCS::VEC_pD unknowns_;

    std::map<std::string, std::size_t> pointIndex_; ///< id -> index in `system_.points`
    std::vector<Point2d> positions_;                ///< current positions, input order
    std::vector<GCS::Point> gcsPoints_;             ///< input order
    std::map<std::string, Arg> elements_;           ///< every id that a rule may name
    std::deque<GCS::Line> lines_;
    std::deque<GCS::Circle> circles_;
    std::deque<GCS::Arc> arcs_;
    /// The radius unknown of every circle and arc, by curve id (for `movableRadii`).
    std::vector<std::pair<std::string, double*>> radii_;
    /// The two target numbers of every held point, in the order of `system_.held` (for `dragTo`).
    std::vector<std::pair<double*, double*>> heldTargets_;
    bool dragReady_ = false;  ///< `startDrag` ran
    std::vector<Item> items_; ///< sorted by id: the tag of an item is its index + 1
    GCS::System gcs_;
};

void Model::invalid(const std::string& itemId, const std::string& why) {
    throw InvalidSystem(itemId + ": " + why);
}

Model::Model(const SketchSystem& system) : system_(system) {
    addPoints();
    addCurves();
    addItems();
    addHeld();
}

void Model::addPoints() {
    positions_.reserve(system_.points.size());
    for (std::size_t i = 0; i < system_.points.size(); ++i) {
        const SolverPoint& point = system_.points[i];
        if (point.id.empty() || !pointIndex_.emplace(point.id, i).second) {
            invalid(point.id, "point id is empty or used twice");
        }
        if (!finite(point.position)) {
            invalid(point.id, "point has a coordinate that is not a number");
        }
        positions_.push_back(point.position);
    }
    // Unknowns are declared in id order, so the order of the input does not change the answer.
    std::vector<std::size_t> order(system_.points.size());
    for (std::size_t i = 0; i < order.size(); ++i) {
        order[i] = i;
    }
    std::ranges::sort(order, [&](std::size_t a, std::size_t b) {
        return naturalLess(system_.points[a].id, system_.points[b].id);
    });
    gcsPoints_.resize(system_.points.size());
    for (const std::size_t i : order) {
        gcsPoints_[i].x = unknown(positions_[i].x);
        gcsPoints_[i].y = unknown(positions_[i].y);
    }
    for (const auto& [id, i] : pointIndex_) {
        Arg arg;
        arg.point = &gcsPoints_[i];
        elements_.emplace(id, arg);
    }
}

void Model::addCurves() {
    std::vector<const SolverCurve*> curves;
    curves.reserve(system_.curves.size());
    for (const SolverCurve& curve : system_.curves) {
        curves.push_back(&curve);
    }
    std::ranges::sort(curves, [](const SolverCurve* a, const SolverCurve* b) {
        return naturalLess(a->id, b->id);
    });

    for (const SolverCurve* curve : curves) {
        const std::size_t need = curve->kind == SolverCurveKind::line     ? 2
                                 : curve->kind == SolverCurveKind::circle ? 2
                                                                          : 3;
        if (curve->points.size() != need) {
            invalid(curve->id, "wrong number of points for the curve");
        }
        std::vector<std::size_t> at;
        for (const std::string& pointId : curve->points) {
            const auto found = pointIndex_.find(pointId);
            if (found == pointIndex_.end()) {
                invalid(curve->id, "unknown point " + pointId);
            }
            at.push_back(found->second);
        }
        Arg arg;
        switch (curve->kind) {
        case SolverCurveKind::line: {
            GCS::Line& line = lines_.emplace_back();
            line.p1 = gcsPoints_[at[0]];
            line.p2 = gcsPoints_[at[1]];
            arg.line = &line;
            arg.onCurve = {&gcsPoints_[at[0]], &gcsPoints_[at[1]]};
            break;
        }
        case SolverCurveKind::circle: {
            const Point2d centre = positions_[at[0]];
            const Point2d rim = positions_[at[1]];
            const double radius = std::hypot(rim.x - centre.x, rim.y - centre.y);
            if (!(radius > kMinRadius)) {
                invalid(curve->id, "circle without a radius");
            }
            GCS::Circle& circle = circles_.emplace_back();
            circle.center = gcsPoints_[at[0]];
            circle.rad = unknown(radius);
            radii_.push_back({curve->id, circle.rad});
            // The point on the circle stays on it: this is what makes the radius "the distance".
            gcs_.addConstraintPointOnCircle(gcsPoints_[at[1]], circle, 0);
            arg.circle = &circle;
            arg.onCurve = {&gcsPoints_[at[1]]};
            break;
        }
        case SolverCurveKind::arc: {
            const Point2d centre = positions_[at[0]];
            const Point2d start = positions_[at[1]];
            const double radius = std::hypot(start.x - centre.x, start.y - centre.y);
            Point2d& end = positions_[at[2]];
            const double endDistance = std::hypot(end.x - centre.x, end.y - centre.y);
            if (!(radius > kMinRadius) || !(endDistance > kMinRadius)) {
                invalid(curve->id, "arc without a radius");
            }
            // The end only gives the direction: put it on the circle.
            end.x = centre.x + (end.x - centre.x) * radius / endDistance;
            end.y = centre.y + (end.y - centre.y) * radius / endDistance;
            const double startAngle = std::atan2(start.y - centre.y, start.x - centre.x);
            double endAngle = std::atan2(end.y - centre.y, end.x - centre.x);
            while (endAngle <= startAngle) {
                endAngle += kTwoPi;
            }
            GCS::Arc& arc = arcs_.emplace_back();
            arc.center = gcsPoints_[at[0]];
            arc.start = gcsPoints_[at[1]];
            arc.end = gcsPoints_[at[2]];
            arc.rad = unknown(radius);
            arc.startAngle = unknown(startAngle);
            arc.endAngle = unknown(endAngle);
            radii_.push_back({curve->id, arc.rad});
            gcs_.addConstraintArcRules(arc, 0);
            arg.arc = &arc;
            arg.circle = &arc;
            arg.onCurve = {&gcsPoints_[at[1]], &gcsPoints_[at[2]]};
            break;
        }
        }
        if (!elements_.emplace(curve->id, arg).second) {
            invalid(curve->id, "id is used twice");
        }
    }
}

void Model::addItems() {
    for (const SolverRule& rule : system_.rules) {
        items_.push_back({&rule.id, &rule, nullptr});
    }
    for (const SolverMeasure& measure : system_.measures) {
        items_.push_back({&measure.id, nullptr, &measure});
    }
    std::ranges::stable_sort(
        items_, [](const Item& a, const Item& b) { return naturalLess(*a.id, *b.id); });
    std::set<std::string> seen;
    for (const Item& item : items_) {
        if (item.id->empty() || !seen.insert(*item.id).second) {
            invalid(*item.id, "rule or measure id is empty or used twice");
        }
    }
    // Tags start at 1: PlaneGCS treats tag 0 as an internal constraint that is never reported.
    int tag = 1;
    for (const Item& item : items_) {
        if (item.rule != nullptr) {
            applyRule(*item.rule, tag);
        } else {
            applyMeasure(*item.measure, tag);
        }
        ++tag;
    }
}

void Model::addHeld() {
    for (const HeldPoint& held : system_.held) {
        const auto found = pointIndex_.find(held.point);
        if (found == pointIndex_.end()) {
            invalid(held.point, "held point is unknown");
        }
        if (!finite(held.target)) {
            invalid(held.point, "held point has a target that is not a number");
        }
        GCS::Point& point = gcsPoints_[found->second];
        // Tag -1: a drag the rules may overrule (PlaneGCS treats it as the lower priority).
        double* xNumber = constant(held.target.x);
        double* yNumber = constant(held.target.y);
        gcs_.addConstraintCoordinateX(point, xNumber, GCS::DefaultTemporaryConstraint);
        gcs_.addConstraintCoordinateY(point, yNumber, GCS::DefaultTemporaryConstraint);
        heldTargets_.push_back({xNumber, yNumber});
    }
}

std::vector<Arg> Model::resolve(const std::string& itemId,
                                const std::vector<std::string>& ids) const {
    std::vector<Arg> args;
    args.reserve(ids.size());
    for (const std::string& id : ids) {
        const auto found = elements_.find(id);
        if (found == elements_.end()) {
            invalid(itemId, "unknown element " + id);
        }
        args.push_back(found->second);
    }
    return args;
}

void Model::fixPoint(GCS::Point& point, int tag) {
    gcs_.addConstraintCoordinateX(point, constant(*point.x), tag);
    gcs_.addConstraintCoordinateY(point, constant(*point.y), tag);
}

namespace {

/// The direction PlaneGCS calls the normal of a curve at one of its points: to the left of a line,
/// towards the middle of a circle or arc.
std::pair<double, double> normalAt(const Arg& arg, const GCS::Point& at) {
    double x = 0.0;
    double y = 0.0;
    if (arg.line != nullptr) {
        x = -(*arg.line->p2.y - *arg.line->p1.y);
        y = *arg.line->p2.x - *arg.line->p1.x;
    } else {
        x = *arg.circle->center.x - *at.x;
        y = *arg.circle->center.y - *at.y;
    }
    const double length = std::hypot(x, y);
    return {x / length, y / length};
}

} // namespace

void Model::tangent(Arg& first, Arg& second, int tag) {
    // Curves that meet in a point of both (a joint): "the normals are parallel there". The three
    // plain distance equations of two circles touching at a shared point would be dependent
    // (a degenerate triangle), which the analysis would report as redundancy.
    for (GCS::Point* joint : first.onCurve) {
        if (std::ranges::find(second.onCurve, joint) == second.onCurve.end()) {
            continue;
        }
        const auto [x1, y1] = normalAt(first, *joint);
        const auto [x2, y2] = normalAt(second, *joint);
        const double angle = (x1 * x2 + y1 * y2 >= 0.0) ? 0.0 : std::numbers::pi;
        gcs_.addConstraintAngleViaPoint(*first.curve(), *second.curve(), *joint, constant(angle),
                                        tag);
        return;
    }
    if (first.line != nullptr) {
        if (second.arc != nullptr) {
            gcs_.addConstraintTangent(*first.line, *second.arc, tag);
        } else {
            gcs_.addConstraintTangent(*first.line, *second.circle, tag);
        }
        return;
    }
    if (first.arc != nullptr && second.arc != nullptr) {
        gcs_.addConstraintTangent(*first.arc, *second.arc, tag);
    } else if (first.arc != nullptr) {
        gcs_.addConstraintTangent(*second.circle, *first.arc, tag);
    } else if (second.arc != nullptr) {
        gcs_.addConstraintTangent(*first.circle, *second.arc, tag);
    } else {
        gcs_.addConstraintTangent(*first.circle, *second.circle, tag);
    }
}

void Model::applyRule(const SolverRule& rule, int tag) {
    std::vector<Arg> args = resolve(rule.id, rule.elements);
    const auto wrong = [&] { invalid(rule.id, "the rule does not take these elements"); };
    switch (rule.kind) {
    case SketchRuleKind::horizontal:
        if (fits(args, {Shape::line})) {
            gcs_.addConstraintHorizontal(*args[0].line, tag);
        } else if (fits(args, {Shape::point, Shape::point})) {
            gcs_.addConstraintHorizontal(*args[0].point, *args[1].point, tag);
        } else {
            wrong();
        }
        break;
    case SketchRuleKind::vertical:
        if (fits(args, {Shape::line})) {
            gcs_.addConstraintVertical(*args[0].line, tag);
        } else if (fits(args, {Shape::point, Shape::point})) {
            gcs_.addConstraintVertical(*args[0].point, *args[1].point, tag);
        } else {
            wrong();
        }
        break;
    case SketchRuleKind::parallel:
        if (!fits(args, {Shape::line, Shape::line})) {
            wrong();
        }
        gcs_.addConstraintParallel(*args[0].line, *args[1].line, tag);
        break;
    case SketchRuleKind::perpendicular:
        if (!fits(args, {Shape::line, Shape::line})) {
            wrong();
        }
        gcs_.addConstraintPerpendicular(*args[0].line, *args[1].line, tag);
        break;
    case SketchRuleKind::tangent:
        if (fits(args, {Shape::line, Shape::round}) || fits(args, {Shape::round, Shape::round})) {
            tangent(args[0], args[1], tag);
        } else if (fits(args, {Shape::round, Shape::line})) {
            tangent(args[1], args[0], tag);
        } else {
            wrong();
        }
        break;
    case SketchRuleKind::equal:
        if (fits(args, {Shape::line, Shape::line})) {
            gcs_.addConstraintEqualLength(*args[0].line, *args[1].line, tag);
        } else if (fits(args, {Shape::round, Shape::round})) {
            if (args[0].arc != nullptr && args[1].arc != nullptr) {
                gcs_.addConstraintEqualRadius(*args[0].arc, *args[1].arc, tag);
            } else if (args[0].arc != nullptr) {
                gcs_.addConstraintEqualRadius(*args[1].circle, *args[0].arc, tag);
            } else if (args[1].arc != nullptr) {
                gcs_.addConstraintEqualRadius(*args[0].circle, *args[1].arc, tag);
            } else {
                gcs_.addConstraintEqualRadius(*args[0].circle, *args[1].circle, tag);
            }
        } else {
            wrong();
        }
        break;
    case SketchRuleKind::symmetric:
        if (fits(args, {Shape::point, Shape::point, Shape::line})) {
            gcs_.addConstraintP2PSymmetric(*args[0].point, *args[1].point, *args[2].line, tag);
        } else if (fits(args, {Shape::point, Shape::point, Shape::point})) {
            gcs_.addConstraintP2PSymmetric(*args[0].point, *args[1].point, *args[2].point, tag);
        } else {
            wrong();
        }
        break;
    case SketchRuleKind::midpoint:
        if (!fits(args, {Shape::point, Shape::line})) {
            wrong();
        }
        gcs_.addConstraintP2PSymmetric(args[1].line->p1, args[1].line->p2, *args[0].point, tag);
        break;
    case SketchRuleKind::fixed:
        if (args.empty()) {
            wrong();
        }
        for (Arg& arg : args) {
            if (arg.point != nullptr) {
                fixPoint(*arg.point, tag);
            } else if (arg.line != nullptr) {
                fixPoint(arg.line->p1, tag);
                fixPoint(arg.line->p2, tag);
            } else if (arg.arc != nullptr) {
                fixPoint(arg.arc->center, tag);
                fixPoint(arg.arc->start, tag);
                fixPoint(arg.arc->end, tag);
            } else {
                // A circle: the centre and the radius (the point on the circle may still turn).
                fixPoint(arg.circle->center, tag);
                gcs_.addConstraintEqual(arg.circle->rad, constant(*arg.circle->rad), tag);
            }
        }
        break;
    case SketchRuleKind::pointOnCurve:
        if (fits(args, {Shape::point, Shape::line})) {
            gcs_.addConstraintPointOnLine(*args[0].point, *args[1].line, tag);
        } else if (fits(args, {Shape::point, Shape::circle})) {
            gcs_.addConstraintPointOnCircle(*args[0].point, *args[1].circle, tag);
        } else if (fits(args, {Shape::point, Shape::arc})) {
            gcs_.addConstraintPointOnArc(*args[0].point, *args[1].arc, tag);
        } else {
            wrong();
        }
        break;
    case SketchRuleKind::concentric:
        if (!fits(args, {Shape::round, Shape::round})) {
            wrong();
        }
        gcs_.addConstraintP2PCoincident(args[0].circle->center, args[1].circle->center, tag);
        break;
    case SketchRuleKind::coincident:
        if (!fits(args, {Shape::point, Shape::point})) {
            wrong();
        }
        gcs_.addConstraintP2PCoincident(*args[0].point, *args[1].point, tag);
        break;
    }
}

void Model::applyMeasure(const SolverMeasure& measure, int tag) {
    std::vector<Arg> args = resolve(measure.id, measure.elements);
    const auto wrong = [&] { invalid(measure.id, "the measure does not take these elements"); };
    if (!std::isfinite(measure.value)) {
        invalid(measure.id, "the value is not a number");
    }
    const bool positive = measure.value > 0.0;
    switch (measure.kind) {
    case SketchMeasureKind::length:
        if (fits(args, {Shape::line})) {
            if (!positive) {
                invalid(measure.id, "a length must be larger than zero");
            }
            gcs_.addConstraintP2PDistance(args[0].line->p1, args[0].line->p2,
                                          constant(measure.value), tag);
        } else if (fits(args, {Shape::arc})) {
            if (!positive) {
                invalid(measure.id, "a length must be larger than zero");
            }
            gcs_.addConstraintArcLength(*args[0].arc, constant(measure.value), tag);
        } else {
            wrong();
        }
        break;
    case SketchMeasureKind::distance:
        if (!positive) {
            invalid(measure.id, "a distance must be larger than zero");
        }
        if (fits(args, {Shape::point, Shape::point})) {
            gcs_.addConstraintP2PDistance(*args[0].point, *args[1].point, constant(measure.value),
                                          tag);
        } else if (fits(args, {Shape::point, Shape::line})) {
            gcs_.addConstraintP2LDistance(*args[0].point, *args[1].line, constant(measure.value),
                                          tag);
        } else if (fits(args, {Shape::line, Shape::line})) {
            // Two parallel lines: the first point of the second to the first.
            gcs_.addConstraintP2LDistance(args[1].line->p1, *args[0].line, constant(measure.value),
                                          tag);
        } else {
            wrong();
        }
        break;
    case SketchMeasureKind::horizontalDistance:
        if (!fits(args, {Shape::point, Shape::point})) {
            wrong();
        }
        gcs_.addConstraintDifference(args[0].point->x, args[1].point->x, constant(measure.value),
                                     tag);
        break;
    case SketchMeasureKind::verticalDistance:
        if (!fits(args, {Shape::point, Shape::point})) {
            wrong();
        }
        gcs_.addConstraintDifference(args[0].point->y, args[1].point->y, constant(measure.value),
                                     tag);
        break;
    case SketchMeasureKind::radius:
    case SketchMeasureKind::diameter:
        if (!fits(args, {Shape::round})) {
            wrong();
        }
        if (!positive) {
            invalid(measure.id, "a radius or diameter must be larger than zero");
        }
        if (measure.kind == SketchMeasureKind::radius) {
            gcs_.addConstraintEqual(args[0].circle->rad, constant(measure.value), tag);
        } else {
            gcs_.addConstraintProportional(args[0].circle->rad, constant(measure.value), 0.5, tag);
        }
        break;
    case SketchMeasureKind::angle: {
        // Radians, brought into (-pi, pi]: PlaneGCS reads an angle through atan2.
        double radians = std::remainder(measure.value * kDegreesToRadians, kTwoPi);
        if (radians <= -std::numbers::pi) {
            radians += kTwoPi;
        }
        if (fits(args, {Shape::line})) {
            gcs_.addConstraintP2PAngle(args[0].line->p1, args[0].line->p2, constant(radians), tag);
        } else if (fits(args, {Shape::line, Shape::line})) {
            gcs_.addConstraintL2LAngle(*args[0].line, *args[1].line, constant(radians), tag);
        } else {
            wrong();
        }
        break;
    }
    }
}

bool allFinite(const std::vector<Point2d>& points) {
    return std::ranges::all_of(points, [](const Point2d& p) { return finite(p); });
}

void Model::run(const SolveOptions& options, SolveResult& result) {
    const bool dragging = !system_.held.empty();
    gcs_.declareUnknowns(unknowns_);
    gcs_.initSolution(GCS::DogLeg);

    const auto idsOf = [&](const GCS::VEC_I& tags, std::vector<std::string>& out) {
        for (const int tag : tags) {
            if (tag >= 1 && static_cast<std::size_t>(tag) <= items_.size()) {
                out.push_back(*items_[static_cast<std::size_t>(tag) - 1].id);
            }
        }
        std::ranges::sort(out, naturalLess);
        out.erase(std::unique(out.begin(), out.end()), out.end());
    };

    // Diagnosis at the current values of the unknowns. A linear dependency shows only near the
    // solution, so this runs again after the solve (for both outcomes).
    const auto diagnose = [&]() {
        result.conflicting.clear();
        result.redundant.clear();
        result.movablePoints.clear();
        result.movableRadii.clear();
        result.dof = gcs_.diagnose(GCS::DogLeg);
        GCS::VEC_I conflicting;
        GCS::VEC_I redundant;
        GCS::VEC_I partlyRedundant;
        gcs_.getConflicting(conflicting);
        gcs_.getRedundant(redundant);
        gcs_.getPartiallyRedundant(partlyRedundant);
        redundant.insert(redundant.end(), partlyRedundant.begin(), partlyRedundant.end());
        idsOf(conflicting, result.conflicting);
        idsOf(redundant, result.redundant);
        // The parameters the rules and measures do not fix: a point can still move when one of
        // its coordinates is among them, a circle or arc when its radius is.
        GCS::VEC_pD dependent;
        gcs_.getDependentParams(dependent);
        // Without any rule PlaneGCS analyses nothing and names no parameter, although all of
        // them are free: when the sketch can still move but none is named, every one counts.
        const bool allFree = dependent.empty() && result.dof > 0;
        const auto notFixed = [&](const double* param) {
            return allFree || std::ranges::find(dependent, param) != dependent.end();
        };
        for (std::size_t i = 0; i < gcsPoints_.size(); ++i) {
            if (notFixed(gcsPoints_[i].x) || notFixed(gcsPoints_[i].y)) {
                result.movablePoints.push_back(system_.points[i].id);
            }
        }
        for (const auto& [id, rad] : radii_) {
            if (notFixed(rad)) {
                result.movableRadii.push_back(id);
            }
        }
        std::ranges::sort(result.movablePoints, naturalLess);
        std::ranges::sort(result.movableRadii, naturalLess);
    };

    if (options.diagnose) {
        diagnose();
        if (!result.conflicting.empty()) {
            result.status = SolveStatus::conflict;
            return;
        }
    }

    // DogLeg first; when it finds nothing, Levenberg-Marquardt from the same start values
    // (`solve` always starts again from the values given to `initSolution`).
    const auto reached = [&](int status) {
        // Converged without Success: redundant rules leave a rank-deficient system. The
        // diagnosis below tells conflict from redundancy; the caller's checks follow it.
        return status == GCS::Success ||
               ((dragging || options.diagnose) && status == GCS::Converged);
    };
    int status = gcs_.solve(true, GCS::DogLeg);
    if (!reached(status) && options.fallback) {
        status = gcs_.solve(true, GCS::LevenbergMarquardt);
    }
    const bool solved = finishSolve(status, dragging || options.diagnose, result);
    if (options.diagnose) {
        gcs_.initSolution(GCS::DogLeg);
        diagnose();
    }
    if (!solved) {
        if (!result.conflicting.empty()) {
            result.status = SolveStatus::conflict;
            result.detail.clear();
        }
        return;
    }
    if (!result.conflicting.empty()) {
        result.status = SolveStatus::conflict;
    }
}

bool Model::startDrag() {
    // Analyse once. `initSolution` stores the reference configuration (the points the drag starts
    // with): every later `solve` resets to it and solves from there.
    gcs_.declareUnknowns(unknowns_);
    gcs_.initSolution(GCS::DogLeg);
    dragReady_ = true;
    return true;
}

void Model::dragTo(const std::vector<Point2d>& targets, SolveResult& result) {
    result.dof = -1; // a drag is not analysed: conflicts are not reported, only a failure
    if (!dragReady_) {
        result.status = SolveStatus::failed;
        result.detail = "the drag was not started";
        return;
    }
    if (targets.size() != heldTargets_.size()) {
        result.status = SolveStatus::failed;
        result.detail = "the drag got " + std::to_string(targets.size()) + " targets for " +
                        std::to_string(heldTargets_.size()) + " held points";
        return;
    }
    for (const Point2d& target : targets) {
        if (!finite(target)) {
            result.status = SolveStatus::failed;
            result.detail = "a drag target is not a number";
            return;
        }
    }
    for (std::size_t i = 0; i < targets.size(); ++i) {
        *heldTargets_[i].first = targets[i].x;
        *heldTargets_[i].second = targets[i].y;
    }
    // No initSolution: solve starts from the reference configuration of the drag start, with the
    // new targets in the held constraints.
    const int status = gcs_.solve(true, GCS::DogLeg);
    finishSolve(status, true, result); // dragging counts Converged as reached, like in `run`
}

bool Model::finishSolve(int status, bool convergedCounts, SolveResult& result) {
    const bool reached = status == GCS::Success || (convergedCounts && status == GCS::Converged);
    gcs_.applySolution();
    for (std::size_t i = 0; i < positions_.size(); ++i) {
        positions_[i] = {*gcsPoints_[i].x, *gcsPoints_[i].y};
    }
    if (!reached) {
        result.status = SolveStatus::failed;
        result.detail = "the solver found no solution (status " + std::to_string(status) + ")";
        return false;
    }
    if (!allFinite(positions_)) {
        result.status = SolveStatus::failed;
        result.detail = "the solution has a coordinate that is not a number";
        return false;
    }
    result.status = SolveStatus::ok;
    for (std::size_t i = 0; i < result.points.size(); ++i) {
        result.points[i].position = positions_[i];
    }
    return true;
}

/// A drag that is going on (AP-2.2c4): the system is built and analysed once when the drag starts;
/// every move only changes the targets of the held points and solves again. Owns its system, so
/// the `Model` (which keeps pointers into its deques) always refers to a stable system.
class PlaneGcsDrag final : public SketchDrag {
public:
    explicit PlaneGcsDrag(const SketchSystem& system)
        : system_(system), model_(std::make_unique<Model>(system_)) {}

    /// Analyses the system once. False when it fails.
    bool start() { return model_->startDrag(); }

    SolveResult moveTo(const std::vector<Point2d>& targets) override {
        SolveResult result;
        result.points = system_.points;
        try {
            model_->dragTo(targets, result);
        } catch (const InvalidSystem& error) {
            result.status = SolveStatus::failed;
            result.detail = error.what();
        } catch (const std::exception& error) {
            result.status = SolveStatus::failed;
            result.detail = std::string("the solver stopped: ") + error.what();
        } catch (...) {
            result.status = SolveStatus::failed;
            result.detail = "the solver stopped";
        }
        if (result.status != SolveStatus::ok) {
            result.points = system_.points; // nothing moves unless the solve worked
        }
        return result;
    }

private:
    SketchSystem system_; ///< owned copy; declared before the model so it outlives it
    std::unique_ptr<Model> model_;
};

class PlaneGcsSolver final : public SketchSolver {
public:
    SolveResult solve(const SketchSystem& system, const SolveOptions& options) const override {
        SolveResult result;
        result.points = system.points;
        try {
            Model model(system);
            model.run(options, result);
        } catch (const InvalidSystem& error) {
            result.status = SolveStatus::failed;
            result.detail = error.what();
        } catch (const std::exception& error) {
            result.status = SolveStatus::failed;
            result.detail = std::string("the solver stopped: ") + error.what();
        } catch (...) {
            result.status = SolveStatus::failed;
            result.detail = "the solver stopped";
        }
        if (result.status != SolveStatus::ok) {
            result.points = system.points; // nothing moves unless the solve worked
            result.movablePoints.clear();
            result.movableRadii.clear();
        }
        return result;
    }

    std::unique_ptr<SketchDrag> beginDrag(const SketchSystem& system) const override {
        if (system.held.empty()) {
            return nullptr;
        }
        try {
            auto drag = std::make_unique<PlaneGcsDrag>(system);
            if (!drag->start()) {
                return nullptr;
            }
            return drag;
        } catch (...) {
            return nullptr;
        }
    }
};

} // namespace

std::unique_ptr<SketchSolver> makePlaneGcsSolver() {
    return std::make_unique<PlaneGcsSolver>();
}

} // namespace fission::geo::sketch
