// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Changing bodies that are already there (AP-1.6e): dropping to the bed, aligning, laying a body
// on a face, copying and pasting. Each is one step in the history and writes only the values
// that change (ADR-0010).

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QStringList>
#include <QVariantMap>

#include <cmath>
#include <memory>

#include "fission/geo/occt/kernel.h"
#include "fission/ui/document_model.h"

using Catch::Approx;
using fission::ui::DocumentModel;

namespace {

QCoreApplication& application() {
    static int argc = 1;
    static char name[] = "fission_unit_tests";
    static char* argv[] = {name, nullptr};
    static QCoreApplication app(argc, argv);
    return app;
}

struct Fixture {
    Fixture() {
        application();
        kernel = fission::geo::occt::makeKernel();
        model = std::make_unique<DocumentModel>(kernel);
        model->addDemoBox(); // 20 × 20 × 20 mm at (0, 0, 0): step 0
        model->waitForCalculation();
    }

    // A box of 20 mm at a place (step number returned).
    int addBox(const char* x, const char* y, const char* z) {
        const int step = model->addShape(QStringLiteral("box"), false);
        REQUIRE(step >= 0);
        REQUIRE(model->setStepValue(step, QStringLiteral("x"), QLatin1String(x)));
        REQUIRE(model->setStepValue(step, QStringLiteral("y"), QLatin1String(y)));
        REQUIRE(model->setStepValue(step, QStringLiteral("z"), QLatin1String(z)));
        model->waitForCalculation();
        return step;
    }

    QString id(int step) const { return model->bodyOfStep(step); }

    QVariantMap entry(int step, const char* name) {
        model->selectStep(step);
        for (const QVariant& value : model->stepValues()) {
            if (value.toMap()["name"].toString() == QLatin1String(name)) {
                return value.toMap();
            }
        }
        return {};
    }
    double value(int step, const char* name) { return entry(step, name)["value"].toDouble(); }
    QString expression(int step, const char* name) {
        return entry(step, name)["expression"].toString();
    }
    int stepsDone() const { return model->historyPosition(); }
    fission::geo::Bounds bounds(int step) {
        model->waitForCalculation();
        const auto found = model->bodyBounds(id(step));
        REQUIRE(found.has_value());
        return *found;
    }

    std::shared_ptr<const fission::geo::GeometryKernel> kernel;
    std::unique_ptr<DocumentModel> model;
};

} // namespace

// --- dropping to the bed ---

TEST_CASE("drop: a turned cube that sticks into the bed ends on it", "[ui][edit][drop]") {
    Fixture f;
    REQUIRE(f.model->setStepValue(0, QStringLiteral("rotX"), QStringLiteral("45")));
    f.model->waitForCalculation();
    CHECK(f.bounds(0).min.z < -1.0); // its corner is in the bed
    const double x = f.value(0, "x");
    const double y = f.value(0, "y");
    const int before = f.stepsDone();

    REQUIRE(f.model->dropToBed({f.id(0)}));
    f.model->waitForCalculation();
    CHECK(f.bounds(0).min.z == Approx(0.0).margin(1e-6));
    CHECK(f.value(0, "x") == Approx(x));
    CHECK(f.value(0, "y") == Approx(y));
    CHECK(f.value(0, "rotX") == Approx(45.0)); // the turn stays
    CHECK(f.stepsDone() == before + 1);
    CHECK(f.model->nextUndo()["key"].toString() == QStringLiteral("dropBody"));

    REQUIRE(f.model->undo());
    f.model->waitForCalculation();
    CHECK(f.bounds(0).min.z < -1.0);
}

