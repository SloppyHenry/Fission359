// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Helpers for the tests of persistent references (AP-2.10c/d): a document with the built-in
// steps plus the test step `markFace`, which paints nothing and only checks that its reference
// is found again. It stands in for the steps that will use references (fillet, drill; AP-2.4,
// AP-2.5, AP-2.7).

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "fission/core/body_looks.h"
#include "fission/core/document.h"
#include "fission/core/feature_type.h"
#include "fission/core/recompute.h"
#include "fission/core/resolve.h"
#include "fission/features/builtin.h"
#include "fission/geo/occt/kernel.h"

namespace fission::test {

/// Passes its first input on unchanged. Needs the reference `target`; what it found is kept in
/// `seen` (the last calculation that ran).
class MarkFaceType final : public core::FeatureType {
public:
    std::string_view id() const override { return "markFace"; }
    int version() const override { return 1; }
    std::vector<core::ParamSpec> parameters() const override { return {}; }
    std::optional<core::FeatureIssue> validate(const core::ResolvedParams&) const override {
        return std::nullopt;
    }
    core::FeatureResult execute(const core::ExecuteInput& input,
                                const core::ExecuteContext&) const override {
        const auto found = input.refs.find("target");
        if (input.inputs.empty() || found == input.refs.end() || found->second.empty()) {
            throw geo::GeometryError(geo::GeometryErrorCode::invalidInput,
                                     "markFace: needs a body and the reference 'target'");
        }
        seen = found->second;
        const core::FeatureOutput& body = input.inputs.front();
        core::FeatureResult result;
        result.outputs.push_back({input.feature.outputs.at(0), body.solid, body.faces});
        return result;
    }
    core::Description describe(const core::ResolvedParams&) const override {
        return {"markFace", {}};
    }
    bool passesOnNames() const override { return true; }
    std::vector<core::HandleSpec> handles(const core::ResolvedParams&) const override { return {}; }

    mutable std::vector<core::ResolvedRef> seen;
};

struct RefWorld {
    core::Document document{5};
    core::FeatureRegistry registry;
    std::unique_ptr<geo::GeometryKernel> kernel = geo::occt::makeKernel();
    core::RecomputeCache cache;
    const MarkFaceType* mark = nullptr;

    RefWorld() {
        features::registerBuiltinFeatures(registry);
        auto type = std::make_unique<MarkFaceType>();
        mark = type.get();
        registry.add(std::move(type));
    }

    core::FeatureId add(const char* type, std::map<std::string, std::string> values = {},
                        std::vector<core::BodyId> inputs = {},
                        std::map<std::string, std::string> words = {}) {
        core::FeatureDraft draft = core::defaultDraft(*registry.find(type));
        for (const auto& [name, expression] : values) {
            draft.params.at(name).expression = expression;
        }
        for (const auto& [name, text] : words) {
            draft.texts[name] = text;
        }
        draft.inputs = std::move(inputs);
        return *document.addFeature(std::move(draft));
    }

    core::BodyId bodyOf(const core::FeatureId& id) const {
        return document.findFeature(id)->outputs.at(0);
    }

    core::FeatureId plate(const char* width = "40", const char* depth = "40",
                          const char* height = "10") {
        return add("box", {{"width", width}, {"depth", depth}, {"height", height}, {"x", "0"}});
    }

    core::RecomputeResult run() {
        auto result = core::recompute(document, registry, *kernel, cache);
        REQUIRE(result.has_value());
        return std::move(*result);
    }

    core::BodyLooks looksOf(const core::RecomputeResult& result, const core::FeatureId& step) {
        const core::BodyResult& body = result.bodies.at(bodyOf(step));
        return core::describeBody(body.faces, kernel->signatures(body.solid));
    }

    /// "The person clicked it": a reference to the element called `name` of the body `step`
    /// made, as it looks in `result`.
    core::PersistentRef pick(const core::RecomputeResult& result, const core::FeatureId& step,
                             core::ElementKind kind, const std::string& name) {
        const core::BodyLooks looks = looksOf(result, step);
        const auto& list = kind == core::ElementKind::face ? looks.faces : looks.edges;
        for (const core::NamedLook& entry : list) {
            if (entry.name == name) {
                const auto ref =
                    core::makeRef(looks, kind, entry.id,
                                  core::placementOf(document, registry, result.features, name));
                REQUIRE(ref.has_value());
                return *ref;
            }
        }
        FAIL("no element called " << name);
        return {};
    }

    /// A markFace step on the body of `target`, pointing at `ref`.
    core::FeatureId mark_(const core::FeatureId& target, core::PersistentRef ref) {
        core::FeatureDraft draft = core::defaultDraft(*registry.find("markFace"));
        draft.inputs = {bodyOf(target)};
        draft.refs["target"] = {std::move(ref)};
        return *document.addFeature(std::move(draft));
    }
};

} // namespace fission::test
