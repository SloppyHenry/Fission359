// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Hollowing a body (AP-2.6). The kernel's own call is not trusted: for a box (with round vertical
// corners) and a cylinder the inner body is built anew, smaller by the wall (a rounding r becomes
// r - wall), and taken from the outer one; the result is checked against a formula. For every
// other body the kernel's call is used, and its result must pass a plausibility check, otherwise
// the answer is "not sure" and nothing is returned (docs/spikes/kernel-robustheit.md, case C2).

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <map>
#include <numbers>
#include <set>
#include <string>
#include <vector>

#include "fission/geo/error.h"
#include "fission/geo/occt/solids.h"
#include "fission/geo/plausibility.h"

#include "occt_build.h"
#include <BRepAdaptor_Surface.hxx>
#include <BRepOffsetAPI_MakeThickSolid.hxx>
#include <Standard_Failure.hxx>

namespace fission::geo::occt {

namespace {

constexpr double kPi = std::numbers::pi;

const TopoDS_Shape& shapeOf(const Solid& solid) {
    const auto* data = dynamic_cast<const OcctSolidData*>(solid.data());
    if (data == nullptr) {
        throw GeometryError(GeometryErrorCode::invalidInput,
                            "hollow: a body is empty or was not created by this kernel");
    }
    return data->shape();
}

[[noreturn]] void unsure(const std::string& detail) {
    throw GeometryError(GeometryErrorCode::invalidInput, "hollow: " + detail, "unsure");
}

// A box with round vertical corners (radius 0 = sharp), or a cylinder, standing along z.
struct Prism {
    bool cylinder = false;
    std::array<double, 3> lo{};
    std::array<double, 3> hi{};
    double radius = 0.0; // corner radius of the box, radius of the cylinder
};

// Which of the six sides are meant: [axis][0 = low side, 1 = high side].
using Sides = std::array<std::array<bool, 2>, 3>;

// What a face is called by its place on a prism: "left", "top", "cornerFrontLeft", "side" ...
std::string labelOf(const ElementSignature& face, const Prism& prism) {
    if (face.shape == SignatureShape::plane) {
        const Point3d n = face.direction;
        if (n.x < -0.5)
            return "left";
        if (n.x > 0.5)
            return "right";
        if (n.y < -0.5)
            return "front";
        if (n.y > 0.5)
            return "back";
        return n.z < 0.0 ? "bottom" : "top";
    }
    if (prism.cylinder) {
        return "side";
    }
    const double midX = 0.5 * (prism.lo[0] + prism.hi[0]);
    const double midY = 0.5 * (prism.lo[1] + prism.hi[1]);
    const bool front = face.centroid.y < midY;
    const bool left = face.centroid.x < midX;
    return std::string("corner") + (front ? "Front" : "Back") + (left ? "Left" : "Right");
}

// Volume of a prism with the given extents; `corners` is how many of the four rounded vertical
// corners are still there (a corner on a side that is open is cut away).
double volumeOf(const Prism& prism, int corners) {
    const double width = prism.hi[0] - prism.lo[0];
    const double depth = prism.hi[1] - prism.lo[1];
    const double height = prism.hi[2] - prism.lo[2];
    if (prism.cylinder) {
        return kPi * prism.radius * prism.radius * height;
    }
    return width * depth * height -
           corners * (1.0 - kPi / 4.0) * prism.radius * prism.radius * height;
}

// Recognises what the body is by looking at its faces and checking its volume against the formula.
bool recognise(const BodySignatures& looks, const SolidReport& report, Prism& prism) {
    prism.lo = {report.boxMin.x, report.boxMin.y, report.boxMin.z};
    prism.hi = {report.boxMax.x, report.boxMax.y, report.boxMax.z};
    int planes = 0;
    int cylinders = 0;
    double radius = -1.0;
    for (const ElementSignature& face : looks.faces) {
        if (face.shape == SignatureShape::plane) {
            const Point3d n = face.direction;
            const bool along = std::max({std::abs(n.x), std::abs(n.y), std::abs(n.z)}) > 1.0 - 1e-9;
            if (!along) {
                return false;
            }
            ++planes;
        } else if (face.shape == SignatureShape::cylinder) {
            if (std::abs(std::abs(face.direction.z) - 1.0) > 1e-9) {
                return false;
            }
            if (radius >= 0.0 && std::abs(radius - face.radius) > 1e-9) {
                return false;
            }
            radius = face.radius;
            ++cylinders;
        } else {
            return false;
        }
    }
    if (planes == 6 && (cylinders == 0 || cylinders == 4)) {
        prism.cylinder = false;
        prism.radius = cylinders == 0 ? 0.0 : radius;
    } else if (planes == 2 && cylinders == 1) {
        prism.cylinder = true;
        prism.radius = radius;
    } else {
        return false;
    }
    return std::abs(volumeOf(prism, 4) - report.volume) <= 1e-6 * std::max(1.0, report.volume);
}

NamedSolid build(const Prism& prism) {
    if (prism.cylinder) {
        return makeNamedCylinder(
            prism.radius, prism.hi[2] - prism.lo[2],
            {0.5 * (prism.lo[0] + prism.hi[0]), 0.5 * (prism.lo[1] + prism.hi[1]), prism.lo[2]});
    }
    return makeNamedBox(prism.hi[0] - prism.lo[0], prism.hi[1] - prism.lo[1],
                        prism.hi[2] - prism.lo[2], {prism.lo[0], prism.lo[1], prism.lo[2]},
                        prism.radius);
}

// The rebuilt body with the names of `original` where a face has the same place on it, or with
// `rename(label)` for all of them when `original` is null.
NamedSolid renamed(const NamedSolid& rebuilt, const std::map<std::string, std::string>& byLabel,
                   const Prism& prism,
                   const std::function<std::string(const std::string&)>& other) {
    const BodySignatures looks = signatures(rebuilt.solid);
    NamedSolid result;
    result.solid = rebuilt.solid;
    for (const NamedFace& face : rebuilt.faces) {
        const std::string label = labelOf(looks.faces.at(face.faceId - 1), prism);
        const auto known = byLabel.find(label);
        result.faces.push_back(
            {face.faceId, known != byLabel.end() ? known->second : other(label)});
    }
    return result;
}

NamedSolid hollowPrism(const NamedSolid& solid, const BodySignatures& looks, const Prism& prism,
                       const Sides& open, double wall, bool outside, const std::string& prefix,
                       const SolidReport& before, bool anyOpen) {
    // How far the cutter reaches beyond an open side.
    const double reach = wall + 1.0;
    std::array<int, 3> closed{};
    double widest = 0.0; // the thickest wall the body can take (exclusive)
    double thinnest = 1e300;
    double largest = 1e300;
    for (int axis = 0; axis < 3; ++axis) {
        closed[axis] = (open[axis][0] ? 0 : 1) + (open[axis][1] ? 0 : 1);
        const double size = prism.hi[axis] - prism.lo[axis];
        if (!(prism.cylinder && axis < 2)) {
            thinnest = std::min(thinnest, size);
            if (closed[axis] > 0) {
                largest = std::min(largest, size / closed[axis]);
            }
        }
    }
    if (prism.cylinder) {
        // The wall of a cylinder cannot reach past its axis.
        thinnest = std::min(thinnest, 2.0 * prism.radius);
        largest = std::min(largest, prism.radius);
    }
    widest = largest;
    if (!outside && !(wall < widest - 1e-9)) {
        GeometryError error(GeometryErrorCode::invalidInput, "hollow: the wall is too thick",
                            "thickness");
        error.withNumber("given", wall).withNumber("thinnest", thinnest);
        const double fits = std::floor((widest - 1e-9) * 10.0) / 10.0;
        if (fits >= 0.1) {
            error.withNumber("maxValue", fits);
        }
        throw error;
    }

    // What is taken away (cutter) and what it is taken from (outer).
    Prism cutter = prism;
    Prism hull = prism;
    for (int axis = 0; axis < 3; ++axis) {
        for (int side = 0; side < 2; ++side) {
            const double sign = side == 0 ? 1.0 : -1.0;
            const double edge = side == 0 ? prism.lo[axis] : prism.hi[axis];
            const bool skip = prism.cylinder && axis < 2;
            if (outside) {
                // The body itself is the cavity; the hull grows by the wall, except at openings.
                (side == 0 ? cutter.lo : cutter.hi)[axis] =
                    open[axis][side] ? edge - sign * reach : edge;
                (side == 0 ? hull.lo : hull.hi)[axis] =
                    open[axis][side] || skip ? edge : edge - sign * wall;
            } else {
                (side == 0 ? cutter.lo : cutter.hi)[axis] =
                    open[axis][side] ? edge - sign * reach : (skip ? edge : edge + sign * wall);
            }
        }
    }
    if (outside) {
        // A sharp box keeps sharp corners (the wall is the inner box pushed outwards).
        hull.radius = prism.cylinder || prism.radius > 0.0 ? prism.radius + wall : 0.0;
        if (prism.cylinder) {
            hull.lo[0] = prism.lo[0] - wall;
            hull.hi[0] = prism.hi[0] + wall;
            hull.lo[1] = prism.lo[1] - wall;
            hull.hi[1] = prism.hi[1] + wall;
        }
    } else {
        cutter.radius = prism.cylinder ? prism.radius - wall : std::max(0.0, prism.radius - wall);
        if (prism.cylinder) {
            cutter.lo[0] = prism.lo[0] + wall;
            cutter.hi[0] = prism.hi[0] - wall;
            cutter.lo[1] = prism.lo[1] + wall;
            cutter.hi[1] = prism.hi[1] - wall;
        } else {
            cutter.radius = std::min(
                cutter.radius,
                0.5 * std::min(cutter.hi[0] - cutter.lo[0], cutter.hi[1] - cutter.lo[1]) - 1e-9);
            cutter.radius = std::max(cutter.radius, 0.0);
        }
    }

    // The names of the faces of the body, by their place on it.
    std::map<std::string, std::string> roleByLabel;
    for (const NamedFace& face : solid.faces) {
        roleByLabel[labelOf(looks.faces.at(face.faceId - 1), prism)] = face.role;
    }
    const NamedSolid outer =
        outside ? renamed(build(hull), roleByLabel, prism,
                          [](const std::string& label) { return "wall[" + label + "]"; })
                : solid;
    const NamedSolid inner = renamed(build(cutter), {}, prism, [&](const std::string& label) {
        return prefix + "[" + label + "]";
    });

    NamedSolid result = booleanNamed(BooleanOp::subtract, outer, {inner});

    // The formula: what the outer body has, minus what the cutter takes from it. A corner that
    // lies on an open side is cut away together with the extension.
    Prism overlap = cutter;
    for (int axis = 0; axis < 3; ++axis) {
        overlap.lo[axis] = std::max(cutter.lo[axis], hull.lo[axis]);
        overlap.hi[axis] = std::min(cutter.hi[axis], hull.hi[axis]);
    }
    int corners = 0;
    for (int x = 0; x < 2; ++x) {
        for (int y = 0; y < 2; ++y) {
            corners += (open[0][x] || open[1][y]) ? 0 : 1;
        }
    }
    const double outerVolume = outside ? measure(outer.solid).volume : before.volume;
    const double expected = outerVolume - volumeOf(overlap, corners);
    Expectation expectation;
    expectation.volumeMin = expected * (1.0 - 1e-6);
    expectation.volumeMax = expected * (1.0 + 1e-6);
    expectation.solids = 1;
    expectation.shells = anyOpen ? 1 : 2;
    const PlausibilityReport judged =
        checkPlausibility(before, measure(result.solid), expectation, outside);
    if (!judged.plausible) {
        unsure(judged.reason);
    }
    return result;
}

// --- any other body: the kernel's call, believed only when it looks right ---------------------

NamedSolid hollowByKernel(const NamedSolid& solid, const std::vector<std::uint32_t>& openFaces,
                          double wall, bool outside, const std::string& prefix,
                          const SolidReport& before) {
    if (openFaces.empty()) {
        unsure("a closed hollow body is only made for a box and a cylinder");
    }
    const TopoDS_Shape& shape = shapeOf(solid.solid);
    ShapeMap faces;
    TopExp::MapShapes(shape, TopAbs_FACE, faces);
    std::vector<std::string> roles(static_cast<std::size_t>(faces.Extent()));
    for (const NamedFace& face : solid.faces) {
        if (face.faceId >= 1 && face.faceId <= roles.size()) {
            roles[face.faceId - 1] = face.role;
        }
    }
    if (std::any_of(roles.begin(), roles.end(), [](const std::string& r) { return r.empty(); })) {
        throw GeometryError(GeometryErrorCode::invalidInput, "hollow: an input face has no role");
    }
    // The size must be small against the body: the estimate below only holds then.
    const double thinnest =
        std::min({before.boxMax.x - before.boxMin.x, before.boxMax.y - before.boxMin.y,
                  before.boxMax.z - before.boxMin.z});
    if (!(wall <= 0.25 * thinnest)) {
        GeometryError error(GeometryErrorCode::invalidInput, "hollow: the wall is too thick",
                            "thickness");
        error.withNumber("given", wall).withNumber("thinnest", thinnest);
        const double fits = std::floor(0.25 * thinnest * 10.0) / 10.0;
        if (fits >= 0.1) {
            error.withNumber("maxValue", fits);
        }
        throw error;
    }
    std::set<std::uint32_t> opened(openFaces.begin(), openFaces.end());
    NCollection_List<TopoDS_Shape> closing;
    double wallArea = 0.0;
    for (int i = 1; i <= faces.Extent(); ++i) {
        if (opened.contains(static_cast<std::uint32_t>(i))) {
            closing.Append(faces(i));
        } else {
            GProp_GProps properties;
            BRepGProp::SurfaceProperties(faces(i), properties);
            wallArea += properties.Mass();
        }
    }
    if (closing.Size() != opened.size()) {
        throw GeometryError(GeometryErrorCode::invalidInput, "hollow: an open face is not there");
    }
    try {
        BRepOffsetAPI_MakeThickSolid maker;
        maker.MakeThickSolidByJoin(shape, closing, outside ? wall : -wall, 1e-4, BRepOffset_Skin,
                                   false, false, GeomAbs_Intersection, false);
        maker.Build();
        if (!maker.IsDone() || maker.Shape().IsNull()) {
            unsure("the kernel did not give a result");
        }
        const TopoDS_Shape result = maker.Shape();
        NamedSolid made;
        made.solid = wrap(result);
        const SolidReport after = measure(made.solid);
        Expectation expectation;
        expectation.volumeMin = 0.75 * wallArea * wall;
        expectation.volumeMax = 1.25 * wallArea * wall;
        expectation.solids = 1;
        expectation.shells = 1;
        const PlausibilityReport judged = checkPlausibility(before, after, expectation, outside);
        if (!judged.plausible) {
            unsure(judged.reason);
        }

        // Names: a face that stays keeps its name; the new wall is named after the face it
        // follows.
        ShapeMap finalFaces;
        TopExp::MapShapes(result, TopAbs_FACE, finalFaces);
        std::vector<std::string> finalRoles(static_cast<std::size_t>(finalFaces.Extent()));
        const auto claim = [&](const TopoDS_Shape& piece, const std::string& role) {
            const int index = finalFaces.FindIndex(piece);
            if (index > 0 && finalRoles[static_cast<std::size_t>(index - 1)].empty()) {
                finalRoles[static_cast<std::size_t>(index - 1)] = role;
            }
        };
        std::set<std::string> taken;
        const auto unique = [&](std::string role) {
            std::string candidate = role;
            for (int n = 2; taken.contains(candidate); ++n) {
                candidate = role + "#" + std::to_string(n);
            }
            taken.insert(candidate);
            return candidate;
        };
        for (int i = 1; i <= faces.Extent(); ++i) {
            const std::string& role = roles[static_cast<std::size_t>(i - 1)];
            if (opened.contains(static_cast<std::uint32_t>(i)) || maker.IsDeleted(faces(i))) {
                made.history.deleted.push_back(role);
                continue;
            }
            const auto& changed = maker.Modified(faces(i));
            const std::string kept = outside ? prefix + "[" + role + "]" : role;
            if (changed.IsEmpty()) {
                claim(faces(i), kept);
            } else {
                for (const TopoDS_Shape& piece : changed) {
                    claim(piece, kept);
                }
            }
        }
        for (int i = 1; i <= faces.Extent(); ++i) {
            if (opened.contains(static_cast<std::uint32_t>(i))) {
                continue;
            }
            const std::string& role = roles[static_cast<std::size_t>(i - 1)];
            for (const TopoDS_Shape& piece : maker.Generated(faces(i))) {
                claim(piece, outside ? role : prefix + "[" + role + "]");
            }
        }
        for (std::string& role : finalRoles) {
            if (!role.empty()) {
                taken.insert(role);
            }
        }
        // Duplicate names (a face that became several) are told apart.
        {
            std::set<std::string> seen;
            for (std::string& role : finalRoles) {
                if (role.empty() || seen.insert(role).second) {
                    continue;
                }
                std::string candidate;
                for (int n = 2; seen.contains(candidate = role + "#" + std::to_string(n)); ++n) {
                }
                seen.insert(candidate);
                role = candidate;
            }
            taken = seen;
        }
        int wallPieces = 0;
        for (std::string& role : finalRoles) {
            if (role.empty()) {
                role = unique(prefix + "[wall#" + std::to_string(++wallPieces) + "]");
            }
        }
        for (std::size_t i = 0; i < finalRoles.size(); ++i) {
            made.faces.push_back(
                {faceNumber(result, finalFaces(static_cast<int>(i) + 1)), finalRoles[i]});
        }
        return made;
    } catch (const Standard_Failure&) {
        unsure("the kernel failed");
    }
}

} // namespace

NamedSolid hollowNamed(const NamedSolid& solid, const std::vector<std::uint32_t>& openFaces,
                       double wall, bool outside, const std::string& rolePrefix) {
    if (!std::isfinite(wall) || !(wall > 0.0) || wall > 1.0e6) {
        throw GeometryError(GeometryErrorCode::invalidInput,
                            "hollow: needs a wall that is a positive number", "thickness");
    }
    const SolidReport before = measure(solid.solid);
    const BodySignatures looks = signatures(solid.solid);
    for (const NamedFace& face : solid.faces) {
        if (face.role.empty() || face.faceId < 1 || face.faceId > looks.faces.size()) {
            throw GeometryError(GeometryErrorCode::invalidInput, "hollow: a face has no role");
        }
    }
    for (const std::uint32_t id : openFaces) {
        if (id < 1 || id > looks.faces.size()) {
            throw GeometryError(GeometryErrorCode::invalidInput,
                                "hollow: an open face is not there", "face");
        }
    }
    Prism prism;
    if (recognise(looks, before, prism)) {
        Sides open{};
        for (const std::uint32_t id : openFaces) {
            const ElementSignature& face = looks.faces[id - 1];
            if (face.shape != SignatureShape::plane) {
                throw GeometryError(GeometryErrorCode::invalidInput,
                                    "hollow: only a flat face can be left open", "face");
            }
            const Point3d n = face.direction;
            const int axis = std::abs(n.x) > 0.5 ? 0 : std::abs(n.y) > 0.5 ? 1 : 2;
            const double along = axis == 0 ? n.x : axis == 1 ? n.y : n.z;
            open[axis][along < 0.0 ? 0 : 1] = true;
        }
        for (int axis = 0; axis < 2 && prism.cylinder; ++axis) {
            open[axis] = {false, false};
        }
        return hollowPrism(solid, looks, prism, open, wall, outside, rolePrefix, before,
                           !openFaces.empty());
    }
    return hollowByKernel(solid, openFaces, wall, outside, rolePrefix, before);
}

} // namespace fission::geo::occt
