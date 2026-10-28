// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Making a shape larger or smaller in all directions alike (AP-1.6b2). Every size of the shape
// changes in the same proportion: its values, never the geometry (rule 11), so a box with 5 mm
// corners at 200 % has 10 mm corners, round and exact.

#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "fission/core/expression.h"

namespace fission::core {

/// The text of a number as the program writes values: up to 12 significant digits (a snapped
/// 12.5 reads back as "12.5", not "12.5000000001") and always a dot, whatever the language of
/// the computer.
std::string numberText(double value);

/// A value of a step grown or shrunk by `factor` (1.5 = 150 %):
/// - a number (also `40/2` or `2,5 cm`: nothing named in it) becomes the new number, in mm for a
///   length;
/// - a formula is kept: `Breite/2` becomes `(Breite/2) * 1.5` and a lone name `Breite * 1.5`;
/// - a formula that already ends in a factor gets one factor instead of two: `(Breite/2) * 1.5`
///   times 2 is `(Breite/2) * 3`, and times 1/1.5 it is `Breite/2` again.
/// The result is worth exactly `factor` times the old value. Nothing when `expression` is not a
/// valid expression of `kind` or `factor` is not a positive number.
std::optional<std::string> scaledExpression(std::string_view expression, QuantityKind kind,
                                            double factor);

/// A value of a step moved by `delta` (in mm or degrees; the arrow keys of the number field):
/// - a number becomes the new number;
/// - a formula is kept: `Breite/2` becomes `(Breite/2) + 1` and a lone name `Breite + 1`;
/// - a formula that already ends in a step gets one step instead of two: `(Breite/2) + 1` moved
///   by 1 is `(Breite/2) + 2`, and moved back by 2 it is `Breite/2` again.
/// Nothing when `expression` is not a valid expression of `kind` or `delta` is not finite.
std::optional<std::string> adjustedExpression(std::string_view expression, QuantityKind kind,
                                              double delta);

} // namespace fission::core
