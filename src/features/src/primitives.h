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

inline double valueOf(const core::ResolvedParams& params, const char* name) {
    return params.at(name).value;
}

} // namespace fission::features
