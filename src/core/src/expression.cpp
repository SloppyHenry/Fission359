// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/core/expression.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <locale>
#include <sstream>

namespace fission::core {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr int kMaxDepth = 200;  ///< Nesting of brackets; deeper is an error, not a crash.
constexpr int kMaxExponent = 8; ///< Largest power of length or angle a formula may pass through.

constexpr Dimension kPlain{0, 0};
constexpr Dimension kLength{1, 0};
constexpr Dimension kAngle{0, 1};

bool isNameStart(unsigned char c) {
    return std::isalpha(c) != 0 || c == '_' || c >= 0x80;
}
bool isNameByte(unsigned char c) {
    return isNameStart(c) || std::isdigit(c) != 0;
}
bool isDigit(char c) {
    return std::isdigit(static_cast<unsigned char>(c)) != 0;
}

std::string lowerAscii(std::string_view text) {
    std::string result(text);
    for (char& c : result) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return result;
}

struct UnitDefinition {
    const char* name;
    double factor;
    Dimension dimension;
};

// Units a number can carry. Lengths are stored in millimetres, angles in degrees.
constexpr UnitDefinition kUnits[] = {
    {"mm", 1.0, kLength},   {"cm", 10.0, kLength},   {"m", 1000.0, kLength},
    {"in", 25.4, kLength},  {"zoll", 25.4, kLength}, {"\"", 25.4, kLength},
    {"ft", 304.8, kLength}, {"deg", 1.0, kAngle},    {"\xC2\xB0", 1.0, kAngle},
};

const UnitDefinition* findUnit(std::string_view name) {
    for (const UnitDefinition& unit : kUnits) {
        if (name == unit.name) {
            return &unit;
        }
    }
    return nullptr;
}

enum class Function { min, max, round, sin, cos, sqrt };

struct FunctionDefinition {
    const char* name;
    Function function;
    std::size_t minArguments;
    std::size_t maxArguments;
};

constexpr std::size_t kMany = 64;

constexpr FunctionDefinition kFunctions[] = {
    {"min", Function::min, 2, kMany}, {"max", Function::max, 2, kMany},
    {"round", Function::round, 1, 2}, {"sin", Function::sin, 1, 1},
    {"cos", Function::cos, 1, 1},     {"sqrt", Function::sqrt, 1, 1},
};

const FunctionDefinition* findFunction(std::string_view foldedName) {
    for (const FunctionDefinition& function : kFunctions) {
        if (foldedName == function.name) {
            return &function;
        }
    }
    return nullptr;
}

// Thrown inside the parser and the evaluator, caught at their entry points.
struct Failure {
    ExpressionError error;
};

[[noreturn]] void fail(ExpressionErrorCode code, SourceRange where, std::string subject = {}) {
    throw Failure{ExpressionError{code, where, std::move(subject), {}}};
}

} // namespace

struct Expression::Node {
    enum class Type { number, name, negate, add, subtract, multiply, divide, unit, call };

    Type type = Type::number;
    SourceRange range;
    double number = 0.0;
    std::string text;     ///< Name, function or unit as typed.
    SourceRange auxRange; ///< `unit`: the unit itself.
    const UnitDefinition* unit = nullptr;
    const FunctionDefinition* function = nullptr;
    std::vector<std::shared_ptr<const Node>> children;
};

namespace {

using Node = Expression::Node;
using NodePtr = std::shared_ptr<const Node>;

// Recursive descent:
//   expression = term (("+"|"-") term)*
//   term       = unary (("*"|"/") unary)*
//   unary      = ("-"|"+") unary | postfix
//   postfix    = primary unit*
//   primary    = number | name | name "(" [expression ((";"|",") expression)*] ")" | "(" expression
//   ")"
class Parser {
public:
    explicit Parser(std::string_view text) : text_(text) {}

    NodePtr run() {
        skipSpace();
        if (pos_ >= text_.size()) {
            fail(ExpressionErrorCode::empty, {0, text_.size()});
        }
        NodePtr root = expression();
        skipSpace();
        if (pos_ < text_.size()) {
            fail(ExpressionErrorCode::unexpectedCharacter, {pos_, pos_ + characterLength(pos_)});
        }
        return root;
    }