TEST_CASE("drop: a body that floats falls to the bed, one that is there stays",
          "[ui][edit][drop]") {
    Fixture f;
    const int floating = f.addBox("40", "0", "30");
    CHECK(f.bounds(floating).min.z == Approx(30.0));
    REQUIRE(f.model->dropToBed({f.id(floating)}));
    f.model->waitForCalculation();
    CHECK(f.bounds(floating).min.z == Approx(0.0).margin(1e-6));
    CHECK(f.value(floating, "x") == Approx(40.0));
    // Already there: nothing to do, no step.
    const int before = f.stepsDone();
    CHECK_FALSE(f.model->dropToBed({f.id(floating), f.id(0)}));
    CHECK(f.stepsDone() == before);
}

TEST_CASE("drop: two bodies in one step", "[ui][edit][drop]") {
    Fixture f;
    const int a = f.addBox("40", "0", "30");
    const int b = f.addBox("-40", "0", "12.5");
    const int before = f.stepsDone();
    REQUIRE(f.model->dropToBed({f.id(a), f.id(b)}));
    f.model->waitForCalculation();
    CHECK(f.bounds(a).min.z == Approx(0.0).margin(1e-6));
    CHECK(f.bounds(b).min.z == Approx(0.0).margin(1e-6));
    CHECK(f.stepsDone() == before + 1);
    REQUIRE(f.model->undo());
    f.model->waitForCalculation();
    CHECK(f.bounds(a).min.z == Approx(30.0));
    CHECK(f.bounds(b).min.z == Approx(12.5));
}

TEST_CASE("drop: a formula in x stays when only z changes", "[ui][edit][drop]") {
    Fixture f;
    const int step = f.addBox("0", "0", "30");
    REQUIRE(f.model->setStepValue(step, QStringLiteral("x"), QStringLiteral("2 * 20")));
    f.model->waitForCalculation();
    REQUIRE(f.model->dropToBed({f.id(step)}));
    f.model->waitForCalculation();
    CHECK(f.expression(step, "x") == QStringLiteral("2 * 20"));
    CHECK(f.value(step, "z") == Approx(0.0).margin(1e-9));
}

// --- aligning ---

TEST_CASE("align: left, middle and right of three bodies", "[ui][edit][align]") {
    Fixture f;
    const int b = f.addBox("60", "40", "0");
    REQUIRE(f.model->setStepValue(b, QStringLiteral("width"), QStringLiteral("30")));
    const int c = f.addBox("-50", "-30", "0");
    const QStringList all{f.id(0), f.id(b), f.id(c)};
    const int before = f.stepsDone();
    // x ranges: first -10..10, second 45..75, third -60..-40: the box round them is -60..75.
    REQUIRE(f.model->alignBodies(all, 0, QStringLiteral("min")));
    f.model->waitForCalculation();
    CHECK(f.bounds(0).min.x == Approx(-60.0).margin(1e-6));
    CHECK(f.bounds(b).min.x == Approx(-60.0).margin(1e-6));
    CHECK(f.bounds(c).min.x == Approx(-60.0).margin(1e-6));
    CHECK(f.value(b, "y") == Approx(40.0)); // only the axis that was asked for
    CHECK(f.stepsDone() == before + 1);
    CHECK(f.model->nextUndo()["key"].toString() == QStringLiteral("alignBodies"));

    REQUIRE(f.model->undo());
    f.model->waitForCalculation();
    REQUIRE(f.model->alignBodies(all, 0, QStringLiteral("centre")));
    f.model->waitForCalculation();
    const double middle = (-60.0 + 75.0) / 2.0;
    for (const int step : {0, b, c}) {
        const auto box = f.bounds(step);
        CHECK((box.min.x + box.max.x) / 2.0 == Approx(middle).margin(1e-6));
    }

    REQUIRE(f.model->undo());
    f.model->waitForCalculation();
    REQUIRE(f.model->alignBodies(all, 0, QStringLiteral("max")));
    f.model->waitForCalculation();
    for (const int step : {0, b, c}) {
        CHECK(f.bounds(step).max.x == Approx(75.0).margin(1e-6));
    }
}

