// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Mirroring and patterns in the sketch mode (AP-2.1e3): pure, exact maths in plane coordinates
// (mm), no window and no document. What comes out is a `SketchEdit` (see sketch_edit.h) with new
// elements only (the originals stay); the sketch controller runs it as one transaction. The copies
// are independent elements, no linked pattern (that belongs to the rules of AP-2.2).

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "fission/core/sketch.h"
#include "fission/interaction/sketch_edit.h"

namespace fission::interaction {

/// At most this many pieces in a pattern, the original included (decision 43).
inline constexpr int kSketchMaxPatternCount = 200;

/// Copies that lie on an existing element within this distance (mm) are not made.
inline constexpr double kSketchCopyTolerance = 1e-6;

enum class SketchCopyNotice {
    none,
    noAxis,      ///< mirror: the element is no line, or the line has no length
    tooMany,     ///< more than `kSketchMaxPatternCount` pieces
    nothing,     ///< fewer than 2 pieces, a distance of 0, an angle of 0 or beyond 360 degrees
    nothingNew,  ///< every copy would lie exactly on an element that is there already
    noSelection, ///< no element chosen (or only the axis itself)
};

struct SketchCopyResult {
    SketchEdit edit; ///< new elements only (ids empty); empty with a notice
    SketchCopyNotice notice = SketchCopyNotice::none;
};

/// The element (any kind, a point too) nearest to `at` within `reach` mm.
std::optional<std::string> pickElementNear(const std::vector<core::SketchElement>& elements,
                                           const core::SketchPoint& at, double reach);

/// The elements `ids` mirrored at the infinite line along the line element `axis`. Points and
/// lines are mirrored, circles at their middle, arcs with start and end swapped (an arc stays
/// counter-clockwise), curves through their points; helper lines stay helper lines. The axis is
/// never mirrored; a copy that lies on an element already there (also its own original: an
/// element on the axis) is not made.
SketchCopyResult mirrorElements(const std::vector<core::SketchElement>& elements,
                                const std::vector<std::string>& ids, const std::string& axis);

/// `count` pieces in a row, the original included: copy k (1 to count - 1) is the original moved
/// by k times `step` (mm). A step of length 0 or a count under 2 makes nothing.
SketchCopyResult linearPattern(const std::vector<core::SketchElement>& elements,
                               const std::vector<std::string>& ids, const core::SketchPoint& step,
                               int count);

/// `count` pieces round `centre`, the original included. At 360 degrees (or -360) they are spread
/// evenly (the last copy does not fall on the original); under that the original and the last
/// copy are `angleDegrees` apart (counter-clockwise when positive). An angle of 0 or beyond 360
/// degrees makes nothing.
SketchCopyResult circularPattern(const std::vector<core::SketchElement>& elements,
                                 const std::vector<std::string>& ids,
                                 const core::SketchPoint& centre, int count, double angleDegrees);

} // namespace fission::interaction
