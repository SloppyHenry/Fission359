// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The import step (AP-2.13): a file embedded in the document, one step per body, one Undo step
// for the whole import, errors on the step.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

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

Bytes fixture(const char* name) {
    const std::filesystem::path path =
        std::filesystem::path(FISSION_TEST_FIXTURES_DIR) / "step" / name;
    std::ifstream stream(path, std::ios::binary);
    REQUIRE(stream);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

struct Importer {
    FeatureRegistry registry;
    std::unique_ptr<fission::geo::GeometryKernel> kernel = fission::geo::occt::makeKernel();
    RecomputeCache cache;
    History history{Document(11)};

    Importer() { fission::features::registerBuiltinFeatures(registry); }

    // One step per body, the way the interface makes them.
    std::vector<ImportedPart> parts(int count) {
        std::vector<ImportedPart> made;
        for (int part = 1; part <= count; ++part) {
            FeatureDraft draft = defaultDraft(*registry.find("import"));
            draft.params["part"] = {std::to_string(part), QuantityKind::plain};
            made.push_back({std::move(draft), "Part " + std::to_string(part), Color{1, 2, 3}});
        }
        return made;
    }

    bool importFile(const char* file, int count, const std::string& name = "assembly.step") {
        return history.execute(makeImportFile(name, fixture(file), parts(count)));
    }

    RecomputeResult calculate() {
        auto result = recompute(history.document(), registry, *kernel, cache);
        REQUIRE(result.has_value());
        return std::move(*result);
    }
};

} // namespace

TEST_CASE("import: one step per body, named and calculated", "[features][import]") {
    Importer importer;
    REQUIRE(importer.importFile("assembly-two-parts.step", 2));
    const Document& document = importer.history.document();
    REQUIRE(document.features().size() == 2);
    CHECK(document.features()[0].texts.at("asset") == "assembly.step");
    CHECK(document.features()[1].texts.at("asset") == "assembly.step");
    REQUIRE(document.assets().contains("assembly.step"));
    REQUIRE(document.bodies().size() == 2);
    CHECK(document.bodies()[0].name == "Part 1");
    CHECK(document.bodies()[1].color == Color{1, 2, 3});

    const RecomputeResult result = importer.calculate();
    REQUIRE(result.bodies.size() == 2);
    for (const Feature& feature : document.features()) {
        const FeatureState* state = result.state(feature.id);
        REQUIRE(state != nullptr);
        CHECK(state->status == FeatureStatus::ok);
    }
    for (const auto& [id, body] : result.bodies) {
        const auto report = importer.kernel->measure(body.solid);
        CHECK(report.valid);
        REQUIRE(static_cast<int>(body.faces.size()) == report.faces);
        CHECK(body.faces.front().name.starts_with("F"));
        CHECK(body.faces.front().name.find("/import/face") != std::string::npos);
    }
}

TEST_CASE("import: the whole import is one step of the history", "[features][import]") {
    Importer importer;
    const Document before = importer.history.document();
    REQUIRE(importer.importFile("assembly-two-parts.step", 2));
    const Document after = importer.history.document();

    REQUIRE(importer.history.undo());
    CHECK(importer.history.document().sameContentAs(before));
    CHECK(importer.history.document().assets().empty());
    CHECK_FALSE(importer.history.canUndo());

    REQUIRE(importer.history.redo());
    CHECK(importer.history.document().sameContentAs(after));
    CHECK(importer.history.document().features()[0].id == after.features()[0].id);
    CHECK(importer.history.document().lastFeatureNumber() == after.lastFeatureNumber());
    CHECK_FALSE(importer.history.canRedo());
}

TEST_CASE("import: the same file twice gets two names", "[features][import]") {
    Importer importer;
    REQUIRE(importer.importFile("block-with-hole.step", 1, "block.step"));
    REQUIRE(importer.importFile("block-with-hole.step", 1, "block.step"));
    const Document& document = importer.history.document();
    CHECK(document.assets().size() == 2);
    CHECK(document.features()[0].texts.at("asset") == "block.step");
    CHECK(document.features()[1].texts.at("asset") == "block-2.step");
    // Undo of the second takes only its file.
    REQUIRE(importer.history.undo());
    CHECK(importer.history.document().assets().size() == 1);
}

