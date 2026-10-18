// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The lettering step (AP-1.5b): typed words in the document, the errors on the step, Undo.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>

#include "fission/core/commands.h"
#include "fission/core/document.h"
#include "fission/core/feature_type.h"
#include "fission/core/history.h"
#include "fission/core/recompute.h"
#include "fission/features/builtin.h"
#include "fission/geo/occt/kernel.h"

using Catch::Approx;
using namespace fission::core;

namespace {

struct Letters {
    FeatureRegistry registry;
    std::unique_ptr<fission::geo::GeometryKernel> kernel;
    RecomputeCache cache;
    History history{Document(7)};
    FeatureId step;

    Letters() {
        fission::features::registerBuiltinFeatures(registry);
        fission::geo::occt::KernelOptions options;
        const std::string dir = FISSION_APP_FONTS_DIR;
        options.fontFiles = {dir + "/IBMPlexSans-Regular.ttf", dir + "/IBMPlexSans-Bold.ttf",
                             dir + "/IBMPlexSans-Italic.ttf", dir + "/IBMPlexSans-BoldItalic.ttf",
                             dir + "/IBMPlexMono-Regular.ttf"};
        kernel = fission::geo::occt::makeKernel(options);
        step = *history.addFeature(defaultDraft(*registry.find("text")));
    }

    RecomputeResult calculate() {
        auto result = recompute(history.document(), registry, *kernel, cache);
        REQUIRE(result.has_value());
        return std::move(*result);
    }

    void type(const std::string& field, const std::string& words) {
        REQUIRE(history.execute(makeSetFeatureText(step, field, words)));
    }
};

} // namespace

TEST_CASE("text step: a new one has words, a font and sizes", "[features][text]") {
    Letters letters;
    const Feature& feature = *letters.history.document().findFeature(letters.step);
    CHECK(feature.type == "text");
    CHECK(feature.texts.at("text") == "Text");
    CHECK(feature.texts.at("font") == "IBM Plex Sans");
    CHECK(feature.texts.at("style") == "regular");
    CHECK(feature.params.at("size").expression == "10");
    CHECK(feature.params.at("height").expression == "2");
}

TEST_CASE("text step: lettering is made, named and keeps its place", "[features][text]") {
    Letters letters;
    letters.type("text", "LENA");
    const RecomputeResult result = letters.calculate();
    const FeatureState* state = result.state(letters.step);
    REQUIRE(state != nullptr);
    CHECK(state->status == FeatureStatus::ok);
    REQUIRE(result.bodies.size() == 1);
    const BodyResult& body = result.bodies.begin()->second;
    const auto report = letters.kernel->measure(body.solid);
    CHECK(report.valid);
    CHECK(report.solids == 4);
    CHECK(static_cast<int>(body.faces.size()) == report.faces);
    CHECK(body.faces.front().name.starts_with("F1/text/glyph"));
}

TEST_CASE("text step: other words make a new body, the same words come from the memory",
          "[features][text]") {
    Letters letters;
    letters.type("text", "Hallo");
    CHECK(letters.calculate().executed == 1);
    CHECK(letters.calculate().remembered == 1);

    letters.type("text", "Hallo!");
    CHECK(letters.calculate().executed == 1);

    letters.history.undo(); // back to "Hallo": nothing to calculate
    const RecomputeResult back = letters.calculate();
    CHECK(back.executed == 0);
    CHECK(back.remembered == 1);
}

TEST_CASE("text step: a missing letter is an error on the step, typing it away fixes it",
          "[features][text]") {
    Letters letters;
    letters.type("text", "Frohe Weihnachten 🎄");

    RecomputeResult result = letters.calculate();
    const FeatureState* state = result.state(letters.step);
    REQUIRE(state != nullptr);
    CHECK(state->status == FeatureStatus::error);
    REQUIRE(state->issue.has_value());
    CHECK(state->issue->code == FeatureIssueCode::textGlyphsMissing);
    CHECK(state->issue->subject == "🎄"); // the person is told which letter

    letters.type("text", "Frohe Weihnachten");
    result = letters.calculate();
    CHECK(result.state(letters.step)->status == FeatureStatus::ok);
}

