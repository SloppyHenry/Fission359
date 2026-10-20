// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/interaction/snap.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <optional>

#include "fission/interaction/drag.h"

namespace fission::interaction {

namespace {

// --- small vector helpers (the points are plain arrays of double) ---
Vec3 sub(const Vec3& a, const Vec3& b) {
    return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}
Vec3 add(const Vec3& a, const Vec3& b) {
    return {a[0] + b[0], a[1] + b[1], a[2] + b[2]};
}
Vec3 scaled(const Vec3& a, double f) {
    return {a[0] * f, a[1] * f, a[2] * f};
}
Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
double dot(const Vec3& a, const Vec3& b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
double length(const Vec3& a) {
    return std::sqrt(dot(a, a));
}

Vec3 toVec(const geo::Point3f& p) {
    return {static_cast<double>(p.x), static_cast<double>(p.y), static_cast<double>(p.z)};
}

// The display mesh holds single precision numbers: 12.3 comes as 12.30000019. A value that is
// within the noise of single precision of a multiple of 0.0001 mm is taken for that multiple, so
// that a body caught on a corner at 12.3 reads 12.3 in the fields and not 12.3000002.
double cleaned(double value) {
    return cleanedCoordinate(value);
}

} // namespace

double cleanedCoordinate(double value) {
    const double rounded = std::round(value * 1e4) / 1e4;
    const double noise = 3e-7 * std::max(1.0, std::abs(value));
    return std::abs(value - rounded) <= noise ? (rounded == 0.0 ? 0.0 : rounded) : value;
}

double snapGridDisplayStep(double step, double pixelsPerMillimetre) {
    // 1, 2, 5, 10, 20, 50 ...: 0.1 mm becomes 0.2, 0.5, 1 as the picture gets smaller.
    while (step * pixelsPerMillimetre < 8.0 && step < 1e6) {
        const double mantissa = step / std::pow(10.0, std::floor(std::log10(step)));
        step *= mantissa < 1.5 ? 2.0 : (mantissa < 3.5 ? 2.5 : 2.0);
    }
    return step;
}

namespace {

Vec3 cleaned(const Vec3& p) {
    return {cleaned(p[0]), cleaned(p[1]), cleaned(p[2])};
}

// The middle of a polyline, half way along its length.
Vec3 middleOf(const std::vector<geo::Point3f>& points) {
    double total = 0.0;
    for (std::size_t i = 1; i < points.size(); ++i) {
        total += length(sub(toVec(points[i]), toVec(points[i - 1])));
    }
    double left = total / 2.0;
    for (std::size_t i = 1; i < points.size(); ++i) {
        const Vec3 a = toVec(points[i - 1]);
        const Vec3 b = toVec(points[i]);
        const double piece = length(sub(b, a));
        if (left <= piece && piece > 0.0) {
            return add(a, scaled(sub(b, a), left / piece));
        }
        left -= piece;
    }
    return toVec(points.back());
}

// The centre of the circle a closed polyline runs round, if it is one.
std::optional<Vec3> circleCentreOf(const std::vector<geo::Point3f>& points) {
    constexpr std::size_t kLeastPoints = 8;
    if (points.size() < kLeastPoints + 1) {
        return std::nullopt;
    }
    const Vec3 first = toVec(points.front());
    const Vec3 last = toVec(points.back());
    if (length(sub(first, last)) > 1e-3) {
        return std::nullopt; // open
    }
    const std::size_t count = points.size() - 1; // the last point is the first again
    const Vec3 a = toVec(points[0]);
    const Vec3 b = toVec(points[count / 3]);
    const Vec3 c = toVec(points[2 * count / 3]);
    const Vec3 ab = sub(b, a);
    const Vec3 ac = sub(c, a);
    const Vec3 normal = cross(ab, ac);
    const double normalSquared = dot(normal, normal);
    if (normalSquared < 1e-12) {
        return std::nullopt; // the three points lie on a line
    }
    const Vec3 toCentre = scaled(
        cross(sub(scaled(ac, dot(ab, ab)), scaled(ab, dot(ac, ac))), normal), 0.5 / normalSquared);
    const Vec3 centre = add(a, toCentre);
    const double radius = length(toCentre);
    if (radius < 1e-6) {
        return std::nullopt;
    }
    const Vec3 up = scaled(normal, 1.0 / std::sqrt(normalSquared));
    const double tolerance = 1e-3 * radius + 1e-4;
    for (std::size_t i = 0; i < count; ++i) {
        const Vec3 off = sub(toVec(points[i]), centre);
        if (std::abs(length(off) - radius) > tolerance || std::abs(dot(off, up)) > tolerance) {
            return std::nullopt; // an oval, or not flat
        }
    }
    return centre;
}

} // namespace

bool SnapMatch::onPoint() const {
    return length(sub(mover.position, target.position)) < 1e-4;
}

std::vector<SnapPoint> snapPointsOf(const geo::TriangleMesh& mesh) {
    std::vector<SnapPoint> points;
    for (const geo::CornerPoint& corner : mesh.corners) {
        points.push_back({cleaned(toVec(corner.position)), SnapKind::corner});
    }
    for (const geo::EdgePolyline& edge : mesh.edges) {
        if (edge.points.size() < 2) {
            continue;
        }
        if (const auto centre = circleCentreOf(edge.points)) {
            points.push_back({cleaned(*centre), SnapKind::circleCentre});
            continue;
        }
        if (length(sub(toVec(edge.points.front()), toVec(edge.points.back()))) > 1e-3) {
            points.push_back({cleaned(middleOf(edge.points)), SnapKind::edgeMiddle});
        }
    }

    // The middle of every flat face: the centre of its area.
    struct FaceSum {
        Vec3 normal{};
        bool flat = true;
        double area = 0.0;
        Vec3 weighted{};
    };
    std::map<std::uint32_t, FaceSum> faces;
    for (std::size_t t = 0; t < mesh.faceIds.size() && 3 * t + 2 < mesh.indices.size(); ++t) {
        const Vec3 a = toVec(mesh.positions[mesh.indices[3 * t]]);
        const Vec3 b = toVec(mesh.positions[mesh.indices[3 * t + 1]]);
        const Vec3 c = toVec(mesh.positions[mesh.indices[3 * t + 2]]);
        const Vec3 twice = cross(sub(b, a), sub(c, a));
        const double area = length(twice) / 2.0;
        if (area < 1e-12) {
            continue;
        }
        const Vec3 normal = scaled(twice, 1.0 / (2.0 * area));
        FaceSum& face = faces[mesh.faceIds[t]];
        if (face.area == 0.0) {
            face.normal = normal;
        } else if (dot(face.normal, normal) < 1.0 - 1e-4) {
            face.flat = false;
        }
        face.area += area;
        face.weighted = add(face.weighted, scaled(add(add(a, b), c), area / 3.0));
    }
    for (const auto& [id, face] : faces) {
        if (face.flat && face.area > 0.0) {
            points.push_back(
                {cleaned(scaled(face.weighted, 1.0 / face.area)), SnapKind::faceMiddle});
        }
    }
    return points;
}

std::vector<SnapPoint> snapPointsOfBox(const Vec3& low, const Vec3& high) {
    std::vector<SnapPoint> points;
    const Vec3 mid{(low[0] + high[0]) / 2.0, (low[1] + high[1]) / 2.0, (low[2] + high[2]) / 2.0};
    // Every combination of low / middle / high on the three axes, except the middle of the box
    // itself: the number of axes at the middle says what it is.
    for (int x = 0; x < 3; ++x) {
        for (int y = 0; y < 3; ++y) {
            for (int z = 0; z < 3; ++z) {
                const int middles = (x == 1) + (y == 1) + (z == 1);
                if (middles == 3) {
                    continue;
                }
                const Vec3 p{x == 0 ? low[0] : (x == 1 ? mid[0] : high[0]),
                             y == 0 ? low[1] : (y == 1 ? mid[1] : high[1]),
                             z == 0 ? low[2] : (z == 1 ? mid[2] : high[2])};
                const SnapKind kind = middles == 0   ? SnapKind::corner
                                      : middles == 1 ? SnapKind::edgeMiddle
                                                     : SnapKind::faceMiddle;
                points.push_back({p, kind});
            }
        }
    }
    return points;
}

std::vector<SnapPoint> snapPointsOfBoxSide(const Vec3& low, const Vec3& high, int axis,
                                           bool upper) {
    std::vector<SnapPoint> points;
    const auto side = static_cast<std::size_t>(axis);
    const std::size_t u = (side + 1) % 3;
    const std::size_t v = (side + 2) % 3;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            Vec3 p{};
            p[side] = upper ? high[side] : low[side];
            p[u] = i == 0 ? low[u] : (i == 1 ? (low[u] + high[u]) / 2.0 : high[u]);
            p[v] = j == 0 ? low[v] : (j == 1 ? (low[v] + high[v]) / 2.0 : high[v]);
            const int middles = (i == 1) + (j == 1);
            points.push_back({p, middles == 0   ? SnapKind::corner
                                 : middles == 1 ? SnapKind::edgeMiddle
                                                : SnapKind::faceMiddle});
        }
    }
    return points;
}

