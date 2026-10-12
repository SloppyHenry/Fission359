// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Kernel robustness catalog, group D: text outlines, embossed on and cut into a plate.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "support.h"
#include <BRepPrimAPI_MakePrism.hxx>
#include <NCollection_String.hxx>
#include <StdPrs_BRepFont.hxx>
#include <StdPrs_BRepTextBuilder.hxx>
#include <gp_Ax3.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

using Catch::Approx;
using namespace fission::test::geo;

namespace {

// Outlines of `text` (UTF-8) as flat faces in the plane z = 0, lower left corner at (x, y).
TopoDS_Shape textOutline(const char* text, double size, double x, double y) {
    StdPrs_BRepFont font;
    const bool loaded = font.Init(NCollection_String(testFontPath().c_str()), size, 0);
    REQUIRE(loaded);
    StdPrs_BRepTextBuilder builder;
    const gp_Ax3 pen(gp_Pnt(x, y, 0.0), gp_Dir(0.0, 0.0, 1.0), gp_Dir(1.0, 0.0, 0.0));
    return builder.Perform(font, NCollection_String(text), pen);
}

// The outline pulled up into a body from z = z0 to z0 + height.
TopoDS_Shape textBody(const char* text, double size, double x, double y, double z0, double height) {
    TopoDS_Shape outline = textOutline(text, size, x, y);
    gp_Trsf lift;
    lift.SetTranslation(gp_Vec(0.0, 0.0, z0));
    outline.Move(TopLoc_Location(lift));
    return BRepPrimAPI_MakePrism(outline, gp_Vec(0.0, 0.0, height)).Shape();
}

} // namespace

TEST_CASE("D1 emboss text on a plate", "[geometry][text]") {
    const TopoDS_Shape plate = makeBox(80, 30, 3);
    const TopoDS_Shape letters = textBody("Hallo Welt", 10.0, 5.0, 10.0, 3.0, 1.5);
    const ShapeStats text = measure(letters);
    const BoolResult result = fuse(plate, letters);
    REQUIRE(result.done);
    const ShapeStats stats = measure(result.shape);

    note("text faces", text.faces);
    note("result faces", stats.faces);
    CHECK(text.valid);
    CHECK(stats.valid);
    CHECK(stats.volume == Approx(80.0 * 30.0 * 3.0 + text.volume).epsilon(1e-6));
    CHECK(stats.solids == 1);
}

TEST_CASE("D2 cut text into a plate", "[geometry][text]") {
    const TopoDS_Shape plate = makeBox(80, 30, 3);
    const TopoDS_Shape letters = textBody("Hallo Welt", 10.0, 5.0, 10.0, 1.5, 3.0);
    const BoolResult result = cut(plate, letters);
    REQUIRE(result.done);
    const ShapeStats stats = measure(result.shape);

    // The outline area times the depth that lies inside the plate (1.5 mm).
    const double outlineArea = measure(textOutline("Hallo Welt", 10.0, 5.0, 10.0)).area;
    note("outline area", outlineArea);
    CHECK(stats.valid);
    CHECK(stats.volume == Approx(80.0 * 30.0 * 3.0 - outlineArea * 1.5).epsilon(1e-6));
    CHECK(stats.solids == 1);
}

TEST_CASE("D3 German umlauts and sharp s", "[geometry][text]") {
    // Twelve letters; seven of them carry two dots, which are separate faces.
    const TopoDS_Shape outline = textOutline("Gr\xC3\xB6\xC3\x9F"
                                             "e \xC3\xA4\xC3\xB6\xC3\xBC\xC3\x9F "
                                             "\xC3\x84\xC3\x96\xC3\x9C",
                                             10.0, 0.0, 0.0);
    const ShapeStats stats = measure(outline);

    note("faces", stats.faces);
    note("width", stats.sizeX);
    CHECK(stats.faces >= 12 + 7 * 2);
    CHECK(stats.area > 0.0);
    CHECK(measure(BRepPrimAPI_MakePrism(outline, gp_Vec(0.0, 0.0, 2.0)).Shape()).valid);
}
