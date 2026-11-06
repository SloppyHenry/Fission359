// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "fission/core/body_looks.h"
#include "fission/core/document.h"
#include "fission/core/feature_type.h"
#include "fission/core/help_reason.h"
#include "fission/core/persistent_ref.h"
#include "fission/core/recompute.h"
#include "fission/geo/rotation.h"
#include "fission/geo/types.h"

namespace fission::core {

/// How a body sits in the world compared with the frame of the step that made it: the step's
/// own coordinates turned by `angles` around `centre`, then moved by `offset` (ADR-0010).
/// References are saved in the step's own coordinates ("body coordinates"), so a body that was
/// moved or turned as a whole never looks changed (ADR-0010 point 4).
struct Placement {
    geo::Point3d centre;
    geo::Angles angles;
    geo::Point3d offset;

    bool operator==(const Placement&) const = default;
};

/// The place of the step that made the element called `elementName` ("F5/box/top",
/// "edge[F5/box/top|...]"): its turn about the middle of its body (ADR-0010 point 4). Identity
/// when the step is not found, was not calculated or does not turn. References are saved in and
/// compared in the coordinates of that step.
Placement placementOf(const Document& document, const FeatureRegistry& registry,
                      const std::vector<FeatureState>& states, const std::string& elementName);

/// Where a reference used to point, to be drawn as a ghost: four corners of a square of the same
/// area as the old face (round about its centre, in its plane or along its axis), or the two ends
/// of the old edge. World coordinates (mm), the place of the creating step put back.
struct RefGhost {
    ElementKind kind = ElementKind::face;
    std::vector<geo::Point3d> points; ///< 4 for a face, 2 for an edge.
};

RefGhost ghostOf(const PersistentRef& ref, const Placement& placement);

/// A reference to the face or edge `id` of a body that looks like `looks` now: what the person
/// picked. Saved in the coordinates of the creating step (see `placementOf`). Nothing for a
/// number the body does not have or an element without a name (it could not be found again).
std::optional<PersistentRef> makeRef(const BodyLooks& looks, ElementKind kind, std::uint32_t id,
                                     const Placement& placement);

/// The place of the step that made the element with this name (see `placementOf`). Every
/// element is compared in the coordinates of its own step, so elements of different steps (the
/// old and the new box after a step was replaced) can be compared at all.
using PlacementFn = std::function<Placement(const std::string& elementName)>;

/// `looks` as seen from the steps' own coordinates: each element with the placement taken out.
BodyLooks toBodyFrame(const BodyLooks& looks, const PlacementFn& placementOf);
BodyLooks toBodyFrame(const BodyLooks& looks, const Placement& placement);
ElementLook toBodyFrame(const ElementLook& look, const Placement& placement);

/// The limits of the resolution (ADR-0003 point 4). The values come from the measurements in
/// docs/spikes/persistente-referenzen.md; do not loosen them without a new measurement and an
/// addendum to the ADR.
struct ResolveLimits {
    double directionGateDegrees = 15.0; ///< Similarity search: direction must be this close.
    double plausibleDegrees = 45.0;     ///< A name hit that turned more is suspicious.
    double positionRadiusFactor = 0.25; ///< Distance scale relative to the body diagonal.
    double minScore = 0.80;             ///< A winner needs at least this similarity ...
    double minMargin = 0.20;            ///< ... and this much lead over the strongest rival.
    double consumedSlack = 0.05; ///< A consumed look-alike this close to the best is a rival.
    double consumedFloor = 0.55; ///< Below this a consumed look-alike is no rival.
    double nearestAbsolute =
        0.02;                  ///< Winner within this fraction of the diagonal of the old place ...
    double nearestRatio = 0.1; ///< ... and at least this much closer than the runner-up.
    double swapOther = 0.9;    ///< Places swapped: an equal element sits at the old place ...
    double swapNamed = 0.5;    ///< ... while the named one is away from it ...
    double swapGap = 0.4;      ///< ... by at least this much.
};

/// The limits in use. The switches for "turn a building block off" in the tests live here too:
/// they exist so that a test can prove that each block is needed.
struct ResolveSwitches {
    bool useConsumed = true;         ///< Consumed elements compete in the similarity search.
    bool checkSwap = true;           ///< "Places swapped" check on name hits.
    bool checkTurn = true;           ///< 45° rule on name hits.
    bool recogniseNeighbours = true; ///< Neighbours whose name is gone are matched by their look.
};

/// How a found element was recognised.
enum class FoundBy { name, similarity };

struct ResolveOutcome {
    enum class State { found, needsHelp };

    State state = State::needsHelp;
    HelpReason reason = HelpReason::notFound; ///< Only for `needsHelp`.
    FoundBy by = FoundBy::name;               ///< Only for `found`.
    std::string element;                      ///< Name of the found element (may be empty).
    std::uint32_t id = 0;                     ///< Its number in the body (face / edge).
    std::size_t index = 0;                    ///< Its position in `BodyLooks::faces` / `edges`.
    double score = 0.0;                       ///< Similarity (1 for a name hit).
    FeatureId consumedBy; ///< For `HelpReason::consumed`: the step that used the element up.
    std::string detail;   ///< Short English explanation for tests and logs, never for the UI.
};

/// Finds the element a reference points to in `now` (the body as it is, in world coordinates)
/// or says honestly that it cannot (rule 6). `consumed` is the body's memory of used-up
/// elements; `placement` tells how each element of `now` sits compared with the coordinates of the
/// step that made it, in which the references were saved. Only the strict mode exists (ADR-0003
/// point 6).
ResolveOutcome resolve(const PersistentRef& ref, const BodyLooks& now,
                       const std::vector<ConsumedElement>& consumed, const PlacementFn& placement,
                       const ResolveLimits& limits = {}, const ResolveSwitches& switches = {});
/// The same with one place for every element (a body that was moved or turned as a whole).
ResolveOutcome resolve(const PersistentRef& ref, const BodyLooks& now,
                       const std::vector<ConsumedElement>& consumed, const Placement& placement,
                       const ResolveLimits& limits = {}, const ResolveSwitches& switches = {});

/// The reference after a safe resolution: it points to the current name and look (in the step's
/// own coordinates). Unchanged unless the outcome is `found`.
PersistentRef refreshed(const PersistentRef& ref, const BodyLooks& now,
                        const ResolveOutcome& outcome, const PlacementFn& placement);
PersistentRef refreshed(const PersistentRef& ref, const BodyLooks& now,
                        const ResolveOutcome& outcome, const Placement& placement);

} // namespace fission::core
