// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Kernel robustness catalog, group E: threads (helix sweep) and transitions between profiles.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>

#include "support.h"
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepLib.hxx>
#include <BRepOffsetAPI_MakePipeShell.hxx>
#include <BRepOffsetAPI_ThruSections.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <Geom2d_Line.hxx>
#include <Geom_CylindricalSurface.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS.hxx>
#include <gp_Ax2.hxx>
#include <gp_Ax3.hxx>
#include <gp_Circ.hxx>
#include <gp_Dir.hxx>
#include <gp_Dir2d.hxx>
#include <gp_Pnt.hxx>
#include <gp_Pnt2d.hxx>

using Catch::Approx;
using namespace fission::test::geo;

namespace {

constexpr double kPi = std::numbers::pi;

// Metric-like thread: trapezoid ridge on a core of radius 3.9, ridge tip at radius 4.8,
// pitch 1.25 mm. The ridge starts slightly inside the core so that the union is solid.
struct Thread {
    double coreRadius = 3.9;
    double tipRadius = 4.8;
    double baseWidth = 1.0;
    double tipWidth = 0.1;
    double pitch = 1.25;
    int turns = 8;
};

// Area and centroid radius of the trapezoid profile (Pappus: V = area * 2 pi * radius * turns).
double threadVolume(const Thread& t) {
    const double height = t.tipRadius - t.coreRadius;
    const double area = 0.5 * (t.baseWidth + t.tipWidth) * height;
    const double centroid = t.coreRadius + height * (t.baseWidth + 2.0 * t.tipWidth) /
                                               (3.0 * (t.baseWidth + t.tipWidth));
    return area * 2.0 * kPi * centroid * t.turns;
}

// Builds the thread ridge as a solid by sweeping the profile along a helix on a cylinder.
TopoDS_Shape makeThread(const Thread& t) {
    const double midRadius = 0.5 * (t.coreRadius + t.tipRadius);
    const occ::handle<Geom_CylindricalSurface> surface =
        new Geom_CylindricalSurface(gp_Ax3(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)), midRadius);
    const occ::handle<Geom2d_Line> line =
        new Geom2d_Line(gp_Pnt2d(0.0, 0.0), gp_Dir2d(2.0 * kPi, t.pitch));
    const double length = t.turns * std::hypot(2.0 * kPi, t.pitch);
    TopoDS_Edge helix = BRepBuilderAPI_MakeEdge(line, surface, 0.0, length);
    BRepLib::BuildCurves3d(helix);
    const TopoDS_Wire spine = BRepBuilderAPI_MakeWire(helix);

    // Profile in the half-plane through the axis, at the start of the helix (x > 0, y = 0).
    BRepBuilderAPI_MakePolygon polygon;
    polygon.Add(gp_Pnt(t.coreRadius, 0, -0.5 * t.baseWidth));
    polygon.Add(gp_Pnt(t.tipRadius, 0, -0.5 * t.tipWidth));
    polygon.Add(gp_Pnt(t.tipRadius, 0, 0.5 * t.tipWidth));
    polygon.Add(gp_Pnt(t.coreRadius, 0, 0.5 * t.baseWidth));
    polygon.Close();

    BRepOffsetAPI_MakePipeShell pipe(spine);
    pipe.SetMode(gp_Dir(0.0, 0.0, 1.0)); // fixed binormal: the profile keeps its attitude
    pipe.Add(polygon.Wire());
    pipe.Build();
    if (!pipe.IsDone() || !pipe.MakeSolid()) {
        return {};
    }
    return pipe.Shape();
}

} // namespace

TEST_CASE("E1 thread ridge as a helix sweep", "[geometry][thread]") {
    const Thread thread;
    TopoDS_Shape ridge;
    const double seconds = timed([&] { ridge = makeThread(thread); });
    addKernelSeconds(seconds);
    REQUIRE_FALSE(ridge.IsNull());
    const ShapeStats stats = measure(ridge);

    note("faces", stats.faces);
    note("volume", stats.volume);
    note("expected", threadVolume(thread));
    CHECK(stats.valid);
    CHECK(stats.solids == 1);
    CHECK(stats.volume == Approx(threadVolume(thread)).epsilon(2e-3));
}

