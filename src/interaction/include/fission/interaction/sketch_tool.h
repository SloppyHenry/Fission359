// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The drawing tools of the sketch mode (AP-2.1c1, AP-2.1c2, AP-2.1d): line (a chain of clicks, with
// the arc that joins its end tangentially), rectangle, circle, point, arc, slot, polygon and the
// curve (a spline through the clicked points). A state
// machine in plane coordinates (mm) with no window and no document: clicks, the pointer and typed
// numbers go in; the preview and the finished elements come out. The sketch controller wires it to
// the view and turns each finished batch into one command.

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "fission/core/sketch.h"
#include "fission/interaction/sketch_shapes.h"

namespace fission::interaction {

/// `trim`, `extend` (AP-2.1e1), `offset` and `fillet` (AP-2.1e2) draw nothing: they edit what is
/// drawn, which the sketch controller does with `SketchTrimmer` and `findExtension`; here they are
/// only names that can be chosen.
enum class SketchToolKind {
    none,
    line,
    rectangle,
    circle,
    point,
    arc,
    slot,
    polygon,
    spline,
    trim,
    extend,
    offset, ///< AP-2.1e2: a copy of a chain at a distance
    fillet  ///< AP-2.1e2: round (variant 0) or chamfer (1) a corner
};

/// Variants of a tool: the rectangle by two corners (0) or by its middle and a corner (1); the
/// circle by its middle and a point on it (0, the radius) or by three points on it (1); the arc by
/// three points (0: start, end, a point on it) or by middle, start and end (1); the polygon by a
/// corner (0, the circle round it) or by the middle of a side (1, the circle inside it).
inline constexpr int kSketchVariantFirst = 0;
inline constexpr int kSketchVariantSecond = 1;

/// What a click or a key did, apart from the finished batches.
enum class SketchToolNotice {
    none,
    collinear, ///< three points on one line: no circle or arc goes through them
    tooSmall,  ///< shorter than `kSketchMinimumSize`: nothing was drawn
    badSides   ///< a typed number of sides that is not a whole number from 3 to 64
};

struct SketchToolStep {
    std::vector<SketchBatch> done;
    SketchToolNotice notice = SketchToolNotice::none;
};

/// One number the person may type while the element is pulled, and what it is now.
struct SketchReadoutField {
    /// "length" and "angle" (line, the first step of the slot, the arc by middle: only "angle"),
    /// "width" and "height" (rectangle), "radius" (circle, arc by three points, arc joining a
    /// line), "width" (slot), "radius", "sides" and "angle" (polygon).
    std::string name;
    double value = 0.0; ///< mm, or degrees against the u axis for "angle"
    bool typed = false; ///< came from the keyboard, not from the pointer
};

/// The element being pulled, to be drawn strongly, and its numbers.
struct SketchDraft {
    std::vector<core::SketchElement> elements;
    std::vector<SketchReadoutField> fields;
    core::SketchPoint at; ///< where the numbers belong (the pointer, or what the typing made of it)
};

class SketchTool {
public:
    /// Chooses a tool (and variant); a running element is dropped. `none` = no tool.
    void select(SketchToolKind kind, int variant = kSketchVariantFirst);
    SketchToolKind kind() const noexcept { return kind_; }
    int variant() const noexcept { return variant_; }

    /// The switch "helper line": everything drawn from now on is an aid (dashed).
    void setHelperLine(bool on) noexcept { helper_ = on; }
    bool helperLine() const noexcept { return helper_; }

    /// An element is being pulled (the first click was made).
    bool busy() const noexcept { return !placed_.empty(); }
    /// How many clicks of the running element are made.
    std::size_t placedCount() const noexcept { return placed_.size(); }
    /// How many numbers can be typed now (0 when nothing is pulled or the tool has none).
    int fieldCount() const;

