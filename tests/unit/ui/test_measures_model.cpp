// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Named measures in the interface (AP-2.9): the list for the table, making and renaming measures,
// "Save as named measure" at a field, the completion in number fields, and flow B step 7: a
// measure changes, everything that uses it follows.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QStringList>
#include <QVariantMap>

#include <memory>

#include "fission/geo/occt/kernel.h"
#include "fission/ui/document_model.h"
#include "fission/ui/numbers.h"

using Catch::Approx;
using fission::ui::DocumentModel;
using fission::ui::Numbers;

namespace {

QCoreApplication& application() {
    static int argc = 1;
    static char name[] = "fission_unit_tests";
    static char* argv[] = {name, nullptr};
    static QCoreApplication app(argc, argv);
    return app;
}

struct World {
    std::shared_ptr<const fission::geo::GeometryKernel> kernel = fission::geo::occt::makeKernel();
    DocumentModel model{kernel};
    Numbers numbers;

    World() {
        application();
        model.setNumbers(&numbers);
        model.addDemoBox(); // 20 x 20 x 20 mm, step 0
        model.waitForCalculation();
    }

    fission::geo::SolidReport box() {
        model.waitForCalculation();
        const auto bodies = model.exportBodies({});
        REQUIRE(bodies.size() == 1);
        return kernel->measure(bodies.front().solid);
    }
    QVariantMap measureNamed(const QString& name) const {
        for (const QVariant& entry : model.measures()) {
            if (entry.toMap().value("name").toString() == name) {
                return entry.toMap();
            }
        }
        return {};
    }
    QString expressionOf(int step, const char* field) const {
        return QString::fromStdString(model.document()
                                          .features()
                                          .at(static_cast<std::size_t>(step))
                                          .params.at(field)
                                          .expression);
    }
};

} // namespace

TEST_CASE("measures: flow B step 7, the width changes and everything follows", "[measures-ui]") {
    World world;
    REQUIRE(world.model.addMeasure("Breite", "30", "length").isEmpty());
    REQUIRE(world.model.setStepValue(0, "width", "Breite"));
    REQUIRE(world.model.setStepValue(0, "x", "Breite / 2"));
    auto report = world.box();
    CHECK(report.boxMax.x - report.boxMin.x == Approx(30.0).epsilon(1e-6));
    CHECK(report.boxMin.x == Approx(15.0 - 15.0 + 0.0).margin(20.0)); // placed by the formula

    const double firstMiddle = (report.boxMin.x + report.boxMax.x) / 2.0;
    REQUIRE(world.model.setMeasureExpression("Breite", "40"));
    report = world.box();
    CHECK(report.boxMax.x - report.boxMin.x == Approx(40.0).epsilon(1e-6));
    // x = Breite / 2 moved with it (the middle of the box is x or around it, depending on the
    // shape's anchor): the formula was worked out again, not kept.
    CHECK((report.boxMin.x + report.boxMax.x) / 2.0 != Approx(firstMiddle).margin(1e-6));
    CHECK(report.valid);
}

TEST_CASE("measures: the list says value, unit, where it is used and what is wrong",
          "[measures-ui]") {
    World world;
    REQUIRE(world.model.addMeasure("Breite", "30", "length").isEmpty());
    REQUIRE(world.model.addMeasure("Halb", "Breite / 2", "length").isEmpty());
    REQUIRE(world.model.addMeasure("Winkel", "45", "angle").isEmpty());
    REQUIRE(world.model.setStepValue(0, "width", "Breite"));
    world.model.waitForCalculation();

    const QVariantMap breite = world.measureNamed("Breite");
    REQUIRE_FALSE(breite.isEmpty());
    CHECK(breite.value("valid").toBool());
    CHECK(breite.value("value").toDouble() == Approx(30.0));
    CHECK(breite.value("unit").toString() == QStringLiteral("mm"));
    CHECK(breite.value("usedIn").toList().size() == 1);
    CHECK(breite.value("usedIn").toList().front().toInt() == 0);
    CHECK(breite.value("usedByMeasures").toStringList() == QStringList{"Halb"});
    CHECK(world.measureNamed("Halb").value("value").toDouble() == Approx(15.0));
    CHECK(world.measureNamed("Winkel").value("unit").toString() == QStringLiteral("°"));
    CHECK(world.measureNamed("Winkel").value("value").toDouble() == Approx(45.0));

    // A formula with a mistake says which.
    REQUIRE(world.model.setMeasureExpression("Halb", "Breite / 0"));
    world.model.waitForCalculation();
    const QVariantMap broken = world.measureNamed("Halb");
    CHECK_FALSE(broken.value("valid").toBool());
    CHECK(broken.value("error").toMap().value("code").toString() ==
          QStringLiteral("divisionByZero"));
}

