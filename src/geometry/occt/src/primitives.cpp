// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The basic shapes of AP-1.5 (everything in primitives.h except the box and the cylinder).

#include <cmath>
#include <initializer_list>
#include <numbers>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include "fission/geo/error.h"
#include "fission/geo/occt/solids.h"

#include "occt_build.h"
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRepPrimAPI_MakeTorus.hxx>
#include <BRepPrimAPI_MakeWedge.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <gp_Pln.hxx>
#include <gp_Vec.hxx>

namespace fission::geo::occt {

namespace {

constexpr double kPi = std::numbers::pi;
constexpr int kMaxCorners = 64;

[[noreturn]] void refuse(const char* what, const char* why) {
    throw GeometryError(GeometryErrorCode::invalidInput, std::string(what) + ": " + why);
}

void requirePositive(const char* what, std::initializer_list<double> values) {
    for (const double value : values) {
        if (!isPositiveFinite(value)) {
            refuse(what, "sizes must be positive numbers");
        }
    }
}

void requireCorners(const char* what, int count) {
    if (count < 3 || count > kMaxCorners) {
        refuse(what, "the number of corners must be between 3 and 64");
    }
}

gp_Ax2 upright(const Point3d& at) {
    return gp_Ax2(gp_Pnt(at.x, at.y, at.z), gp_Dir(0.0, 0.0, 1.0));
}

// Names the faces of a polygon pulled up: flat top and bottom, and `side<n>` for the side that
// stands on the n-th edge of the outline (1-based).
NamedSolid nameUpright(const TopoDS_Shape& shape, const char* what,
                       const std::vector<gp_Pnt>& outline) {
    return nameFaces(shape, what, [&](const TopoDS_Face& face) -> std::string {
        const gp_Dir normal = outwardNormal(face);
        if (normal.Z() > 0.5) {
            return "top";
        }
        if (normal.Z() < -0.5) {
            return "bottom";
        }
        // The edge of the outline nearest to the middle of the face is the one it stands on.
        const gp_Pnt centre = faceCentre(face);
        std::size_t nearest = 0;
        double best = 1e300;
        for (std::size_t i = 0; i < outline.size(); ++i) {
            const gp_Pnt& a = outline[i];
            const gp_Pnt& b = outline[(i + 1) % outline.size()];
            const double dx = centre.X() - (a.X() + b.X()) / 2.0;
            const double dy = centre.Y() - (a.Y() + b.Y()) / 2.0;
            const double distance = dx * dx + dy * dy;
            if (distance < best) {
                best = distance;
                nearest = i;
            }
        }
        return "side" + std::to_string(nearest + 1);
    });
}

// Polygon (corners counter-clockwise seen from above) pulled up by `height`.
TopoDS_Shape polygonBody(const std::vector<gp_Pnt>& outline, double height) {
    BRepBuilderAPI_MakePolygon polygon;
    for (const gp_Pnt& corner : outline) {
        polygon.Add(corner);
    }
    polygon.Close();
    const TopoDS_Face face = BRepBuilderAPI_MakeFace(polygon.Wire(), /*OnlyPlane=*/true).Face();
    return BRepPrimAPI_MakePrism(face, gp_Vec(0.0, 0.0, height)).Shape();
}

NamedSolid makeSphere(const SphereSpec& spec, const Point3d& base) {
    requirePositive("sphere", {spec.radius});
    const Point3d centre{base.x, base.y, base.z + spec.radius};
    const TopoDS_Shape shape = BRepPrimAPI_MakeSphere(upright(centre), spec.radius).Shape();
    return nameFaces(shape, "sphere", [](const TopoDS_Face&) { return std::string("surface"); });
}

NamedSolid makeHemisphere(const HemisphereSpec& spec, const Point3d& base) {
    requirePositive("hemisphere", {spec.radius});
    const TopoDS_Shape shape =
        BRepPrimAPI_MakeSphere(upright(base), spec.radius, 0.0, kPi / 2.0).Shape();
    return nameFaces(shape, "hemisphere", [](const TopoDS_Face& face) -> std::string {
        return isFlat(face) ? "bottom" : "surface";
    });
}

NamedSolid makeCone(const ConeSpec& spec, const Point3d& base) {
    requirePositive("cone", {spec.radius, spec.height});
    if (!std::isfinite(spec.topRadius) || spec.topRadius < 0.0) {
        refuse("cone", "the top radius must be zero or positive");
    }
    const bool equal = std::abs(spec.radius - spec.topRadius) <= 1e-12 * spec.radius;
    // Same radius at both ends is a cylinder; the cone maker does not take it.
    const TopoDS_Shape shape =
        equal
            ? BRepPrimAPI_MakeCylinder(upright(base), spec.radius, spec.height).Shape()
            : BRepPrimAPI_MakeCone(upright(base), spec.radius, spec.topRadius, spec.height).Shape();
    return nameFaces(shape, "cone", [&](const TopoDS_Face& face) -> std::string {
        if (!isFlat(face)) {
            return "side";
        }
        return faceCentre(face).Z() < base.z + spec.height / 2.0 ? "bottom" : "top";
    });
}

NamedSolid makeTorus(const TorusSpec& spec, const Point3d& base) {
    requirePositive("torus", {spec.radius, spec.tubeRadius});
    if (!(spec.tubeRadius < spec.radius)) {
        refuse("torus", "the tube must be thinner than the distance to the axis");
    }
    const Point3d centre{base.x, base.y, base.z + spec.tubeRadius};
    const TopoDS_Shape shape =
        BRepPrimAPI_MakeTorus(upright(centre), spec.radius, spec.tubeRadius).Shape();
    return nameFaces(shape, "torus", [](const TopoDS_Face&) { return std::string("surface"); });
}

NamedSolid makeTube(const TubeSpec& spec, const Point3d& base) {
    requirePositive("tube", {spec.radius, spec.wall, spec.height});
    if (!(spec.wall < spec.radius)) {
        refuse("tube", "the wall must be thinner than the radius");
    }
    const gp_Ax2 axis = upright(base);
    const TopoDS_Wire outer =
        BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(gp_Circ(axis, spec.radius)).Edge()).Wire();
    const TopoDS_Wire inner =
        BRepBuilderAPI_MakeWire(
            BRepBuilderAPI_MakeEdge(gp_Circ(axis, spec.radius - spec.wall)).Edge())
            .Wire();
    BRepBuilderAPI_MakeFace ring(gp_Pln(axis), outer);
    ring.Add(TopoDS::Wire(inner.Reversed()));
    const TopoDS_Shape shape =
        BRepPrimAPI_MakePrism(ring.Face(), gp_Vec(0.0, 0.0, spec.height)).Shape();
    const double middle = spec.radius - spec.wall / 2.0;
    return nameFaces(shape, "tube", [&](const TopoDS_Face& face) -> std::string {
        if (isFlat(face)) {
            return faceCentre(face).Z() < base.z + spec.height / 2.0 ? "bottom" : "top";
        }
        return BRepAdaptor_Surface(face).Cylinder().Radius() > middle ? "outer" : "inner";
    });
}

NamedSolid makePrism(const PrismSpec& spec, const Point3d& base) {
    requirePositive("prism", {spec.radius, spec.height});
    requireCorners("prism", spec.sides);
    // One flat side faces front (−Y); the corners run counter-clockwise from above.
    std::vector<gp_Pnt> outline;
    for (int k = 0; k < spec.sides; ++k) {
        const double angle = -kPi / 2.0 - kPi / spec.sides + 2.0 * kPi * k / spec.sides;
        outline.emplace_back(base.x + spec.radius * std::cos(angle),
                             base.y + spec.radius * std::sin(angle), base.z);
    }
    return nameUpright(polygonBody(outline, spec.height), "prism", outline);
}

NamedSolid makeStar(const StarSpec& spec, const Point3d& base) {
    requirePositive("star", {spec.radius, spec.innerRadius, spec.height});
    requireCorners("star", spec.points);
    if (!(spec.innerRadius < spec.radius)) {
        refuse("star", "the notches must lie closer to the middle than the tips");
    }
    // One tip points to the back (+Y); tips and notches alternate counter-clockwise.
    std::vector<gp_Pnt> outline;
    for (int k = 0; k < 2 * spec.points; ++k) {
        const double angle = kPi / 2.0 + kPi * k / spec.points;
        const double reach = k % 2 == 0 ? spec.radius : spec.innerRadius;
        outline.emplace_back(base.x + reach * std::cos(angle), base.y + reach * std::sin(angle),
                             base.z);
    }
    return nameUpright(polygonBody(outline, spec.height), "star", outline);
}

NamedSolid makePyramid(const PyramidSpec& spec, const Point3d& base) {
    requirePositive("pyramid", {spec.sizeX, spec.sizeY, spec.height});
    // The wedge maker stands on its Y axis. Turn it so that its height runs along our Z.
    const gp_Ax2 axis(gp_Pnt(base.x - spec.sizeX / 2.0, base.y + spec.sizeY / 2.0, base.z),
                      gp_Dir(0.0, -1.0, 0.0), gp_Dir(1.0, 0.0, 0.0));
    const TopoDS_Shape shape =
        BRepPrimAPI_MakeWedge(axis, spec.sizeX, spec.height, spec.sizeY, spec.sizeX / 2.0,
                              spec.sizeY / 2.0, spec.sizeX / 2.0, spec.sizeY / 2.0)
            .Shape();
    return nameFaces(shape, "pyramid", [&](const TopoDS_Face& face) -> std::string {
        if (outwardNormal(face).Z() < -0.5) {
            return "bottom";
        }
        // Slanted sides: told apart by where their middle lies, measured in lengths of the
        // sides so that a long, thin pyramid is named right too.
        const gp_Pnt centre = faceCentre(face);
        const double across = (centre.X() - base.x) / spec.sizeX;
        const double along = (centre.Y() - base.y) / spec.sizeY;
        if (std::abs(across) > std::abs(along)) {
            return across < 0.0 ? "left" : "right";
        }
        return along < 0.0 ? "front" : "back";
    });
}

NamedSolid makeWedge(const WedgeSpec& spec, const Point3d& base) {
    requirePositive("wedge", {spec.sizeX, spec.sizeY, spec.height});
    // The side view (a right triangle: low at the front, high at the back) pulled along X.
    const double x0 = base.x - spec.sizeX / 2.0;
    const double y0 = base.y - spec.sizeY / 2.0;
    const double y1 = base.y + spec.sizeY / 2.0;
    BRepBuilderAPI_MakePolygon triangle;
    triangle.Add(gp_Pnt(x0, y0, base.z));
    triangle.Add(gp_Pnt(x0, y1, base.z));
    triangle.Add(gp_Pnt(x0, y1, base.z + spec.height));
    triangle.Close();
    const TopoDS_Face face = BRepBuilderAPI_MakeFace(triangle.Wire(), /*OnlyPlane=*/true).Face();
    const TopoDS_Shape shape = BRepPrimAPI_MakePrism(face, gp_Vec(spec.sizeX, 0.0, 0.0)).Shape();
    return nameFaces(shape, "wedge", [](const TopoDS_Face& side) -> std::string {
        const gp_Dir normal = outwardNormal(side);
        if (normal.X() > 0.5) {
            return "right";
        }
        if (normal.X() < -0.5) {
            return "left";
        }
        if (normal.Z() < -0.5) {
            return "bottom";
        }
        if (normal.Y() > 0.5) {
            return "back";
        }
        return "slope";
    });
}

} // namespace

