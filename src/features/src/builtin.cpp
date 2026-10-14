// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/features/builtin.h"

#include <memory>

namespace fission::features {

namespace detail {
std::unique_ptr<const core::FeatureType> makeBoxType();
std::unique_ptr<const core::FeatureType> makeCylinderType();
} // namespace detail

void registerBuiltinFeatures(core::FeatureRegistry& registry) {
    registry.add(detail::makeBoxType());
    registry.add(detail::makeCylinderType());
}

} // namespace fission::features
