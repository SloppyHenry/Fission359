// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The corner handles of the tool "Size" (AP-1.6b2): a corner makes the chosen body larger or
// smaller in all directions alike. Every size of the step changes by the same factor (values,
// never the geometry, rule 11); the corner across at the bottom stays where it is. One drag is
// one step in the history, rests on whole percent and can be typed instead.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QPointF>
#include <QVariantMap>

#include <algorithm>
#include <cmath>
#include <memory>

#include "fission/geo/occt/kernel.h"
#include "fission/ui/document_model.h"
#include "fission/ui/scale_controller.h"
#include "fission/ui/viewport_3d.h"

using Catch::Approx;
using fission::ui::DocumentModel;
using fission::ui::ScaleController;
using fission::ui::Viewport3D;

namespace {

QCoreApplication& application() {
    static int argc = 1;
    static char name[] = "fission_unit_tests";
    static char* argv[] = {name, nullptr};
    static QCoreApplication app(argc, argv);
    return app;
}

struct Fixture {
    explicit Fixture(double yaw = 35.0, double pitch = 30.0, double distance = 160.0) {
        application();
        kernel = fission::geo::occt::makeKernel();
        model = std::make_unique<DocumentModel>(kernel);
        model->addDemoBox(); // 20 × 20 × 20 mm at (0, 0, 0)
        model->waitForCalculation();
        viewport.setProperty("animated", false);
        viewport.setWidth(1000.0);
        viewport.setHeight(600.0);
        viewport.setCamera(yaw, pitch, distance);
        viewport.setScene(model->scene());
        model->selectStep(0);
        corners = std::make_unique<ScaleController>(model.get());
        corners->attach(&viewport);
        corners->setEnabled(true);
    }

    void refresh() {
        model->waitForCalculation();
        viewport.setScene(model->scene());
        corners->refresh();
    }

    QVariantMap valueEntry(const QString& name) const {
        for (const QVariant& entry : model->stepValues()) {
            if (entry.toMap()["name"].toString() == name) {
                return entry.toMap();
            }
        }
        return {};
    }
    double value(const char* name) const {
        return valueEntry(QLatin1String(name))["value"].toDouble();
    }
    QString expression(const char* name) const {
        return valueEntry(QLatin1String(name))["expression"].toString();
    }
    int stepsDone() const { return model->historyPosition(); }

    QPointF corner(int index) const {
        const QVariantMap entry = corners->corners().at(index).toMap();
        return {entry["x"].toDouble(), entry["y"].toDouble()};
    }

    // Takes a corner and pulls it `pixels` away from the corner across (on the screen; negative
    // pulls it in), in 8 small moves.
    void pull(int index, double pixels, bool release = true) {
        const QPointF from = corner(index);
        const QPointF across = corner((index + 2) % 4);
        QPointF way = from - across;
        way /= std::hypot(way.x(), way.y());
        REQUIRE(corners->beginDrag(index, from.x(), from.y()));
        for (int i = 1; i <= 8; ++i) {
            const QPointF at = from + way * (pixels * i / 8.0);
            corners->dragTo(at.x(), at.y());
        }
        if (release) {
            REQUIRE(corners->endDrag());
        }
        refresh();
    }

    // The box round the body as it is on screen now.
    struct Extent {
        double minX = 1e30, maxX = -1e30, minY = 1e30, maxY = -1e30, minZ = 1e30, maxZ = -1e30;
    };
    Extent extent() const {
        Extent box;
        for (const auto& body : model->scene()->bodies) {
            for (const auto& p : body.mesh->positions) {
                box.minX = std::min<double>(box.minX, p.x);
                box.maxX = std::max<double>(box.maxX, p.x);
                box.minY = std::min<double>(box.minY, p.y);
                box.maxY = std::max<double>(box.maxY, p.y);
                box.minZ = std::min<double>(box.minZ, p.z);
                box.maxZ = std::max<double>(box.maxZ, p.z);
            }
        }
        return box;
    }

    std::shared_ptr<const fission::geo::GeometryKernel> kernel;
    std::unique_ptr<DocumentModel> model;
    Viewport3D viewport;
    std::unique_ptr<ScaleController> corners;
};

} // namespace

