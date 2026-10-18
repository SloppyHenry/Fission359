// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/core/scaling.h"

#include <cctype>
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>

namespace fission::core {

namespace {

std::string_view trimmed(std::string_view text) {
    const auto space = [](char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; };
    while (!text.empty() && space(text.front())) {
        text.remove_prefix(1);
    }
    while (!text.empty() && space(text.back())) {
        text.remove_suffix(1);
    }
    return text;
}

/// Digits with at most one decimal separator (comma or dot): a factor as the program writes it.
bool isPlainNumber(std::string_view text) {
    bool digit = false;
    int separators = 0;
    for (const char c : text) {
        if (std::isdigit(static_cast<unsigned char>(c)) != 0) {
            digit = true;
        } else if (c == '.' || c == ',') {
            ++separators;
        } else {
            return false;
        }
    }
    return digit && separators <= 1;
}

/// `(inner)` with the first bracket closed by the last one (not `(a) + (b)`).
bool isBracketed(std::string_view text) {
    if (text.size() < 2 || text.front() != '(' || text.back() != ')') {
        return false;
    }
    int depth = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        depth += text[i] == '(' ? 1 : text[i] == ')' ? -1 : 0;
        if (depth == 0 && i + 1 < text.size()) {
            return false;
        }
    }
    return depth == 0;
}

/// A formula of the form `(inner) * factor` or `name * factor`, taken apart.
struct Factored {
    std::string_view inner;
    double factor = 1.0;
};

std::optional<Factored> factoredOf(std::string_view text) {
    // The last `*` outside of brackets: what follows must be a plain number.
    int depth = 0;
    std::optional<std::size_t> star;
    for (std::size_t i = 0; i < text.size(); ++i) {
        depth += text[i] == '(' ? 1 : text[i] == ')' ? -1 : 0;
        if (depth == 0 && text[i] == '*') {
            star = i;
        }
    }
    if (!star) {
        return std::nullopt;
    }
    const std::string_view head = trimmed(text.substr(0, *star));
    const std::string_view tail = trimmed(text.substr(*star + 1));
    if (!isPlainNumber(tail)) {
        return std::nullopt;
    }
    const auto factor = evaluateExpression(tail, QuantityKind::plain);
    if (!factor || !(*factor > 0.0)) {
        return std::nullopt;
    }
    if (isBracketed(head)) {
        return Factored{trimmed(head.substr(1, head.size() - 2)), *factor};
    }
    if (isValidMeasureName(head)) {
        return Factored{head, *factor};
    }
    return std::nullopt;
}

} // namespace

std::string numberText(double value) {
    if (!std::isfinite(value)) {
        return "0";
    }
    if (std::abs(value) < 1e-9) {
        return "0"; // also no "-0"
    }
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(12) << value;
    std::string text = out.str();
    if (text.find('e') != std::string::npos) {
        // Formulas know no exponents: write it out.
        out.str({});
        out << std::fixed << std::setprecision(9) << value;
        text = out.str();
        while (!text.empty() && text.back() == '0') {
            text.pop_back();
        }
        if (!text.empty() && text.back() == '.') {
            text.pop_back();
        }
    }
    return text;
}

std::optional<std::string> scaledExpression(std::string_view expression, QuantityKind kind,
                                            double factor) {
    if (!std::isfinite(factor) || !(factor > 0.0)) {
        return std::nullopt;
    }
    const auto parsed = Expression::parse(expression);
    if (!parsed) {
        return std::nullopt;
    }
    if (parsed.value().references().empty()) {
        // A number: the new number. (A unit typed with it is worked out; a length is in mm.)
        EvaluationContext context;
        context.kind = kind;
        const auto value = parsed.value().evaluate(context);
        if (!value) {
            return std::nullopt;
        }
        return numberText(value.value().value * factor);
    }
    const std::string_view text = trimmed(expression);
    std::string_view inner = text;
    double present = 1.0;
    if (const auto factored = factoredOf(text)) {
        inner = factored->inner;
        present = factored->factor;
    }
    const double combined = present * factor;
    std::string result;
    if (std::abs(combined - 1.0) < 1e-12) {
        result = std::string(inner); // back to the formula as it was typed
    } else if (isValidMeasureName(inner)) {
        result = std::string(inner) + " * " + numberText(combined);
    } else {
        result = "(" + std::string(inner) + ") * " + numberText(combined);
    }
    if (!Expression::parse(result)) {
        return std::nullopt; // cannot happen for a formula that parsed; better safe
    }
    return result;
}

} // namespace fission::core
