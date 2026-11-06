// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "fission/geo/signature.h"
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
///
/// `history` tells what the operation did to the names of its inputs' faces. It is empty for
/// operations that make a body from nothing and for pure moves (every face stays as it was).
struct NamedSolid {
    Solid solid;
    std::vector<NamedFace> faces;
    ElementHistory history = {};
};

} // namespace fission::geo