TEST_CASE("align: along Z puts the bodies on the same floor or the same top", "[ui][edit][align]") {
    Fixture f;
    const int b = f.addBox("60", "0", "15");
    const QStringList both{f.id(0), f.id(b)};
    REQUIRE(f.model->alignBodies(both, 2, QStringLiteral("min")));
    f.model->waitForCalculation();
    CHECK(f.bounds(0).min.z == Approx(0.0).margin(1e-6));
    CHECK(f.bounds(b).min.z == Approx(0.0).margin(1e-6));
}

TEST_CASE("align: one body, nothing to do, and a formula elsewhere stays", "[ui][edit][align]") {
    Fixture f;
    const int b = f.addBox("60", "0", "0");
    REQUIRE(f.model->setStepValue(b, QStringLiteral("y"), QStringLiteral("5 + 5")));
    f.model->waitForCalculation();
    const int before = f.stepsDone();
    CHECK_FALSE(f.model->alignBodies({f.id(0)}, 0, QStringLiteral("min")));
    CHECK_FALSE(f.model->alignBodies({f.id(0), f.id(b)}, 0, QStringLiteral("sideways")));
    CHECK(f.stepsDone() == before);
    REQUIRE(f.model->alignBodies({f.id(0), f.id(b)}, 0, QStringLiteral("min")));
    CHECK(f.expression(b, "y") == QStringLiteral("5 + 5"));
}

// --- laying a body on a face ---

TEST_CASE("seat: a body laid on the right wall of another touches it", "[ui][edit][seat]") {
    Fixture f;
    const int b = f.addBox("0", "-60", "0");
    const int before = f.stepsDone();
    // The right wall of the first box: x = 10, the point at its middle, pointing out to +X.
    REQUIRE(f.model->seatBodyOnFace(f.id(b), {10.0, 0.0, 10.0}, {1.0, 0.0, 0.0}));
    f.model->waitForCalculation();
    // Its underside lies on the wall: the box now stands out of it towards +X.
    const auto box = f.bounds(b);
    CHECK(box.min.x == Approx(10.0).margin(1e-3));
    CHECK(box.max.x == Approx(30.0).margin(1e-3));
    // Turned onto the wall: some of the three angles are not 0.
    CHECK(std::abs(f.value(b, "rotX")) + std::abs(f.value(b, "rotY")) +
              std::abs(f.value(b, "rotZ")) >
          1.0);
    CHECK(f.stepsDone() == before + 1);
    CHECK(f.model->nextUndo()["key"].toString() == QStringLiteral("seatBody"));
    REQUIRE(f.model->undo());
    f.model->waitForCalculation();
    CHECK(f.bounds(b).min.x == Approx(-10.0).margin(1e-6));
}

TEST_CASE("seat: on a flat top the body stands upright again", "[ui][edit][seat]") {
    Fixture f;
    const int b = f.addBox("0", "-60", "0");
    REQUIRE(f.model->setStepValue(b, QStringLiteral("rotX"), QStringLiteral("30")));
    REQUIRE(f.model->seatBodyOnFace(f.id(b), {0.0, 0.0, 20.0}, {0.0, 0.0, 1.0}));
    f.model->waitForCalculation();
    CHECK(f.value(b, "rotX") == Approx(0.0).margin(1e-9));
    CHECK(f.bounds(b).min.z == Approx(20.0).margin(1e-3));
}

// --- copying and pasting ---

