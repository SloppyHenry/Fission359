// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Stretching a body (AP-2.4b): cut it at a flat place that crosses no detail, move one part away
// and fill the gap with the cross-section. Rounds, holes and lettering stay exactly as they are
// (rule 11); nothing is scaled.

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>
#include <vector>

#include "fission/geo/error.h"
#include "fission/geo/occt/solids.h"

#include "occt_build.h"
#include <BRepBndLib.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <Bnd_Box.hxx>
#include <Standard_Failure.hxx>
#include <gp_Cylinder.hxx>
#include <gp_Vec.hxx>

namespace fission::geo::occt {

namespace {

constexpr double kTolerance = 1e-6;
constexpr double kMinimumFree = 1e-3; // a free stretch shorter than this is no place to cut

const TopoDS_Shape& shapeOf(const Solid& solid) {
    const auto* data = dynamic_cast<const OcctSolidData*>(solid.data());
    if (data == nullptr) {
        throw GeometryError(GeometryErrorCode::invalidInput,
                            "stretch: a body is empty or was not created by this kernel");
    }
    return data->shape();
}

// The axis of x, y or z the way points along (+1 or -1), or -1 if it is none of them.
int axisOf(Point3d way, double& sign) {
    const double parts[3] = {way.x, way.y, way.z};
    for (int k = 0; k < 3; ++k) {
        bool alone = true;
        for (int other = 0; other < 3; ++other) {
            alone = alone && (other == k || std::abs(parts[other]) < 1e-9);
        }
        if (alone && std::abs(std::abs(parts[k]) - 1.0) < 1e-9) {
            sign = parts[k] > 0.0 ? 1.0 : -1.0;
            return k;
        }
    }
    return -1;
}

gp_Dir unitAlong(int axis, double sign) {
    return gp_Dir(axis == 0 ? sign : 0.0, axis == 1 ? sign : 0.0, axis == 2 ? sign : 0.0);
}

// Coordinate along the way (sign * x_k) of a point.
double along(const gp_Pnt& point, int axis, double sign) {
    return sign * (axis == 0 ? point.X() : axis == 1 ? point.Y() : point.Z());
}

using Interval = std::pair<double, double>;

// The stretch along the way (as a coordinate along it) that a face blocks; none when a cut through
// the face leaves it as it is (a flat face along the way, a round wall around it).
std::optional<Interval> blockedBy(const TopoDS_Face& face, int axis, double sign) {
    const BRepAdaptor_Surface surface(face);
    const gp_Dir way = unitAlong(axis, sign);
    bool blocks = true;
    if (surface.GetType() == GeomAbs_Plane) {
        const double facing = std::abs(surface.Plane().Axis().Direction().Dot(way));
        blocks = facing > 1e-9; // square on the way: a step; slanted: crossed
    } else if (surface.GetType() == GeomAbs_Cylinder) {
        const gp_Vec cross = gp_Vec(surface.Cylinder().Axis().Direction()).Crossed(gp_Vec(way));
        blocks = cross.Magnitude() > 1e-9; // around the way: stays round
    }
    if (!blocks) {
        return std::nullopt;
    }
    Bnd_Box box;
    BRepBndLib::AddOptimal(face, box, /*useTriangulation=*/false);
    double lo[3], hi[3];
    box.Get(lo[0], lo[1], lo[2], hi[0], hi[1], hi[2]);
    const double a = sign > 0.0 ? lo[axis] : -hi[axis];
    const double b = sign > 0.0 ? hi[axis] : -lo[axis];
    return Interval{a - kTolerance, b + kTolerance};
}

struct Layout {
    double low = 0.0; // body extent along the way
    double high = 0.0;
    std::vector<Interval> blocked; // sorted, merged
};

Layout layoutOf(const TopoDS_Shape& shape, int axis, double sign) {
    Layout layout;
    Bnd_Box whole;
    BRepBndLib::AddOptimal(shape, whole, false);
    double lo[3], hi[3];
    whole.Get(lo[0], lo[1], lo[2], hi[0], hi[1], hi[2]);
    layout.low = sign > 0.0 ? lo[axis] : -hi[axis];
    layout.high = sign > 0.0 ? hi[axis] : -lo[axis];
    ShapeMap faces;
    TopExp::MapShapes(shape, TopAbs_FACE, faces);
    for (int i = 1; i <= faces.Extent(); ++i) {
        if (const auto interval = blockedBy(TopoDS::Face(faces(i)), axis, sign)) {
            layout.blocked.push_back(*interval);
        }
    }
    std::sort(layout.blocked.begin(), layout.blocked.end());
    std::vector<Interval> merged;
    for (const Interval& next : layout.blocked) {
        if (!merged.empty() && next.first <= merged.back().second) {
            merged.back().second = std::max(merged.back().second, next.second);
        } else {
            merged.push_back(next);
        }
    }
    layout.blocked = std::move(merged);
    return layout;
}

// Nothing blocks the cut between `from` and `to` (a single plane when they are equal).
bool isPlain(const Layout& layout, double from, double to) {
    if (!(from > layout.low + kTolerance) || !(to < layout.high - kTolerance)) {
        return false;
    }
    return std::none_of(layout.blocked.begin(), layout.blocked.end(), [&](const Interval& blocked) {
        return to >= blocked.first && from <= blocked.second;
    });
}

// A box for cutting: everything on one side of `position` (along the way).
TopoDS_Shape sideBox(int axis, double sign, double position, bool beyond, double reach) {
    // In world coordinates the side is [position, reach] along +x_k for sign > 0, and mirrored.
    const double from = beyond ? position : -reach;
    const double to = beyond ? reach : position;
    double lo = sign > 0.0 ? from : -to;
    double hi = sign > 0.0 ? to : -from;
    gp_Pnt corner(-reach, -reach, -reach);
    double size[3] = {2.0 * reach, 2.0 * reach, 2.0 * reach};
    double start[3] = {-reach, -reach, -reach};
    start[axis] = lo;
    size[axis] = hi - lo;
    corner = gp_Pnt(start[0], start[1], start[2]);
    return BRepPrimAPI_MakeBox(corner, size[0], size[1], size[2]).Shape();
}

NamedSolid toolOf(const TopoDS_Shape& shape) {
    return nameFaces(shape, "stretch", [](const TopoDS_Face&) { return std::string("stretch[cut]"); });
}

} // namespace

std::optional<double> plainCutOf(const Solid& solid, Point3d way, double near) {
    double sign = 1.0;
    const int axis = axisOf(way, sign);
    if (axis < 0 || !std::isfinite(near)) {
        return std::nullopt;
    }
    try {
        const Layout layout = layoutOf(shapeOf(solid), axis, sign);
        // The free stretches between the blocked ones, inside the body.
        std::vector<Interval> free;
        double from = layout.low + kTolerance;
        for (const Interval& blocked : layout.blocked) {
            if (blocked.first > from) {
                free.emplace_back(from, std::min(blocked.first, layout.high - kTolerance));
            }
            from = std::max(from, blocked.second);
        }
        if (from < layout.high - kTolerance) {
            free.emplace_back(from, layout.high - kTolerance);
        }
        std::optional<double> best;
        double bestDistance = 0.0;
        for (const Interval& stretch : free) {
            if (!(stretch.second - stretch.first > kMinimumFree)) {
                continue;
            }
            const double middle = 0.5 * (stretch.first + stretch.second);
            const double distance = near < stretch.first    ? stretch.first - near
                                    : near > stretch.second ? near - stretch.second
                                                            : 0.0;
            if (!best || distance < bestDistance) {
                best = middle;
                bestDistance = distance;
            }
        }
        return best;
    } catch (const Standard_Failure&) {
        return std::nullopt;
    }
}

NamedSolid stretchNamed(const NamedSolid& solid, Point3d way, double position, double distance) {
    double sign = 1.0;
    const int axis = axisOf(way, sign);
    if (axis < 0) {
        throw GeometryError(GeometryErrorCode::invalidInput,
                            "stretch: the way must run along x, y or z", "face");
    }
    if (!std::isfinite(position) || !std::isfinite(distance) || distance == 0.0 ||
        std::abs(distance) > 1.0e6) {
        throw GeometryError(GeometryErrorCode::invalidInput,
                            "stretch: needs a distance that is not zero", "distance");
    }
    try {
        const TopoDS_Shape& shape = shapeOf(solid.solid);
        const Layout layout = layoutOf(shape, axis, sign);
        // When shrinking, the whole piece that is taken out must be free of details.
        if (!isPlain(layout, std::min(position, position + std::min(distance, 0.0)), position)) {
            throw GeometryError(GeometryErrorCode::invalidInput,
                                "stretch: the cut would cross a rounding, hole or step",
                                "position");
        }
        if (distance < 0.0 && !(position + distance > layout.low + kTolerance)) {
            throw GeometryError(GeometryErrorCode::invalidInput,
                                "stretch: the body would shrink to nothing", "distance");
        }
        const double reach = 4.0 * (std::abs(layout.high) + std::abs(layout.low) +
                                    std::abs(distance) + std::abs(position) + 100.0);
        // The part that stays and the part that moves. When shrinking, the stretch between the
        // two planes is taken out.
        const double stayEnd = distance < 0.0 ? position + distance : position;
        const NamedSolid fixed =
            booleanNamed(BooleanOp::subtract, solid,
                         {toolOf(sideBox(axis, sign, stayEnd, true, reach))});
        const NamedSolid moving = booleanNamed(
            BooleanOp::subtract, solid, {toolOf(sideBox(axis, sign, position, false, reach))});
        const gp_Dir unit = unitAlong(axis, sign);
        const NamedSolid moved = translateNamed(
            moving, {unit.X() * distance, unit.Y() * distance, unit.Z() * distance});

        std::vector<NamedSolid> tools{moved};
        if (distance > 0.0) {
            // The gap is filled with the cross-section: the faces of the fixed part that look
            // along the way at the cut, pulled out by the distance.
            ShapeMap faces;
            TopExp::MapShapes(shapeOf(fixed.solid), TopAbs_FACE, faces);
            for (int i = 1; i <= faces.Extent(); ++i) {
                const TopoDS_Face face = TopoDS::Face(faces(i));
                if (!isFlat(face) || outwardNormal(face).Dot(unit) < 1.0 - 1e-9 ||
                    std::abs(along(faceCentre(face), axis, sign) - position) > 1e-6) {
                    continue;
                }
                BRepPrimAPI_MakePrism prism(face, gp_Vec(unit.XYZ() * distance));
                tools.push_back(toolOf(prism.Shape()));
            }
            if (tools.size() == 1) {
                throw GeometryError(GeometryErrorCode::kernelFailure,
                                    "stretch: no cross-section found at the cut");
            }
        }
        return booleanNamed(BooleanOp::merge, fixed, tools);
    } catch (const Standard_Failure&) {
        throw GeometryError(GeometryErrorCode::kernelFailure, "stretch: the kernel failed");
    }
}

} // namespace fission::geo::occt