TEST_CASE("scale: the tool shows a square on each top corner of the box round the body",
          "[ui][scale]") {
    Fixture f;
    REQUIRE(f.corners->corners().size() == 4);
    const auto& drawn = f.viewport.handleGraphics(Viewport3D::HandleSource::corners);
    REQUIRE(drawn.dots.size() == 4);
    for (const auto& dot : drawn.dots) {
        CHECK(dot.square);
        CHECK(dot.centre.z() == Approx(20.0).margin(1e-3)); // on top
        CHECK(std::abs(dot.centre.x()) == Approx(10.0).margin(1e-3));
        CHECK(std::abs(dot.centre.y()) == Approx(10.0).margin(1e-3));
    }
    CHECK_FALSE(drawn.lines.empty()); // the dashed box
    for (int i = 0; i < 4; ++i) {
        CHECK(f.corners->corners().at(i).toMap()["shown"].toBool());
    }
    // Where each square is on the screen is where the window puts its place to grab.
    const QPointF frontRight = f.corner(1);
    const QPointF projected = f.viewport.project(10.0, -10.0, 20.0);
    CHECK(frontRight.x() == Approx(projected.x()).margin(0.5));
    CHECK(frontRight.y() == Approx(projected.y()).margin(0.5));

    f.corners->setEnabled(false); // another tool
    CHECK(f.viewport.handleGraphics(Viewport3D::HandleSource::corners).empty());
    CHECK_FALSE(f.corners->corners().at(0).toMap()["shown"].toBool());
    CHECK_FALSE(f.corners->beginDrag(1, frontRight.x(), frontRight.y()));
}

TEST_CASE("scale: pulling a corner makes every size larger alike, the corner across stays",
          "[ui][scale]") {
    Fixture f;
    REQUIRE(f.model->setStepValue(0, QStringLiteral("z"), QStringLiteral("5 - 5")));
    f.refresh(); // a formula that comes out as 0: the body stands on the bed
    const int before = f.stepsDone();

    f.pull(1, 80.0); // the front right corner, outwards

    const double factor = f.value("width") / 20.0;
    CHECK(factor > 1.2);
    // On whole percent, and the same for every size.
    CHECK(factor * 100.0 == Approx(std::round(factor * 100.0)).margin(1e-9));
    CHECK(f.value("depth") == Approx(20.0 * factor));
    CHECK(f.value("height") == Approx(20.0 * factor));
    CHECK(f.value("cornerRadius") == Approx(0.0)); // a sharp corner stays sharp
    // The back left corner at the bottom stayed where it was; the body is still on the bed.
    const auto box = f.extent();
    CHECK(box.minX == Approx(-10.0).margin(1e-3));
    CHECK(box.maxY == Approx(10.0).margin(1e-3));
    CHECK(box.minZ == Approx(0.0).margin(1e-3));
    CHECK(box.maxX == Approx(-10.0 + 20.0 * factor).margin(1e-3));
    // The height of the place did not move: the formula there stays.
    CHECK(f.expression("z") == QStringLiteral("5 - 5"));
    // One step, with the percentage in its name, and no new step in the recipe.
    CHECK(f.stepsDone() == before + 1);
    const QVariantMap step = f.model->nextUndo();
    CHECK(step["key"].toString() == QStringLiteral("scaleBody"));
    CHECK(step["value"].toString().toDouble() == Approx(factor * 100.0));
    CHECK(f.model->recipe().size() == 1);

    REQUIRE(f.model->undo());
    f.refresh();
    CHECK(f.value("width") == Approx(20.0));
    CHECK(f.value("x") == Approx(0.0));
    CHECK(f.value("y") == Approx(0.0));
}

TEST_CASE("scale: pulling a corner in makes the body smaller, still on the bed", "[ui][scale]") {
    Fixture f;
    f.pull(3, -40.0); // the back left corner, inwards
    const double factor = f.value("width") / 20.0;
    CHECK(factor < 0.9);
    CHECK(f.value("height") == Approx(20.0 * factor));
    const auto box = f.extent();
    // The front right corner at the bottom stays.
    CHECK(box.maxX == Approx(10.0).margin(1e-3));
    CHECK(box.minY == Approx(-10.0).margin(1e-3));
    CHECK(box.minZ == Approx(0.0).margin(1e-3));
    CHECK(f.model->nextUndo()["value"].toString().toDouble() < 90.0);
}

