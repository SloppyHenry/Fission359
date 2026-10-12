// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Smoke tests: prove that OpenCASCADE is available and works. They intentionally use the
// library directly; product code reaches OpenCASCADE only through src/geometry/occt.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <GProp_GProps.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>

namespace {

double volumeOf(const TopoDS_Shape& shape) {
    GProp_GProps properties;
    BRepGProp::VolumeProperties(shape, properties);
    return properties.Mass();
}

} // namespace

TEST_CASE("OpenCASCADE builds a box with the right volume", "[smoke][occt]") {
    const TopoDS_Shape box = BRepPrimAPI_MakeBox(10.0, 20.0, 30.0).Shape();

    CHECK(BRepCheck_Analyzer(box).IsValid());
    CHECK(volumeOf(box) == Catch::Approx(6000.0).epsilon(1e-9));
}

TEST_CASE("OpenCASCADE cuts a hole into a box", "[smoke][occt]") {
    const TopoDS_Shape box = BRepPrimAPI_MakeBox(20.0, 20.0, 10.0).Shape();
    const gp_Ax2 axis(gp_Pnt(10.0, 10.0, -1.0), gp_Dir(0.0, 0.0, 1.0));
    const TopoDS_Shape cylinder = BRepPrimAPI_MakeCylinder(axis, 5.0, 12.0).Shape();

    BRepAlgoAPI_Cut cut(box, cylinder);
    REQUIRE(cut.IsDone());

    const double expected = 20.0 * 20.0 * 10.0 - 3.14159265358979323846 * 5.0 * 5.0 * 10.0;
    CHECK(BRepCheck_Analyzer(cut.Shape()).IsValid());
    CHECK(volumeOf(cut.Shape()) == Catch::Approx(expected).epsilon(1e-6));
}
