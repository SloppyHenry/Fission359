// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/core/sketch_solve.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>
#include <set>
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

SketchDragging::SketchDragging(const SketchData& sketch, const SketchMeasureValues& values,
                               const geo::SketchSolver& solver,
                               const std::vector<std::string>& held) {
    m_start = sketch;
    geo::SketchSystem system = toSolverSystem(sketch, values);
    for (const std::string& pointId : held) {
        // The element of the point: the part of "L1.0" before the last '.'.
        const std::string elementId = pointId.substr(0, pointId.rfind('.'));
        const SketchElement* element = sketch.find(elementId);
        if (element == nullptr || element->link) {
            continue; // unknown element, or follows a body edge: never dragged
        }
        const auto known =
            std::ranges::find_if(system.points, [&pointId](const geo::SolverPoint& point) {
                return point.id == pointId;
            });
        if (known == system.points.end()) {
            continue; // no solver point with that id
        }
        m_held.push_back(pointId);
        system.held.push_back({pointId, known->position}); // the target starts where it is
    }
    if (!m_held.empty()) {
        m_drag = solver.beginDrag(system);
    }
}

std::optional<SketchData> SketchDragging::moveTo(const std::vector<geo::Point2d>& targets) {
    if (m_drag == nullptr) {
        return std::nullopt;
    }
    const geo::SolveResult result = m_drag->moveTo(targets);
    if (result.status != geo::SolveStatus::ok) {
        return std::nullopt; // the caller keeps the last good state
    }
    SketchData out = m_start;
    applySolution(out, result);
    return out;
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

namespace {

/// An end point of an element that takes part in the automatic rules: its point id, its position
/// and the id of the element it belongs to.
struct EndPoint {
    std::string ref;
    SketchPoint point;
    std::string elementId;
};

/// The end points of an element: a point element has its point, a line its start and end, an arc
/// its start and end (not the centre), a spline its first and last point. Circles and elements
/// with a link take no part.
std::vector<EndPoint> endPointsOf(const SketchElement& element) {
    std::vector<EndPoint> endPoints;
    if (element.link || element.kind == SketchElementKind::circle) {
        return endPoints;
    }
    const auto add = [&](std::size_t index) {
        if (index < element.points.size()) {
            endPoints.push_back({pointId(element.id, index), element.points[index], element.id});
        }
    };
    switch (element.kind) {
    case SketchElementKind::point:
        add(0);
        break;
    case SketchElementKind::line:
        add(0);
        add(1);
        break;
    case SketchElementKind::arc:
        add(1);
        add(2);
        break;
    case SketchElementKind::spline:
        add(0);
        add(element.points.size() - 1); // first and last (the same point when there is only one)
        break;
    case SketchElementKind::circle:
        break;
    }
    return endPoints;
}

double distance(const SketchPoint& a, const SketchPoint& b) {
    return std::sqrt((a.u - b.u) * (a.u - b.u) + (a.v - b.v) * (a.v - b.v));
}

} // namespace

std::vector<AutoRule> autoSketchRules(const SketchData& sketch,
                                      const std::vector<std::string>& newIds) {
    constexpr double tol = 1e-9;

    // The end points of every element that takes part.
    std::vector<EndPoint> endPoints;
    for (const SketchElement& element : sketch.elements) {
        const std::vector<EndPoint> own = endPointsOf(element);
        endPoints.insert(endPoints.end(), own.begin(), own.end());
    }

    // A rule is the same rule when kind and elements (in any order) match, so the key marks the
    // rules the sketch already has and the ones emitted earlier as seen; for a coincident pair
    // between two new elements the sorted refs make the pair count once.
    const auto keyOf = [](const std::string& kind, std::vector<std::string> elements) {
        std::ranges::sort(elements);
        std::string key = kind;
        for (const std::string& element : elements) {
            key += '\n';
            key += element;
        }
        return key;
    };
    std::set<std::string> seen;
    for (const SketchRule& rule : sketch.rules) {
        seen.insert(keyOf(rule.kind, rule.elements));
    }

    std::vector<AutoRule> rules;
    const auto add = [&](const std::string& kind, std::vector<std::string> elements) {
        if (seen.insert(keyOf(kind, elements)).second) {
            rules.push_back(AutoRule{kind, std::move(elements)});
        }
    };

    for (const std::string& newId : newIds) {
        const SketchElement* element = sketch.find(newId);
        if (element == nullptr || element->link) {
            continue; // not in the sketch, or follows a body edge: takes no part
        }

        // (a) a level or upright line
        if (element->kind == SketchElementKind::line && element->points.size() == 2) {
            const double du = element->points[1].u - element->points[0].u;
            const double dv = element->points[1].v - element->points[0].v;
            if (std::sqrt(du * du + dv * dv) > tol) {
                if (std::abs(dv) <= tol) {
                    add("horizontal", {newId});
                } else if (std::abs(du) <= tol) {
                    add("vertical", {newId});
                }
            }
        }

        // (b) an end point of the new element on an end point of another element
        for (const EndPoint& ownEnd : endPointsOf(*element)) {
            for (const EndPoint& other : endPoints) {
                if (other.elementId == newId) {
                    continue; // the same element
                }
                if (distance(ownEnd.point, other.point) <= tol) {
                    add("coincident", {ownEnd.ref, other.ref});
                }
            }
        }

        // (c) an arc that starts or ends at an end of a line and runs on along it: the radius to
        // that end is perpendicular to the line
        if (element->kind == SketchElementKind::arc && element->points.size() == 3) {
            const SketchPoint centre = element->points[0];
            for (std::size_t i = 1; i <= 2; ++i) {
                const SketchPoint p = element->points[i];
                for (const SketchElement& other : sketch.elements) {
                    if (other.kind != SketchElementKind::line || other.link ||
                        other.points.size() != 2) {
                        continue;
                    }
                    if (distance(p, other.points[0]) > tol && distance(p, other.points[1]) > tol) {
                        continue; // no end of the line at p
                    }
                    const SketchPoint r{p.u - centre.u, p.v - centre.v};
                    const SketchPoint d{other.points[1].u - other.points[0].u,
                                        other.points[1].v - other.points[0].v};
                    const double dot = r.u * d.u + r.v * d.v;
                    const double lenR = std::sqrt(r.u * r.u + r.v * r.v);
                    const double lenD = std::sqrt(d.u * d.u + d.v * d.v);
                    if (std::abs(dot) <= tol * lenR * lenD) {
                        add("tangent", {newId, other.id}); // once per line
                    }
                }
            }
        }
        // the other way round: a new line an arc starts or ends on
        if (element->kind == SketchElementKind::line && element->points.size() == 2) {
            const SketchPoint d{element->points[1].u - element->points[0].u,
                                element->points[1].v - element->points[0].v};
            const double lenD = std::sqrt(d.u * d.u + d.v * d.v);
            for (std::size_t i = 0; i < 2; ++i) {
                const SketchPoint q = element->points[i];
                for (const SketchElement& arc : sketch.elements) {
                    if (arc.kind != SketchElementKind::arc || arc.link || arc.points.size() != 3) {
                        continue;
                    }
                    for (std::size_t j = 1; j <= 2; ++j) {
                        const SketchPoint p = arc.points[j];
                        if (distance(p, q) > tol) {
                            continue; // no end of the arc at q
                        }
                        const SketchPoint r{p.u - arc.points[0].u, p.v - arc.points[0].v};
                        const double dot = r.u * d.u + r.v * d.v;
                        const double lenR = std::sqrt(r.u * r.u + r.v * r.v);
                        if (std::abs(dot) <= tol * lenR * lenD) {
                            add("tangent", {arc.id, newId}); // once per arc
                        }
                    }
                }
            }
        }
    }

    return rules;
}

} // namespace fission::core
