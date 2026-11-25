// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The mirror from the interface (AP-2.8c): one undo step, the plane on the right side of the body,
// the plane turned to another axis, the single number and the arrow.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QVariantMap>

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

struct MirrorWorld {
    MirrorWorld() {
        application();
        kernel = fission::geo::occt::makeKernel();
        model = std::make_unique<DocumentModel>(kernel);
        model->addDemoBox(); // 20 x 20 x 20 mm: step 0
        model->waitForCalculation();
        body = model->bodyOfStep(0);
    }

    QVariantMap value(const char* name) const {
        for (const QVariant& entry : model->stepValues()) {
            if (entry.toMap().value("name").toString() == QLatin1String(name)) {
                return entry.toMap();
            }
        }
        return {};
    }

    QVariantMap step(int index) const { return model->recipe().at(index).toMap(); }

    std::shared_ptr<const fission::geo::GeometryKernel> kernel;
    std::unique_ptr<DocumentModel> model;
    QString body;
};

} // namespace

TEST_CASE("mirror ui: a copy touches the original, one undo takes it away", "[ui][mirror]") {
    MirrorWorld f;
    const int stepsBefore = static_cast<int>(f.model->recipe().size());
    const QString made = f.model->addMirror(f.body, QStringLiteral("copy"));
    REQUIRE_FALSE(made.isEmpty());
    f.model->waitForCalculation();
    // The step and a step each for the two bodies, as after a split.
    const int mirrorIndex = stepsBefore;
    CHECK(f.step(mirrorIndex).value("type").toString() == "mirror");
    CHECK(f.step(mirrorIndex).value("state").toString() == "ok");
    CHECK(f.model->currentStep() == mirrorIndex);
    CHECK(f.model->result()->bodies.size() >= 2);
    // Only one number is shown, the rest is hidden.
    CHECK(f.value("planeX").value("group").toString() == "size");
    CHECK(f.value("planeY").value("group").toString() == "hidden");
    CHECK(f.value("normalX").value("group").toString() == "hidden");
    // The arrow stands on the plane and points along x.
    REQUIRE(f.model->stepHandles().size() == 1);
    CHECK(f.model->stepHandles().front().toMap().value("parameter").toString() == "planeX");
    REQUIRE(f.model->undo());
    f.model->waitForCalculation();
    CHECK(static_cast<int>(f.model->recipe().size()) == stepsBefore);
}

TEST_CASE("mirror ui: the other axis is one step of the history", "[ui][mirror]") {
    MirrorWorld f;
    REQUIRE_FALSE(f.model->addMirror(f.body, QStringLiteral("copy")).isEmpty());
    f.model->waitForCalculation();
    const int index = f.model->currentStep();
    const double planeX = f.value("planeX").value("value").toDouble();
    CHECK_FALSE(f.model->setMirrorAxis(index, 3));
    CHECK_FALSE(f.model->setMirrorAxis(0, 1)); // not a mirror step
    REQUIRE(f.model->setMirrorAxis(index, 1));
    f.model->waitForCalculation();
    CHECK(f.value("planeY").value("group").toString() == "size");
    CHECK(f.value("planeX").value("group").toString() == "hidden");
    CHECK(f.value("normalY").value("value").toDouble() == Approx(1.0));
    CHECK(f.value("normalX").value("value").toDouble() == Approx(0.0));
    CHECK(f.step(index).value("state").toString() == "ok");
    REQUIRE(f.model->undo());
    f.model->waitForCalculation();
    CHECK(f.value("planeX").value("value").toDouble() == Approx(planeX));
    CHECK(f.value("normalX").value("value").toDouble() == Approx(1.0));
}

TEST_CASE("mirror ui: merge and only make one body; bad requests change nothing", "[ui][mirror]") {
    MirrorWorld f;
    CHECK(f.model->addMirror(QStringLiteral("nobody"), QStringLiteral("copy")).isEmpty());
    CHECK(f.model->addMirror(f.body, QStringLiteral("wobble")).isEmpty());
    const int before = static_cast<int>(f.model->recipe().size());
    const QString made = f.model->addMirror(f.body, QStringLiteral("merge"));
    REQUIRE_FALSE(made.isEmpty());
    f.model->waitForCalculation();
    CHECK(static_cast<int>(f.model->recipe().size()) == before + 1);
    CHECK(f.step(before).value("state").toString() == "ok");
    double volume = 0.0;
    for (const auto& [id, result] : f.model->result()->bodies) {
        if (QString::fromStdString(id.value) == made) {
            volume = f.kernel->measure(result.solid).volume;
        }
    }
    CHECK(volume == Approx(16000.0).epsilon(1e-9));
    CHECK(f.model->addMirror(f.body, QStringLiteral("copy")).isEmpty()); // used by the step
}

TEST_CASE("mirror ui: the number moves the plane, the original changes the image", "[ui][mirror]") {
    MirrorWorld f;
    const QString made = f.model->addMirror(f.body, QStringLiteral("only"));
    REQUIRE_FALSE(made.isEmpty());
    f.model->waitForCalculation();
    const int index = f.model->currentStep();
    REQUIRE(f.model->setStepValue(index, QStringLiteral("planeX"), QStringLiteral("50")));
    f.model->waitForCalculation();
    CHECK(f.step(index).value("state").toString() == "ok");
    REQUIRE(f.model->setStepValue(0, QStringLiteral("height"), QStringLiteral("10")));
    f.model->waitForCalculation();
    for (const auto& [id, result] : f.model->result()->bodies) {
        if (QString::fromStdString(id.value) == made) {
            CHECK(f.kernel->measure(result.solid).volume == Approx(4000.0).epsilon(1e-9));
        }
    }
}
