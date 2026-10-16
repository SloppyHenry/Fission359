// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The pick pass (AP-1.4b): what the pointer points at is decided from a picture of numbers.
// The first tests need no graphics card. The last ones draw the pass on the real graphics
// interface of the machine (Metal on the Mac) and are skipped where there is none.

#include <catch2/catch_test_macros.hpp>

#include <QMatrix4x4>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <memory>
#include <optional>
#include <vector>

#include "fission/geo/occt/kernel.h"
#include "fission/render/benchmark_scene.h"
#include "fission/render/pick.h"
#include "fission/render/scene_renderer.h"

using namespace fission::render;
using fission::core::SelectableKind;
using fission::core::SelectionFilter;

namespace {

using Picture = std::array<std::uint32_t, kPickSize * kPickSize>;

Picture emptyPicture() {
    Picture picture{};
    return picture;
}

void put(Picture& picture, int dx, int dy, SelectableKind kind, unsigned slot, unsigned number) {
    picture[static_cast<std::size_t>((kPickRadius + dy) * kPickSize + kPickRadius + dx)] =
        packPickId(kind, slot, number);
}

} // namespace

TEST_CASE("pick numbers: pack and unpack are the same", "[pick]") {
    for (const SelectableKind kind :
         {SelectableKind::face, SelectableKind::edge, SelectableKind::vertex}) {
        const std::uint32_t id = packPickId(kind, 1023, kPickMaxNumber);
        const PickId back = unpackPickId(id);
        CHECK(back.valid);
        CHECK(back.kind == kind);
        CHECK(back.slot == 1023);
        CHECK(back.number == kPickMaxNumber);
    }
    CHECK(unpackPickId(packPickId(SelectableKind::face, 1, 1)).slot == 1);
    CHECK_FALSE(unpackPickId(0).valid);
    // What cannot be written is "nothing", never a wrong number.
    CHECK(packPickId(SelectableKind::body, 1, 1) == 0);
    CHECK(packPickId(SelectableKind::face, 0, 1) == 0);
    CHECK(packPickId(SelectableKind::face, kPickSlots, 1) == 0);
    CHECK(packPickId(SelectableKind::face, 1, 0) == 0);
    CHECK(packPickId(SelectableKind::face, 1, kPickMaxNumber + 1) == 0);
}

TEST_CASE("pick: the face under the pointer, the background is nothing", "[pick]") {
    Picture picture = emptyPicture();
    CHECK_FALSE(choosePicked(picture, SelectionFilter::all()).has_value());

    put(picture, 0, 0, SelectableKind::face, 3, 5);
    const auto picked = choosePicked(picture, SelectionFilter::all());
    REQUIRE(picked.has_value());
    CHECK(picked->kind == SelectableKind::face);
    CHECK(picked->slot == 3);
    CHECK(picked->number == 5);

    // A face next to the pointer does not count: only what is under it.
    Picture beside = emptyPicture();
    put(beside, 1, 0, SelectableKind::face, 3, 5);
    CHECK_FALSE(choosePicked(beside, SelectionFilter::all()).has_value());
}

TEST_CASE("pick: an edge within reach beats the face, a far one does not", "[pick]") {
    Picture picture = emptyPicture();
    put(picture, 0, 0, SelectableKind::face, 1, 2);
    put(picture, 3, 0, SelectableKind::edge, 1, 9);
    auto picked = choosePicked(picture, SelectionFilter::all());
    REQUIRE(picked.has_value());
    CHECK(picked->kind == SelectableKind::edge);
    CHECK(picked->number == 9);

    Picture far = emptyPicture();
    put(far, 0, 0, SelectableKind::face, 1, 2);
    put(far, 6, 0, SelectableKind::edge, 1, 9);
    picked = choosePicked(far, SelectionFilter::all());
    REQUIRE(picked.has_value());
    CHECK(picked->kind == SelectableKind::face);
}