NamedSolid makeNamedPrimitive(const PrimitiveSpec& spec, Point3d base) {
    if (!isFinite(base)) {
        throw GeometryError(GeometryErrorCode::invalidInput, "shape: the position must be finite");
    }
    try {
        return std::visit(
            [&base](const auto& shape) -> NamedSolid {
                using Shape = std::decay_t<decltype(shape)>;
                if constexpr (std::is_same_v<Shape, SphereSpec>) {
                    return makeSphere(shape, base);
                } else if constexpr (std::is_same_v<Shape, HemisphereSpec>) {
                    return makeHemisphere(shape, base);
                } else if constexpr (std::is_same_v<Shape, ConeSpec>) {
                    return makeCone(shape, base);
                } else if constexpr (std::is_same_v<Shape, TorusSpec>) {
                    return makeTorus(shape, base);
                } else if constexpr (std::is_same_v<Shape, TubeSpec>) {
                    return makeTube(shape, base);
                } else if constexpr (std::is_same_v<Shape, PrismSpec>) {
                    return makePrism(shape, base);
                } else if constexpr (std::is_same_v<Shape, PyramidSpec>) {
                    return makePyramid(shape, base);
                } else if constexpr (std::is_same_v<Shape, WedgeSpec>) {
                    return makeWedge(shape, base);
                } else {
                    return makeStar(shape, base);
                }
            },
            spec);
    } catch (const Standard_Failure&) {
        throw GeometryError(GeometryErrorCode::kernelFailure, "shape: the kernel failed");
    }
}

} // namespace fission::geo::occt
