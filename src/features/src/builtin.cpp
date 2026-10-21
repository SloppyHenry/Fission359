// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/features/builtin.h"

#include <memory>

namespace fission::features {

namespace detail {
std::unique_ptr<const core::FeatureType> makeBoxType();
std::unique_ptr<const core::FeatureType> makeCylinderType();
void addShapeTypes(core::FeatureRegistry& registry);
std::unique_ptr<const core::FeatureType> makeTextType();
std::unique_ptr<const core::FeatureType> makeCombineType();
std::unique_ptr<const core::FeatureType> makePlaceType();
std::unique_ptr<const core::FeatureType> makeSplitType();
} // namespace detail

void registerBuiltinFeatures(core::FeatureRegistry& registry) {
    registry.add(detail::makeBoxType());
    registry.add(detail::makeCylinderType());
    detail::addShapeTypes(registry);
    registry.add(detail::makeTextType());
    registry.add(detail::makeCombineType());
    registry.add(detail::makePlaceType());
    registry.add(detail::makeSplitType());
}

} // namespace fission::features