TEST_CASE("pick: the nearer of two edges wins, a corner is preferred a little", "[pick]") {
    Picture picture = emptyPicture();
    put(picture, -2, 0, SelectableKind::edge, 1, 1);
    put(picture, 3, 0, SelectableKind::edge, 1, 2);
    auto picked = choosePicked(picture, SelectionFilter::all());
    REQUIRE(picked.has_value());
    CHECK(picked->number == 1);

    // A corner two pixels farther than an edge still wins (it is a small target) ...
    Picture corner = emptyPicture();
    put(corner, 1, 0, SelectableKind::edge, 1, 1);
    put(corner, 0, 3, SelectableKind::vertex, 1, 4);
    picked = choosePicked(corner, SelectionFilter::all());
    REQUIRE(picked.has_value());
    CHECK(picked->kind == SelectableKind::vertex);

    // ... but not when the edge is right under the pointer and the corner is far.
    Picture edgeUnder = emptyPicture();
    put(edgeUnder, 0, 0, SelectableKind::edge, 1, 1);
    put(edgeUnder, 5, 0, SelectableKind::vertex, 1, 4);
    picked = choosePicked(edgeUnder, SelectionFilter::all());
    REQUIRE(picked.has_value());
    CHECK(picked->kind == SelectableKind::edge);
}

TEST_CASE("pick: the filter decides what reacts", "[pick]") {
    Picture picture = emptyPicture();
    put(picture, 0, 0, SelectableKind::face, 2, 7);
    put(picture, 2, 0, SelectableKind::edge, 2, 3);

    auto picked = choosePicked(picture, SelectionFilter::only(SelectableKind::face));
    REQUIRE(picked.has_value());
    CHECK(picked->kind == SelectableKind::face);

    picked = choosePicked(picture, SelectionFilter::only(SelectableKind::edge));
    REQUIRE(picked.has_value());
    CHECK(picked->kind == SelectableKind::edge);

    // Only bodies: the face under the pointer counts as its body.
    picked = choosePicked(picture, SelectionFilter::only(SelectableKind::body));
    REQUIRE(picked.has_value());
    CHECK(picked->kind == SelectableKind::body);
    CHECK(picked->slot == 2);

    // Only corners, none near: nothing reacts, not even the face.
    CHECK_FALSE(choosePicked(picture, SelectionFilter::only(SelectableKind::vertex)).has_value());
}

// --- on the real graphics interface -----------------------------------------------------

namespace {

std::unique_ptr<QRhi> realRhi() {
#if defined(Q_OS_MACOS)
    QRhiMetalInitParams params;
    return std::unique_ptr<QRhi>(QRhi::create(QRhi::Metal, &params));
#else
    return nullptr; // see the closing note of AP-1.4: Linux is measured on the test machine
#endif
}

constexpr int kWidth = 400;
constexpr int kHeight = 300;
constexpr float kScale = 10.0F; // pixels per millimetre

// Looking straight down at a box of 20 mm: x to the right, y up on the screen.
struct TopView {
    QMatrix4x4 view;
    QMatrix4x4 projection;
    TopView() {
        view.lookAt({10, 10, 100}, {10, 10, 0}, {0, 1, 0});
        projection.ortho(-0.5F * kWidth / kScale, 0.5F * kWidth / kScale, -0.5F * kHeight / kScale,
                         0.5F * kHeight / kScale, 1.0F, 500.0F);
    }
    QPoint pixel(float x, float y) const {
        return {static_cast<int>(std::lround(kWidth / 2.0F + (x - 10.0F) * kScale)),
                static_cast<int>(std::lround(kHeight / 2.0F - (y - 10.0F) * kScale))};
    }
};

struct Gpu {
    std::unique_ptr<QRhi> rhi = realRhi();
    std::unique_ptr<SceneRenderer> renderer;
    std::shared_ptr<const fission::geo::TriangleMesh> mesh;
    TopView camera;

    Gpu() {
        if (!rhi) {
            return;
        }
        renderer = std::make_unique<SceneRenderer>(rhi.get());
        const auto kernel = fission::geo::occt::makeKernel();
        const auto box = kernel->makeBox(20, 20, 20, {0, 0, 0});
        mesh = std::make_shared<fission::geo::TriangleMesh>(kernel->tessellate(box.solid, {}));
        RenderScene scene;
        RenderBody body;
        body.id = "box";
        body.meshVersion = 1;
        body.mesh = mesh;
        scene.bodies.push_back(body);
        upload(scene);
    }

