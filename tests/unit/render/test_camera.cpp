// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "fission/render/camera.h"

using Catch::Approx;
using namespace fission::render;

namespace {

OrbitCamera makeCamera() {
    OrbitCamera camera;
    camera.setViewportSize({1000.0, 600.0});
    camera.frame({-128, -128, 0}, {128, 128, 40});
    return camera;
}

} // namespace

TEST_CASE("standard views look from the right side", "[camera]") {
    OrbitCamera camera = makeCamera();

    camera.setPose(camera.poseFor(StandardView::front));
    CHECK(camera.eye().y() < camera.pose().target.y());
    CHECK(camera.eye().x() == Approx(camera.pose().target.x()).margin(1e-3));

    camera.setPose(camera.poseFor(StandardView::right));
    CHECK(camera.eye().x() > camera.pose().target.x());

    camera.setPose(camera.poseFor(StandardView::left));
    CHECK(camera.eye().x() < camera.pose().target.x());

    camera.setPose(camera.poseFor(StandardView::back));
    CHECK(camera.eye().y() > camera.pose().target.y());

    camera.setPose(camera.poseFor(StandardView::top));
    CHECK(camera.eye().z() > camera.pose().target.z() + 0.99F * camera.pose().distance);
}

TEST_CASE("dragging turns the scene with the pointer", "[camera]") {
    OrbitCamera camera = makeCamera();
    camera.setPose(camera.poseFor(StandardView::front));
    // Turning the table keeps the target in place and moves the camera to the left, so that
    // the scene seems to follow the pointer to the right.
    camera.orbit(30.0F, 0.0F);
    CHECK(camera.eye().x() < camera.pose().target.x());
    CHECK(camera.pose().yaw > 0.0F);

    // Dragging down looks more from above; the angle never goes past straight down.
    const float pitch = camera.pose().pitch;
    camera.orbit(0.0F, 20.0F);
    CHECK(camera.pose().pitch > pitch);
    camera.orbit(0.0F, 100000.0F);
    CHECK(camera.pose().pitch <= 89.5F);
}

TEST_CASE("zooming keeps the point under the pointer in place", "[camera]") {
    OrbitCamera camera = makeCamera();
    const QPointF cursor(700.0, 200.0);
    const auto before = camera.pointOnPlane(cursor);
    REQUIRE(before.has_value());

    camera.zoomAt(0.5F, cursor);

    const auto after = camera.pointOnPlane(cursor);
    REQUIRE(after.has_value());
    CHECK(after->x() == Approx(before->x()).margin(1e-2));
    CHECK(after->y() == Approx(before->y()).margin(1e-2));
    const QPointF projected = camera.project(*before);
    CHECK(projected.x() == Approx(cursor.x()).margin(0.5));
    CHECK(projected.y() == Approx(cursor.y()).margin(0.5));
}

TEST_CASE("zoom is limited", "[camera]") {
    OrbitCamera camera = makeCamera();
    for (int i = 0; i < 200; ++i) {
        camera.zoomAt(0.5F);
    }
    CHECK(camera.pose().distance >= 2.0F);
    for (int i = 0; i < 200; ++i) {
        camera.zoomAt(2.0F);
    }
    CHECK(camera.pose().distance < 1.0e5F);
}

TEST_CASE("framing fits the box into the window", "[camera]") {
    for (const QSizeF size : {QSizeF(1000, 600), QSizeF(400, 800)}) {
        OrbitCamera camera;
        camera.setViewportSize(size);
        camera.frame({-128, -128, 0}, {128, 128, 40});
        for (const float x : {-128.0F, 128.0F}) {
            for (const float y : {-128.0F, 128.0F}) {
                const QPointF p = camera.project({x, y, 0.0F});
                CHECK(p.x() >= 0.0);
                CHECK(p.x() <= size.width());
                CHECK(p.y() >= 0.0);
                CHECK(p.y() <= size.height());
            }
        }
    }
}