    std::vector<NameReference> takeReferences() { return std::move(references_); }

private:
    std::size_t characterLength(std::size_t at) const {
        std::size_t length = 1;
        while (at + length < text_.size() &&
               (static_cast<unsigned char>(text_[at + length]) & 0xC0) == 0x80) {
            ++length;
        }
        return length;
    }

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

    static NodePtr make(Node::Type type, SourceRange range, std::vector<NodePtr> children = {}) {
        auto node = std::make_shared<Node>();
        node->type = type;
        node->range = range;
        node->children = std::move(children);
        return node;
    }

    NodePtr expression() {
        NodePtr left = term();
        for (;;) {
            Node::Type type{};
            if (accept('+')) {
                type = Node::Type::add;
            } else if (accept('-')) {
                type = Node::Type::subtract;
            } else {
                return left;
            }
            NodePtr right = term();
            const SourceRange range{left->range.begin, right->range.end};
            left = make(type, range, {left, right});
        }
    }

    NodePtr term() {
        NodePtr left = unary();
        for (;;) {
            Node::Type type{};
            if (accept('*')) {
                type = Node::Type::multiply;
            } else if (accept('/')) {
                type = Node::Type::divide;
            } else {
                return left;
            }
            NodePtr right = unary();
            const SourceRange range{left->range.begin, right->range.end};
            left = make(type, range, {left, right});
        }
    }

    NodePtr unary() {
        skipSpace();
        const std::size_t start = pos_;
        if (accept('-')) {
            enter();
            NodePtr operand = unary();
            leave();
            return make(Node::Type::negate, {start, operand->range.end}, {operand});
        }
        if (accept('+')) {
            enter();
            NodePtr operand = unary();
            leave();
            return operand;
        }
        return postfix();
    }

    NodePtr postfix() {
        NodePtr value = primary();
        for (;;) {
            const std::size_t save = pos_;
            skipSpace();
            const std::size_t start = pos_;
            std::string unitText;
            if (pos_ < text_.size() && text_[pos_] == '"') {
                unitText = "\"";
                ++pos_;
            } else if (text_.substr(pos_, 2) == "\xC2\xB0") {
                unitText = "\xC2\xB0";
                pos_ += 2;
            } else {
                while (pos_ < text_.size() &&
                       std::isalpha(static_cast<unsigned char>(text_[pos_])) != 0 &&
                       static_cast<unsigned char>(text_[pos_]) < 0x80) {
                    ++pos_;
                }
                unitText = lowerAscii(text_.substr(start, pos_ - start));
            }
            if (unitText.empty()) {
                pos_ = save;
                return value;
            }
            const UnitDefinition* unit = findUnit(unitText);
            if (unit == nullptr) {
                fail(ExpressionErrorCode::unknownUnit, {start, pos_},
                     std::string(text_.substr(start, pos_ - start)));
            }
            auto node = std::make_shared<Node>();
            node->type = Node::Type::unit;
            node->range = {value->range.begin, pos_};
            node->auxRange = {start, pos_};
            node->text = unitText;
            node->unit = unit;
            node->children = {value};
            value = node;
        }
    }

    NodePtr primary() {
        skipSpace();
        if (pos_ >= text_.size()) {
            fail(ExpressionErrorCode::unexpectedEnd, {text_.size(), text_.size()});
        }
        const std::size_t start = pos_;
        const char c = text_[pos_];

        if (c == '(') {
            ++pos_;
            enter();
            NodePtr inner = expression();
            leave();
            if (!accept(')')) {
                fail(ExpressionErrorCode::missingClosingBracket, {start, pos_});
            }
            // Keep the brackets in the range so a unit after them reads as one piece.
            auto node = std::make_shared<Node>(*inner);
            node->range = {start, pos_};
            return node;
        }
        if (isDigit(c) || c == '.' || c == ',') {
            return number();
        }
        if (isNameStart(static_cast<unsigned char>(c))) {
            return nameOrCall();
        }
        fail(ExpressionErrorCode::unexpectedCharacter, {start, start + characterLength(start)});
    }

