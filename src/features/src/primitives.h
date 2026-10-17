// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Private to src/features: shared helpers of the primitive feature types.

#pragma once

#include <cmath>
#include <initializer_list>
#include <optional>
#include <string>
#include <vector>

#include "fission/core/feature_type.h"

namespace fission::features {

/// The first value in `names` that is not a positive finite number, as an issue.
inline std::optional<core::FeatureIssue> requirePositive(const core::ResolvedParams& params,
                                                         std::initializer_list<const char*> names) {
    for (const char* name : names) {
        const auto found = params.find(name);
        if (found == params.end() || !std::isfinite(found->second.value) ||
            !(found->second.value > 0.0)) {
            core::FeatureIssue issue;
            issue.code = core::FeatureIssueCode::parameterOutOfRange;
            issue.subject = name;
            return issue;
        }
    }
    return std::nullopt;
}

/// A word-valued field of the step, or `fallback` when the step has none of that name.
inline const std::string& textOf(const core::Feature& feature, const char* name,
                                 const std::string& fallback) {
    const auto found = feature.texts.find(name);
    return found != feature.texts.end() ? found->second : fallback;
}

/// A handle: `extent` is the length it measures (it ends at `origin`, along `direction`).
inline core::HandleSpec makeHandle(const char* parameter, core::HandleKind kind,
                                   geo::Point3d origin, geo::Point3d direction, double extent,
                                   double travel = 1.0) {
    core::HandleSpec handle;
    handle.id = parameter;
    handle.kind = kind;
    handle.parameter = parameter;
    handle.origin = origin;
    handle.direction = direction;
    handle.travel = travel;
    handle.extent = extent;
    return handle;
}

/// The same handle at both ends of an axis through `centre` (a box grows to both sides, so its
/// width can be taken at the left or at the right face).
inline void addBothEnds(std::vector<core::HandleSpec>& out, const char* parameter,
                        geo::Point3d centre, geo::Point3d axis, double halfLength, double extent,
                        double travel) {
    for (const double side : {1.0, -1.0}) {
        out.push_back(makeHandle(parameter, core::HandleKind::distance,
                                 {centre.x + side * axis.x * halfLength,
                                  centre.y + side * axis.y * halfLength,
                                  centre.z + side * axis.z * halfLength},
                                 {side * axis.x, side * axis.y, side * axis.z}, extent, travel));
    }
}

/// The same handle all around a vertical axis (a cylinder is round: its radius can be taken
/// anywhere). `distance` is from the axis; `inward` points the handle at the axis (a wall that
/// grows inwards). `firstAngle` and `count` fix the places (radians).
inline void addRing(std::vector<core::HandleSpec>& out, const char* parameter,
                    core::HandleKind kind, double cx, double cy, double z, double distance,
                    double extent, bool inward = false, int count = 8, double firstAngle = 0.0) {
    constexpr double kTwoPi = 6.28318530717958647692;
    for (int k = 0; k < count; ++k) {
        const double angle = firstAngle + kTwoPi * k / count;
        const double dx = std::cos(angle);
        const double dy = std::sin(angle);
        out.push_back(makeHandle(parameter, kind, {cx + distance * dx, cy + distance * dy, z},
                                 inward ? geo::Point3d{-dx, -dy, 0.0} : geo::Point3d{dx, dy, 0.0},
                                 extent));
    }
}

/// Gives the handles from place `first` on the role `role` (the ones just added).
inline void markRole(std::vector<core::HandleSpec>& out, std::size_t first, core::HandleRole role) {
    for (std::size_t i = first; i < out.size(); ++i) {
        out[i].role = role;
    }
}

inline double valueOf(const core::ResolvedParams& params, const char* name) {
    return params.at(name).value;
}

} // namespace fission::features
