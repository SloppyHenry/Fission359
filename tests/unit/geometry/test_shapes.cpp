// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Golden tests for the basic shapes (AP-1.5): volume, area and size against the analytic values,
// the naming contract (every face has exactly one role) and the true roundness of rounded corners.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <map>
#include <numbers>
#include <set>
#include <string>
#include <vector>

#include "fission/geo/error.h"
#include "fission/geo/occt/solids.h"

using Catch::Approx;
using namespace fission::geo;

namespace {

constexpr double kPi = std::numbers::pi;

// Relative tolerance 1e-6, as required for golden tests (docs/03-architektur.md §10).
Approx exactly(double expected) {
    return Approx(expected).epsilon(1e-6);
}

struct Measured {
    NamedSolid made;
    SolidReport report;
};

Measured measured(NamedSolid made) {
    Measured result;
    result.report = occt::measure(made.solid);
    result.made = std::move(made);
    return result;
}

Measured shape(const PrimitiveSpec& spec, Point3d base = {}) {
    return measured(occt::makeNamedPrimitive(spec, base));
}

// Every face has exactly one role, and no two faces share one.
std::set<std::string> rolesOf(const Measured& m) {
    std::set<std::string> roles;
    std::set<std::uint32_t> ids;
    for (const NamedFace& face : m.made.faces) {
        roles.insert(face.role);
        ids.insert(face.faceId);
    }
    CHECK(static_cast<int>(m.made.faces.size()) == m.report.faces);
    CHECK(roles.size() == m.made.faces.size());
    CHECK(ids.size() == m.made.faces.size());
    return roles;
}

// The normal of a flat face, taken from its first triangle.
Point3f faceNormalOf(const TriangleMesh& mesh, std::uint32_t faceId) {
    for (std::size_t t = 0; t < mesh.faceIds.size(); ++t) {
        if (mesh.faceIds[t] == faceId) {
            return mesh.normals[mesh.indices[3 * t]];
        }
    }
    FAIL("face not in the mesh");
    return {};
}

std::uint32_t idOf(const Measured& m, const std::string& role) {
    for (const NamedFace& face : m.made.faces) {
        if (face.role == role) {
            return face.faceId;
        }
    }
    FAIL("no face called " << role);
    return 0;
}

// Signed volume of the mesh: equals the true volume only if the mesh is closed and faces outwards.
double meshVolume(const TriangleMesh& mesh) {
    double volume = 0.0;
    for (std::size_t t = 0; t < mesh.triangleCount(); ++t) {
        const Point3f& a = mesh.positions[mesh.indices[3 * t]];
        const Point3f& b = mesh.positions[mesh.indices[3 * t + 1]];
        const Point3f& c = mesh.positions[mesh.indices[3 * t + 2]];
        volume += (static_cast<double>(a.x) *
                       (static_cast<double>(b.y) * c.z - static_cast<double>(b.z) * c.y) +
                   static_cast<double>(a.y) *
                       (static_cast<double>(b.z) * c.x - static_cast<double>(b.x) * c.z) +
                   static_cast<double>(a.z) *
                       (static_cast<double>(b.x) * c.y - static_cast<double>(b.y) * c.x)) /
                  6.0;
    }
    return volume;
}

GeometryErrorCode codeOf(const PrimitiveSpec& spec) {
    try {
        occt::makeNamedPrimitive(spec);
    } catch (const GeometryError& error) {
        return error.code();
    }
    return GeometryErrorCode::kernelFailure; // not reached when the call throws
}

} // namespace

TEST_CASE("sphere: volume and area are exact, it rests on the plane", "[shape][golden]") {
    const Measured m = shape(SphereSpec{12.0}, {5.0, -3.0, 2.0});
    CHECK(m.report.valid);
    CHECK(m.report.solids == 1);
    CHECK(m.report.volume == exactly(4.0 / 3.0 * kPi * 12.0 * 12.0 * 12.0));
    CHECK(m.report.area == exactly(4.0 * kPi * 12.0 * 12.0));
    CHECK(m.report.boxMin.z == Approx(2.0).margin(1e-9));
    CHECK(m.report.boxMax.z == Approx(26.0).margin(1e-9));
    CHECK(m.report.boxMin.x == Approx(-7.0).margin(1e-9));
    CHECK(rolesOf(m) == std::set<std::string>{"surface"});
}

