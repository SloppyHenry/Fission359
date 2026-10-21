// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Combining bodies through the geometry layer (AP-1.7a, ADR-0011): golden values (volume, area,
// box, topology, validity) and the naming contract (every face keeps the role of its source).

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <map>
#include <numbers>
#include <set>
#include <string>

#include "fission/geo/error.h"
#include "fission/geo/occt/solids.h"

using Catch::Approx;
using namespace fission::geo;

namespace {

constexpr double kPi = std::numbers::pi;

Approx exactly(double expected) {
    return Approx(expected).epsilon(1e-6);
}

// Roles as a step passes them on: "F1/box/top".
NamedSolid called(NamedSolid solid, const std::string& prefix) {
    for (NamedFace& face : solid.faces) {
        face.role = prefix + "/" + face.role;
    }
    return solid;
}

NamedSolid plate() {
    return called(occt::makeNamedBox(40.0, 40.0, 10.0, {-20.0, -20.0, 0.0}), "F1/box");
}

NamedSolid pin(double radius, double height, double x, double z) {
    return called(occt::makeNamedCylinder(radius, height, {x, 0.0, z}), "F2/cylinder");
}

std::set<std::string> rolesOf(const NamedSolid& solid) {
    std::set<std::string> roles;
    for (const NamedFace& face : solid.faces) {
        roles.insert(face.role);
    }
    return roles;
}

void checkNamed(const NamedSolid& solid) {
    const SolidReport report = occt::measure(solid.solid);
    CHECK(report.valid);
    REQUIRE(solid.faces.size() == static_cast<std::size_t>(report.faces));
    std::set<std::uint32_t> ids;
    std::set<std::string> roles;
    for (const NamedFace& face : solid.faces) {
        CHECK(ids.insert(face.faceId).second);
        CHECK(roles.insert(face.role).second);
    }
}

} // namespace

TEST_CASE("boolean: a hole through a plate", "[boolean][golden]") {
    const NamedSolid cut =
        occt::booleanNamed(BooleanOp::subtract, plate(), {pin(3.0, 20.0, 0.0, -5.0)});
    const SolidReport report = occt::measure(cut.solid);
    CHECK(report.valid);
    CHECK(report.solids == 1);
    CHECK(report.faces == 7);
    CHECK(report.volume == exactly(16000.0 - kPi * 9.0 * 10.0));
    CHECK(report.area ==
          exactly(2.0 * 1600.0 - 2.0 * kPi * 9.0 + 4.0 * 400.0 + 2.0 * kPi * 3.0 * 10.0));
    CHECK(report.boxMin.x == Approx(-20.0));
    CHECK(report.boxMax.z == Approx(10.0));
    checkNamed(cut);

    // The six plate faces keep their names, the wall of the hole is named after the pin.
    const auto roles = rolesOf(cut);
    for (const char* face : {"top", "bottom", "front", "back", "left", "right"}) {
        CHECK(roles.count(std::string("F1/box/") + face) == 1);
    }
    CHECK(roles.count("F2/cylinder/side") == 1);
}

TEST_CASE("boolean: merging overlapping boxes", "[boolean][golden]") {
    const NamedSolid other =
        called(occt::makeNamedBox(40.0, 40.0, 10.0, {10.0, -20.0, 0.0}), "F2/box");
    const NamedSolid merged = occt::booleanNamed(BooleanOp::merge, plate(), {other});
    const SolidReport report = occt::measure(merged.solid);
    CHECK(report.valid);
    CHECK(report.solids == 1);
    CHECK(report.volume == exactly(70.0 * 40.0 * 10.0));
    CHECK(report.faces == 6); // equal-surface faces were joined into one
    checkNamed(merged);
    // The joined top carries the name of the earlier input (the target).
    const auto roles = rolesOf(merged);
    CHECK(roles.count("F1/box/top") == 1);
    CHECK(roles.count("F2/box/top") == 0);
    CHECK(roles.count("F2/box/right") == 1);
}

TEST_CASE("boolean: parts that do not touch stay one body", "[boolean][golden]") {
    const NamedSolid far =
        called(occt::makeNamedBox(10.0, 10.0, 10.0, {100.0, 0.0, 0.0}), "F2/box");
    const NamedSolid merged = occt::booleanNamed(BooleanOp::merge, plate(), {far});
    const SolidReport report = occt::measure(merged.solid);
    CHECK(report.solids == 2);
    CHECK(report.volume == exactly(16000.0 + 1000.0));
    checkNamed(merged);
}

TEST_CASE("boolean: intersect keeps the overlap", "[boolean][golden]") {
    const NamedSolid other =
        called(occt::makeNamedBox(40.0, 40.0, 10.0, {10.0, -20.0, 5.0}), "F2/box");
    const NamedSolid common = occt::booleanNamed(BooleanOp::intersect, plate(), {other});
    const SolidReport report = occt::measure(common.solid);
    CHECK(report.valid);
    CHECK(report.volume == exactly(10.0 * 40.0 * 5.0));
    CHECK(report.faces == 6);
    checkNamed(common);
}

