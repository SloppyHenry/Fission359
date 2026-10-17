// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/features/builtin.h"

#include <memory>

namespace fission::features {

namespace detail {
std::unique_ptr<const core::FeatureType> makeBoxType();
std::unique_ptr<const core::FeatureType> makeCylinderType();
void addShapeTypes(core::FeatureRegistry& registry);
} // namespace detail

void registerBuiltinFeatures(core::FeatureRegistry& registry) {
    registry.add(detail::makeBoxType());
    registry.add(detail::makeCylinderType());
    detail::addShapeTypes(registry);
}

} // namespace fission::features