TEST_CASE("text step: empty words and unknown fonts are errors with a hint", "[features][text]") {
    Letters letters;
    letters.type("text", "  ");
    FeatureState state = *letters.calculate().state(letters.step);
    REQUIRE(state.issue.has_value());
    CHECK(state.issue->code == FeatureIssueCode::parameterOutOfRange);
    CHECK(state.issue->subject == "text");

    letters.type("text", "Hallo");
    letters.type("font", "Gibt Es Nicht Sans");
    state = *letters.calculate().state(letters.step);
    REQUIRE(state.issue.has_value());
    CHECK(state.issue->code == FeatureIssueCode::fontNotFound);
    CHECK(state.issue->subject == "Gibt Es Nicht Sans");

    letters.type("font", "IBM Plex Mono");
    CHECK(letters.calculate().state(letters.step)->status == FeatureStatus::ok);
}

TEST_CASE("text step: typing words is an Undo step of its own", "[features][text]") {
    Letters letters;
    letters.type("text", "Eins");
    letters.history.undo();
    CHECK(letters.history.document().findFeature(letters.step)->texts.at("text") == "Text");
    letters.history.redo();
    CHECK(letters.history.document().findFeature(letters.step)->texts.at("text") == "Eins");

    // A name the step does not have is refused.
    CHECK_FALSE(letters.history.execute(makeSetFeatureText(letters.step, "colour", "red")));
    // Typing the same words again changes nothing.
    const auto before = letters.history.steps().size();
    letters.type("text", "Eins");
    CHECK(letters.history.steps().size() == before);
}

TEST_CASE("text step: bold and italic are words of the step, with Undo", "[features][text]") {
    Letters letters;
    letters.type("text", "Hallo");
    const auto volume = [&] {
        const RecomputeResult result = letters.calculate();
        REQUIRE(result.state(letters.step)->status == FeatureStatus::ok);
        return letters.kernel->measure(result.bodies.begin()->second.solid).volume;
    };
    const double regular = volume();
    letters.type("style", "bold");
    const double bold = volume();
    CHECK(bold > regular * 1.1);
    letters.type("style", "boldItalic");
    CHECK(volume() > 0.0);

    letters.history.undo();
    CHECK(letters.history.document().findFeature(letters.step)->texts.at("style") == "bold");
    CHECK(volume() == bold); // back from the memory, the same body
}

TEST_CASE("text step: a missing style is an error that names the font", "[features][text]") {
    Letters letters;
    letters.type("font", "IBM Plex Mono"); // bundled here with its regular style only
    letters.type("style", "bold");
    FeatureState state = *letters.calculate().state(letters.step);
    REQUIRE(state.issue.has_value());
    CHECK(state.issue->code == FeatureIssueCode::fontStyleMissing);
    CHECK(state.issue->subject == "IBM Plex Mono");

    letters.type("style", "regular");
    CHECK(letters.calculate().state(letters.step)->status == FeatureStatus::ok);

    // A style nobody knows is refused, not guessed.
    letters.type("style", "heavy");
    state = *letters.calculate().state(letters.step);
    CHECK(state.status == FeatureStatus::error);
}

TEST_CASE("text step: turns about the middle of its box, names stay", "[features][text][turn]") {
    Letters letters;
    letters.type("text", "LENA");
    const RecomputeResult flat = letters.calculate();
    const Feature& feature = *letters.history.document().findFeature(letters.step);
    const auto& type = *letters.registry.find("text");
    const FeatureState& state = *flat.state(letters.step);
    const fission::geo::Point3d centre = type.bodyCentre(state.params);
    const auto& body = feature.outputs.front();
    REQUIRE(flat.bodies.contains(body));
    const BodyResult& outcome = flat.bodies.at(body);
    const fission::geo::SolidReport before = letters.kernel->measure(outcome.solid);
    CHECK(centre.x == Approx((before.boxMin.x + before.boxMax.x) / 2).margin(1e-3));
    CHECK(centre.y == Approx((before.boxMin.y + before.boxMax.y) / 2).margin(1e-3));
    CHECK(centre.z == Approx((before.boxMin.z + before.boxMax.z) / 2).margin(1e-3));

    REQUIRE(letters.history.execute(makeSetFeatureValue(letters.step, "rotX", "90")));
    const RecomputeResult turned = letters.calculate();
    REQUIRE(turned.bodies.contains(body));
    const BodyResult& after = turned.bodies.at(body);
    const fission::geo::SolidReport report = letters.kernel->measure(after.solid);
    CHECK(report.valid);
    CHECK(report.volume == Approx(before.volume).epsilon(1e-6));
    CHECK(after.faces.size() == outcome.faces.size());
}
