// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "fission/geo/solid.h"

namespace fission::geo {

/// A face together with the role its creator gave it ("top", "side"). `faceId` counts like
/// `TriangleMesh::faceIds` (1-based, in the order of the body's faces).
struct NamedFace {
    std::uint32_t faceId = 0;
    std::string role;
};

/// A body and the roles of its faces. The naming contract (ADR-0003): an operation names **every**
/// face it creates, so `faces` has exactly one entry per face of `solid`.
struct NamedSolid {
    Solid solid;
    std::vector<NamedFace> faces;
};

} // namespace fission::geo