TEST_CASE("scale: the corner held stays under the pointer", "[ui][scale]") {
    Fixture f;
    // Along the line from the corner across at the bottom (back left, which stays) through the
    // front right one at the top, half as far again.
    const QPointF from = f.corner(1);
    const QPointF across = f.viewport.project(-10.0, 10.0, 0.0);
    const QPointF to = from + (from - across) * 0.5;
    REQUIRE(f.corners->beginDrag(1, from.x(), from.y()));
    f.corners->dragTo(to.x(), to.y());
    // While dragging the square goes with the factor at once.
    const QPointF held = f.corner(1);
    CHECK(std::hypot(held.x() - to.x(), held.y() - to.y()) < 6.0);
    REQUIRE(f.corners->endDrag());
    f.refresh();
    const QPointF after = f.corner(1); // from the new body
    CHECK(std::hypot(after.x() - to.x(), after.y() - to.y()) < 6.0);
}

TEST_CASE("scale: rounded corners grow with the box and stay round", "[ui][scale]") {
    // Ablauf D, step 1, for the corner handle: 5 mm corners at 200 % are 10 mm corners.
    Fixture f;
    REQUIRE(f.model->setStepValue(0, QStringLiteral("width"), QStringLiteral("40")));
    REQUIRE(f.model->setStepValue(0, QStringLiteral("depth"), QStringLiteral("40")));
    REQUIRE(f.model->setStepValue(0, QStringLiteral("height"), QStringLiteral("10")));
    REQUIRE(f.model->setStepValue(0, QStringLiteral("cornerRadius"), QStringLiteral("5")));
    f.refresh();
    f.pull(1, 30.0, /*release=*/false);
    REQUIRE(f.corners->typeInsteadOfDrag());
    REQUIRE(f.corners->setPercent(200.0));
    REQUIRE(f.corners->commitEdit());
    f.refresh();
    CHECK(f.valueEntry(QStringLiteral("cornerRadius"))["value"].toDouble() == Approx(10.0));
    CHECK(f.value("width") == Approx(80.0));
    CHECK(f.value("height") == Approx(20.0));
    // The numbers are numbers again, no formula with a factor.
    CHECK(f.expression("width").isEmpty());
    CHECK(f.expression("cornerRadius").isEmpty());
    const auto state = f.model->result()->features.front();
    CHECK(state.status == fission::core::FeatureStatus::ok);
    CHECK(f.model->nextUndo()["value"].toString() == QStringLiteral("200"));
}

TEST_CASE("scale: typing replaces dragging, Esc takes everything back", "[ui][scale]") {
    Fixture f;
    const int before = f.stepsDone();
    f.pull(1, 50.0, /*release=*/false);
    CHECK(f.corners->editing());
    REQUIRE(f.corners->typeInsteadOfDrag());
    REQUIRE(f.corners->setPercent(150.0));
    CHECK(f.corners->factor() == Approx(1.5));
    REQUIRE(f.corners->size().size() == 3);
    CHECK(f.corners->size()[0].toDouble() == Approx(30.0).margin(1e-3));
    f.refresh();
    CHECK(f.value("width") == Approx(30.0));
    // What the shape cannot have is refused, and nothing changes.
    CHECK_FALSE(f.corners->setPercent(0.0));
    CHECK_FALSE(f.corners->setPercent(-50.0));
    CHECK(f.corners->factor() == Approx(1.5));
    f.corners->cancelEdit();
    f.refresh();
    CHECK_FALSE(f.corners->editing());
    CHECK(f.value("width") == Approx(20.0));
    CHECK(f.value("x") == Approx(0.0));
    CHECK(f.stepsDone() == before);

    // Esc while dragging, and taking a corner without moving, leave no step either.
    f.pull(1, 50.0, /*release=*/false);
    f.corners->cancelDrag();
    f.refresh();
    CHECK(f.value("width") == Approx(20.0));
    const QPointF at = f.corner(1);
    REQUIRE(f.corners->beginDrag(1, at.x(), at.y()));
    REQUIRE(f.corners->endDrag());
    CHECK(f.stepsDone() == before);
}

TEST_CASE("scale: pulled smaller than possible, the corner stops at the smallest size",
          "[ui][scale]") {
    // Pulled far in, past nothing: the corner stops on the smallest whole percent the shape can
    // have (1 % of 20 mm is 0.2 mm), it does not stay behind where it was.
    Fixture f;
    f.pull(1, -2000.0);
    const double factor = f.value("width") / 20.0;
    CHECK(factor == Approx(0.01).margin(1e-9));
    CHECK(f.value("height") == Approx(0.2));
    CHECK(f.model->result()->features.front().status == fission::core::FeatureStatus::ok);
}

