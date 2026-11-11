// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/core/sketch_solve.h"

#include <algorithm>
#include <charconv>
#include <limits>
#include <utility>

namespace fission::core {

namespace {

/// The solver's id of point `index` of an element ("L1", 0 -> "L1.0").
std::string pointId(const std::string& elementId, std::size_t index) {
    return elementId + "." + std::to_string(index);
}

/// Maps one reference of a rule or measure to the solver's id: a bare id of a point element
/// names its only point ("P1" -> "P1.0"); everything else stays as it is (curve id or "L1.0").
std::string solverRef(const SketchData& sketch, const std::string& ref) {
    if (const SketchElement* element = sketch.find(ref);
        element && element->kind == SketchElementKind::point) {
        return pointId(ref, 0);
    }
    return ref;
}

/// The number of a rule or measure id ("R10" -> 10); ids that are not "R<number>" sort last.
int ruleIdNumber(const std::string& id) {
    if (id.size() > 1 && id.front() == 'R') {
        try {
            return std::stoi(id.substr(1));
        } catch (...) {
            // not a number after the "R": sort last
        }
    }
    return std::numeric_limits<int>::max();
}

/// "L2" before "L10": element ids are a letter and a number, the number compares as one.
bool elementIdLess(const std::string& a, const std::string& b) {
    if (a.size() < 2 || b.size() < 2 || a.front() != b.front()) {
        return a < b;
    }
    int numberA = 0;
    int numberB = 0;
    std::from_chars(a.data() + 1, a.data() + a.size(), numberA);
    std::from_chars(b.data() + 1, b.data() + b.size(), numberB);
    if (numberA != numberB) {
        return numberA < numberB;
    }
    return a < b;
}

/// Sorts by the number in the id, so "R2" comes before "R10"; equal numbers by id, so the
/// order is total and the solver always sees the same system.
template <typename SolverItem> void sortByIdNumber(std::vector<SolverItem>& items) {
    std::ranges::stable_sort(items, [](const SolverItem& a, const SolverItem& b) {
        const int na = ruleIdNumber(a.id);
        const int nb = ruleIdNumber(b.id);
        if (na != nb) {
            return na < nb;
        }
        return a.id < b.id;
    });
}

} // namespace

geo::SketchSystem toSolverSystem(const SketchData& sketch, const SketchMeasureValues& values) {
    geo::SketchSystem system;

    // Every point of every element becomes a solver point. Lines, circles and arcs also become
    // curves made of those points. The point order of our elements matches the solver's exactly:
    // line {start, end}; circle {middle, a point on the circle} = the solver's {centre, point on
    // the circle}; arc {middle, start, end} = the solver's {centre, start, end} (both
    // counter-clockwise). So the indices map 1:1 (0->0, 1->1, 2->2) for all three. Splines and
    // stand-alone points only give their points.
    for (const SketchElement& element : sketch.elements) {
        for (std::size_t i = 0; i < element.points.size(); ++i) {
            system.points.push_back(
                {pointId(element.id, i), {element.points[i].u, element.points[i].v}});
        }
        switch (element.kind) {
        case SketchElementKind::line:
            system.curves.push_back({element.id,
                                     geo::SolverCurveKind::line,
                                     {pointId(element.id, 0), pointId(element.id, 1)}});
            break;
        case SketchElementKind::circle:
            system.curves.push_back({element.id,
                                     geo::SolverCurveKind::circle,
                                     {pointId(element.id, 0), pointId(element.id, 1)}});
            break;
        case SketchElementKind::arc:
            system.curves.push_back(
                {element.id,
                 geo::SolverCurveKind::arc,
                 {pointId(element.id, 0), pointId(element.id, 1), pointId(element.id, 2)}});
            break;
        case SketchElementKind::point:
        case SketchElementKind::spline:
            break; // only the points above
        }
    }

    // Rules with a known kind word, sorted by their number.
    for (const SketchRule& rule : sketch.rules) {
        const std::optional<geo::SketchRuleKind> kind = sketchRuleKindFromWord(rule.kind);
        if (!kind) {
            continue; // unknown kind word: left out
        }
        std::vector<std::string> elements;
        elements.reserve(rule.elements.size());
        for (const std::string& ref : rule.elements) {
            elements.push_back(solverRef(sketch, ref));
        }
        system.rules.push_back({rule.id, *kind, std::move(elements)});
    }
    sortByIdNumber(system.rules);

    // An element with a link follows a body edge: its points are part of the system, but an
    // implicit fixed rule keeps them where the edge puts them. The id "link:<element id>" cannot
    // clash with the "R<n>" ids of the sketch's own rules (the solver takes any id string).
    for (const SketchElement& element : sketch.elements) {
        if (element.link) {
            system.rules.push_back({"link:" + element.id,
                                    geo::SketchRuleKind::fixed,
                                    {solverRef(sketch, element.id)}});
        }
    }

    // Measures with a known kind word and a value, sorted by their number.
    for (const SketchMeasure& measure : sketch.measures) {
        const std::optional<geo::SketchMeasureKind> kind = sketchMeasureKindFromWord(measure.kind);
        const auto value = values.find(measure.id);
        if (!kind || value == values.end()) {
            continue; // unknown kind word or no value given: left out
        }
        std::vector<std::string> elements;
        elements.reserve(measure.elements.size());
        for (const std::string& ref : measure.elements) {
            elements.push_back(solverRef(sketch, ref));
        }
        system.measures.push_back({measure.id, *kind, std::move(elements), value->second});
    }
    sortByIdNumber(system.measures);

    return system;
}

bool applySolution(SketchData& sketch, const geo::SolveResult& result) {
    if (result.status != geo::SolveStatus::ok) {
        return false;
    }
    // Match the solved points by id, not by position.
    std::map<std::string, geo::Point2d> solved;
    for (const geo::SolverPoint& point : result.points) {
        solved.emplace(point.id, point.position);
    }
    bool changed = false;
    for (SketchElement& element : sketch.elements) {
        if (element.link) {
            continue; // follows a body edge: its points stay
        }
        for (std::size_t i = 0; i < element.points.size(); ++i) {
            const auto it = solved.find(pointId(element.id, i));
            if (it == solved.end()) {
                continue;
            }
            const SketchPoint solvedPoint{it->second.x, it->second.y};
            if (!(solvedPoint == element.points[i])) {
                element.points[i] = solvedPoint;
                changed = true;
            }
        }
    }
    return changed;
}

SketchMeasureValues storedMeasureValues(const SketchData& sketch) {
    SketchMeasureValues values;
    for (const SketchMeasure& measure : sketch.measures) {
        values[measure.id] = measure.lastValue;
    }
    return values;
}

geo::SolveResult solveSketch(SketchData& sketch, const SketchMeasureValues& values,
                             const geo::SketchSolver& solver) {
    geo::SolveResult result = solver.solve(toSolverSystem(sketch, values), geo::SolveOptions{});
    applySolution(sketch, result); // writes back only when the status is ok
    return result;
}

std::vector<std::string> movableElements(const SketchData& sketch, const geo::SolveResult& result) {
    // The solver reports the movable points ("L1.0") and radii ("C1"); both are empty unless the
    // status is ok. The point order of the elements matches the solver's (see toSolverSystem):
    // line {start, end}, circle {centre, a point on the circle}, arc {centre, start, end}.
    const auto pointMoves = [&](const std::string& id) {
        return std::ranges::find(result.movablePoints, id) != result.movablePoints.end();
    };
    const auto radiusMoves = [&](const std::string& id) {
        return std::ranges::find(result.movableRadii, id) != result.movableRadii.end();
    };
    std::vector<std::string> movable;
    for (const SketchElement& element : sketch.elements) {
        if (element.link) {
            continue; // follows a body edge: fixed
        }
        bool canMove = false;
        switch (element.kind) {
        case SketchElementKind::point:
            canMove = pointMoves(pointId(element.id, 0));
            break;
        case SketchElementKind::line:
            canMove = pointMoves(pointId(element.id, 0)) || pointMoves(pointId(element.id, 1));
            break;
        case SketchElementKind::circle:
            // The point on the circle (index 1) only turns round the centre: it does not count.
            canMove = pointMoves(pointId(element.id, 0)) || radiusMoves(element.id);
            break;
        case SketchElementKind::arc:
            canMove = pointMoves(pointId(element.id, 0)) || pointMoves(pointId(element.id, 1)) ||
                      pointMoves(pointId(element.id, 2)) || radiusMoves(element.id);
            break;
        case SketchElementKind::spline:
            for (std::size_t i = 0; i < element.points.size(); ++i) {
                if (pointMoves(pointId(element.id, i))) {
                    canMove = true;
                    break;
                }
            }
            break;
        }
        if (canMove) {
            movable.push_back(element.id);
        }
    }
    std::ranges::sort(movable, elementIdLess);
    return movable;
}

} // namespace fission::core
