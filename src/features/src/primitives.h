// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Private to src/features: shared helpers of the primitive feature types.

#pragma once

#include <cmath>
#include <initializer_list>
#include <optional>
#include <string>

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

inline double valueOf(const core::ResolvedParams& params, const char* name) {
    return params.at(name).value;
}

} // namespace fission::features
