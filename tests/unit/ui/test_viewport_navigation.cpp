// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Plays every mouse style through the real 3D view item with synthetic input: which gesture
// turns, moves or zooms the view (AP-1.3, acceptance criterion "all presets are played
// through in a test script").

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QMouseEvent>
#include <QNativeGestureEvent>
#include <QPointingDevice>
#include <QWheelEvent>

#include <cmath>
#include <memory>

#include "fission/render/navigation.h"
#include "fission/ui/viewport_3d.h"

using Catch::Approx;
using fission::render::kMouseStyles;
using fission::render::mouseStyleName;
using fission::render::NavigationAction;
using fission::ui::Viewport3D;

namespace {

QCoreApplication& application() {
    static int argc = 1;
    static char name[] = "fission_unit_tests";
    static char* argv[] = {name, nullptr};
    static QCoreApplication app(argc, argv);
    return app;
}

// Makes the protected input handlers reachable.
class TestViewport : public Viewport3D {
public:
    using Viewport3D::event;
    using Viewport3D::mouseDoubleClickEvent;
    using Viewport3D::mouseMoveEvent;
    using Viewport3D::mousePressEvent;
    using Viewport3D::wheelEvent;

    TestViewport() {
        setProperty("animated", false);
        setWidth(1000.0);
        setHeight(600.0);
    }

    void drag(Qt::MouseButton button, Qt::KeyboardModifiers modifiers, QPointF delta) {
        const QPointF start(500.0, 300.0);
        const auto* device = QPointingDevice::primaryPointingDevice();
        QMouseEvent press(QEvent::MouseButtonPress, start, start, button, button, modifiers, device);
        mousePressEvent(&press);
        const QPointF end = start + delta;
        QMouseEvent move(QEvent::MouseMove, end, end, Qt::NoButton, button, modifiers, device);
        mouseMoveEvent(&move);
    }

    void wheel(QPoint angle, QPoint pixels, Qt::KeyboardModifiers modifiers) {
        const QPointF at(500.0, 300.0);
        QWheelEvent event(at, at, pixels, angle, Qt::NoButton, modifiers, Qt::NoScrollPhase, false);
        wheelEvent(&event);
    }

