// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace fission::core {

/// Named measures usable in formulas (docs/02-ux-konzept.md §2.3), values in millimetres.
/// Names are matched without regard to case; "Höhe" and "Hoehe" are the same name.
using NamedMeasures = std::map<std::string, double>;

/// The kind of number a field takes: lengths know units (mm, cm, in), angles and plain numbers
/// do not (an angle may carry a degree sign).
enum class QuantityKind { length, angle, plain };

/// Evaluates what a person typed into a number field: `25+3`, `40/2`, `2,5 cm`, `1 in`,
/// `Breite/2`. Comma and dot are both decimal separators. Lengths come back in millimetres;
/// a bare number is taken as `defaultUnitMillimetres` (1.0 for the mm fields).
///
/// Returns nothing when the text is not a valid expression (including division by zero).
std::optional<double> evaluateExpression(std::string_view text,
                                         QuantityKind kind = QuantityKind::length,
                                         const NamedMeasures& names = {},
                                         double defaultUnitMillimetres = 1.0);

/// The form in which names are compared: lower case, umlauts written out.
std::string foldMeasureName(std::string_view name);

} // namespace fission::core
