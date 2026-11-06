// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Rounding and bevelling edges (AP-2.5). One body goes in, the edges come from the reference
// "edges" (persistent, ADR-0003), one body comes out that carries the names of the input. When
// the size does not work, the step looks for the largest one that does and says so (rule 5).

#include <algorithm>
#include <cmath>
#include <memory>

#include "fission/features/builtin.h"

#include "primitives.h"

namespace fission::features {

namespace {

using namespace fission::core;

constexpr int kMaxTries = 20;     // halvings when looking for the largest size
constexpr double kSizeStep = 0.1; // the largest size is rounded down to this (mm)

class BlendType final : public FeatureType {
public:
    BlendType(std::string id, geo::BlendKind kind, std::string sizeName)
        : id_(std::move(id)), kind_(kind), sizeName_(std::move(sizeName)) {}

    std::string_view id() const override { return id_; }
    int version() const override { return 1; }

    std::vector<ParamSpec> parameters() const override {
        std::vector<ParamSpec> own{{sizeName_, QuantityKind::length, "2"}};
        if (kind_ == geo::BlendKind::chamfer) {
            // 0: the same distance on both sides.
            own.push_back({"distance2", QuantityKind::length, "0", true});
        }
        // Where the handle stands: a point on the first edge and the way into the body between
        // its two faces (a unit vector, all 0 = no handle). Set when the step is made; the
        // person does not edit them (they are not shown).
        for (const char* name : {"anchorX", "anchorY", "anchorZ"}) {
            own.push_back({name, QuantityKind::length, "0"});
        }
        for (const char* name : {"anchorDx", "anchorDy", "anchorDz"}) {
            own.push_back({name, QuantityKind::plain, "0"});
        }
        return own;
    }

    std::optional<FeatureIssue> validate(const ResolvedParams& params) const override {
        if (auto issue = requirePositive(params, {sizeName_.c_str()})) {
            return issue;
        }
        const auto second = params.find("distance2");
        if (second != params.end() &&
            (!std::isfinite(second->second.value) || second->second.value < 0.0)) {
            FeatureIssue issue;
            issue.code = FeatureIssueCode::parameterOutOfRange;
            issue.subject = "distance2";
            return issue;
        }
        return std::nullopt;
    }

    FeatureResult execute(const ExecuteInput& input, const ExecuteContext& context) const override {
        const auto picked = input.refs.find("edges");
        if (input.inputs.size() != 1 || picked == input.refs.end() || picked->second.empty()) {
            throw geo::GeometryError(geo::GeometryErrorCode::invalidInput,
                                     id_ + ": needs one body and the reference 'edges'");
        }
        const FeatureOutput& body = input.inputs.front();
        const geo::NamedSolid whole{body.solid, body.faces};
        std::vector<geo::BlendEdge> edges;
        for (const ResolvedRef& ref : picked->second) {
            edges.push_back({ref.id, ref.element});
        }
        geo::BlendSpec spec;
        spec.kind = kind_;
        spec.size = valueOf(input.params, sizeName_.c_str());
        const auto second = input.params.find("distance2");
        spec.size2 = second != input.params.end() ? second->second.value : 0.0;
        const std::string prefix =
            elementName(input.feature, kind_ == geo::BlendKind::round ? "round" : "bevel");
        try {
            geo::NamedSolid made = context.kernel.blendEdges(whole, edges, spec, prefix);
            FeatureResult result;
            result.outputs.push_back({input.feature.outputs.at(0), std::move(made.solid),
                                      std::move(made.faces), std::move(made.history)});
            return result;
        } catch (const geo::GeometryError& error) {
            if (error.code() != geo::GeometryErrorCode::blendTooLarge) {
                throw;
            }
            throw tooLarge(error, whole, edges, spec, prefix, context);
        }
    }

    Description describe(const ResolvedParams& params) const override {
        return {id_, {params.at(sizeName_)}};
    }

    Description describeStep(const Feature& feature, const ResolvedParams& params) const override {
        // The number of edges is part of the sentence ("Round 4 edges 2 mm").
        return {id_, {Quantity{double(edgeCount(feature)), {}}, params.at(sizeName_)}};
    }

    bool passesOnNames() const override { return true; }