TEST_CASE("boolean: a tool that misses the target", "[boolean][golden]") {
    const NamedSolid away =
        called(occt::makeNamedBox(10.0, 10.0, 10.0, {100.0, 0.0, 0.0}), "F2/box");

    const NamedSolid same = occt::booleanNamed(BooleanOp::subtract, plate(), {away});
    CHECK(occt::measure(same.solid).volume == exactly(16000.0));
    CHECK(rolesOf(same) == rolesOf(plate()));
    checkNamed(same);

    try {
        occt::booleanNamed(BooleanOp::intersect, plate(), {away});
        FAIL("an empty overlap must be reported");
    } catch (const GeometryError& error) {
        CHECK(error.code() == GeometryErrorCode::emptyResult);
    }
}

TEST_CASE("boolean: cutting everything away is an empty result", "[boolean]") {
    const NamedSolid big =
        called(occt::makeNamedBox(100.0, 100.0, 100.0, {-50.0, -50.0, -50.0}), "F2/box");
    try {
        occt::booleanNamed(BooleanOp::subtract, plate(), {big});
        FAIL("nothing is left");
    } catch (const GeometryError& error) {
        CHECK(error.code() == GeometryErrorCode::emptyResult);
    }
}

TEST_CASE("boolean: a slot that cuts a face in two numbers the pieces", "[boolean][naming]") {
    // A bar across the whole plate, lifting out the middle of the top face's strip.
    const NamedSolid slot =
        called(occt::makeNamedBox(10.0, 60.0, 5.0, {-5.0, -30.0, 5.0}), "F2/box");
    const NamedSolid cut = occt::booleanNamed(BooleanOp::subtract, plate(), {slot});
    const SolidReport report = occt::measure(cut.solid);
    CHECK(report.valid);
    CHECK(report.volume == exactly(16000.0 - 10.0 * 40.0 * 5.0));
    checkNamed(cut);
    const auto roles = rolesOf(cut);
    CHECK(roles.count("F1/box/top#1") == 1);
    CHECK(roles.count("F1/box/top#2") == 1);
    CHECK(roles.count("F1/box/top") == 0);
}

TEST_CASE("boolean: a tool with no role for a face is refused", "[boolean]") {
    NamedSolid broken = pin(3.0, 20.0, 0.0, -5.0);
    broken.faces.pop_back();
    CHECK_THROWS_AS(occt::booleanNamed(BooleanOp::subtract, plate(), {broken}), GeometryError);
    CHECK_THROWS_AS(occt::booleanNamed(BooleanOp::subtract, plate(), {}), GeometryError);
}

TEST_CASE("translate: a moved body keeps its faces and their names", "[solid][golden]") {
    const NamedSolid moved = occt::translateNamed(plate(), {5.0, -7.0, 3.0});
    const SolidReport report = occt::measure(moved.solid);
    CHECK(report.valid);
    CHECK(report.volume == exactly(16000.0));
    CHECK(report.boxMin.x == Approx(-15.0));
    CHECK(report.boxMin.y == Approx(-27.0));
    CHECK(report.boxMin.z == Approx(3.0));
    CHECK(rolesOf(moved) == rolesOf(plate()));
    checkNamed(moved);
    CHECK_THROWS_AS(occt::translateNamed(plate(), {std::nan(""), 0.0, 0.0}), GeometryError);
}

// ---------------------------------------------------------------------------------------------
// The combinations of the kernel spike (AP-0.5, tests/geometry/test_booleans.cpp, A1 - A10) through
// the geometry layer: each gives a valid body with the expected numbers, or a GeometryError that
// the program can explain. A body that is invalid or has a wrong number of names never comes out.

namespace {

NamedSolid box(double sx, double sy, double sz, Point3d corner, const char* name) {
    return called(occt::makeNamedBox(sx, sy, sz, corner), name);
}

NamedSolid cylinder(double radius, double height, Point3d base, const char* name) {
    return called(occt::makeNamedCylinder(radius, height, base), name);
}

// Runs `combine`; true when it gave a body (checked: valid, named), false when it refused with a
// GeometryError.
template <typename F> bool givesValidOrRefuses(F&& combine, SolidReport* out = nullptr) {
    try {
        const NamedSolid result = combine();
        checkNamed(result);
        if (out != nullptr) {
            *out = occt::measure(result.solid);
        }
        return true;
    } catch (const GeometryError& error) {
        CHECK(!std::string(error.what()).empty());
        return false;
    }
}

} // namespace

TEST_CASE("spike A1: two boxes that share a whole face merge into one box", "[boolean][spike]") {
    const NamedSolid a = box(20, 20, 20, {0, 0, 0}, "F1/box");
    const NamedSolid b = box(20, 20, 20, {20, 0, 0}, "F2/box");
    const NamedSolid merged = occt::booleanNamed(BooleanOp::merge, a, {b});
    const SolidReport report = occt::measure(merged.solid);
    CHECK(report.valid);
    CHECK(report.solids == 1);
    CHECK(report.faces == 6);
    CHECK(report.volume == exactly(16000.0));
    checkNamed(merged);
}

