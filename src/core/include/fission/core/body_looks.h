// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "fission/core/persistent_ref.h"
#include "fission/core/recompute.h"
#include "fission/geo/signature.h"

namespace fission::core {

/// A face or edge of a body: its name (empty if it has none, see `ElementNames`) and its look.
struct NamedLook {
    std::uint32_t id = 0; ///< Number of the face / edge in the body (as in the mesh).
    std::string name;
    ElementLook look;
};

/// What every face and edge of one body looks like now, with names. The base of the resolution
/// of references (AP-2.10b). Entries are in face / edge order of the body.
struct BodyLooks {
    std::vector<NamedLook> faces;
    std::vector<NamedLook> edges;
    double diagonal = 0.0; ///< Diagonal of the body's box (mm).
};

/// Translates the kernel's signatures into looks: names the faces (`faces` as `BodyResult`
/// carries them), names the edges after their two faces (`ElementNames`, the same rules as the
/// selection) and fills in the neighbours.
BodyLooks describeBody(const std::vector<BodyFace>& faces, const geo::BodySignatures& signatures);

/// Looks of `before` (an input of a step) that do not exist in `after` (its result): the faces
/// and edges the step used up. Unnamed elements cannot be remembered and are left out.
std::vector<ConsumedElement> consumedBetween(const BodyLooks& before, const BodyLooks& after,
                                             const FeatureId& step);

/// Adds `more` to `list` unless an element of that name is already in it (the first stays).
void mergeConsumed(std::vector<ConsumedElement>& list, const std::vector<ConsumedElement>& more);

} // namespace fission::core