    // The grip sits as far from the first edge as the size, on the way into the body: dragging
    // it along that way makes the rounding or the bevel larger (rule 1).
    std::vector<HandleSpec> handles(const ResolvedParams& params) const override {
        const geo::Point3d way{valueOf(params, "anchorDx"), valueOf(params, "anchorDy"),
                               valueOf(params, "anchorDz")};
        const double length = std::hypot(way.x, way.y, way.z);
        if (!(length > 0.5)) {
            return {};
        }
        const double size = valueOf(params, sizeName_.c_str());
        const geo::Point3d unit{way.x / length, way.y / length, way.z / length};
        const geo::Point3d edge{valueOf(params, "anchorX"), valueOf(params, "anchorY"),
                                valueOf(params, "anchorZ")};
        std::vector<HandleSpec> out{makeHandle(
            sizeName_.c_str(), HandleKind::distance,
            {edge.x + unit.x * size, edge.y + unit.y * size, edge.z + unit.z * size}, unit, size)};
        markRole(out, 0, HandleRole::rounding);
        return out;
    }

    std::vector<std::string> sizeParameters() const override { return {sizeName_}; }

private:
    static std::size_t edgeCount(const Feature& feature) {
        const auto found = feature.refs.find("edges");
        return found == feature.refs.end() ? 0 : found->second.size();
    }

    bool works(const geo::NamedSolid& whole, const std::vector<geo::BlendEdge>& edges,
               geo::BlendSpec spec, double size, const std::string& prefix,
               const ExecuteContext& context) const {
        spec.size = size;
        try {
            (void)context.kernel.blendEdges(whole, edges, spec, prefix);
            return true;
        } catch (const geo::GeometryError& error) {
            if (error.code() == geo::GeometryErrorCode::blendTooLarge ||
                error.code() == geo::GeometryErrorCode::kernelFailure ||
                error.code() == geo::GeometryErrorCode::invalidResult) {
                return false;
            }
            throw;
        }
    }

    // The error to report: the size that failed, the largest that works (found by halving, as
    // far as it is at least one step), where it is, and the edges concerned.
    geo::GeometryError tooLarge(const geo::GeometryError& failed, const geo::NamedSolid& whole,
                                const std::vector<geo::BlendEdge>& edges,
                                const geo::BlendSpec& spec, const std::string& prefix,
                                const ExecuteContext& context) const {
        geo::GeometryError report(geo::GeometryErrorCode::blendTooLarge, failed.what(), sizeName_);
        report.withNumber("given", spec.size);
        report.withPreview(failed.preview());
        double low = 0.0;
        double high = spec.size;
        for (int attempt = 0; attempt < kMaxTries && !context.cancel.stop_requested(); ++attempt) {
            const double middle = 0.5 * (low + high);
            if (works(whole, edges, spec, middle, prefix, context)) {
                low = middle;
            } else {
                high = middle;
            }
        }
        const double largest = std::floor(low / kSizeStep) * kSizeStep;
        if (largest >= kSizeStep && works(whole, edges, spec, largest, prefix, context)) {
            report.withNumber("maxValue", largest);
        }
        std::vector<std::string> names;
        try {
            const geo::BodySignatures looks = context.kernel.signatures(whole.solid);
            for (const geo::BlendEdge& edge : edges) {
                if (edge.id >= 1 && edge.id <= looks.edges.size()) {
                    if (names.empty()) {
                        report.at(looks.edges[edge.id - 1].centroid);
                    }
                    names.push_back(edge.name.empty() ? "e" + std::to_string(edge.id) : edge.name);
                }
            }
        } catch (const geo::GeometryError&) {
        }
        report.concerning(std::move(names));
        return report;
    }

    std::string id_;
    geo::BlendKind kind_;
    std::string sizeName_;
};

} // namespace

namespace detail {
std::unique_ptr<const core::FeatureType> makeFilletType() {
    return std::make_unique<BlendType>("fillet", geo::BlendKind::round, "radius");
}
std::unique_ptr<const core::FeatureType> makeChamferType() {
    return std::make_unique<BlendType>("chamfer", geo::BlendKind::chamfer, "distance");
}
} // namespace detail

} // namespace fission::features