TEST_CASE("E2 screw: core and thread united, ends trimmed", "[geometry][thread]") {
    const Thread thread;
    const TopoDS_Shape ridge = makeThread(thread);
    REQUIRE_FALSE(ridge.IsNull());
    const double length = thread.turns * thread.pitch; // 10 mm
    const TopoDS_Shape core = makeCylinder(thread.coreRadius + 0.05, length);

    const BoolResult joined = fuse(core, ridge);
    REQUIRE(joined.done);
    const ShapeStats before = measure(joined.shape);
    note("union valid", before.valid ? "yes" : "no");
    note("union faces", before.faces);

    // The ridge overshoots both ends by half its width: trim to the screw length.
    const BoolResult trimmed = common(joined.shape, makeCylinder(10.0, length));
    REQUIRE(trimmed.done);
    const ShapeStats stats = measure(trimmed.shape);

    note("trimmed faces", stats.faces);
    note("volume", stats.volume);
    CHECK(before.valid);
    CHECK(stats.valid);
    CHECK(stats.solids == 1);
    CHECK(stats.volume > kPi * thread.coreRadius * thread.coreRadius * length);
}

TEST_CASE("E3 nut: hexagon minus a threaded rod", "[geometry][thread]") {
    const Thread thread;
    const TopoDS_Shape ridge = makeThread(thread);
    REQUIRE_FALSE(ridge.IsNull());
    const double length = thread.turns * thread.pitch;
    const BoolResult rod = fuse(makeCylinder(thread.coreRadius + 0.05, length), ridge);
    REQUIRE(rod.done);

    // Hexagonal nut 13 mm across the flats, 6.5 mm high.
    const double corner = 13.0 / (2.0 * std::cos(kPi / 6.0));
    BRepBuilderAPI_MakePolygon hexagon;
    for (int index = 0; index < 6; ++index) {
        const double angle = index * kPi / 3.0;
        hexagon.Add(gp_Pnt(corner * std::cos(angle), corner * std::sin(angle), 1.5));
    }
    hexagon.Close();
    const TopoDS_Shape blank =
        BRepPrimAPI_MakePrism(BRepBuilderAPI_MakeFace(hexagon.Wire()).Face(), gp_Vec(0, 0, 6.5))
            .Shape();

    const BoolResult nut = cut(blank, rod.shape);
    REQUIRE(nut.done);
    const ShapeStats stats = measure(nut.shape);
    const double inside = measure(common(blank, rod.shape).shape).volume;

    note("faces", stats.faces);
    note("volume", stats.volume);
    CHECK(stats.valid);
    CHECK(stats.solids == 1);
    CHECK(stats.volume == Approx(measure(blank).volume - inside).epsilon(1e-6));
}

TEST_CASE("E4 transition from a circle to a square", "[geometry][loft]") {
    const gp_Circ circleGeometry(gp_Ax2(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)), 10.0);
    const TopoDS_Wire circle = BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(circleGeometry));
    BRepBuilderAPI_MakePolygon square;
    square.Add(gp_Pnt(-10, -10, 20));
    square.Add(gp_Pnt(10, -10, 20));
    square.Add(gp_Pnt(10, 10, 20));
    square.Add(gp_Pnt(-10, 10, 20));
    square.Close();

    BRepOffsetAPI_ThruSections loft(/*isSolid=*/true, /*ruled=*/false);
    loft.AddWire(circle);
    loft.AddWire(square.Wire());
    loft.Build();
    REQUIRE(loft.IsDone());
    const ShapeStats stats = measure(loft.Shape());

    note("faces", stats.faces);
    note("volume", stats.volume);
    CHECK(stats.valid);
    CHECK(stats.volume > kPi * 100.0 * 20.0);
    CHECK(stats.volume < 400.0 * 20.0);
}