TEST_CASE("hemisphere: flat side down", "[shape][golden]") {
    const Measured m = shape(HemisphereSpec{10.0}, {0.0, 0.0, 1.0});
    CHECK(m.report.valid);
    CHECK(m.report.volume == exactly(2.0 / 3.0 * kPi * 1000.0));
    CHECK(m.report.area == exactly(3.0 * kPi * 100.0));
    CHECK(m.report.boxMin.z == Approx(1.0).margin(1e-9));
    CHECK(m.report.boxMax.z == Approx(11.0).margin(1e-9));
    CHECK(rolesOf(m) == std::set<std::string>{"bottom", "surface"});

    const TriangleMesh mesh = occt::tessellate(m.made.solid);
    const Point3f bottom = faceNormalOf(mesh, idOf(m, "bottom"));
    CHECK(bottom.z == Approx(-1.0).margin(1e-4));
}

TEST_CASE("cone: pointed and truncated", "[shape][golden]") {
    const double r = 10.0;
    const double h = 24.0;
    SECTION("pointed") {
        const Measured m = shape(ConeSpec{r, 0.0, h});
        CHECK(m.report.valid);
        CHECK(m.report.volume == exactly(kPi * r * r * h / 3.0));
        CHECK(m.report.area == exactly(kPi * r * (r + std::hypot(r, h))));
        CHECK(rolesOf(m) == std::set<std::string>{"bottom", "side"});
    }
    SECTION("truncated") {
        const double top = 4.0;
        const Measured m = shape(ConeSpec{r, top, h});
        CHECK(m.report.valid);
        CHECK(m.report.volume == exactly(kPi * h / 3.0 * (r * r + r * top + top * top)));
        CHECK(m.report.area ==
              exactly(kPi * (r + top) * std::hypot(r - top, h) + kPi * (r * r + top * top)));
        CHECK(rolesOf(m) == std::set<std::string>{"bottom", "side", "top"});
    }
    SECTION("the same radius at both ends is a cylinder") {
        const Measured m = shape(ConeSpec{r, r, h});
        CHECK(m.report.valid);
        CHECK(m.report.volume == exactly(kPi * r * r * h));
        CHECK(rolesOf(m) == std::set<std::string>{"bottom", "side", "top"});
    }
}

TEST_CASE("torus: a ring resting on the plane", "[shape][golden]") {
    const Measured m = shape(TorusSpec{15.0, 5.0}, {0.0, 0.0, 0.0});
    CHECK(m.report.valid);
    CHECK(m.report.volume == exactly(2.0 * kPi * kPi * 15.0 * 25.0));
    CHECK(m.report.area == exactly(4.0 * kPi * kPi * 15.0 * 5.0));
    CHECK(m.report.boxMin.z == Approx(0.0).margin(1e-6));
    CHECK(m.report.boxMax.z == Approx(10.0).margin(1e-6));
    CHECK(m.report.boxMax.x == Approx(20.0).margin(1e-6));
    CHECK(rolesOf(m) == std::set<std::string>{"surface"});
}

TEST_CASE("tube: outer and inner wall", "[shape][golden]") {
    const Measured m = shape(TubeSpec{10.0, 2.0, 30.0});
    CHECK(m.report.valid);
    CHECK(m.report.volume == exactly(kPi * 30.0 * (100.0 - 64.0)));
    CHECK(m.report.area == exactly(2.0 * kPi * 30.0 * (10.0 + 8.0) + 2.0 * kPi * (100.0 - 64.0)));
    CHECK(rolesOf(m) == std::set<std::string>{"bottom", "top", "outer", "inner"});
}

TEST_CASE("prism: one flat side faces front", "[shape][golden]") {
    for (const int n : {3, 4, 5, 6, 8, 12}) {
        INFO("sides " << n);
        const double r = 10.0;
        const double h = 7.0;
        const Measured m = shape(PrismSpec{n, r, h});
        CHECK(m.report.valid);
        CHECK(m.report.volume == exactly(0.5 * n * r * r * std::sin(2.0 * kPi / n) * h));
        CHECK(m.report.faces == n + 2);
        const std::set<std::string> roles = rolesOf(m);
        CHECK(roles.contains("side1"));
        CHECK(roles.contains("top"));
        CHECK(roles.contains("bottom"));

        const TriangleMesh mesh = occt::tessellate(m.made.solid);
        const Point3f front = faceNormalOf(mesh, idOf(m, "side1"));
        CHECK(front.y == Approx(-1.0).margin(1e-4));
        CHECK(front.x == Approx(0.0).margin(1e-4));
        CHECK(meshVolume(mesh) == exactly(m.report.volume));
    }
}

