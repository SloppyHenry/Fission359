// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Trimming and extending in the sketch mode (AP-2.1e1): pure logic in plane coordinates (mm), no
// window and no document. The crossings come from the geometry layer; what comes out is a
// `SketchEdit`: which elements move, which go and which are new. The sketch controller runs one
// edit as one transaction of the sketch commands (move points, remove, add).

#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "fission/core/commands.h"
#include "fission/core/sketch.h"
#include "fission/geo/kernel.h"

namespace fission::interaction {

/// Why an edit did less than was asked.
enum class SketchEditNotice {
    none,
    curvePartial,    ///< a curve (spline) that is crossed cannot be trimmed in part
    nothingToExtend, ///< no other curve in the way of the end
    failed           ///< the geometry layer could not calculate the crossings
};

/// What an edit changes in the sketch, in the order it is done: `moves` (the first piece of a
/// trimmed element keeps its id and gets new end points), then `removed`, then `added` (new
/// elements; their ids are empty, the document gives them out).
struct SketchEdit {
    std::vector<core::SketchPointMove> moves;
    std::vector<std::string> removed;
    std::vector<core::SketchElement> added;
    SketchEditNotice notice = SketchEditNotice::none;

    bool empty() const noexcept { return moves.empty() && removed.empty() && added.empty(); }
};

/// A line, circle or arc reduced to what the maths needs (used by trimming and extending).
struct SketchShape {
    core::SketchElementKind kind = core::SketchElementKind::line;
    core::SketchPoint first;  ///< line: start; circle, arc: the middle
    core::SketchPoint last;   ///< line: end; arc: the point where it starts to go (its start)
    core::SketchPoint finish; ///< arc: the point that gives its end direction
    double radius = 0.0;
    double start = 0.0; ///< circle, arc: the angle of parameter 0 (radians)
    double span = 0.0;  ///< line: the length (mm); arc: the sweep; circle: 2π (radians)
};

/// One piece of an element between its neighbouring crossings (or its ends). A line or an arc is
/// cut at the crossings inside it; a circle that is cut at two or more crossings is a ring of
/// pieces; a circle with fewer, and a curve (spline), is one piece.
struct SketchPiece {
    std::string element;
    std::size_t index = 0;

    bool operator==(const SketchPiece&) const = default;
};

/// The pieces of the elements of a sketch, to find the one under the pointer and to take some
/// away (decisions 33 and 34).
class SketchTrimmer {
public:
    /// Crossings nearer than 0.01 mm to an end of an element, or to the one before, do not cut.
    SketchTrimmer(const geo::GeometryKernel& kernel,
                  const std::vector<core::SketchElement>& elements);

    /// False when the geometry layer could not calculate the crossings: nothing can be trimmed.
    bool usable() const noexcept { return usable_; }

    /// The piece nearest to `at` within `reach` mm (points have no pieces). Where two elements are
    /// equally near (at a corner), the one with the id `prefer` wins, so that a stroke along an
    /// element stays on it; else the first.
    std::optional<SketchPiece> pieceAt(const core::SketchPoint& at, double reach,
                                       const std::string& prefer = {}) const;
    /// The piece as a chain of short lines, for drawing it (red).
    std::vector<core::SketchPoint> outline(const SketchPiece& piece) const;
    /// Takes the pieces away, all against the sketch as it is now (the crossings of pieces that
    /// go in the same stroke still count). A curve that is crossed stays and gives
    /// `curvePartial`; a curve nothing crosses goes whole.
    SketchEdit trim(const std::vector<SketchPiece>& marked) const;

private:
    struct Cut {
        double parameter = 0.0;
        core::SketchPoint point;
    };
    struct Part {
        core::SketchElement element;
        std::vector<Cut> cuts; ///< inside the element, by parameter
        bool crossed = false;  ///< any crossing at all (a curve with one cannot be trimmed)
        SketchShape shape;
        std::vector<core::SketchPoint> curve; ///< a spline as the kernel draws it
    };

    std::size_t pieceCount(const Part& part) const;
    std::optional<std::size_t> pieceOf(const Part& part, double parameter) const;
    // the places a piece starts and ends: parameter and point
    Cut pieceStart(const Part& part, std::size_t index) const;
    Cut pieceEnd(const Part& part, std::size_t index) const;
    const Part* find(const std::string& element) const;

    std::vector<Part> parts_;
    bool usable_ = true;
};

/// An end of a line or arc that can go on up to the next curve.
struct SketchExtension {
    std::string element;
    bool atEnd = true; ///< the end of the element (else its start)
    core::SketchPoint from;
    core::SketchPoint target;
    /// The way from `from` to `target`, as a chain of short lines, to show it.
    std::vector<core::SketchPoint> preview;
};

/// The extension a click at `at` would make: the line or arc within `reach` mm of `at` (the nearest
/// one), its end that is nearer to `at`, and the nearest crossing with another curve beyond that
/// end (a line goes straight on, an arc on its circle; crossings at the end itself do not count).
/// Nothing when no line or arc is near or nothing lies in the way. `usable` (if given) is set to
/// false when the geometry layer failed.
std::optional<SketchExtension> findExtension(const geo::GeometryKernel& kernel,
                                             const std::vector<core::SketchElement>& elements,
                                             const core::SketchPoint& at, double reach,
                                             bool* usable = nullptr);

/// The edit that makes the extension: the end point of the element moves to the target.
SketchEdit extensionEdit(const std::vector<core::SketchElement>& elements,
                         const SketchExtension& extension);

} // namespace fission::interaction