TEST_CASE("measures: a cycle is shown with its path", "[measures-ui]") {
    World world;
    REQUIRE(world.model.addMeasure("A", "B + 1", "length").isEmpty());
    REQUIRE(world.model.addMeasure("B", "A + 1", "length").isEmpty());
    world.model.waitForCalculation();
    const QVariantMap error = world.measureNamed("A").value("error").toMap();
    CHECK(error.value("code").toString() == QStringLiteral("cycle"));
    CHECK(error.value("path").toStringList().size() >= 3); // a, b, a
    CHECK(error.value("path").toStringList().front() == error.value("path").toStringList().back());
}

TEST_CASE("measures: new names must be usable and new", "[measures-ui]") {
    World world;
    CHECK(world.model.addMeasure("", "1", "length") == QStringLiteral("invalidName"));
    CHECK(world.model.addMeasure("2x", "1", "length") == QStringLiteral("invalidName"));
    CHECK(world.model.addMeasure("zwei Worte", "1", "length") == QStringLiteral("invalidName"));
    CHECK(world.model.addMeasure("Breite", "1", "metres") == QStringLiteral("invalidName"));
    REQUIRE(world.model.addMeasure("Breite", "1", "length").isEmpty());
    CHECK(world.model.addMeasure("breite", "2", "length") == QStringLiteral("duplicateName"));
    CHECK(world.model.measures().size() == 1);
}

TEST_CASE("measures: renaming follows in all formulas and is one undo step", "[measures-ui]") {
    World world;
    REQUIRE(world.model.addMeasure("Breite", "30", "length").isEmpty());
    REQUIRE(world.model.addMeasure("Breite2", "10", "length").isEmpty());
    REQUIRE(world.model.addMeasure("Halb", "Breite / 2", "length").isEmpty());
    REQUIRE(world.model.setStepValue(0, "width", "Breite + Breite2"));
    REQUIRE(world.model.setStepValue(0, "depth", "Halb"));
    const int position = world.model.historyPosition();

    REQUIRE(world.model.renameMeasure("Breite", "Halterbreite").isEmpty());
    CHECK(world.model.historyPosition() == position + 1);
    CHECK(world.expressionOf(0, "width") == QStringLiteral("Halterbreite + Breite2"));
    CHECK(world.measureNamed("Halb").value("expression").toString() ==
          QStringLiteral("Halterbreite / 2"));
    world.model.waitForCalculation();
    CHECK(world.measureNamed("Halterbreite").value("valid").toBool());
    CHECK(world.box().boxMax.x - world.box().boxMin.x == Approx(40.0).epsilon(1e-6));

    REQUIRE(world.model.undo());
    CHECK(world.model.historyPosition() == position);
    CHECK(world.expressionOf(0, "width") == QStringLiteral("Breite + Breite2"));
    CHECK_FALSE(world.measureNamed("Breite").isEmpty());

    CHECK(world.model.renameMeasure("Breite", "Breite2") == QStringLiteral("duplicateName"));
    CHECK(world.model.renameMeasure("Breite", "no good") == QStringLiteral("invalidName"));
    CHECK(world.model.historyPosition() == position); // nothing happened
}

TEST_CASE("measures: save a field's value as a named measure", "[measures-ui]") {
    World world;
    const int position = world.model.historyPosition();
    const QString name = world.model.saveStepValueAsMeasure(0, "width", "Breite");
    CHECK(name == QStringLiteral("Breite"));
    CHECK(world.expressionOf(0, "width") == QStringLiteral("Breite"));
    REQUIRE_FALSE(world.measureNamed("Breite").isEmpty());
    CHECK(world.measureNamed("Breite").value("expression").toString() == QStringLiteral("20"));
    CHECK(world.model.historyPosition() == position + 1); // measure and field: one step
    CHECK(world.box().boxMax.x - world.box().boxMin.x == Approx(20.0).epsilon(1e-6));

    // The name is taken: the next gets a number, and it is a usable name.
    const QString second = world.model.saveStepValueAsMeasure(0, "depth", "Breite");
    CHECK(second == QStringLiteral("Breite2"));
    CHECK(world.model.saveStepValueAsMeasure(0, "height", "zwei Worte!") ==
          QStringLiteral("zweiWorte"));
    CHECK(world.model.saveStepValueAsMeasure(0, "height", "7x") == QStringLiteral("M7x"));

    REQUIRE(world.model.undo());
    REQUIRE(world.model.undo());
    REQUIRE(world.model.undo());
    REQUIRE(world.model.undo());
    CHECK(world.model.historyPosition() == position);
    CHECK(world.model.measures().isEmpty());
    CHECK(world.expressionOf(0, "width") == QStringLiteral("20"));

    CHECK(world.model.saveStepValueAsMeasure(7, "width", "X").isEmpty());
    CHECK(world.model.saveStepValueAsMeasure(0, "nonsense", "X").isEmpty());
}