std::vector<SnapPoint> snapPointsOfBed(double sizeX, double sizeY) {
    const double hx = sizeX / 2.0;
    const double hy = sizeY / 2.0;
    std::vector<SnapPoint> points;
    for (const double x : {-hx, hx}) {
        for (const double y : {-hy, hy}) {
            points.push_back({{x, y, 0.0}, SnapKind::corner});
        }
    }
    points.push_back({{-hx, 0.0, 0.0}, SnapKind::edgeMiddle});
    points.push_back({{hx, 0.0, 0.0}, SnapKind::edgeMiddle});
    points.push_back({{0.0, -hy, 0.0}, SnapKind::edgeMiddle});
    points.push_back({{0.0, hy, 0.0}, SnapKind::edgeMiddle});
    points.push_back({{0.0, 0.0, 0.0}, SnapKind::faceMiddle});
    return points;
}

SnapResult snapOffset(const std::vector<SnapPoint>& movers, const std::vector<SnapPoint>& targets,
                      const Vec3& offset, const std::array<bool, 3>& free, const Vec3& reach) {
    SnapResult result;
    result.offset = offset;

    // An axis that is free but has no reach cannot catch.
    std::array<bool, 3> axes{};
    int count = 0;
    for (std::size_t a = 0; a < 3; ++a) {
        axes[a] = free[a] && reach[a] > 0.0;
        count += axes[a] ? 1 : 0;
    }
    if (count == 0 || movers.empty() || targets.empty()) {
        return result;
    }

    // The catches are written down at the end, when the move is final: both points are shown
    // where they are after it.
    std::array<const SnapPoint*, 3> caughtMover{};
    std::array<const SnapPoint*, 3> caughtTarget{};
    const auto finish = [&] {
        for (std::size_t a = 0; a < 3; ++a) {
            if (caughtMover[a] != nullptr) {
                SnapMatch match{*caughtMover[a], *caughtTarget[a]};
                match.mover.position = add(match.mover.position, result.offset);
                result.axes[a] = match;
            }
        }
        return result;
    };

    // 1. One point of the body on one point of the others, in every free axis at once. Of equal
    //    ones the pair nearest in the axes that stay is taken, then the better kind of point.
    {
        double bestScore = 1e30;
        double bestApart = 1e30;
        const SnapPoint* bestMover = nullptr;
        const SnapPoint* bestTarget = nullptr;
        for (const SnapPoint& mover : movers) {
            for (const SnapPoint& target : targets) {
                double score = 0.0;
                double apart = 0.0;
                bool inReach = true;
                for (std::size_t a = 0; a < 3 && inReach; ++a) {
                    const double d = target.position[a] - (mover.position[a] + offset[a]);
                    if (!axes[a]) {
                        apart += d * d;
                        continue;
                    }
                    const double miss = std::abs(d) / reach[a];
                    inReach = miss <= 1.0;
                    score += miss * miss;
                }
                if (!inReach) {
                    continue;
                }
                // Corners before circle centres before edge middles before face middles.
                apart += 1e-9 * static_cast<double>(target.kind);
                const bool tie = std::abs(score - bestScore) <= 1e-9;
                if (bestMover == nullptr || score < bestScore - 1e-9 ||
                    (tie && apart < bestApart)) {
                    bestScore = std::min(score, bestScore);
                    bestApart = apart;
                    bestMover = &mover;
                    bestTarget = &target;
                }
            }
        }
        // With one free axis this is the same as the catch of that axis below, which also looks
        // at the other axes for the nearest of several equal ones.
        if (bestMover != nullptr && count > 1) {
            for (std::size_t a = 0; a < 3; ++a) {
                if (axes[a]) {
                    result.offset[a] = bestTarget->position[a] - bestMover->position[a];
                    caughtMover[a] = bestMover;
                    caughtTarget[a] = bestTarget;
                }
            }
            return finish();
        }
    }

    // 2. Each axis on its own: the nearest line-up. Of several equal ones (the four corners of a
    //    face line up in the same way) the one nearest in the other axes is taken.
    for (std::size_t a = 0; a < 3; ++a) {
        if (!axes[a]) {
            continue;
        }
        double bestMiss = 1e30;
        double bestApart = 1e30;
        const SnapPoint* bestMover = nullptr;
        const SnapPoint* bestTarget = nullptr;
        for (const SnapPoint& mover : movers) {
            for (const SnapPoint& target : targets) {
                const double miss =
                    std::abs(target.position[a] - (mover.position[a] + offset[a])) / reach[a];
                if (miss > 1.0 || miss > bestMiss + 1e-6) {
                    continue;
                }
                double apart = 0.0;
                for (std::size_t other = 0; other < 3; ++other) {
                    if (other != a) {
                        const double d =
                            target.position[other] - (mover.position[other] + offset[other]);
                        apart += d * d;
                    }
                }
                apart += 1e-9 * static_cast<double>(target.kind);
                const bool tie = std::abs(miss - bestMiss) <= 1e-6;
                if (bestMover == nullptr || miss < bestMiss - 1e-6 || (tie && apart < bestApart)) {
                    bestMiss = std::min(miss, bestMiss);
                    bestApart = apart;
                    bestMover = &mover;
                    bestTarget = &target;
                }
            }
        }
        if (bestMover != nullptr) {
            // The other axes are judged by where the body is before this catch (`offset`).
            caughtMover[a] = bestMover;
            caughtTarget[a] = bestTarget;
        }
    }
    for (std::size_t a = 0; a < 3; ++a) {
        if (caughtMover[a] != nullptr) {
            result.offset[a] = caughtTarget[a]->position[a] - caughtMover[a]->position[a];
        }
    }
    return finish();
}