    /// The pointer is at `position`; `alt` = the key that makes a helper line is held.
    void moveTo(const core::SketchPoint& position, bool alt = false);
    /// A click at `position`.
    SketchToolStep click(const core::SketchPoint& position, bool alt = false);
    /// A double click: ends a chain of lines (the click that came first has put its point), or
    /// sets the curve through the points clicked so far.
    SketchToolStep doubleClick();
    /// Backspace: takes the last point of the curve back (the curve is the only tool that has
    /// points to take back). False when there was nothing to take back.
    bool undoPoint();
    /// The points clicked so far of the element being pulled (not yet in the sketch).
    const std::vector<core::SketchPoint>& placedPoints() const noexcept { return placed_; }
    /// A typed number (mm; degrees for the angle) for field 0, 1 or 2; nothing clears it. A
    /// number of sides that is not allowed is not taken (`badSides`); the old number stays.
    SketchToolNotice setTyped(int field, std::optional<double> value);
    bool hasTyped() const noexcept { return typed_[0] || typed_[1] || typed_[2]; }
    /// Enter: the element as the typed numbers and the pointer make it.
    SketchToolStep accept(bool alt = false);
    /// Esc: ends the running element (a chain of lines stays as far as it is drawn). While an arc
    /// is pulled from the end of a line it ends only that. False when nothing was running.
    bool cancel();

    /// The button goes down at `position` (AP-2.1c2). Returns true when it is on the end of the
    /// last line of a chain (within `reach` mm): pulling on from there makes an arc that joins
    /// that line tangentially, once the pointer is more than `dragDistance` mm away. Else the
    /// press is nothing special (the click that follows is a click).
    bool press(const core::SketchPoint& position, double reach, double dragDistance);
    /// The button goes up: sets the arc when one was pulled. Nothing otherwise.
    SketchToolStep release(const core::SketchPoint& position, bool alt = false);
    /// An arc is being pulled from the end of a line.
    bool dragging() const noexcept { return dragging_; }

    /// The number of sides the polygon has now (the last one used).
    int polygonSides() const noexcept { return sides_; }
    void setPolygonSides(int sides) noexcept;

    SketchDraft draft() const;

private:
    struct Shape {
        SketchBatch elements;
        std::vector<SketchReadoutField> fields;
        SketchBatch guides; ///< aids drawn while the element is pulled (dashed)
        core::SketchPoint at;
        bool tooSmall = false;
        bool straight = false; ///< an arc was wanted that is a straight line
        std::optional<TangentArc> tangent;
    };

    Shape shape(const core::SketchPoint& pointer, bool alt) const;
    SketchToolStep finish(const core::SketchPoint& pointer, bool alt);
    void markHelper(SketchBatch& batch, bool alt) const;
    core::SketchPoint pulledEnd(const core::SketchPoint& first, const core::SketchPoint& pointer,
                                Shape& out) const;
    Shape tangentShape(const core::SketchPoint& pointer) const;
    Shape arcShape(const core::SketchPoint& pointer) const;
    Shape slotShape(const core::SketchPoint& pointer) const;
    Shape polygonShape(const core::SketchPoint& pointer) const;
    Shape splineShape(const core::SketchPoint& pointer) const;
    SketchToolStep finishSpline(std::vector<core::SketchPoint> points, bool alt);
    SketchToolStep advanceSlot(const core::SketchPoint& pointer, bool alt);
    void clearTyped() noexcept;

    SketchToolKind kind_ = SketchToolKind::none;
    int variant_ = kSketchVariantFirst;
    bool helper_ = false;
    std::vector<core::SketchPoint> placed_;
    core::SketchPoint pointer_;
    bool alt_ = false;
    std::optional<double> typed_[3];
    int sides_ = kSketchDefaultSides;
    // the arc that joins the end of the last line (AP-2.1c2)
    std::optional<core::SketchPoint> lastDirection_; ///< unit vector at the end of the chain
    bool armed_ = false;                             ///< pressed on the end of the chain
    bool dragging_ = false; ///< ... and pulled further than `dragDistance_`
    core::SketchPoint pressAt_;
    double dragDistance_ = 0.0;
    // the arc by middle, start and end: how far the pointer has turned (radians, counter-clockwise)
    double sweep_ = 0.0;
    double lastAngle_ = 0.0;
};

} // namespace fission::interaction