    // Digits with an optional decimal separator. A comma only counts as decimal separator when a
    // digit follows, so that `min(2, 5)` reads as two arguments and `2,5` as one number.
    NodePtr number() {
        const std::size_t start = pos_;
        std::string digits;
        bool sawDigit = false;
        bool sawSeparator = false;
        while (pos_ < text_.size()) {
            const char c = text_[pos_];
            if (isDigit(c)) {
                digits += c;
                sawDigit = true;
            } else if (c == '.' && !sawSeparator) {
                digits += '.';
                sawSeparator = true;
            } else if (c == ',' && !sawSeparator && pos_ + 1 < text_.size() &&
                       isDigit(text_[pos_ + 1])) {
                digits += '.';
                sawSeparator = true;
            } else {
                break;
            }
            ++pos_;
        }
        if (!sawDigit) {
            fail(ExpressionErrorCode::unexpectedCharacter, {start, start + characterLength(start)});
        }
        auto node = std::make_shared<Node>();
        node->type = Node::Type::number;
        node->range = {start, pos_};
        // Not std::strtod: it follows the C locale, which Qt sets from the environment. In a
        // German terminal it would stop at the dot and read "19.9" as 19.
        std::istringstream in(digits);
        in.imbue(std::locale::classic());
        in >> node->number;
        if (in.fail() || !std::isfinite(node->number)) { // fails when out of range
            fail(ExpressionErrorCode::notFinite, node->range);
        }
        return node;
    }

    NodePtr nameOrCall() {
        const std::size_t start = pos_;
        while (pos_ < text_.size() && isNameByte(static_cast<unsigned char>(text_[pos_]))) {
            ++pos_;
        }
        const std::string name(text_.substr(start, pos_ - start));
        const SourceRange nameRange{start, pos_};

        const std::size_t afterName = pos_;
        skipSpace();
        if (pos_ < text_.size() && text_[pos_] == '(') {
            ++pos_;
            const FunctionDefinition* function = findFunction(lowerAscii(name));
            if (function == nullptr) {
                fail(ExpressionErrorCode::unknownFunction, nameRange, name);
            }
            std::vector<NodePtr> arguments;
            skipSpace();
            if (pos_ < text_.size() && text_[pos_] == ')') {
                ++pos_;
            } else {
                enter();
                for (;;) {
                    arguments.push_back(expression());
                    if (accept(';') || accept(',')) {
                        continue;
                    }
                    break;
                }
                leave();
                if (!accept(')')) {
                    fail(ExpressionErrorCode::missingClosingBracket, {start, pos_});
                }
            }
            const SourceRange range{start, pos_};
            if (arguments.size() < function->minArguments ||
                arguments.size() > function->maxArguments) {
                fail(ExpressionErrorCode::wrongArgumentCount, range, name);
            }
            auto node = std::make_shared<Node>();
            node->type = Node::Type::call;
            node->range = range;
            node->text = name;
            node->function = function;
            node->children = std::move(arguments);
            return node;
        }
        pos_ = afterName;

        references_.push_back({name, nameRange});
        auto node = std::make_shared<Node>();
        node->type = Node::Type::name;
        node->range = nameRange;
        node->text = name;
        return node;
    }

    void enter() {
        if (++depth_ > kMaxDepth) {
            fail(ExpressionErrorCode::unexpectedCharacter, {pos_, pos_});
        }
    }
    void leave() { --depth_; }

    std::string_view text_;
    std::size_t pos_ = 0;
    int depth_ = 0;
    std::vector<NameReference> references_;
};

class Evaluator {
public:
    explicit Evaluator(const EvaluationContext& context) : context_(context) {}

    Quantity run(const Node& root) {
        Quantity value = eval(root);
        const Dimension wanted = dimensionOf(context_.kind);
        if (value.dimension == wanted) {
            return value;
        }
        if (value.dimension == kPlain) {
            // A bare number is worth the field's default unit.
            const double scale = unitScale(wanted).value_or(1.0);
            return finite({value.value * scale, wanted}, root.range);
        }
        fail(ExpressionErrorCode::wrongKind, root.range);
    }

private:
    // What a bare number is worth when it joins a quantity of this dimension.
    std::optional<double> unitScale(Dimension target) const {
        if (target == kLength) {
            return context_.defaultUnitMillimetres;
        }
        if (target == kAngle) {
            return 1.0;
        }
        return std::nullopt;
    }

