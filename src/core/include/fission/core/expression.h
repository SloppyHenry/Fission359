// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace fission::core {

/// Named measures usable in formulas (docs/02-ux-konzept.md §2.3), values in millimetres.
/// Names are matched without regard to case; "Höhe" and "Hoehe" are the same name.
using NamedMeasures = std::map<std::string, double>;

/// The kind of number a field takes: lengths know units (mm, cm, in), angles and plain numbers
/// do not (an angle may carry a degree sign).
enum class QuantityKind { length, angle, plain };

/// What a value is made of: powers of length and angle. A plain number is {0, 0}, a length
/// {1, 0}, an angle {0, 1}, an area {2, 0}. Formulas may pass through areas (`sqrt(a*a + b*b)`)
/// but a field only takes the three kinds above.
struct Dimension {
    int length = 0;
    int angle = 0;

    bool operator==(const Dimension&) const = default;
};

/// A number with its dimension; lengths in millimetres, angles in degrees.
struct Quantity {
    double value = 0.0;
    Dimension dimension;
};

/// The dimension a field of this kind takes.
Dimension dimensionOf(QuantityKind kind) noexcept;

/// A stretch of the typed text, in bytes of the UTF-8 string (`end` is one past the last byte).
/// Lets the number field underline exactly the part that is wrong.
struct SourceRange {
    std::size_t begin = 0;
    std::size_t end = 0;

    bool operator==(const SourceRange&) const = default;
};

/// Why an expression or a named measure could not be evaluated. The user interface turns these
/// codes into sentences (message catalogue); no text is built in the core.
enum class ExpressionErrorCode {
    empty,                  ///< Nothing was typed.
    unexpectedCharacter,    ///< A character that has no meaning at this place.
    unexpectedEnd,          ///< The text ends where a value was expected (`5+`).
    missingClosingBracket,  ///< `(3` or `min(1; 2`.
    unknownName,            ///< A name that is not a measure. `subject` is the name as typed.
    unknownFunction,        ///< `subject` is the function name as typed.
    unknownUnit,            ///< `subject` is the unit as typed (`2 km`).
    unitNotAllowed,         ///< A unit after something that already has one.
    wrongArgumentCount,     ///< `subject` is the function name.
    divisionByZero,         ///< `where` is the divisor.
    incompatibleQuantities, ///< `5 mm + 30°`: lengths and angles do not add up.
    wrongKind,  ///< The result is not what the field takes (a length in an angle field).
    outOfRange, ///< `sqrt` of a negative number and the like.
    notFinite,  ///< The result is too large to calculate with.
    // Named measures (measures.h)
    invalidName,      ///< A measure name with characters formulas cannot read.
    duplicateName,    ///< Two measures with the same name (case and umlauts folded).
    cycle,            ///< Measures that depend on each other; `path` lists them (a, b, a).
    brokenDependency, ///< Uses a measure that itself has an error. `subject` is that measure.
};

struct ExpressionError {
    ExpressionErrorCode code = ExpressionErrorCode::empty;
    SourceRange where;
    std::string subject;
    std::vector<std::string> path; ///< For `cycle`: the names in order, first = last.
};

/// A value or an error; the core does not use exceptions for what a person can type.
template <class T> class Outcome {
public:
    Outcome(T value) : data_(std::move(value)) {}
    Outcome(ExpressionError error) : data_(std::move(error)) {}

    bool ok() const noexcept { return std::holds_alternative<T>(data_); }
    explicit operator bool() const noexcept { return ok(); }
    const T& value() const { return std::get<T>(data_); }
    const ExpressionError& error() const { return std::get<ExpressionError>(data_); }

private:
    std::variant<T, ExpressionError> data_;
};

/// How a name in a formula is looked up. Receives the folded name (see `foldMeasureName`).
using MeasureLookup = std::function<std::optional<Quantity>(std::string_view foldedName)>;

struct EvaluationContext {
    QuantityKind kind = QuantityKind::length;
    /// What a bare number is worth in a length field: 1.0 for mm, 25.4 when the field counts in
    /// inches.
    double defaultUnitMillimetres = 1.0;
    MeasureLookup lookup; ///< May be empty (then every name is unknown).
};

/// A name used in a formula, as typed, with its place in the text.
struct NameReference {
    std::string name;
    SourceRange where;
};

/// A parsed formula. Cheap to copy (the syntax tree is shared and never changes).
///
/// Grammar: numbers with comma or dot, `+ - * /`, brackets, units (`mm cm m in " ft zoll °`) after
/// a plain number or bracket, named measures, and the functions `min(a; b…)`, `max`, `round(x)`,
/// `round(x; step)`, `sin`, `cos` (angles in degrees) and `sqrt`. Function arguments are
/// separated by `;` (a comma followed by a space works too, because `2,5` is a number).
class Expression {
public:
    struct Node;

    /// Parses `text`. Syntax errors carry the place in `where`.
    static Outcome<Expression> parse(std::string_view text);

    /// Calculates the value. The result has the dimension of `context.kind`; a plain number is
    /// taken as `defaultUnitMillimetres` in a length field and as degrees in an angle field.
    Outcome<Quantity> evaluate(const EvaluationContext& context) const;

    /// Every measure name the formula uses, in order of appearance (repeats included).
    const std::vector<NameReference>& references() const noexcept { return references_; }

    const std::string& source() const noexcept { return source_; }

private:
    std::string source_;
    std::shared_ptr<const Node> root_;
    std::vector<NameReference> references_;
};

/// Evaluates what a person typed into a number field: `25+3`, `40/2`, `2,5 cm`, `1 in`,
/// `Breite/2`. Comma and dot are both decimal separators. Lengths come back in millimetres;
/// a bare number is taken as `defaultUnitMillimetres` (1.0 for the mm fields).
///
/// Returns nothing when the text is not a valid expression (including division by zero). Use
/// `Expression` where the reason matters.
std::optional<double> evaluateExpression(std::string_view text,
                                         QuantityKind kind = QuantityKind::length,
                                         const NamedMeasures& names = {},
                                         double defaultUnitMillimetres = 1.0);

/// The form in which names are compared: lower case, umlauts written out.
std::string foldMeasureName(std::string_view name);

/// Whether `name` can be used as a named measure: a letter or underscore, then letters, digits
/// and underscores (umlauts and other non-ASCII letters count as letters).
bool isValidMeasureName(std::string_view name);

} // namespace fission::core