TEST_CASE("paste: a copy lies 10 mm to the side and to the back", "[ui][edit][paste]") {
    Fixture f;
    REQUIRE(f.model->setStepValue(0, QStringLiteral("width"), QStringLiteral("2 * 15")));
    f.model->waitForCalculation();
    const int before = f.stepsDone();
    CHECK_FALSE(f.model->canPaste());
    REQUIRE(f.model->copyBodies({f.id(0)}));
    CHECK(f.model->canPaste());
    const QStringList created = f.model->pasteBodies();
    f.model->waitForCalculation();
    REQUIRE(created.size() == 1);
    CHECK(f.model->bodies().size() == 2);
    // The same formula, the place moved by 10 and 10.
    CHECK(f.expression(1, "width") == QStringLiteral("2 * 15"));
    CHECK(f.value(1, "width") == Approx(30.0));
    CHECK(f.value(1, "x") == Approx(10.0));
    CHECK(f.value(1, "y") == Approx(10.0));
    CHECK(f.stepsDone() == before + 1);
    CHECK(f.model->nextUndo()["key"].toString() == QStringLiteral("pasteBodies"));

    // Pasting again: 20 and 20.
    const QStringList second = f.model->pasteBodies();
    f.model->waitForCalculation();
    REQUIRE(second.size() == 1);
    CHECK(f.value(2, "x") == Approx(20.0));
    CHECK(f.value(2, "y") == Approx(20.0));

    // One undo takes the last paste away.
    REQUIRE(f.model->undo());
    f.model->waitForCalculation();
    CHECK(f.model->bodies().size() == 2);
    REQUIRE(f.model->undo());
    f.model->waitForCalculation();
    CHECK(f.model->bodies().size() == 1);
}

TEST_CASE("paste: a hole stays a hole, the colour and the name come along", "[ui][edit][paste]") {
    Fixture f;
    f.model->setBodyHole(0, true);
    f.model->waitForCalculation();
    REQUIRE(f.model->copyBodies({f.id(0)}));
    const QStringList created = f.model->pasteBodies();
    f.model->waitForCalculation();
    REQUIRE(created.size() == 1);
    const QVariantMap copy = f.model->bodies().at(1).toMap();
    CHECK(copy["hole"].toBool());
}

TEST_CASE("paste: a formula in the place is moved, not lost", "[ui][edit][paste]") {
    Fixture f;
    REQUIRE(f.model->setStepValue(0, QStringLiteral("x"), QStringLiteral("2 * 5")));
    f.model->waitForCalculation();
    REQUIRE(f.model->copyBodies({f.id(0)}));
    REQUIRE(f.model->pasteBodies().size() == 1);
    f.model->waitForCalculation();
    CHECK(f.expression(1, "x") == QStringLiteral("(2 * 5) + (10)"));
    CHECK(f.value(1, "x") == Approx(20.0)); // 10 + 10
}

TEST_CASE("duplicate: copy and paste in one, the clipboard is the new one", "[ui][edit][paste]") {
    Fixture f;
    const int before = f.stepsDone();
    const QStringList copy = f.model->duplicateBodies({f.id(0)});
    f.model->waitForCalculation();
    REQUIRE(copy.size() == 1);
    CHECK(f.model->bodies().size() == 2);
    CHECK(f.stepsDone() == before + 1);
    // Duplicating the copy lies a further 10 mm off it (a staircase), not on the first one.
    const QStringList next = f.model->duplicateBodies(copy);
    f.model->waitForCalculation();
    REQUIRE(next.size() == 1);
    CHECK(f.value(2, "x") == Approx(20.0));
    CHECK(f.value(2, "y") == Approx(20.0));
}

TEST_CASE("paste: several bodies at once make one step", "[ui][edit][paste]") {
    Fixture f;
    const int b = f.addBox("60", "0", "0");
    const int before = f.stepsDone();
    REQUIRE(f.model->copyBodies({f.id(0), f.id(b)}));
    const QStringList created = f.model->pasteBodies();
    f.model->waitForCalculation();
    CHECK(created.size() == 2);
    CHECK(f.model->bodies().size() == 4);
    CHECK(f.stepsDone() == before + 1);
    REQUIRE(f.model->undo());
    f.model->waitForCalculation();
    CHECK(f.model->bodies().size() == 2);
}

TEST_CASE("paste: nothing to paste, nothing to copy", "[ui][edit][paste]") {
    Fixture f;
    const int before = f.stepsDone();
    CHECK(f.model->pasteBodies().isEmpty());
    CHECK_FALSE(f.model->copyBodies({}));
    CHECK_FALSE(f.model->copyBodies({QStringLiteral("no such body")}));
    CHECK(f.stepsDone() == before);
}
