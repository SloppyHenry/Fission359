// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// What a failed step tells the interface (AP-2.14): numbers, place and affected elements travel
// from the kernel error to the message, the failed result is kept as a mesh to show, the action
// "use the largest value" is one undo step, and no raw kernel text reaches the interface.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QVariantMap>

#include <memory>

#include "fission/core/document.h"
#include "fission/core/feature_type.h"
#include "fission/core/recompute.h"
#include "fission/features/builtin.h"
#include "fission/geo/error.h"
#include "fission/geo/occt/kernel.h"
#include "fission/ui/document_model.h"

namespace core = fission::core;
namespace geo = fission::geo;
using Catch::Approx;
using fission::ui::DocumentModel;

namespace {

constexpr const char* kRawText = "RAW-KERNEL-TEXT Standard_ConstructionError";

// A box that only works up to 3.1 mm: bigger fails like a rounding that is too large, with the
// number, the place, the faces concerned and a mesh of what went wrong.
class LimitedType final : public core::FeatureType {
public:
    std::string_view id() const override { return "limited"; }
    int version() const override { return 1; }
    std::vector<core::ParamSpec> parameters() const override {
        return {{"size", core::QuantityKind::length, "2"}};
    }
    std::optional<core::FeatureIssue> validate(const core::ResolvedParams&) const override {
        return std::nullopt;
    }
    core::FeatureResult execute(const core::ExecuteInput& input,
                                const core::ExecuteContext& context) const override {
        const double size = input.params.at("size").value;
        if (size > 3.1) {
            auto mesh = std::make_shared<geo::TriangleMesh>();
            mesh->positions = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
            mesh->normals = {{0, 0, 1}, {0, 0, 1}, {0, 0, 1}};
            mesh->indices = {0, 1, 2};
            mesh->faceIds = {1};
            throw geo::GeometryError(geo::GeometryErrorCode::invalidInput, kRawText, "size")
                .withNumber("maxValue", 3.1)
                .withNumber("given", size)
                .at({1.0, 2.0, 3.0})
                .concerning({"F1/limited/top"})
                .withPreview(std::move(mesh));
        }
        auto made = context.kernel.makeBox(size, size, size, {}, 0.0);
        core::FeatureResult result;
        result.outputs.push_back({input.feature.outputs.at(0), made.solid, made.faces});
        return result;
    }
    core::Description describe(const core::ResolvedParams& params) const override {
        return {"limited", {params.at("size")}};
    }
    std::vector<core::HandleSpec> handles(const core::ResolvedParams&) const override { return {}; }
};

QCoreApplication& application() {
    static int argc = 1;
    static char name[] = "fission_unit_tests";
    static char* argv[] = {name, nullptr};
    static QCoreApplication app(argc, argv);
    return app;
}

core::Document documentWith(core::FeatureRegistry& registry, const char* size) {
    core::Document document{5};
    core::FeatureDraft draft = core::defaultDraft(*registry.find("limited"));
    draft.params.at("size").expression = size;
    REQUIRE(document.addFeature(std::move(draft)));
    return document;
}

} // namespace

TEST_CASE("messages: numbers, place and elements of a kernel error reach the step", "[messages]") {
    core::FeatureRegistry registry;
    fission::features::registerBuiltinFeatures(registry);
    registry.add(std::make_unique<LimitedType>());
    const core::Document document = documentWith(registry, "50");
    const auto kernel = geo::occt::makeKernel();
    core::RecomputeCache cache;
    const auto result = core::recompute(document, registry, *kernel, cache);
    REQUIRE(result.has_value());

    const core::FeatureState& state = result->features.front();
    CHECK(state.status == core::FeatureStatus::error);
    REQUIRE(state.issue.has_value());
    CHECK(state.issue->code == core::FeatureIssueCode::kernelRejected);
    CHECK(state.issue->subject == "size");
    CHECK(state.issue->numbers.at("maxValue") == Approx(3.1));
    CHECK(state.issue->numbers.at("given") == Approx(50.0));
    REQUIRE(state.issue->where.has_value());
    CHECK(state.issue->where->y == Approx(2.0));
    CHECK(state.issue->elements == std::vector<std::string>{"F1/limited/top"});
    REQUIRE(state.failedPreview != nullptr);
    CHECK(state.failedPreview->triangleCount() == 1);

    // A step that works has no preview of a failure.
    const core::Document fine = documentWith(registry, "3");
    core::RecomputeCache other;
    const auto good = core::recompute(fine, registry, *kernel, other);
    REQUIRE(good.has_value());
    CHECK(good->features.front().status == core::FeatureStatus::ok);
    CHECK(good->features.front().failedPreview == nullptr);
}

TEST_CASE("messages: the largest value is one undo step and the raw text stays inside",
          "[messages][ui]") {
    application();
    core::FeatureRegistry registry;
    fission::features::registerBuiltinFeatures(registry);
    registry.add(std::make_unique<LimitedType>());
    const core::Document document = documentWith(registry, "50");

    std::shared_ptr<const geo::GeometryKernel> kernel = geo::occt::makeKernel();
    std::vector<std::unique_ptr<const core::FeatureType>> extra;
    extra.push_back(std::make_unique<LimitedType>());
    DocumentModel model(kernel, std::move(extra));
    model.replaceDocument(document);
    model.waitForCalculation();

    const QVariantMap step = model.recipe().at(0).toMap();
    CHECK(step.value("state").toString() == "error");
    const QVariantMap problem = step.value("problem").toMap();
    CHECK(problem.value("code").toString() == "kernelRejected");
    CHECK(problem.value("field").toString() == "size");
    CHECK(problem.value("numbers").toMap().value("maxValue").toDouble() == Approx(3.1));
    CHECK(problem.value("elements").toStringList() ==
          QStringList{QStringLiteral("F1/limited/top")});
    CHECK(problem.value("preview").toBool());

    // No raw kernel text in anything the interface is given about the problem.
    for (auto it = problem.cbegin(); it != problem.cend(); ++it) {
        CHECK_FALSE(it.value().toString().contains(QStringLiteral("RAW-KERNEL-TEXT")));
        CHECK_FALSE(
            it.value().toStringList().join(' ').contains(QStringLiteral("RAW-KERNEL-TEXT")));
    }

    // Only the report to copy has it, with version and kind of step.
    const QString report = model.problemReport(0);
    CHECK(report.contains(QStringLiteral("RAW-KERNEL-TEXT")));
    CHECK(report.contains(QStringLiteral("step: limited")));
    CHECK(report.contains(QStringLiteral("code: kernelRejected")));
    CHECK(model.problemReport(5).isEmpty());

    const int before = model.historyPosition();
    REQUIRE(model.useMaximum(0));
    model.waitForCalculation();
    CHECK(model.historyPosition() == before + 1);
    CHECK(model.recipe().at(0).toMap().value("state").toString() == "ok");
    CHECK(model.recipe().at(0).toMap().value("values").toList().at(0).toDouble() == Approx(3.1));
    CHECK_FALSE(model.useMaximum(0)); // no problem any more, nothing to use

    REQUIRE(model.undo());
    model.waitForCalculation();
    CHECK(model.recipe().at(0).toMap().value("state").toString() == "error");
}
