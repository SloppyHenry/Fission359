// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The pattern from the interface (AP-2.8b): one step and one undo, the starting values, the way
// and the copies left out, the arrow of a row.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QVariantMap>

#include <chrono>
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

struct PatternWorld {
    PatternWorld() {
        application();
        kernel = fission::geo::occt::makeKernel();
        model = std::make_unique<DocumentModel>(kernel);
        model->addDemoBox(); // 20 x 20 x 20 mm: step 0
        model->waitForCalculation();
        body = model->bodyOfStep(0);
    }

    QVariantMap step(int index) const { return model->recipe().at(index).toMap(); }

    double volumeOf(const QString& bodyId) const {
        for (const auto& [id, made] : model->result()->bodies) {
            if (QString::fromStdString(id.value) == bodyId) {
                return kernel->measure(made.solid).volume;
            }
        }
        return -1.0;
    }

    QVariantMap value(const char* name) const {
        for (const QVariant& entry : model->stepValues()) {
            if (entry.toMap().value("name").toString() == QLatin1String(name)) {
                return entry.toMap();
            }
        }
        return {};
    }

    std::shared_ptr<const fission::geo::GeometryKernel> kernel;
    std::unique_ptr<DocumentModel> model;
    QString body;
};

} // namespace

TEST_CASE("pattern ui: a row of three, one body width and 5 mm apart", "[ui][pattern]") {
    PatternWorld f;
    const int stepsBefore = static_cast<int>(f.model->recipe().size());
    const QString made = f.model->addPattern(f.body, QStringLiteral("linear"));
    REQUIRE_FALSE(made.isEmpty());
    f.model->waitForCalculation();
    CHECK(static_cast<int>(f.model->recipe().size()) == stepsBefore + 1);
    CHECK(f.step(stepsBefore).value("type").toString() == "pattern");
    CHECK(f.step(stepsBefore).value("state").toString() == "ok");
    CHECK(f.value("count").value("value").toDouble() == Approx(3.0));
    CHECK(f.value("spacing").value("value").toDouble() == Approx(25.0)); // 20 + 5
    CHECK(f.volumeOf(made) == Approx(3 * 8000.0).epsilon(1e-9));
    // The step is chosen, its arrow stands at the second copy and the internal values are hidden.
    CHECK(f.model->currentStep() == stepsBefore);
    REQUIRE(f.model->stepHandles().size() == 1);
    const QVariantMap arrow = f.model->stepHandles().front().toMap();
    CHECK(arrow.value("parameter").toString() == "spacing");
    CHECK(arrow.value("dx").toDouble() == Approx(1.0));
    CHECK(f.value("dirX").value("group").toString() == "hidden");
    CHECK(f.value("anchorX").value("group").toString() == "hidden");
    CHECK(f.value("axisX").value("group").toString() == "hidden"); // only a circle shows it
    CHECK(f.value("total").value("group").toString() == "hidden");
    // One undo takes the whole step away.
    REQUIRE(f.model->undo());
    f.model->waitForCalculation();
    CHECK(static_cast<int>(f.model->recipe().size()) == stepsBefore);
}

TEST_CASE("pattern ui: a circle of six, the axis steps aside, no arrow", "[ui][pattern]") {
    PatternWorld f;
    const int stepsBefore = static_cast<int>(f.model->recipe().size());
    const QString made = f.model->addPattern(f.body, QStringLiteral("circular"));
    REQUIRE_FALSE(made.isEmpty());
    f.model->waitForCalculation();
    CHECK(f.step(stepsBefore).value("state").toString() == "ok");
    CHECK(f.value("count").value("value").toDouble() == Approx(6.0));
    CHECK(f.value("total").value("value").toDouble() == Approx(360.0));
    CHECK(f.value("total").value("group").toString() == "size");
    CHECK(f.value("spacing").value("group").toString() == "hidden");
    CHECK(f.value("axisX").value("group").toString() == "position");
    CHECK(f.model->stepHandles().isEmpty());
    // Six copies that do not touch: the axis is a body diagonal and 5 mm away.
    CHECK(f.volumeOf(made) == Approx(6 * 8000.0).epsilon(1e-6));
}

TEST_CASE("pattern ui: only one body, never one that another step uses", "[ui][pattern]") {
    PatternWorld f;
    CHECK(f.model->addPattern(QStringLiteral("nobody"), QStringLiteral("linear")).isEmpty());
    CHECK(f.model->addPattern(f.body, QStringLiteral("wobble")).isEmpty());
    const QString made = f.model->addPattern(f.body, QStringLiteral("linear"));
    REQUIRE_FALSE(made.isEmpty());
    f.model->waitForCalculation();
    CHECK(f.model->addPattern(f.body, QStringLiteral("linear")).isEmpty()); // used by the first
}