    Quantity finite(Quantity value, SourceRange where) const {
        if (!std::isfinite(value.value)) {
            fail(ExpressionErrorCode::notFinite, where);
        }
        if (std::abs(value.dimension.length) > kMaxExponent ||
            std::abs(value.dimension.angle) > kMaxExponent) {
            fail(ExpressionErrorCode::outOfRange, where);
        }
        return value;
    }

    // Brings two quantities to the same dimension: a bare number joins a length or an angle.
    void align(Quantity& a, Quantity& b, SourceRange where) const {
        if (a.dimension == b.dimension) {
            return;
        }
        Quantity* plain = a.dimension == kPlain ? &a : (b.dimension == kPlain ? &b : nullptr);
        Quantity* other = plain == &a ? &b : &a;
        if (plain != nullptr) {
            if (const auto scale = unitScale(other->dimension)) {
                plain->value *= *scale;
                plain->dimension = other->dimension;
                return;
            }
        }
        fail(ExpressionErrorCode::incompatibleQuantities, where);
    }

    Quantity eval(const Node& node) {
        switch (node.type) {
        case Node::Type::number:
            return {node.number, kPlain};
        case Node::Type::name: {
            if (context_.lookup) {
                if (const auto found = context_.lookup(foldMeasureName(node.text))) {
                    return *found;
                }
            }
            fail(ExpressionErrorCode::unknownName, node.range, node.text);
        }
        case Node::Type::negate: {
            Quantity value = eval(*node.children[0]);
            value.value = -value.value;
            return value;
        }
        case Node::Type::add:
        case Node::Type::subtract: {
            Quantity left = eval(*node.children[0]);
            Quantity right = eval(*node.children[1]);
            align(left, right, node.range);
            left.value =
                node.type == Node::Type::add ? left.value + right.value : left.value - right.value;
            return finite(left, node.range);
        }
        case Node::Type::multiply: {
            const Quantity left = eval(*node.children[0]);
            const Quantity right = eval(*node.children[1]);
            return finite({left.value * right.value,
                           {left.dimension.length + right.dimension.length,
                            left.dimension.angle + right.dimension.angle}},
                          node.range);
        }
        case Node::Type::divide: {
            const Quantity left = eval(*node.children[0]);
            const Quantity right = eval(*node.children[1]);
            if (right.value == 0.0) {
                fail(ExpressionErrorCode::divisionByZero, node.children[1]->range);
            }
            return finite({left.value / right.value,
                           {left.dimension.length - right.dimension.length,
                            left.dimension.angle - right.dimension.angle}},
                          node.range);
        }
        case Node::Type::unit: {
            const Quantity inner = eval(*node.children[0]);
            if (inner.dimension != kPlain) {
                fail(ExpressionErrorCode::unitNotAllowed, node.auxRange, node.text);
            }
            return finite({inner.value * node.unit->factor, node.unit->dimension}, node.range);
        }
        case Node::Type::call:
            return call(node);
        }
        fail(ExpressionErrorCode::unexpectedCharacter, node.range);
    }