TEST_CASE("import: x, y and z move the body from where the file puts it", "[features][import]") {
    Importer importer;
    REQUIRE(importer.importFile("block-with-hole.step", 1, "block.step"));
    const FeatureId step = importer.history.document().features().front().id;
    const auto boxOf = [&] {
        const RecomputeResult result = importer.calculate();
        REQUIRE(result.bodies.size() == 1);
        return importer.kernel->measure(result.bodies.begin()->second.solid);
    };
    CHECK(boxOf().boxMin.x == Approx(0.0).margin(1e-6));
    REQUIRE(importer.history.execute(makeSetFeatureValue(step, "x", "12,5")));
    REQUIRE(importer.history.execute(makeSetFeatureValue(step, "z", "-3")));
    const auto moved = boxOf();
    CHECK(moved.boxMin.x == Approx(12.5).epsilon(1e-9));
    CHECK(moved.boxMax.x == Approx(32.5).epsilon(1e-9));
    CHECK(moved.boxMin.z == Approx(-3.0).epsilon(1e-9));
    CHECK(moved.valid);
}

TEST_CASE("import: the unit factor scales the whole file alike", "[features][import]") {
    Importer importer;
    REQUIRE(importer.importFile("block-with-hole.step", 1, "block.step"));
    const FeatureId step = importer.history.document().features().front().id;
    REQUIRE(importer.history.execute(makeSetFeatureValue(step, "unitFactor", "25,4")));
    const RecomputeResult result = importer.calculate();
    REQUIRE(result.bodies.size() == 1);
    const auto report = importer.kernel->measure(result.bodies.begin()->second.solid);
    CHECK(report.boxMax.x == Approx(20.0 * 25.4).epsilon(1e-9));
    CHECK(report.faces == 7);
    CHECK(report.valid);
}

TEST_CASE("import: problems are shown on the step, never as a crash", "[features][import]") {
    Importer importer;
    REQUIRE(importer.importFile("block-with-hole.step", 1, "block.step"));
    const FeatureId step = importer.history.document().features().front().id;

    SECTION("a part the file does not have") {
        REQUIRE(importer.history.execute(makeSetFeatureValue(step, "part", "3")));
        const RecomputeResult result = importer.calculate();
        const FeatureState* state = result.state(step);
        REQUIRE(state != nullptr);
        CHECK(state->status == FeatureStatus::error);
        REQUIRE(state->issue);
        CHECK(state->issue->code == FeatureIssueCode::importFileEmpty);
    }
    SECTION("a part number that is no whole number") {
        REQUIRE(importer.history.execute(makeSetFeatureValue(step, "part", "1,5")));
        const RecomputeResult result = importer.calculate();
        const FeatureState* state = result.state(step);
        REQUIRE(state != nullptr);
        REQUIRE(state->issue);
        CHECK(state->issue->code == FeatureIssueCode::parameterOutOfRange);
        CHECK(state->issue->subject == "part");
    }
    SECTION("a unit factor of zero") {
        REQUIRE(importer.history.execute(makeSetFeatureValue(step, "unitFactor", "0")));
        const RecomputeResult result = importer.calculate();
        const FeatureState* state = result.state(step);
        REQUIRE(state != nullptr);
        REQUIRE(state->issue);
        CHECK(state->issue->code == FeatureIssueCode::parameterOutOfRange);
    }
    SECTION("a file that is no STEP file") {
        Document broken(5);
        FeatureDraft draft = defaultDraft(*importer.registry.find("import"));
        broken.addAsset("broken.step", Bytes{'n', 'o', 'p', 'e'});
        draft.texts["asset"] = "broken.step";
        const auto id = broken.addFeature(std::move(draft));
        REQUIRE(id);
        auto result = recompute(broken, importer.registry, *importer.kernel, importer.cache);
        REQUIRE(result);
        const FeatureState* state = result->state(*id);
        REQUIRE(state != nullptr);
        REQUIRE(state->issue);
        CHECK(state->issue->code == FeatureIssueCode::importFileUnreadable);
    }
    SECTION("the embedded file is missing") {
        Document lost(5);
        FeatureDraft draft = defaultDraft(*importer.registry.find("import"));
        draft.texts["asset"] = "gone.step";
        const auto id = lost.addFeature(std::move(draft));
        REQUIRE(id);
        auto result = recompute(lost, importer.registry, *importer.kernel, importer.cache);
        REQUIRE(result);
        const FeatureState* state = result->state(*id);
        REQUIRE(state != nullptr);
        REQUIRE(state->issue);
        CHECK(state->issue->code == FeatureIssueCode::importFileUnreadable);
    }
}

TEST_CASE("import: an import with no parts is refused and leaves nothing", "[features][import]") {
    Importer importer;
    const Document before = importer.history.document();
    CHECK_FALSE(
        importer.history.execute(makeImportFile("x.step", fixture("block-with-hole.step"), {})));
    CHECK(importer.history.document().sameContentAs(before));
}