TEST_CASE("spike A3: a hole that only touches the outer wall", "[boolean][spike]") {
    SolidReport report;
    if (givesValidOrRefuses(
            [&] {
                return occt::booleanNamed(BooleanOp::subtract, plate(),
                                          {cylinder(3.0, 20.0, {17.0, 0.0, -5.0}, "F2/cyl")});
            },
            &report)) {
        CHECK(report.valid);
        CHECK(report.volume == exactly(16000.0 - kPi * 9.0 * 10.0));
    }
}

TEST_CASE("spike A4/A5: coaxial cylinders of the same radius", "[boolean][spike]") {
    const NamedSolid lower = cylinder(10.0, 10.0, {0, 0, 0}, "F1/cyl");
    const NamedSolid upper = cylinder(10.0, 10.0, {0, 0, 10.0}, "F2/cyl");
    const NamedSolid merged = occt::booleanNamed(BooleanOp::merge, lower, {upper});
    const SolidReport united = occt::measure(merged.solid);
    CHECK(united.valid);
    CHECK(united.solids == 1);
    CHECK(united.faces == 3); // the walls of the two were joined
    CHECK(united.volume == exactly(kPi * 100.0 * 20.0));
    checkNamed(merged);

    const NamedSolid tall = cylinder(10.0, 20.0, {0, 0, 0}, "F1/cyl");
    const NamedSolid inner = cylinder(10.0, 10.0, {0, 0, 5.0}, "F2/cyl");
    SolidReport cut;
    if (givesValidOrRefuses([&] { return occt::booleanNamed(BooleanOp::subtract, tall, {inner}); },
                            &cut)) {
        CHECK(cut.valid);
        CHECK(cut.volume == exactly(kPi * 100.0 * 10.0));
    }
}

TEST_CASE("spike A6: a grid of 100 holes in one cut", "[boolean][spike]") {
    const NamedSolid slab = called(occt::makeNamedBox(120.0, 120.0, 5.0, {0, 0, 0}), "F1/box");
    std::vector<NamedSolid> holes;
    for (int row = 0; row < 10; ++row) {
        for (int column = 0; column < 10; ++column) {
            holes.push_back(called(
                occt::makeNamedCylinder(2.0, 20.0, {10.0 + 11.0 * column, 10.0 + 11.0 * row, -5.0}),
                ("F" + std::to_string(2 + row * 10 + column) + "/cylinder").c_str()));
        }
    }
    const NamedSolid cut = occt::booleanNamed(BooleanOp::subtract, slab, holes);
    const SolidReport report = occt::measure(cut.solid);
    CHECK(report.valid);
    CHECK(report.solids == 1);
    CHECK(report.faces == 6 + 100);
    CHECK(report.volume == exactly(120.0 * 120.0 * 5.0 - 100.0 * kPi * 4.0 * 5.0));
    checkNamed(cut);
}

TEST_CASE("spike A7: two boxes that touch only along an edge", "[boolean][spike]") {
    const NamedSolid a = box(10, 10, 10, {0, 0, 0}, "F1/box");
    const NamedSolid b = box(10, 10, 10, {10, 10, 0}, "F2/box");
    SolidReport report;
    if (givesValidOrRefuses([&] { return occt::booleanNamed(BooleanOp::merge, a, {b}); },
                            &report)) {
        CHECK(report.valid);
        CHECK(report.volume == exactly(2000.0));
    }
}

TEST_CASE("spike A8: a tiny hole in a large plate", "[boolean][spike]") {
    const NamedSolid big = called(occt::makeNamedBox(200.0, 200.0, 5.0, {0, 0, 0}), "F1/box");
    const NamedSolid tiny = cylinder(0.025, 20.0, {100.0, 100.0, -5.0}, "F2/cyl");
    SolidReport report;
    if (givesValidOrRefuses([&] { return occt::booleanNamed(BooleanOp::subtract, big, {tiny}); },
                            &report)) {
        CHECK(report.valid);
        CHECK(report.faces == 7);
        CHECK(report.volume == exactly(200.0 * 200.0 * 5.0 - kPi * 0.025 * 0.025 * 5.0));
    }
}

TEST_CASE("spike A9/A10: boxes that overlap or part by a hair", "[boolean][spike]") {
    const NamedSolid a = box(10, 10, 10, {0, 0, 0}, "F1/box");

    // Overlapping by 0.00001 mm: a valid body or a message, never an invalid body.
    const NamedSolid hairOverlap = box(10, 10, 10, {9.99999, 0, 0}, "F2/box");
    givesValidOrRefuses([&] { return occt::booleanNamed(BooleanOp::merge, a, {hairOverlap}); });

    // Parted by 0.001 mm: not joined, two bodies in one result.
    const NamedSolid apart = box(10, 10, 10, {10.001, 0, 0}, "F3/box");
    SolidReport report;
    if (givesValidOrRefuses([&] { return occt::booleanNamed(BooleanOp::merge, a, {apart}); },
                            &report)) {
        CHECK(report.solids == 2);
        CHECK(report.volume == exactly(2000.0));
    }
}