    void doubleClick(QPointF at) {
        const auto* device = QPointingDevice::primaryPointingDevice();
        QMouseEvent click(QEvent::MouseButtonDblClick, at, at, Qt::LeftButton, Qt::LeftButton,
                          Qt::NoModifier, device);
        mouseDoubleClickEvent(&click);
    }
};

struct View {
    double yaw, pitch, scale;
    QPointF origin;
};

View look(const TestViewport& viewport) {
    return {viewport.cameraYaw(), viewport.cameraPitch(), viewport.pixelsPerMillimetre(),
            viewport.project(0, 0, 0)};
}

// What a gesture did to the view, read from what changed.
NavigationAction observed(const View& before, const View& after) {
    if (std::abs(after.yaw - before.yaw) > 0.01 || std::abs(after.pitch - before.pitch) > 0.01) {
        return NavigationAction::orbit;
    }
    if (std::abs(after.scale - before.scale) > 1e-4) {
        return NavigationAction::zoom;
    }
    if ((after.origin - before.origin).manhattanLength() > 0.5) {
        return NavigationAction::pan;
    }
    return NavigationAction::none;
}

NavigationAction played(const QString& style, Qt::MouseButton button,
                        Qt::KeyboardModifiers modifiers) {
    TestViewport viewport;
    viewport.setMouseStyle(style);
    const View before = look(viewport);
    viewport.drag(button, modifiers, {40.0, -25.0});
    return observed(before, look(viewport));
}

// One cube of 10 mm on the bed, every face with its own vertices and number (1..6).
std::shared_ptr<const fission::render::RenderScene> cubeScene() {
    auto scene = std::make_shared<fission::render::RenderScene>();
    fission::render::RenderBody body;
    auto owned = std::make_shared<fission::geo::TriangleMesh>();
    auto& mesh = *owned;
    struct Side {
        float nx, ny, nz;
        float u[3];
        float v[3];
    };
    const Side sides[] = {
        {0, 0, 1, {1, 0, 0}, {0, 1, 0}},   {0, 0, -1, {1, 0, 0}, {0, -1, 0}},
        {1, 0, 0, {0, 1, 0}, {0, 0, 1}},   {-1, 0, 0, {0, -1, 0}, {0, 0, 1}},
        {0, -1, 0, {1, 0, 0}, {0, 0, 1}},  {0, 1, 0, {-1, 0, 0}, {0, 0, 1}},
    };
    std::uint32_t faceId = 1;
    for (const Side& side : sides) {
        const std::uint32_t first = static_cast<std::uint32_t>(mesh.positions.size());
        // centre of the face, then the four corners in order
        const float cx = 5.0F * side.nx;
        const float cy = 5.0F * side.ny;
        const float cz = 5.0F + 5.0F * side.nz;
        const int signs[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
        for (const auto& s : signs) {
            mesh.positions.push_back({cx + 5.0F * (s[0] * side.u[0] + s[1] * side.v[0]),
                                      cy + 5.0F * (s[0] * side.u[1] + s[1] * side.v[1]),
                                      cz + 5.0F * (s[0] * side.u[2] + s[1] * side.v[2])});
            mesh.normals.push_back({side.nx, side.ny, side.nz});
        }
        mesh.indices.insert(mesh.indices.end(), {first, first + 1, first + 2, first, first + 2,
                                                 first + 3});
        mesh.faceIds.insert(mesh.faceIds.end(), {faceId, faceId});
        ++faceId;
    }
    body.mesh = owned;
    scene->bodies.push_back(std::move(body));
    return scene;
}

} // namespace

TEST_CASE("every mouse style does what the program it imitates does", "[viewport][navigation]") {
    application();
    using A = NavigationAction;
    const auto none = Qt::NoModifier;
    const auto shift = Qt::ShiftModifier;
    const auto ctrl = Qt::ControlModifier;

    struct Case {
        const char* style;
        Qt::MouseButton button;
        Qt::KeyboardModifiers modifiers;
        NavigationAction expected;
    };
    const Case cases[] = {
        {"fission", Qt::RightButton, none, A::orbit},  {"fission", Qt::MiddleButton, none, A::pan},
        {"fission", Qt::RightButton, shift, A::pan},   {"fission", Qt::LeftButton, none, A::none},
        {"fusion", Qt::MiddleButton, none, A::pan},    {"fusion", Qt::MiddleButton, shift, A::orbit},
        {"fusion", Qt::RightButton, none, A::none},    {"tinkercad", Qt::RightButton, none, A::orbit},
        {"tinkercad", Qt::RightButton, shift, A::pan}, {"tinkercad", Qt::MiddleButton, none, A::pan},
        {"blender", Qt::MiddleButton, none, A::orbit}, {"blender", Qt::MiddleButton, shift, A::pan},
        {"blender", Qt::MiddleButton, ctrl, A::zoom},  {"blender", Qt::RightButton, none, A::none},
        {"sketchup", Qt::MiddleButton, none, A::orbit}, {"sketchup", Qt::MiddleButton, shift, A::pan},
        {"sketchup", Qt::RightButton, none, A::none},
    };
    for (const Case& c : cases) {
        INFO("style " << c.style << ", button " << int(c.button) << ", modifiers "
                      << int(c.modifiers));
        CHECK(played(QString::fromLatin1(c.style), c.button, c.modifiers) == c.expected);
    }
}

TEST_CASE("the wheel zooms in every style, a trackpad turns and moves, a pinch zooms",
          "[viewport][navigation]") {
    application();
    for (const auto style : kMouseStyles) {
        INFO("style " << mouseStyleName(style));
        TestViewport viewport;
        viewport.setMouseStyle(QString::fromUtf8(mouseStyleName(style).data()));

        View before = look(viewport);
        viewport.wheel({0, 120}, {}, Qt::NoModifier); // one notch away from the user: closer
        CHECK(look(viewport).scale > before.scale);

        before = look(viewport);
        viewport.wheel({0, 0}, {30, 20}, Qt::NoModifier); // two fingers
        CHECK(observed(before, look(viewport)) == NavigationAction::orbit);

        before = look(viewport);
        viewport.wheel({0, 0}, {30, 20}, Qt::ShiftModifier);
        CHECK(observed(before, look(viewport)) == NavigationAction::pan);

        before = look(viewport);
        const auto* device = QPointingDevice::primaryPointingDevice();
        QNativeGestureEvent spread(Qt::ZoomNativeGesture, device, 2, QPointF(500, 300),
                                   QPointF(500, 300), QPointF(500, 300), 0.2, QPointF());
        viewport.event(&spread);
        CHECK(look(viewport).scale > before.scale);
    }
}

TEST_CASE("an unknown style name changes nothing", "[viewport][navigation]") {
    application();
    TestViewport viewport;
    viewport.setMouseStyle(QStringLiteral("fusion"));
    viewport.setMouseStyle(QStringLiteral("maya"));
    CHECK(viewport.mouseStyle() == QStringLiteral("fusion"));
}

TEST_CASE("a double click on a face looks at it squarely", "[viewport][navigation]") {
    application();
    TestViewport viewport;
    viewport.setScene(cubeScene());

    // Home view: the top, the front and the right side are visible.
    viewport.setStandardView(QStringLiteral("home"));
    const QPointF top = viewport.project(0, 0, 10);
    viewport.doubleClick(top);
    CHECK(viewport.cameraPitch() == Approx(89.5).margin(0.01));

    viewport.setStandardView(QStringLiteral("home"));
    const QPointF right = viewport.project(5, 0, 5);
    viewport.doubleClick(right);
    CHECK(viewport.cameraPitch() == Approx(0.0).margin(0.01));
    CHECK(viewport.cameraYaw() == Approx(-90.0).margin(0.01));

    viewport.setStandardView(QStringLiteral("home"));
    const QPointF front = viewport.project(0, -5, 5);
    viewport.doubleClick(front);
    CHECK(viewport.cameraPitch() == Approx(0.0).margin(0.01));
    CHECK(viewport.cameraYaw() == Approx(0.0).margin(0.01));
}

TEST_CASE("a double click beside the body looks down at the bed, beyond the bed nothing happens",
          "[viewport][navigation]") {
    application();
    TestViewport viewport;
    viewport.setScene(cubeScene());
    viewport.setStandardView(QStringLiteral("home"));
    viewport.doubleClick(viewport.project(60, 60, 0));
    CHECK(viewport.cameraPitch() == Approx(89.5).margin(0.01));

    viewport.setStandardView(QStringLiteral("home"));
    const View before = look(viewport);
    viewport.doubleClick({2.0, 2.0}); // the corner of the view: only background
    const View after = look(viewport);
    CHECK(observed(before, after) == NavigationAction::none);
}