TEST_CASE("panning moves the picture with the pointer", "[camera]") {
    OrbitCamera camera = makeCamera();
    // The scale is exact for points at the depth of the target (the point looked at).
    const QVector3D point = camera.pose().target;
    const QPointF origin = camera.project(point);
    camera.pan(40.0F, -25.0F);
    const QPointF moved = camera.project(point);
    CHECK(moved.x() == Approx(origin.x() + 40.0).margin(0.5));
    CHECK(moved.y() == Approx(origin.y() - 25.0).margin(0.5));
}

TEST_CASE("pose blending takes the short way round", "[camera]") {
    CameraPose from;
    from.yaw = 170.0F;
    CameraPose to;
    to.yaw = -170.0F;
    const CameraPose middle = blend(from, to, 0.5F);
    CHECK(std::abs(middle.yaw) == Approx(180.0F).margin(1e-3));
    CHECK(blend(from, to, 0.0F).yaw == Approx(170.0F));
    CHECK(blend(from, to, 1.0F).yaw == Approx(-170.0F));
}

TEST_CASE("pixels per millimetre follow the distance", "[camera]") {
    OrbitCamera camera = makeCamera();
    const float near = camera.pixelsPerMillimetre();
    camera.zoomAt(2.0F);
    CHECK(camera.pixelsPerMillimetre() == Approx(near / 2.0F).epsilon(1e-4));
}

TEST_CASE("looking from an edge or a corner of the view cube", "[camera]") {
    OrbitCamera camera = makeCamera();
    const QVector3D target = camera.pose().target;

    // Corner at the front, right, top: the camera stands at +x, -y, +z of the target.
    camera.setPose(camera.poseLookingFrom({1.0F, -1.0F, 1.0F}));
    const QVector3D corner = camera.eye() - target;
    CHECK(corner.x() > 0.0F);
    CHECK(corner.y() < 0.0F);
    CHECK(corner.z() > 0.0F);
    CHECK(corner.x() == Approx(-corner.y()).margin(1e-3));
    CHECK(camera.pose().pitch == Approx(35.264F).margin(0.01));

    // Edge between the left and the back face, at the middle height.
    camera.setPose(camera.poseLookingFrom({-1.0F, 1.0F, 0.0F}));
    const QVector3D edge = camera.eye() - target;
    CHECK(edge.x() < 0.0F);
    CHECK(edge.y() > 0.0F);
    CHECK(edge.z() == Approx(0.0F).margin(1e-3));
}

TEST_CASE("looking straight down keeps the heading", "[camera]") {
    OrbitCamera camera = makeCamera();
    CameraPose pose = camera.pose();
    pose.yaw = 40.0F;
    camera.setPose(pose);
    camera.setPose(camera.poseLookingFrom({0.0F, 0.0F, 1.0F}));
    CHECK(camera.pose().yaw == Approx(40.0F));
    CHECK(camera.pose().pitch == Approx(89.5F));
}

TEST_CASE("a ray through the view centre passes through the target", "[camera]") {
    const OrbitCamera camera = makeCamera();
    const auto ray = camera.rayThrough({500.0, 300.0});
    REQUIRE(ray.has_value());
    const QVector3D toTarget = (camera.pose().target - ray->origin).normalized();
    CHECK(QVector3D::dotProduct(toTarget, ray->direction) == Approx(1.0F).margin(1e-4));
}

TEST_CASE("framing a sphere puts it in the middle and keeps the angles", "[camera]") {
    OrbitCamera camera = makeCamera();
    const CameraPose before = camera.pose();
    const CameraPose pose = camera.poseFraming({10.0F, 20.0F, 5.0F}, 3.0F);
    CHECK(pose.target.x() == Approx(10.0F));
    CHECK(pose.yaw == Approx(before.yaw));
    CHECK(pose.pitch == Approx(before.pitch));
    CHECK(pose.distance < before.distance);
    CHECK(pose.distance >= 2.0F);
}