    Quantity call(const Node& node) {
        std::vector<Quantity> arguments;
        arguments.reserve(node.children.size());
        for (const NodePtr& child : node.children) {
            arguments.push_back(eval(*child));
        }
        switch (node.function->function) {
        case Function::min:
        case Function::max: {
            Quantity result = arguments[0];
            for (std::size_t i = 1; i < arguments.size(); ++i) {
                Quantity next = arguments[i];
                align(result, next, node.range);
                result.value = node.function->function == Function::min
                                   ? std::min(result.value, next.value)
                                   : std::max(result.value, next.value);
            }
            return result;
        }
        case Function::round: {
            Quantity value = arguments[0];
            if (arguments.size() == 1) {
                value.value = std::round(value.value);
                return finite(value, node.range);
            }
            Quantity step = arguments[1];
            align(value, step, node.range);
            if (!(step.value > 0.0)) {
                fail(ExpressionErrorCode::outOfRange, node.children[1]->range);
            }
            value.value = std::round(value.value / step.value) * step.value;
            return finite(value, node.range);
        }
        case Function::sin:
        case Function::cos: {
            const Quantity angle = arguments[0];
            if (angle.dimension != kAngle && angle.dimension != kPlain) {
                fail(ExpressionErrorCode::incompatibleQuantities, node.children[0]->range);
            }
            return {trigonometric(angle.value, node.function->function == Function::sin), kPlain};
        }
        case Function::sqrt: {
            const Quantity value = arguments[0];
            if (value.value < 0.0) {
                fail(ExpressionErrorCode::outOfRange, node.children[0]->range);
            }
            if (value.dimension.length % 2 != 0 || value.dimension.angle % 2 != 0) {
                fail(ExpressionErrorCode::incompatibleQuantities, node.children[0]->range);
            }
            return {std::sqrt(value.value),
                    {value.dimension.length / 2, value.dimension.angle / 2}};
        }
        }
        fail(ExpressionErrorCode::unknownFunction, node.range, node.text);
    }

    // Degrees in; right angles come out exact (cos(90) is 0, not 6e-17).
    static double trigonometric(double degrees, bool sine) {
        double turn = std::fmod(degrees, 360.0);
        if (turn < 0.0) {
            turn += 360.0;
        }
        static constexpr double sines[] = {0.0, 1.0, 0.0, -1.0};
        static constexpr double cosines[] = {1.0, 0.0, -1.0, 0.0};
        if (std::fmod(turn, 90.0) == 0.0) {
            const auto index = static_cast<std::size_t>(turn / 90.0) % 4;
            return sine ? sines[index] : cosines[index];
        }
        const double radians = turn * kPi / 180.0;
        return sine ? std::sin(radians) : std::cos(radians);
    }

    const EvaluationContext& context_;
};

} // namespace

Dimension dimensionOf(QuantityKind kind) noexcept {
    switch (kind) {
    case QuantityKind::length:
        return kLength;
    case QuantityKind::angle:
        return kAngle;
    case QuantityKind::plain:
        break;
    }
    return kPlain;
}

Outcome<Expression> Expression::parse(std::string_view text) {
    try {
        Parser parser(text);
        Expression expression;
        expression.source_ = std::string(text);
        expression.root_ = parser.run();
        expression.references_ = parser.takeReferences();
        return expression;
    } catch (const Failure& failure) {
        return failure.error;
    }
}

Outcome<Quantity> Expression::evaluate(const EvaluationContext& context) const {
    try {
        return Evaluator(context).run(*root_);
    } catch (const Failure& failure) {
        return failure.error;
    }
}

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

bool isValidMeasureName(std::string_view name) {
    if (name.empty() || !isNameStart(static_cast<unsigned char>(name.front()))) {
        return false;
    }
    return std::all_of(name.begin(), name.end(),
                       [](char c) { return isNameByte(static_cast<unsigned char>(c)); });
}

std::optional<double> evaluateExpression(std::string_view text, QuantityKind kind,
                                         const NamedMeasures& names,
                                         double defaultUnitMillimetres) {
    const Outcome<Expression> parsed = Expression::parse(text);
    if (!parsed) {
        return std::nullopt;
    }
    std::map<std::string, double> folded;
    for (const auto& [name, value] : names) {
        folded.emplace(foldMeasureName(name), value);
    }
    EvaluationContext context;
    context.kind = kind;
    context.defaultUnitMillimetres = defaultUnitMillimetres;
    context.lookup = [&folded](std::string_view key) -> std::optional<Quantity> {
        const auto found = folded.find(std::string(key));
        if (found == folded.end()) {
            return std::nullopt;
        }
        return Quantity{found->second, kLength};
    };
    const Outcome<Quantity> result = parsed.value().evaluate(context);
    if (!result) {
        return std::nullopt;
    }
    return result.value().value;
}

} // namespace fission::core