TEST_CASE("measures: a formula keeps its form when it becomes a measure", "[measures-ui]") {
    World world;
    REQUIRE(world.model.setStepValue(0, "width", "2,5 cm"));
    CHECK(world.model.saveStepValueAsMeasure(0, "width", "Wand") == QStringLiteral("Wand"));
    CHECK(world.measureNamed("Wand").value("expression").toString() == QStringLiteral("2,5 cm"));
    world.model.waitForCalculation();
    CHECK(world.measureNamed("Wand").value("value").toDouble() == Approx(25.0));
}

TEST_CASE("measures: comment and slider are kept and undone", "[measures-ui]") {
    World world;
    REQUIRE(world.model.addMeasure("Breite", "30", "length").isEmpty());
    REQUIRE(world.model.setMeasureComment("Breite", "Breite der Halterung"));
    CHECK(world.measureNamed("Breite").value("comment").toString() ==
          QStringLiteral("Breite der Halterung"));
    REQUIRE(world.model.setMeasureSlider("Breite", 10.0, 80.0, 5.0));
    const QVariantMap slider = world.measureNamed("Breite").value("slider").toMap();
    CHECK(slider.value("min").toDouble() == 10.0);
    CHECK(slider.value("max").toDouble() == 80.0);
    CHECK(slider.value("step").toDouble() == 5.0);
    CHECK_FALSE(world.model.setMeasureSlider("Breite", 80.0, 10.0, 5.0)); // upside down
    CHECK_FALSE(world.model.setMeasureSlider("Breite", 0.0, 10.0, 0.0));
    REQUIRE(world.model.clearMeasureSlider("Breite"));
    CHECK_FALSE(world.measureNamed("Breite").contains("slider"));
    REQUIRE(world.model.undo()); // the slider is back
    CHECK(world.measureNamed("Breite").contains("slider"));
}

TEST_CASE("measures: a slider drag is one step of the history", "[measures-ui]") {
    World world;
    REQUIRE(world.model.addMeasure("Breite", "30", "length").isEmpty());
    REQUIRE(world.model.setStepValue(0, "width", "Breite"));
    REQUIRE(world.model.setMeasureSlider("Breite", 10.0, 80.0, 1.0));
    const int position = world.model.historyPosition();
    world.model.beginChange("Breite");
    for (const char* value : {"31", "35", "42", "50", "47"}) {
        REQUIRE(world.model.setMeasureExpression("Breite", value));
    }
    world.model.commitChange();
    CHECK(world.model.historyPosition() == position + 1);
    CHECK(world.measureNamed("Breite").value("expression").toString() == QStringLiteral("47"));
    REQUIRE(world.model.undo());
    CHECK(world.measureNamed("Breite").value("expression").toString() == QStringLiteral("30"));
}

TEST_CASE("measures: who uses a measure before it is deleted", "[measures-ui]") {
    World world;
    REQUIRE(world.model.addMeasure("Breite", "30", "length").isEmpty());
    REQUIRE(world.model.addMeasure("Halb", "Breite / 2", "length").isEmpty());
    REQUIRE(world.model.setStepValue(0, "width", "Breite"));
    const QVariantMap uses = world.model.measureUses("Breite");
    CHECK(uses.value("steps").toList().size() == 1);
    CHECK(uses.value("numbers").toList().front().toInt() == 1);
    CHECK(uses.value("measures").toStringList() == QStringList{"Halb"});
    CHECK(world.model.measureUses("Halb").value("steps").toList().isEmpty());

    REQUIRE(world.model.removeMeasure("Breite"));
    world.model.waitForCalculation();
    // What used it now has a mistake that says so, nothing is quietly wrong.
    CHECK_FALSE(world.measureNamed("Halb").value("valid").toBool());
    CHECK(world.measureNamed("Halb").value("error").toMap().value("code").toString() ==
          QStringLiteral("unknownName"));
    REQUIRE(world.model.undo());
    CHECK_FALSE(world.measureNamed("Breite").isEmpty());
}

TEST_CASE("measures: the completion offers names that begin with what was typed first",
          "[measures-ui]") {
    World world;
    for (const char* name : {"Wand", "Breite", "Halterbreite", "Boden", "Bohrung", "Zwei"}) {
        REQUIRE(world.model.addMeasure(name, "1", "length").isEmpty());
    }
    CHECK(world.numbers.completions("b") ==
          QStringList{"Boden", "Bohrung", "Breite", "Halterbreite"});
    CHECK(world.numbers.completions("Bo") == QStringList{"Boden", "Bohrung"});
    CHECK(world.numbers.completions("breit") == QStringList{"Breite", "Halterbreite"});
    CHECK(world.numbers.completions("zwe") == QStringList{"Zwei"});
    CHECK(world.numbers.completions("Breite") ==
          QStringList{"Halterbreite"}); // itself is not offered
    CHECK(world.numbers.completions("").isEmpty());
    CHECK(world.numbers.completions("xyz").isEmpty());
}
