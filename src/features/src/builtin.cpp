// SPDX-FileCopyrightText: Fission359 contributors
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
std::unique_ptr<const core::FeatureType> makeFilletType();
std::unique_ptr<const core::FeatureType> makeChamferType();
std::unique_ptr<const core::FeatureType> makePushFaceType();
std::unique_ptr<const core::FeatureType> makeStretchType();
std::unique_ptr<const core::FeatureType> makeRemoveFacesType();
std::unique_ptr<const core::FeatureType> makeOffsetFacesType();
std::unique_ptr<const core::FeatureType> makeTiltFaceType();
std::unique_ptr<const core::FeatureType> makeShellType();
std::unique_ptr<const core::FeatureType> makeDraftType();
std::unique_ptr<const core::FeatureType> makeImportType();
std::unique_ptr<const core::FeatureType> makeSketchType();
std::unique_ptr<const core::FeatureType> makeExtrudeType();
} // namespace detail

void registerBuiltinFeatures(core::FeatureRegistry& registry) {
    registry.add(detail::makeBoxType());
    registry.add(detail::makeCylinderType());
    detail::addShapeTypes(registry);
    registry.add(detail::makeTextType());
    registry.add(detail::makeCombineType());
    registry.add(detail::makePlaceType());
    registry.add(detail::makeSplitType());
    registry.add(detail::makeFilletType());
    registry.add(detail::makeChamferType());
    registry.add(detail::makePushFaceType());
    registry.add(detail::makeStretchType());
    registry.add(detail::makeRemoveFacesType());
    registry.add(detail::makeOffsetFacesType());
    registry.add(detail::makeTiltFaceType());
    registry.add(detail::makeShellType());
    registry.add(detail::makeDraftType());
    registry.add(detail::makeImportType());
    registry.add(detail::makeSketchType());
    registry.add(detail::makeExtrudeType());
}

} // namespace fission::features
