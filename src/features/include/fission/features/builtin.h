// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "fission/core/feature_type.h"

namespace fission::features {

/// Adds the feature types that ship with the program (box, cylinder; more with AP-1.5 ff.).
void registerBuiltinFeatures(core::FeatureRegistry& registry);

} // namespace fission::features