    void upload(const RenderScene& scene) {
        QRhiCommandBuffer* cb = nullptr;
        REQUIRE(rhi->beginOffscreenFrame(&cb) == QRhi::FrameOpSuccess);
        renderer->upload(cb, scene, {});
        rhi->endOffscreenFrame();
    }

    // Picks at a pixel and returns the picture; empty if the pass could not run.
    std::vector<std::uint32_t> pick(const QPoint& pixel, const SelectionFilter& filter) {
        std::vector<std::uint32_t> picture;
        QRhiCommandBuffer* cb = nullptr;
        REQUIRE(rhi->beginOffscreenFrame(&cb) == QRhi::FrameOpSuccess);
        const bool started = renderer->pick(
            cb, QSize(kWidth, kHeight), pixel, camera.view, camera.projection, filter,
            [&picture](std::vector<std::uint32_t> p) { picture = std::move(p); });
        REQUIRE(started);
        rhi->endOffscreenFrame();
        return picture;
    }

    std::optional<PickedElement> pickAt(float x, float y,
                                        const SelectionFilter& filter = SelectionFilter::all()) {
        const auto picture = pick(camera.pixel(x, y), filter);
        REQUIRE(picture.size() == static_cast<std::size_t>(kPickSize * kPickSize));
        return choosePicked(picture, filter);
    }
};

} // namespace

TEST_CASE("pick on the graphics card: face, edge and corner of a box", "[pick][gpu]") {
    Gpu gpu;
    if (!gpu.rhi) {
        SKIP("no graphics interface on this machine");
    }
    const unsigned slot = gpu.renderer->slotOfBody("box");
    REQUIRE(slot != 0);
    CHECK(gpu.renderer->bodyOfSlot(slot) == "box");

    // The top face is the one whose triangles point up.
    std::uint32_t topFace = 0;
    for (std::size_t t = 0; t < gpu.mesh->triangleCount(); ++t) {
        if (gpu.mesh->normals[gpu.mesh->indices[3 * t]].z > 0.9F) {
            topFace = gpu.mesh->faceIds[t];
        }
    }
    REQUIRE(topFace != 0);

    auto picked = gpu.pickAt(10.0F, 10.0F); // the middle of the top face
    REQUIRE(picked.has_value());
    CHECK(picked->kind == SelectableKind::face);
    CHECK(picked->slot == slot);
    CHECK(picked->number == topFace);

    // The middle of the edge at x = 0, z = 20 (the left top edge) ...
    picked = gpu.pickAt(0.1F, 10.0F);
    REQUIRE(picked.has_value());
    CHECK(picked->kind == SelectableKind::edge);
    const auto edge = std::find_if(gpu.mesh->edges.begin(), gpu.mesh->edges.end(),
                                   [&](const auto& e) { return e.edgeId == picked->number; });
    REQUIRE(edge != gpu.mesh->edges.end());
    for (const auto& point : edge->points) {
        CHECK(point.x == 0.0F);
        CHECK(point.z == 20.0F);
    }

    // ... and the corner at (20, 20, 20).
    picked = gpu.pickAt(19.9F, 19.9F);
    REQUIRE(picked.has_value());
    CHECK(picked->kind == SelectableKind::vertex);
    const auto corner = std::find_if(gpu.mesh->corners.begin(), gpu.mesh->corners.end(),
                                     [&](const auto& c) { return c.cornerId == picked->number; });
    REQUIRE(corner != gpu.mesh->corners.end());
    CHECK(corner->position.x == 20.0F);
    CHECK(corner->position.y == 20.0F);
    CHECK(corner->position.z == 20.0F);

    // Beside the box there is nothing.
    CHECK_FALSE(gpu.pickAt(30.0F, 10.0F).has_value());
}

namespace {
bool isEdge(std::uint32_t id) {
    const PickId unpacked = unpackPickId(id);
    return unpacked.valid && unpacked.kind == SelectableKind::edge;
}
} // namespace