TEST_CASE("star: tips and notches", "[shape][golden]") {
    const double outer = 20.0;
    const double inner = 8.0;
    const double h = 5.0;
    for (const int points : {3, 5, 8}) {
        INFO("points " << points);
        const Measured m = shape(StarSpec{points, outer, inner, h}, {1.0, 2.0, 0.0});
        CHECK(m.report.valid);
        CHECK(m.report.volume == exactly(points * outer * inner * std::sin(kPi / points) * h));
        CHECK(m.report.faces == 2 * points + 2);
        rolesOf(m);
        // One tip points to the back.
        CHECK(m.report.boxMax.y == Approx(2.0 + outer).margin(1e-9));
    }
}

TEST_CASE("pyramid: the tip stands above the middle", "[shape][golden]") {
    const Measured m = shape(PyramidSpec{30.0, 20.0, 12.0}, {4.0, 5.0, 1.0});
    CHECK(m.report.valid);
    CHECK(m.report.volume == exactly(30.0 * 20.0 * 12.0 / 3.0));
    CHECK(m.report.faces == 5);
    CHECK(m.report.boxMin.x == Approx(-11.0).margin(1e-9));
    CHECK(m.report.boxMax.x == Approx(19.0).margin(1e-9));
    CHECK(m.report.boxMin.y == Approx(-5.0).margin(1e-9));
    CHECK(m.report.boxMax.y == Approx(15.0).margin(1e-9));
    CHECK(m.report.boxMin.z == Approx(1.0).margin(1e-9));
    CHECK(m.report.boxMax.z == Approx(13.0).margin(1e-9));
    CHECK(rolesOf(m) == std::set<std::string>{"bottom", "front", "back", "left", "right"});

    const TriangleMesh mesh = occt::tessellate(m.made.solid);
    CHECK(faceNormalOf(mesh, idOf(m, "front")).y < -0.1F);
    CHECK(faceNormalOf(mesh, idOf(m, "back")).y > 0.1F);
    CHECK(faceNormalOf(mesh, idOf(m, "left")).x < -0.1F);
    CHECK(faceNormalOf(mesh, idOf(m, "right")).x > 0.1F);
    CHECK(meshVolume(mesh) == exactly(m.report.volume));
}

TEST_CASE("wedge: a ramp that is high at the back", "[shape][golden]") {
    const Measured m = shape(WedgeSpec{20.0, 30.0, 10.0});
    CHECK(m.report.valid);
    CHECK(m.report.volume == exactly(20.0 * 30.0 * 10.0 / 2.0));
    CHECK(m.report.faces == 5);
    CHECK(rolesOf(m) == std::set<std::string>{"bottom", "back", "slope", "left", "right"});

    const TriangleMesh mesh = occt::tessellate(m.made.solid);
    const Point3f slope = faceNormalOf(mesh, idOf(m, "slope"));
    CHECK(slope.y < -0.1F); // leans towards the front
    CHECK(slope.z > 0.1F);  // and up
    CHECK(faceNormalOf(mesh, idOf(m, "back")).y == Approx(1.0).margin(1e-4));
    CHECK(meshVolume(mesh) == exactly(m.report.volume));
}

TEST_CASE("shapes reject sizes they cannot make", "[shape]") {
    const double nan = std::nan("");
    CHECK(codeOf(SphereSpec{0.0}) == GeometryErrorCode::invalidInput);
    CHECK(codeOf(SphereSpec{nan}) == GeometryErrorCode::invalidInput);
    CHECK(codeOf(HemisphereSpec{-1.0}) == GeometryErrorCode::invalidInput);
    CHECK(codeOf(ConeSpec{10.0, -1.0, 5.0}) == GeometryErrorCode::invalidInput);
    CHECK(codeOf(TorusSpec{5.0, 5.0}) ==
          GeometryErrorCode::invalidInput); // tube as thick as the radius
    CHECK(codeOf(TubeSpec{5.0, 5.0, 10.0}) == GeometryErrorCode::invalidInput);
    CHECK(codeOf(PrismSpec{2, 10.0, 10.0}) == GeometryErrorCode::invalidInput);
    CHECK(codeOf(PrismSpec{65, 10.0, 10.0}) == GeometryErrorCode::invalidInput);
    CHECK(codeOf(StarSpec{5, 10.0, 10.0, 3.0}) == GeometryErrorCode::invalidInput);
    CHECK(codeOf(PyramidSpec{10.0, 0.0, 10.0}) == GeometryErrorCode::invalidInput);
    CHECK(codeOf(WedgeSpec{10.0, 10.0, -2.0}) == GeometryErrorCode::invalidInput);
}

