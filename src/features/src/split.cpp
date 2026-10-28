// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Splits a body at a plane across one axis of the bed (AP-1.7e, ADR-0011): two new bodies, the
// part below the plane (A) and the part above it (B). The faces of the cut are named
// `F<n>/split/cutA` and `F<n>/split/cutB`; every other face keeps the name it had.

#include <cmath>
#include <memory>

#include "fission/features/builtin.h"

#include "primitives.h"

namespace fission::features {

namespace {

using namespace fission::core;

constexpr double kEdge = 1e-9;

// The axis (0 = x, 1 = y, 2 = z) as a number of the step.
int axisOf(const ResolvedParams& params) {
    return static_cast<int>(std::lround(valueOf(params, "axis")));
}

double along(const geo::Point3d& point, int axis) {
    return axis == 0 ? point.x : axis == 1 ? point.y : point.z;
}

class SplitType final : public FeatureType {
public:
    std::string_view id() const override { return "split"; }
    int version() const override { return 1; }
    int newBodies() const override { return 2; }

    std::vector<ParamSpec> parameters() const override {
        // `anchorU` and `anchorV`: where the arrow of the plane stands on it (the other two axes in
        // the order x, y, z), set when the step is made and not part of what the person edits.
        return {{"axis", QuantityKind::plain, "2"},
                {"position", QuantityKind::length, "0"},
                {"anchorU", QuantityKind::length, "0"},
                {"anchorV", QuantityKind::length, "0"}};
    }

    std::optional<FeatureIssue> validate(const ResolvedParams& params) const override {
        const double axis = valueOf(params, "axis");
        if (!std::isfinite(axis) || axis != std::round(axis) || axis < 0.0 || axis > 2.0) {
            FeatureIssue issue;
            issue.code = FeatureIssueCode::parameterOutOfRange;
            issue.subject = "axis";
            return issue;
        }
        return std::nullopt;
    }

    FeatureResult execute(const ExecuteInput& input, const ExecuteContext& context) const override {
        if (input.inputs.size() != 1 || input.feature.outputs.size() != 2) {
            throw geo::GeometryError(geo::GeometryErrorCode::invalidInput,
                                     "split: needs one body and makes two");
        }
        const FeatureOutput& body = input.inputs.front();
        const int axis = axisOf(input.params);
        const double at = valueOf(input.params, "position");
        const geo::Bounds box = context.kernel.bounds(body.solid);
        const double low = along(box.min, axis);
        const double high = along(box.max, axis);
        if (!(at > low + kEdge) || !(at < high - kEdge)) {
            throw geo::GeometryError(geo::GeometryErrorCode::invalidInput,
                                     "split: the plane does not cut the body", "position");
        }

        // Two blocks, one each side of the plane, large enough to hold the whole body: what the
        // body shares with each of them is a part. Only the face on the plane touches the part.
        const double margin = 1.0 + box.diagonal();
        const auto block = [&](bool lower) {
            geo::Point3d corner{box.min.x - margin, box.min.y - margin, box.min.z - margin};
            double size[3] = {box.max.x - box.min.x + 2.0 * margin,
                              box.max.y - box.min.y + 2.0 * margin,
                              box.max.z - box.min.z + 2.0 * margin};
            double* cornerAxis = axis == 0 ? &corner.x : axis == 1 ? &corner.y : &corner.z;
            if (lower) {
                size[axis] = at - *cornerAxis;
            } else {
                size[axis] = along(box.max, axis) + margin - at;
                *cornerAxis = at;
            }
            geo::NamedSolid made = context.kernel.makeBox(size[0], size[1], size[2], corner, 0.0);
            // The face of the block on the plane is the one named after the side it faces.
            const char* facing = lower ? (axis == 0   ? "right"
                                          : axis == 1 ? "back"
                                                      : "top")
                                       : (axis == 0   ? "left"
                                          : axis == 1 ? "front"
                                                      : "bottom");
            for (geo::NamedFace& face : made.faces) {
                face.role = face.role == facing
                                ? elementName(input.feature, lower ? "cutA" : "cutB")
                                : "split/block/" + face.role;
            }
            return made;
        };

        const geo::NamedSolid whole{body.solid, body.faces};
        FeatureResult result;
        for (const bool lower : {true, false}) {
            geo::NamedSolid part =
                context.kernel.boolean(geo::BooleanOp::intersect, whole, {block(lower)});
            result.outputs.push_back({input.feature.outputs.at(lower ? 0 : 1),
                                      std::move(part.solid), std::move(part.faces)});
        }
        return result;
    }

    Description describe(const ResolvedParams& params) const override {
        return {"split", {params.at("axis"), params.at("position")}};
    }

    bool passesOnNames() const override { return true; }

    std::vector<HandleSpec> handles(const ResolvedParams& params) const override {
        const int axis = axisOf(params);
        if (axis < 0 || axis > 2) {
            return {};
        }
        const double u = valueOf(params, "anchorU");
        const double v = valueOf(params, "anchorV");
        const double at = valueOf(params, "position");
        // The other two axes, in the order x, y, z.
        geo::Point3d origin = axis == 0   ? geo::Point3d{at, u, v}
                              : axis == 1 ? geo::Point3d{u, at, v}
                                          : geo::Point3d{u, v, at};
        geo::Point3d direction{axis == 0 ? 1.0 : 0.0, axis == 1 ? 1.0 : 0.0, axis == 2 ? 1.0 : 0.0};
        return {makeHandle("position", HandleKind::distance, origin, direction, 0.0)};
    }

    std::vector<std::string> sizeParameters() const override { return {}; }
};

} // namespace

namespace detail {
std::unique_ptr<const core::FeatureType> makeSplitType() {
    return std::make_unique<SplitType>();
}
} // namespace detail

} // namespace fission::features