TEST_CASE("scale: a turned body grows about the corner of its box, which stays on the bed",
          "[ui][scale]") {
    Fixture f;
    // A flat plate, tipped 30 degrees about X, then put back on the bed by hand.
    REQUIRE(f.model->setStepValue(0, QStringLiteral("width"), QStringLiteral("40")));
    REQUIRE(f.model->setStepValue(0, QStringLiteral("depth"), QStringLiteral("30")));
    REQUIRE(f.model->setStepValue(0, QStringLiteral("height"), QStringLiteral("6")));
    REQUIRE(f.model->setStepValue(0, QStringLiteral("rotX"), QStringLiteral("30")));
    f.refresh();
    const auto bounds = f.model->currentBodyBounds();
    REQUIRE(bounds.has_value());
    const double lift = -bounds->min.z;
    REQUIRE(f.model->setStepValue(0, QStringLiteral("z"), QString::number(lift, 'g', 15)));
    f.refresh();
    const auto start = f.model->currentBodyBounds();
    REQUIRE(start.has_value());
    CHECK(start->min.z == Approx(0.0).margin(1e-9));

    f.pull(1, 60.0);

    const double factor = f.value("width") / 40.0;
    CHECK(factor > 1.1);
    CHECK(f.value("rotX") == Approx(30.0)); // the turn is no size
    const auto after = f.model->currentBodyBounds();
    REQUIRE(after.has_value());
    // The corner across at the bottom (back left) stays, exactly; the rest grew alike.
    CHECK(after->min.x == Approx(start->min.x).margin(1e-6));
    CHECK(after->max.y == Approx(start->max.y).margin(1e-6));
    CHECK(after->min.z == Approx(0.0).margin(1e-6));
    CHECK(after->max.x - after->min.x == Approx((start->max.x - start->min.x) * factor));
    CHECK(after->max.z - after->min.z == Approx((start->max.z - start->min.z) * factor));
}

TEST_CASE("scale: a sphere grows from the corner of its box", "[ui][scale]") {
    Fixture f;
    const int sphere = f.model->addShape(QStringLiteral("sphere"), false); // radius 10
    REQUIRE(sphere >= 0);
    f.model->selectStep(sphere);
    f.refresh();
    const auto start = f.model->currentBodyBounds();
    REQUIRE(start.has_value());
    CHECK(start->max.z - start->min.z == Approx(20.0).margin(1e-6));
    f.pull(2, 40.0);
    const double factor = f.value("radius") / 10.0;
    CHECK(factor > 1.1);
    const auto after = f.model->currentBodyBounds();
    REQUIRE(after.has_value());
    CHECK(after->min.x == Approx(start->min.x).margin(1e-6));
    CHECK(after->min.y == Approx(start->min.y).margin(1e-6));
    CHECK(after->min.z == Approx(0.0).margin(1e-6));
    CHECK(after->max.x - after->min.x == Approx(20.0 * factor).margin(1e-6));
}

TEST_CASE("scale: a turned body away from the middle grows about its own corner", "[ui][scale]") {
    Fixture f;
    REQUIRE(f.model->setStepValue(0, QStringLiteral("x"), QStringLiteral("-30")));
    REQUIRE(f.model->setStepValue(0, QStringLiteral("y"), QStringLiteral("-30")));
    REQUIRE(f.model->setStepValue(0, QStringLiteral("rotZ"), QStringLiteral("30")));
    f.refresh();
    const auto start = f.model->currentBodyBounds();
    REQUIRE(start.has_value());
    f.pull(1, 30.0, /*release=*/false);
    REQUIRE(f.corners->typeInsteadOfDrag());
    REQUIRE(f.corners->setPercent(150.0));
    REQUIRE(f.corners->commitEdit());
    f.refresh();
    CHECK(f.value("width") == Approx(30.0));
    const auto after = f.model->currentBodyBounds();
    REQUIRE(after.has_value());
    CHECK(after->min.x == Approx(start->min.x).margin(1e-6));
    CHECK(after->max.y == Approx(start->max.y).margin(1e-6));
    CHECK(after->min.z == Approx(0.0).margin(1e-6));
    CHECK(f.value("x") < -20.0);
    CHECK(f.value("y") < -30.0);
}
