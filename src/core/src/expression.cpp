// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/core/expression.h"

#include <cctype>
#include <cmath>
#include <cstdlib>

namespace fission::core {

namespace {

bool isNameByte(unsigned char c) {
    return std::isalpha(c) != 0 || c == '_' || c >= 0x80;
}

// Recursive descent: expression = term (+|- term)*, term = factor (*|/ factor)*,
// factor = [-|+] (number [unit] | name [unit] | "(" expression ")" [unit]).
class Parser {
public:
    Parser(std::string_view text, QuantityKind kind, const NamedMeasures& names, double defaultUnit)
        : text_(text), kind_(kind), names_(names), defaultUnit_(defaultUnit) {}

    std::optional<double> run() {
        const std::optional<double> value = expression();
        skipSpace();
        if (!value || pos_ != text_.size() || !std::isfinite(*value)) {
            return std::nullopt;
        }
        return value;
    }

private:
    void skipSpace() {
        while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_])) != 0) {
            ++pos_;
        }
    }
    bool accept(char c) {
        skipSpace();
        if (pos_ < text_.size() && text_[pos_] == c) {
            ++pos_;
            return true;
        }
        return false;
    }

    std::optional<double> expression() {
        std::optional<double> left = term();
        while (left) {
            if (accept('+')) {
                const auto right = term();
                left = right ? std::optional<double>(*left + *right) : std::nullopt;
            } else if (accept('-')) {
                const auto right = term();
                left = right ? std::optional<double>(*left - *right) : std::nullopt;
            } else {
                break;
            }
        }
        return left;
    }

    std::optional<double> term() {
        std::optional<double> left = factor();
        while (left) {
            if (accept('*')) {
                const auto right = factor();
                left = right ? std::optional<double>(*left * *right) : std::nullopt;
            } else if (accept('/')) {
                const auto right = factor();
                if (!right || *right == 0.0) {
                    return std::nullopt;
                }
                left = *left / *right;
            } else {
                break;
            }
        }
        return left;
    }

    std::optional<double> factor() {
        skipSpace();
        if (accept('-')) {
            const auto value = factor();
            return value ? std::optional<double>(-*value) : std::nullopt;
        }
        if (accept('+')) {
            return factor();
        }
        if (accept('(')) {
            const auto value = expression();
            if (!value || !accept(')')) {
                return std::nullopt;
            }
            return applyUnit(*value, /*bare=*/false);
        }
        if (const auto number = readNumber()) {
            return applyUnit(*number, /*bare=*/true);
        }
        if (const auto named = readName()) {
            return applyUnit(*named, /*bare=*/false);
        }
        return std::nullopt;
    }

    std::optional<double> readNumber() {
        skipSpace();
        const std::size_t start = pos_;
        std::string digits;
        bool sawDigit = false;
        bool sawSeparator = false;
        while (pos_ < text_.size()) {
            const char c = text_[pos_];
            if (std::isdigit(static_cast<unsigned char>(c)) != 0) {
                digits += c;
                sawDigit = true;
            } else if ((c == '.' || c == ',') && !sawSeparator) {
                digits += '.'; // comma and dot both work
                sawSeparator = true;
            } else {
                break;
            }
            ++pos_;
        }
        if (!sawDigit) {
            pos_ = start;
            return std::nullopt;
        }
        return std::strtod(digits.c_str(), nullptr);
    }

    std::optional<double> readName() {
        skipSpace();
        const std::size_t start = pos_;
        while (pos_ < text_.size() && isNameByte(static_cast<unsigned char>(text_[pos_]))) {
            ++pos_;
        }
        if (pos_ == start) {
            return std::nullopt;
        }
        const std::string key = foldMeasureName(text_.substr(start, pos_ - start));
        for (const auto& [name, value] : names_) {
            if (foldMeasureName(name) == key) {
                return value;
            }
        }
        pos_ = start;
        return std::nullopt;
    }

    // A value that is followed by a unit changes meaning: `2,5 cm` is 25 mm. A bare number takes
    // the field's default unit; a named measure or a bracket is already in millimetres.
    std::optional<double> applyUnit(double value, bool bare) {
        const std::size_t save = pos_;
        skipSpace();
        std::string unit;
        if (pos_ < text_.size() && text_[pos_] == '"') {
            unit = "\"";
            ++pos_;
        } else {
            const std::size_t start = pos_;
            while (pos_ < text_.size() &&
                   std::isalpha(static_cast<unsigned char>(text_[pos_])) != 0) {
                ++pos_;
            }
            unit = std::string(text_.substr(start, pos_ - start));
            for (char& c : unit) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
        }
        if (!unit.empty() && kind_ == QuantityKind::length) {
            if (unit == "mm") {
                return value;
            }
            if (unit == "cm") {
                return value * 10.0;
            }
            if (unit == "in" || unit == "zoll" || unit == "\"") {
                return value * 25.4;
            }
        }
        // A degree sign (UTF-8) after an angle is allowed and means nothing.
        pos_ = save;
        if (kind_ == QuantityKind::angle) {
            skipSpace();
            if (text_.substr(pos_, 2) == "\xC2\xB0") {
                pos_ += 2;
            }
            return value;
        }
        return bare && kind_ == QuantityKind::length ? value * defaultUnit_ : value;
    }

    std::string_view text_;
    QuantityKind kind_;
    const NamedMeasures& names_;
    double defaultUnit_;
    std::size_t pos_ = 0;
};

} // namespace

std::string foldMeasureName(std::string_view name) {
    std::string folded;
    for (std::size_t i = 0; i < name.size(); ++i) {
        const auto c = static_cast<unsigned char>(name[i]);
        if (c == 0xC3 && i + 1 < name.size()) {
            const auto next = static_cast<unsigned char>(name[i + 1]);
            const char* replacement = nullptr;
            switch (next) {
            case 0xA4:
            case 0x84:
                replacement = "ae";
                break; // ä Ä
            case 0xB6:
            case 0x96:
                replacement = "oe";
                break; // ö Ö
            case 0xBC:
            case 0x9C:
                replacement = "ue";
                break; // ü Ü
            case 0x9F:
                replacement = "ss";
                break; // ß
            default:
                break;
            }
            if (replacement != nullptr) {
                folded += replacement;
                ++i;
                continue;
            }
        }
        folded += static_cast<char>(std::tolower(c));
    }
    return folded;
}

std::optional<double> evaluateExpression(std::string_view text, QuantityKind kind,
                                         const NamedMeasures& names,
                                         double defaultUnitMillimetres) {
    return Parser(text, kind, names, defaultUnitMillimetres).run();
}

} // namespace fission::core
