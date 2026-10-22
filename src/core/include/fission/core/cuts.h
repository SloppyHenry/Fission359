// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <vector>

#include "fission/core/feature_type.h"
#include "fission/core/history.h"
#include "fission/core/recompute.h"
#include "fission/geo/kernel.h"

namespace fission::core {

/// Keeps the cuts of holes up to date (ADR-0011, rule 13): a body marked "as hole" that touches a
/// body cuts it out, without anybody grouping anything.
///
/// The cuts are ordinary `combine` steps (mode `subtract`, word `auto` = `yes`) that this function
/// adds, changes and removes through commands of `history`; steps the person made by hand are
/// never touched. Call it inside the transaction of the change that may have moved a hole (so
/// that change and its cuts are one step to undo), or let it run alone if none is open.
///
///  - a hole that left a body it cut is taken out of that cut (a cut without holes goes away,
///    and what used it as input uses the body it cut again);
///  - a hole that does not cut any more because it is no longer marked as hole leaves its cuts;
///  - a hole that touches a body it does not cut yet gets a new cut step after the hole's own.
///
/// "Touches" is exact (the kernel intersects both bodies, a box test comes first). A hole that goes
/// all the way through also reaches what it lies on: it is tested made long as well. Calculates the
/// document on the calling thread, but only when it has holes; `cache` is kept between calls so
/// that unchanged steps cost nothing. Returns true if the document changed.
bool reconcileCuts(History& history, const FeatureRegistry& registry,
                   const geo::GeometryKernel& kernel, RecomputeCache& cache);

/// A body a hole cuts or could cut.
struct CutTarget {
    BodyId head; ///< The body as it is shown now.
    BodyId root; ///< The body it started as: what `Body::cutExceptions` names.
    bool cutNow = false;
};

/// The bodies `hole` touches (and cuts, or would cut if it were not taken off the list), in the
/// order of the document. Empty for a body that is no hole.
std::vector<CutTarget> cutTargetsOf(const Document& document, const RecomputeResult& result,
                                    const FeatureRegistry& registry,
                                    const geo::GeometryKernel& kernel, const BodyId& hole);

} // namespace fission::core