TEST_CASE("pick on the graphics card: the picture is the right way up", "[pick][gpu]") {
    Gpu gpu;
    if (!gpu.rhi) {
        SKIP("no graphics interface on this machine");
    }
    // Pointer on the top face, 4 pixels above the lower edge (y = 0) on the screen: that edge
    // must show up in the lower half of the picture, and the left one (x = 0) on its left.
    const QPoint pointer = gpu.camera.pixel(5.0F, 0.0F) + QPoint(0, -4);
    const auto picture = gpu.pick(pointer, SelectionFilter::only(SelectableKind::edge));
    REQUIRE(picture.size() == static_cast<std::size_t>(kPickSize * kPickSize));
    int above = 0;
    int below = 0;
    for (int y = 0; y < kPickSize; ++y) {
        for (int x = 0; x < kPickSize; ++x) {
            if (isEdge(picture[static_cast<std::size_t>(y * kPickSize + x)])) {
                (y < kPickRadius ? above : below) += 1;
            }
        }
    }
    CHECK(below > 0);
    CHECK(above == 0);

    const QPoint nearLeft = gpu.camera.pixel(0.0F, 10.0F) + QPoint(4, 0);
    const auto left = gpu.pick(nearLeft, SelectionFilter::only(SelectableKind::edge));
    int toTheLeft = 0;
    int toTheRight = 0;
    for (int y = 0; y < kPickSize; ++y) {
        for (int x = 0; x < kPickSize; ++x) {
            if (isEdge(left[static_cast<std::size_t>(y * kPickSize + x)])) {
                (x < kPickRadius ? toTheLeft : toTheRight) += 1;
            }
        }
    }
    CHECK(toTheLeft > 0);
    CHECK(toTheRight == 0);
}

TEST_CASE("pick on the graphics card: a filter lets the face show through", "[pick][gpu]") {
    Gpu gpu;
    if (!gpu.rhi) {
        SKIP("no graphics interface on this machine");
    }
    // Right on the edge, but only faces can be chosen: the face under it is found.
    const auto picked = gpu.pickAt(0.05F, 10.0F, SelectionFilter::only(SelectableKind::face));
    REQUIRE(picked.has_value());
    CHECK(picked->kind == SelectableKind::face);
}

TEST_CASE("pick on the graphics card: bodies keep their numbers", "[pick][gpu]") {
    Gpu gpu;
    if (!gpu.rhi) {
        SKIP("no graphics interface on this machine");
    }
    RenderScene scene;
    for (const char* id : {"a", "box", "c"}) {
        RenderBody body;
        body.id = id;
        body.meshVersion = 1;
        body.mesh = gpu.mesh;
        scene.bodies.push_back(body);
    }
    gpu.upload(scene);
    const unsigned box = gpu.renderer->slotOfBody("box");
    CHECK(box != 0);
    CHECK(gpu.renderer->slotOfBody("a") != gpu.renderer->slotOfBody("c"));

    // A body that goes away does not change the numbers of the others.
    scene.bodies.erase(scene.bodies.begin());
    gpu.upload(scene);
    CHECK(gpu.renderer->slotOfBody("box") == box);
    CHECK(gpu.renderer->slotOfBody("a") == 0);
    CHECK(gpu.renderer->bodyOfSlot(box) == "box");
}

TEST_CASE("pick on the graphics card: 2 million triangles in under 16 ms", "[pick][gpu][perf]") {
    Gpu gpu;
    if (!gpu.rhi) {
        SKIP("no graphics interface on this machine");
    }
    // Drawing the picture and having it back must fit the budget of one frame
    // (docs/03-architektur.md §10), at the size of the scene the budget is given for.
    gpu.upload(*makeBenchmarkScene(2'000'000));
    constexpr int kRuns = 30;
    std::vector<double> milliseconds;
    for (int run = 0; run < kRuns; ++run) {
        const auto started = std::chrono::steady_clock::now();
        const auto picture = gpu.pick({kWidth / 2 + run, kHeight / 2}, SelectionFilter::all());
        milliseconds.push_back(
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started)
                .count());
        REQUIRE(picture.size() == static_cast<std::size_t>(kPickSize * kPickSize));
    }
    std::sort(milliseconds.begin(), milliseconds.end());
    const double median = milliseconds[milliseconds.size() / 2];
    WARN("pick at 2 million triangles: median " << median << " ms, worst " << milliseconds.back()
                                                << " ms");
    CHECK(median < 16.0);
}

