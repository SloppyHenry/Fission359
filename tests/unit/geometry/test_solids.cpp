// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Golden tests for the first body operations: box and cylinder, measured and tessellated through
// the OpenCASCADE implementation of the geometry layer (AP-0.8).

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdint>
#include <map>
#include <numbers>
#include <set>

#include "fission/geo/error.h"
#include "fission/geo/mesh.h"
#include "fission/geo/occt/solids.h"

using Catch::Approx;
using namespace fission::geo;

namespace {

// Relative tolerance 1e-6, as required for golden tests (docs/03-architektur.md §10).
Approx exactly(double expected) {
    return Approx(expected).epsilon(1e-6);
}

Point3f minus(const Point3f& a, const Point3f& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Point3f cross(const Point3f& a, const Point3f& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

double dot(const Point3f& a, const Point3f& b) {
    return static_cast<double>(a.x) * b.x + static_cast<double>(a.y) * b.y +
           static_cast<double>(a.z) * b.z;
}

// Signed volume of the mesh (divergence theorem). Positive and equal to the body's volume only
// if the triangles close the body and all face outwards.
double meshVolume(const TriangleMesh& mesh) {
    double volume = 0.0;
    for (std::size_t t = 0; t < mesh.triangleCount(); ++t) {
        const Point3f& a = mesh.positions[mesh.indices[3 * t]];
        const Point3f& b = mesh.positions[mesh.indices[3 * t + 1]];
        const Point3f& c = mesh.positions[mesh.indices[3 * t + 2]];
        volume += dot(a, cross(b, c)) / 6.0;
    }
    return volume;
}

} // namespace

TEST_CASE("box: volume, area, bounding box and topology are exact", "[solid][golden]") {
    const Solid box = occt::makeBox(20.0, 30.0, 40.0, {-10.0, -15.0, 0.0});
    const SolidReport report = occt::measure(box);

    CHECK(report.valid);
    CHECK(report.volume == exactly(24000.0));
    CHECK(report.area == exactly(2.0 * (20.0 * 30.0 + 20.0 * 40.0 + 30.0 * 40.0)));
    CHECK(report.boxMin.x == Approx(-10.0).margin(1e-9));
    CHECK(report.boxMin.y == Approx(-15.0).margin(1e-9));
    CHECK(report.boxMin.z == Approx(0.0).margin(1e-9));
    CHECK(report.boxMax.x == Approx(10.0).margin(1e-9));
    CHECK(report.boxMax.y == Approx(15.0).margin(1e-9));
    CHECK(report.boxMax.z == Approx(40.0).margin(1e-9));
    CHECK(report.solids == 1);
    CHECK(report.faces == 6);
    CHECK(report.edges == 12);
    CHECK(report.vertices == 8);
}

TEST_CASE("box: tessellation is closed, outward facing and has hard edges", "[solid][golden]") {
    const Solid box = occt::makeBox(20.0, 30.0, 40.0);
    const TriangleMesh mesh = occt::tessellate(box);

    // A box needs no more than two triangles per face; every face has its own four vertices.
    CHECK(mesh.triangleCount() == 12);
    CHECK(mesh.positions.size() == 24);
    CHECK(mesh.normals.size() == mesh.positions.size());
    CHECK(mesh.faceIds.size() == mesh.triangleCount());
    CHECK(meshVolume(mesh) == exactly(24000.0));

    std::set<std::uint32_t> faces(mesh.faceIds.begin(), mesh.faceIds.end());
    CHECK(faces == std::set<std::uint32_t>{1, 2, 3, 4, 5, 6});

    const Point3f center{10.0F, 15.0F, 20.0F};
    for (std::size_t t = 0; t < mesh.triangleCount(); ++t) {
        const Point3f& a = mesh.positions[mesh.indices[3 * t]];
        const Point3f& b = mesh.positions[mesh.indices[3 * t + 1]];
        const Point3f& c = mesh.positions[mesh.indices[3 * t + 2]];
        const Point3f geometric = cross(minus(b, a), minus(c, a));
        // Counter-clockwise seen from outside: the triangle's own normal points away from the
        // center, and the stored vertex normals agree with it.
        CHECK(dot(geometric, minus(a, center)) > 0.0);
        for (int corner = 0; corner < 3; ++corner) {
            const Point3f& normal = mesh.normals[mesh.indices[3 * t + corner]];
            CHECK(std::sqrt(dot(normal, normal)) == Approx(1.0).margin(1e-6));
            CHECK(dot(normal, geometric) > 0.0);
        }
    }

    // Twelve straight edges with two points each, numbered like the body's edges.
    REQUIRE(mesh.edges.size() == 12);
    std::set<std::uint32_t> edgeIds;
    for (const EdgePolyline& edge : mesh.edges) {
        CHECK(edge.points.size() == 2);
        edgeIds.insert(edge.edgeId);
    }
    CHECK(edgeIds.size() == 12);
}

TEST_CASE("cylinder: round stays round, smooth normals, closed surface", "[solid][golden]") {
    constexpr double kRadius = 10.0;
    constexpr double kHeight = 25.0;
    const Solid cylinder = occt::makeCylinder(kRadius, kHeight, {5.0, 5.0, 0.0});
    const SolidReport report = occt::measure(cylinder);

    CHECK(report.valid);
    CHECK(report.volume == exactly(std::numbers::pi * kRadius * kRadius * kHeight));
    CHECK(report.area == exactly(2.0 * std::numbers::pi * kRadius * (kRadius + kHeight)));
    CHECK(report.faces == 3);
    CHECK(report.solids == 1);

    TessellationParams fine;
    fine.chordTolerance = 0.005;
    const TriangleMesh mesh = occt::tessellate(cylinder, fine);
    REQUIRE_FALSE(mesh.empty());

    // The mesh stays within the chord tolerance of the true surface: every vertex lies on the
    // cylinder, and the polygon's volume is only slightly smaller than the true volume.
    const double trueVolume = std::numbers::pi * kRadius * kRadius * kHeight;
    const double polygonVolume = meshVolume(mesh);
    CHECK(polygonVolume <= trueVolume);
    CHECK(polygonVolume > trueVolume * (1.0 - 2.0 * 0.005 / kRadius));
    for (const Point3f& point : mesh.positions) {
        const double radial = std::hypot(point.x - 5.0, point.y - 5.0);
        const bool onSide = std::abs(radial - kRadius) < 1e-4;
        const bool onCap = point.z < 1e-4 || std::abs(point.z - kHeight) < 1e-4;
        CHECK((onSide || onCap));
    }

    // On the side the normal is the radial direction at that vertex (smooth shading).
    for (std::size_t i = 0; i < mesh.positions.size(); ++i) {
        const Point3f& p = mesh.positions[i];
        const Point3f& n = mesh.normals[i];
        if (std::abs(std::hypot(p.x - 5.0, p.y - 5.0) - kRadius) < 1e-4 && std::abs(n.z) < 0.5) {
            const double radialX = (p.x - 5.0) / kRadius;
            const double radialY = (p.y - 5.0) / kRadius;
            CHECK(n.x == Approx(radialX).margin(1e-4));
            CHECK(n.y == Approx(radialY).margin(1e-4));
        }
    }
}

TEST_CASE("bodies reject impossible sizes with a geometry error", "[solid]") {
    const auto codeOf = [](auto&& makeIt) {
        try {
            makeIt();
        } catch (const GeometryError& error) {
            return error.code();
        }
        return GeometryErrorCode::kernelFailure; // not reached when the call throws
    };
    CHECK(codeOf([] { occt::makeBox(0.0, 10.0, 10.0); }) == GeometryErrorCode::invalidInput);
    CHECK(codeOf([] { occt::makeBox(-1.0, 10.0, 10.0); }) == GeometryErrorCode::invalidInput);
    CHECK(codeOf([] { occt::makeBox(std::nan(""), 10.0, 10.0); }) ==
          GeometryErrorCode::invalidInput);
    CHECK(codeOf([] { occt::makeCylinder(5.0, 0.0); }) == GeometryErrorCode::invalidInput);
    CHECK_THROWS_AS(occt::measure(Solid{}), GeometryError);
    CHECK_THROWS_AS(occt::tessellate(Solid{}), GeometryError);
}

TEST_CASE("box: every edge knows its two faces and its two corners", "[solid][golden]") {
    const TriangleMesh mesh = occt::tessellate(occt::makeBox(20.0, 30.0, 40.0));
    REQUIRE(mesh.edges.size() == 12);
    REQUIRE(mesh.corners.size() == 8);

    std::map<std::uint32_t, int> edgesPerFace;
    std::map<std::uint32_t, int> edgesPerCorner;
    for (const EdgePolyline& edge : mesh.edges) {
        CHECK(edge.faces[0] != 0);
        CHECK(edge.faces[1] != 0);
        CHECK(edge.faces[0] != edge.faces[1]);
        CHECK(edge.corners[0] != 0);
        CHECK(edge.corners[1] != 0);
        CHECK(edge.corners[0] != edge.corners[1]);
        ++edgesPerFace[edge.faces[0]];
        ++edgesPerFace[edge.faces[1]];
        ++edgesPerCorner[edge.corners[0]];
        ++edgesPerCorner[edge.corners[1]];
    }
    CHECK(edgesPerFace.size() == 6);
    for (const auto& [face, count] : edgesPerFace) {
        CHECK(count == 4); // a face of a box has four edges
    }
    CHECK(edgesPerCorner.size() == 8);
    for (const auto& [corner, count] : edgesPerCorner) {
        CHECK(count == 3); // three edges meet in a corner
    }

    // The corner points sit on the corners of the box (20 x 30 x 40, origin at the corner).
    for (const CornerPoint& corner : mesh.corners) {
        CHECK((corner.position.x == 0.0F || corner.position.x == 20.0F));
        CHECK((corner.position.y == 0.0F || corner.position.y == 30.0F));
        CHECK((corner.position.z == 0.0F || corner.position.z == 40.0F));
    }
}

TEST_CASE("cylinder: the seam belongs to the same face on both sides", "[solid][golden]") {
    const TriangleMesh mesh = occt::tessellate(occt::makeCylinder(10.0, 25.0, {0.0, 0.0, 0.0}));
    int seams = 0;
    int rims = 0;
    for (const EdgePolyline& edge : mesh.edges) {
        if (edge.faces[0] == edge.faces[1]) {
            ++seams;
        } else {
            ++rims;
        }
        CHECK(edge.faces[0] != 0);
    }
    CHECK(seams == 1);
    CHECK(rims == 2);
}
