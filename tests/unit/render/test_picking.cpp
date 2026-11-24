// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "fission/render/picking.h"

using Catch::Approx;
using namespace fission::render;

namespace {

// A flat square of 10 mm at height `z` (face 7), facing up.
RenderBody square(float z) {
    RenderBody body;
    auto owned = std::make_shared<fission::geo::TriangleMesh>();
    auto& mesh = *owned;
    mesh.positions = {{-5, -5, z}, {5, -5, z}, {5, 5, z}, {-5, 5, z}};
    mesh.normals = {{0, 0, 1}, {0, 0, 1}, {0, 0, 1}, {0, 0, 1}};
    mesh.indices = {0, 1, 2, 0, 2, 3};
    mesh.faceIds = {7, 7};
    body.mesh = owned;
    return body;
}

Ray down(float x, float y) {
    return {QVector3D(x, y, 100.0F), QVector3D(0, 0, -1)};
}

} // namespace

TEST_CASE("a ray hits the face it points at, with point and normal", "[picking]") {
    RenderScene scene;
    scene.bodies.push_back(square(10.0F));

    const auto hit = pickSurface(scene, down(1.0F, 2.0F));
    REQUIRE(hit.has_value());
    CHECK(hit->bodyIndex == 0);
    CHECK(hit->faceId == 7);
    CHECK(hit->point.z() == Approx(10.0F));
    CHECK(hit->point.x() == Approx(1.0F));
    CHECK(hit->normal.z() == Approx(1.0F));
    CHECK(hit->distance == Approx(90.0F));
}

TEST_CASE("beside the body the ray finds the print bed, beyond it nothing", "[picking]") {
    RenderScene scene;
    scene.bodies.push_back(square(10.0F));

    const auto bed = pickSurface(scene, down(50.0F, 0.0F));
    REQUIRE(bed.has_value());
    CHECK(bed->bodyIndex == -1);
    CHECK(bed->point.z() == Approx(0.0F));
    CHECK(bed->normal.z() == Approx(1.0F));

    CHECK_FALSE(pickSurface(scene, down(500.0F, 0.0F)).has_value());
}

TEST_CASE("the nearest of two bodies wins", "[picking]") {
    RenderScene scene;
    scene.bodies.push_back(square(10.0F));
    scene.bodies.push_back(square(30.0F));
    const auto hit = pickSurface(scene, down(0.0F, 0.0F));
    REQUIRE(hit.has_value());
    CHECK(hit->bodyIndex == 1);
}

TEST_CASE("the shape of a face is its centre and the radius around it", "[picking]") {
    const RenderBody body = square(10.0F);
    const auto shape = faceShape(body.geometry(), 7);
    REQUIRE(shape.has_value());
    CHECK(shape->center.x() == Approx(0.0F).margin(1e-4));
    CHECK(shape->center.z() == Approx(10.0F));
    CHECK(shape->radius == Approx(std::sqrt(50.0F)));
    CHECK_FALSE(faceShape(body.geometry(), 99).has_value());
}

TEST_CASE("a section plane tells which side is cut away", "[picking][section]") {
    const ClipPlane plane{QVector3D(0, 0, 5), QVector3D(0, 0, 1)};
    CHECK(plane.cuts(QVector3D(1, 2, 6)));
    CHECK_FALSE(plane.cuts(QVector3D(1, 2, 4)));
    CHECK_FALSE(plane.cuts(QVector3D(1, 2, 5))); // exactly on the plane stays
}

TEST_CASE("what the section plane cuts away cannot be picked", "[picking][section]") {
    RenderScene scene;
    scene.bodies.push_back(square(10.0F));
    const ClipPlane cutAbove{QVector3D(0, 0, 5), QVector3D(0, 0, 1)};
    // The face at z = 10 is cut away: the ray goes on to the print bed.
    const auto hit = pickSurface(scene, down(0.0F, 0.0F), {}, cutAbove);
    REQUIRE(hit.has_value());
    CHECK(hit->bodyIndex == -1);
    // The other side of the plane is kept: the face is still there.
    const ClipPlane cutBelow{QVector3D(0, 0, 5), QVector3D(0, 0, -1)};
    const auto kept = pickSurface(scene, down(0.0F, 0.0F), {}, cutBelow);
    REQUIRE(kept.has_value());
    CHECK(kept->bodyIndex == 0);
    // Off: as before.
    CHECK(pickSurface(scene, down(0.0F, 0.0F), {}, std::nullopt)->bodyIndex == 0);
}