TEST_CASE("box with rounded corners: the radius stays a true radius", "[shape][golden]") {
    // Ablauf D, step 1 (docs/02-ux-konzept.md §15): corners of 5 mm stay quarter circles of 5 mm
    // whatever the width is.
    for (const double width : {40.0, 80.0, 17.5}) {
        INFO("width " << width);
        const double radius = 5.0;
        const NamedSolid made =
            occt::makeNamedBox(width, 40.0, 10.0, {-width / 2.0, -20.0, 0.0}, radius);
        const SolidReport report = occt::measure(made.solid);
        CHECK(report.valid);
        CHECK(report.volume == exactly((width * 40.0 - (4.0 - kPi) * radius * radius) * 10.0));
        CHECK(report.faces == 10);

        const Measured m{made, report};
        CHECK(rolesOf(m) == std::set<std::string>{"bottom", "top", "front", "back", "left", "right",
                                                  "cornerFrontLeft", "cornerFrontRight",
                                                  "cornerBackLeft", "cornerBackRight"});

        // Measure the corners: every vertex of a corner face lies exactly 5 mm from the corner's
        // axis (a circle, not an ellipse), also at the finest the viewer asks for.
        TessellationParams fine;
        fine.chordTolerance = 0.001;
        const TriangleMesh mesh = occt::tessellate(made.solid, fine);
        const std::map<std::string, std::pair<double, double>> axes{
            {"cornerFrontLeft", {-width / 2.0 + radius, -20.0 + radius}},
            {"cornerFrontRight", {width / 2.0 - radius, -20.0 + radius}},
            {"cornerBackLeft", {-width / 2.0 + radius, 20.0 - radius}},
            {"cornerBackRight", {width / 2.0 - radius, 20.0 - radius}},
        };
        for (const auto& [role, axis] : axes) {
            INFO(role);
            const std::uint32_t id = idOf(m, role);
            int checked = 0;
            for (std::size_t t = 0; t < mesh.faceIds.size(); ++t) {
                if (mesh.faceIds[t] != id) {
                    continue;
                }
                for (int c = 0; c < 3; ++c) {
                    const Point3f& p =
                        mesh.positions[mesh.indices[3 * t + static_cast<std::size_t>(c)]];
                    CHECK(std::hypot(p.x - axis.first, p.y - axis.second) ==
                          Approx(radius).margin(1e-4));
                    ++checked;
                }
            }
            CHECK(checked > 6);
        }
    }
}

TEST_CASE("box: the radius may be as large as half the shorter side, no larger", "[shape]") {
    const NamedSolid stadium = occt::makeNamedBox(40.0, 20.0, 5.0, {}, 10.0);
    const SolidReport report = occt::measure(stadium.solid);
    CHECK(report.valid);
    CHECK(report.volume == exactly((40.0 * 20.0 - (4.0 - kPi) * 100.0) * 5.0));
    // The straight pieces on the short sides have vanished: no front and back... but left and right
    // stay.
    const Measured m{stadium, report};
    const std::set<std::string> roles = rolesOf(m);
    CHECK(roles.contains("front"));
    CHECK_FALSE(roles.contains("left"));

    CHECK_THROWS_AS(occt::makeNamedBox(40.0, 20.0, 5.0, {}, 10.5), GeometryError);
    CHECK_THROWS_AS(occt::makeNamedBox(40.0, 20.0, 5.0, {}, -1.0), GeometryError);

    const NamedSolid circle = occt::makeNamedBox(20.0, 20.0, 5.0, {}, 10.0);
    CHECK(occt::measure(circle.solid).valid);
    CHECK(occt::measure(circle.solid).volume == exactly(kPi * 100.0 * 5.0));
}
