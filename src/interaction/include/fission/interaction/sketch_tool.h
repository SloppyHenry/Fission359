// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The drawing tools of the sketch mode (AP-2.1c1): line (a chain of clicks), rectangle, circle and
// point. A state machine in plane coordinates (mm) with no window and no document: clicks, the
// pointer and typed numbers go in; the preview and the finished elements come out. The sketch
// controller wires it to the view and turns each finished batch into one command.

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "fission/core/sketch.h"

namespace fission::interaction {

enum class SketchToolKind { none, line, rectangle, circle, point };

/// Variants of a tool: the rectangle by two corners (0) or by its middle and a corner (1); the
/// circle by its middle and a point on it (0, the radius) or by three points on it (1).
inline constexpr int kSketchVariantFirst = 0;
inline constexpr int kSketchVariantSecond = 1;

/// Anything shorter (a line, a side, a radius) is not drawn: mm.
inline constexpr double kSketchMinimumSize = 0.01;

/// Elements that belong together: they become one command (one undo step). The ids are empty;
/// the document gives them out.
using SketchBatch = std::vector<core::SketchElement>;

/// What a click or a key did, apart from the finished batches.
enum class SketchToolNotice {
    none,
    collinear, ///< three points on one line: no circle goes through them
    tooSmall   ///< shorter than `kSketchMinimumSize`: nothing was drawn
};

struct SketchToolStep {
    std::vector<SketchBatch> done;
    SketchToolNotice notice = SketchToolNotice::none;
};

/// One number the person may type while the element is pulled, and what it is now.
struct SketchReadoutField {
    /// "length" and "angle" (line), "width" and "height" (rectangle), "radius" (circle).
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

/// The circle through three points as {middle, first point}; nothing when they lie on one line (or
/// two are the same), so that no circle fits.
std::optional<core::SketchElement> circleThroughThreePoints(const core::SketchPoint& a,
                                                            const core::SketchPoint& b,
                                                            const core::SketchPoint& c);

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
    /// A double click: ends a chain of lines (the click that came first has put its point).
    SketchToolStep doubleClick();
    /// A typed number (mm; degrees for the angle) for field 0 or 1; nothing clears it.
    void setTyped(int field, std::optional<double> value);
    bool hasTyped() const noexcept { return typed_[0] || typed_[1]; }
    /// Enter: the element as the typed numbers and the pointer make it.
    SketchToolStep accept(bool alt = false);
    /// Esc: ends the running element (a chain of lines stays as far as it is drawn). False when
    /// nothing was running.
    bool cancel();

    SketchDraft draft() const;

private:
    struct Shape {
        SketchBatch elements;
        std::vector<SketchReadoutField> fields;
        core::SketchPoint at;
        bool tooSmall = false;
    };

    Shape shape(const core::SketchPoint& pointer, bool alt) const;
    SketchToolStep finish(const core::SketchPoint& pointer, bool alt);
    void markHelper(SketchBatch& batch, bool alt) const;

    SketchToolKind kind_ = SketchToolKind::none;
    int variant_ = kSketchVariantFirst;
    bool helper_ = false;
    std::vector<core::SketchPoint> placed_;
    core::SketchPoint pointer_;
    bool alt_ = false;
    std::optional<double> typed_[2];
};

} // namespace fission::interaction
