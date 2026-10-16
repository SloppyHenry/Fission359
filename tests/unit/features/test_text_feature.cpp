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
        options.fontFiles = {std::string(FISSION_APP_FONTS_DIR) + "/IBMPlexSans-Regular.ttf",
                             std::string(FISSION_APP_FONTS_DIR) + "/IBMPlexMono-Regular.ttf"};
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
