// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "fission/core/expression.h"

namespace fission::core {

/// A named measure (benanntes Maß) as the document stores it: a name and what the person typed.
struct MeasureDefinition {
    std::string name;
    std::string expression;
    QuantityKind kind = QuantityKind::length;
};

/// The outcome for one measure.
struct MeasureResult {
    std::string name;
    std::optional<Quantity> value;        ///< Set when the measure could be calculated.
    std::optional<ExpressionError> error; ///< Set when not; `where` points into its expression.
    std::vector<std::string> dependsOn;   ///< Measures its formula uses directly.
    std::vector<std::string> usedBy;      ///< Measures whose formulas use it directly.
};

/// All measures of a document, calculated together.
struct MeasureResolution {
    /// One entry per definition, in the order of the definitions.
    std::vector<MeasureResult> results;
    /// Indices into `results`, dependencies first: the order in which they were calculated.
    std::vector<std::size_t> order;

    /// Finds a measure by name (case and umlauts do not matter).
    const MeasureResult* find(std::string_view name) const;

    /// Lookup for formulas that use these measures; only calculated measures are found.
    MeasureLookup lookup() const;
};

/// Calculates named measures that use each other.
///
/// Mistakes stay local: a measure with a broken formula, a repeated name or a cycle gets an error
/// and nothing else changes; measures that use it get `brokenDependency`. Nothing is guessed.
MeasureResolution resolveMeasures(const std::vector<MeasureDefinition>& definitions,
                                  double defaultUnitMillimetres = 1.0);

} // namespace fission::core