TEST_CASE("pick region: everything in a rectangle, one entry each", "[pick]") {
    std::vector<std::uint32_t> picture(6 * 6, 0);
    picture[0] = packPickId(SelectableKind::face, 1, 5);
    picture[1] = packPickId(SelectableKind::face, 1, 5);
    picture[2] = packPickId(SelectableKind::face, 1, 6);
    picture[7] = packPickId(SelectableKind::face, 2, 1);
    picture[8] = packPickId(SelectableKind::edge, 2, 3);
    picture[9] = packPickId(SelectableKind::vertex, 3, 8);

    // Bodies can be chosen: one entry per body, in the order they were first seen.
    auto found = chooseInRegion(picture, SelectionFilter::all());
    REQUIRE(found.size() == 3);
    CHECK(found[0].slot == 1);
    CHECK(found[1].slot == 2);
    CHECK(found[2].slot == 3);
    for (const auto& element : found) {
        CHECK(element.kind == SelectableKind::body);
    }

    // Only faces: each face once.
    found = chooseInRegion(picture, SelectionFilter::only(SelectableKind::face));
    REQUIRE(found.size() == 3);
    CHECK(found[0].number == 5);
    CHECK(found[1].number == 6);
    CHECK(found[2].number == 1);

    found = chooseInRegion(picture, SelectionFilter::only(SelectableKind::edge));
    REQUIRE(found.size() == 1);
    CHECK(found[0].number == 3);

    CHECK(chooseInRegion(std::vector<std::uint32_t>(4, 0), SelectionFilter::all()).empty());
    SelectionFilter nothing{false, false, false, false};
    CHECK(chooseInRegion(picture, nothing).empty());
}

TEST_CASE("pick on the graphics card: a rectangle finds what is inside it", "[pick][gpu]") {
    Gpu gpu;
    if (!gpu.rhi) {
        SKIP("no graphics interface on this machine");
    }
    const auto region = [&](const QRect& area, const SelectionFilter& filter) {
        std::vector<std::uint32_t> picture;
        QRhiCommandBuffer* cb = nullptr;
        REQUIRE(gpu.rhi->beginOffscreenFrame(&cb) == QRhi::FrameOpSuccess);
        REQUIRE(gpu.renderer->pickRegion(
            cb, QSize(kWidth, kHeight), area, gpu.camera.view, gpu.camera.projection, filter,
            [&picture](std::vector<std::uint32_t> p) { picture = std::move(p); }));
        gpu.rhi->endOffscreenFrame();
        REQUIRE(picture.size() == static_cast<std::size_t>(area.width() * area.height()));
        return chooseInRegion(picture, filter);
    };
    const QPoint inside = gpu.camera.pixel(10.0F, 10.0F);

    // A rectangle on the box finds the body; one beside it finds nothing.
    auto found = region(QRect(inside.x() - 20, inside.y() - 20, 40, 40), SelectionFilter::all());
    REQUIRE(found.size() == 1);
    CHECK(gpu.renderer->bodyOfSlot(found[0].slot) == "box");
    CHECK(region(QRect(5, 5, 40, 40), SelectionFilter::all()).empty());

    // Looking down, the rectangle over the whole box sees the top face and the four edges of
    // the top; the sides are edge-on and the bottom is hidden.
    const QRect whole(gpu.camera.pixel(-1.0F, 21.0F), gpu.camera.pixel(21.0F, -1.0F));
    found = region(whole, SelectionFilter::only(SelectableKind::face));
    CHECK(found.size() == 1);
    found = region(whole, SelectionFilter::only(SelectableKind::vertex));
    CHECK(found.size() == 4);
}