TEST_CASE("pattern ui: the way is one step of the history", "[ui][pattern]") {
    PatternWorld f;
    REQUIRE_FALSE(f.model->addPattern(f.body, QStringLiteral("linear")).isEmpty());
    f.model->waitForCalculation();
    const int index = f.model->currentStep();
    CHECK_FALSE(f.model->setPatternDirection(index, false, 3, 1));
    CHECK_FALSE(f.model->setPatternDirection(index, true, 0, -1)); // a circle has no minus
    CHECK_FALSE(f.model->setPatternDirection(0, false, 1, 1));     // not a pattern step
    REQUIRE(f.model->setPatternDirection(index, false, 1, -1));
    f.model->waitForCalculation();
    CHECK(f.value("dirX").value("value").toDouble() == Approx(0.0));
    CHECK(f.value("dirY").value("value").toDouble() == Approx(-1.0));
    CHECK(f.step(index).value("state").toString() == "ok");
    REQUIRE(f.model->undo()); // one undo brings back all three numbers
    f.model->waitForCalculation();
    CHECK(f.value("dirX").value("value").toDouble() == Approx(1.0));
    CHECK(f.value("dirY").value("value").toDouble() == Approx(0.0));
}

TEST_CASE("pattern ui: leaving a copy out and the mode are single steps", "[ui][pattern]") {
    PatternWorld f;
    const QString made = f.model->addPattern(f.body, QStringLiteral("linear"));
    REQUIRE_FALSE(made.isEmpty());
    f.model->waitForCalculation();
    const int index = f.model->currentStep();
    REQUIRE(f.model->setStepText(index, QStringLiteral("skip"), QStringLiteral("1")));
    f.model->waitForCalculation();
    CHECK(f.volumeOf(made) == Approx(2 * 8000.0).epsilon(1e-9));
    REQUIRE(f.model->undo());
    f.model->waitForCalculation();
    CHECK(f.volumeOf(made) == Approx(3 * 8000.0).epsilon(1e-9));
    // A skip text that is no list of numbers is a problem of the step, not a crash.
    REQUIRE(f.model->setStepText(index, QStringLiteral("skip"), QStringLiteral("x")));
    f.model->waitForCalculation();
    CHECK(f.step(index).value("state").toString() == "fails");
    REQUIRE(f.model->undo());
    REQUIRE(f.model->setStepText(index, QStringLiteral("mode"), QStringLiteral("circular")));
    f.model->waitForCalculation();
    CHECK(f.step(index).value("state").toString() == "ok");
}

TEST_CASE("pattern ui: one copy and a part of a circle do not crash", "[ui][pattern]") {
    PatternWorld f;
    const QString made = f.model->addPattern(f.body, QStringLiteral("linear"));
    REQUIRE_FALSE(made.isEmpty());
    f.model->waitForCalculation();
    const int index = f.model->currentStep();
    REQUIRE(f.model->setStepValue(index, QStringLiteral("count"), QStringLiteral("1")));
    f.model->waitForCalculation();
    CHECK(f.step(index).value("state").toString() == "ok");
    CHECK(f.volumeOf(made) == Approx(8000.0).epsilon(1e-9));
    REQUIRE(f.model->setStepText(index, QStringLiteral("mode"), QStringLiteral("circular")));
    REQUIRE(f.model->setStepValue(index, QStringLiteral("total"), QStringLiteral("90")));
    f.model->waitForCalculation();
    // One copy over a part of a circle has no way to be spread: a problem, not a crash.
    const QString state = f.step(index).value("state").toString();
    CHECK((state == "ok" || state == "fails"));
}

TEST_CASE("pattern ui: changing the body changes every copy", "[ui][pattern]") {
    PatternWorld f;
    const QString made = f.model->addPattern(f.body, QStringLiteral("linear"));
    REQUIRE_FALSE(made.isEmpty());
    f.model->waitForCalculation();
    REQUIRE(f.model->setStepValue(0, QStringLiteral("height"), QStringLiteral("10")));
    f.model->waitForCalculation();
    CHECK(f.volumeOf(made) == Approx(3 * 4000.0).epsilon(1e-9));
}

TEST_CASE("pattern ui: a hundred copies in time", "[ui][pattern]") {
    PatternWorld f;
    const QString made = f.model->addPattern(f.body, QStringLiteral("linear"));
    REQUIRE_FALSE(made.isEmpty());
    f.model->waitForCalculation();
    const int index = f.model->currentStep();
    const auto start = std::chrono::steady_clock::now();
    REQUIRE(f.model->setStepValue(index, QStringLiteral("count"), QStringLiteral("100")));
    f.model->waitForCalculation();
    const double seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    CHECK(f.step(index).value("state").toString() == "ok");
    CHECK(f.volumeOf(made) == Approx(100 * 8000.0).epsilon(1e-9));
#ifdef NDEBUG
    CHECK(seconds < 3.0);
#else
    CHECK(seconds < 30.0);
#endif
}