std::optional<ScaleCatch> snapScaleFactor(const Vec3& anchor, const Vec3& corner0, double wanted,
                                          const std::vector<SnapPoint>& targets,
                                          const Vec3& reach) {
    // The corner held runs along the diagonal: corner(f) = anchor + (corner0 - anchor) * f. A
    // target is caught on one axis when the corner reaches its coordinate there, and on a point
    // when it reaches all three at once.
    constexpr double kOnPoint = 1e-3; // mm
    const Vec3 way = sub(corner0, anchor);
    struct Candidate {
        double factor = 1.0;
        double miss = 1e30;
        std::size_t axis = 0;
        const SnapPoint* target = nullptr;
        bool onPoint = false;
    };
    Candidate best;
    for (std::size_t a = 0; a < 3; ++a) {
        if (std::abs(way[a]) <= 1e-6 || reach[a] <= 0.0) {
            continue;
        }
        for (const SnapPoint& target : targets) {
            const double factor = (target.position[a] - anchor[a]) / way[a];
            if (!(factor > 0.0)) {
                continue;
            }
            const double miss = std::abs(factor - wanted) * std::abs(way[a]) / reach[a];
            if (miss > 1.0) {
                continue;
            }
            const Vec3 corner = add(anchor, scaled(way, factor));
            const bool onPoint = std::abs(corner[0] - target.position[0]) <= kOnPoint &&
                                 std::abs(corner[1] - target.position[1]) <= kOnPoint &&
                                 std::abs(corner[2] - target.position[2]) <= kOnPoint;
            // A point beats a line-up; then the nearest; of equal ones the better kind of point.
            bool better = best.target == nullptr || (onPoint && !best.onPoint);
            if (!better && onPoint == best.onPoint) {
                better = miss < best.miss - 1e-9 ||
                         (std::abs(miss - best.miss) <= 1e-9 && target.kind < best.target->kind);
            }
            if (better) {
                best = {factor, miss, a, &target, onPoint};
            }
        }
    }
    if (best.target == nullptr) {
        return std::nullopt;
    }
    ScaleCatch caught;
    caught.factor = best.factor;
    caught.result.offset = scaled(way, best.factor - 1.0);
    SnapMatch match;
    match.mover = {add(anchor, scaled(way, best.factor)), SnapKind::corner};
    match.target = *best.target;
    if (best.onPoint) {
        for (auto& axis : caught.result.axes) {
            axis = match;
        }
    } else {
        caught.result.axes[best.axis] = match;
    }
    return caught;
}

} // namespace fission::interaction
