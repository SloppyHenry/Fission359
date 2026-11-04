// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Pulling a face from the interface (AP-2.4a): one step and one undo, a grip on the face, bad
// requests change nothing.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
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

struct Pull {
    Pull() {
        application();
        kernel = fission::geo::occt::makeKernel();
        model = std::make_unique<DocumentModel>(kernel);
        model->addDemoBox(); // 20 × 20 × 20 mm: step 0
        model->waitForCalculation();
        body = model->bodyOfStep(0);
    }

    QVariantMap step(int index) const { return model->recipe().at(index).toMap(); }

    std::shared_ptr<const fission::geo::GeometryKernel> kernel;
    std::unique_ptr<DocumentModel> model;
    QString body;
};

} // namespace

TEST_CASE("push face: one pull is one step and one undo", "[ui][faces]") {
    Pull f;
    const int before = f.model->historyPosition();
    const QString made = f.model->pushFace(f.body, 1);
    REQUIRE_FALSE(made.isEmpty());
    f.model->waitForCalculation();
    CHECK(f.model->historyPosition() == before + 1);
    CHECK(f.step(1).value("type").toString() == "pushFace");
    CHECK(f.step(1).value("state").toString() == "ok");

    // The body is the result now; the box itself is part of it.
    CHECK(f.model->pushFace(f.body, 1).isEmpty());

    REQUIRE(f.model->undo());
    f.model->waitForCalculation();
    CHECK(f.model->recipe().size() == 1);
}

TEST_CASE("push face: a bad request is refused and changes nothing", "[ui][faces]") {
    Pull f;
    const int before = f.model->historyPosition();
    CHECK(f.model->pushFace(f.body, 0).isEmpty());
    CHECK(f.model->pushFace(f.body, 4242).isEmpty());
    CHECK(f.model->pushFace(QStringLiteral("nobody"), 1).isEmpty());
    CHECK(f.model->historyPosition() == before);
}
