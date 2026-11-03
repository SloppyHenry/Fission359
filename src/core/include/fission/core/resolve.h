// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "fission/core/body_looks.h"
#include "fission/core/persistent_ref.h"
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

/// `looks` as seen from the step's own coordinates: the placement taken out again.
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

/// Why a reference could not be resolved safely (codes for the message catalogue, AP-2.14).
enum class HelpReason {
    none,
    split,    ///< The face was cut into pieces.
    consumed, ///< A step used the element up.
    turned,   ///< The name points to an element that turned by more than 45°.
    swapped,  ///< Two equal elements probably changed places.
    notFound  ///< Nothing is clearly the element any more.
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
    std::string detail; ///< Short English explanation for tests and logs, never for the UI.
};

/// Finds the element a reference points to in `now` (the body as it is, in world coordinates)
/// or says honestly that it cannot (rule 6). `consumed` is the body's memory of used-up
/// elements; `placement` tells how `now` sits compared with the step's own coordinates, in which
/// the reference was saved. Only the strict mode exists (ADR-0003 point 6).
ResolveOutcome resolve(const PersistentRef& ref, const BodyLooks& now,
                       const std::vector<ConsumedElement>& consumed, const Placement& placement,
                       const ResolveLimits& limits = {}, const ResolveSwitches& switches = {});

/// The reference after a safe resolution: it points to the current name and look (in the step's
/// own coordinates). Unchanged unless the outcome is `found`.
PersistentRef refreshed(const PersistentRef& ref, const BodyLooks& now,
                        const ResolveOutcome& outcome, const Placement& placement);

} // namespace fission::core
