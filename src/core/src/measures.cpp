// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/core/measures.h"

#include <algorithm>
#include <deque>
#include <map>
#include <unordered_map>

namespace fission::core {

namespace {

constexpr std::size_t kNone = static_cast<std::size_t>(-1);

// One measure while it is being worked out.
struct Item {
    std::optional<Expression> expression;
    std::optional<ExpressionError> error;
    std::vector<std::size_t> dependencies; ///< Indices of defined measures it uses (no repeats).
    std::vector<SourceRange> dependencyPlaces; ///< First place of each dependency in its formula.
};

// Tarjan's algorithm. Components come out dependencies first, which is the order of calculation.
class Components {
public:
    explicit Components(const std::vector<Item>& items)
        : items_(items), index_(items.size(), kNone), low_(items.size(), 0),
          onStack_(items.size(), false) {}

    std::vector<std::vector<std::size_t>> run() {
        for (std::size_t i = 0; i < items_.size(); ++i) {
            if (index_[i] == kNone) {
                visit(i);
            }
        }
        return std::move(components_);
    }

private:
    void visit(std::size_t node) {
        index_[node] = low_[node] = counter_++;
        stack_.push_back(node);
        onStack_[node] = true;
        for (const std::size_t next : items_[node].dependencies) {
            if (index_[next] == kNone) {
                visit(next);
                low_[node] = std::min(low_[node], low_[next]);
            } else if (onStack_[next]) {
                low_[node] = std::min(low_[node], index_[next]);
            }
        }
        if (low_[node] == index_[node]) {
            std::vector<std::size_t> component;
            std::size_t member = kNone;
            do {
                member = stack_.back();
                stack_.pop_back();
                onStack_[member] = false;
                component.push_back(member);
            } while (member != node);
            components_.push_back(std::move(component));
        }
    }

    const std::vector<Item>& items_;
    std::vector<std::size_t> index_;
    std::vector<std::size_t> low_;
    std::vector<bool> onStack_;
    std::vector<std::size_t> stack_;
    std::vector<std::vector<std::size_t>> components_;
    std::size_t counter_ = 0;
};

// Shortest way from `from` back to itself through measures of one component.
std::vector<std::size_t> shortestCycle(const std::vector<Item>& items, std::size_t from,
                                       const std::vector<bool>& inComponent) {
    std::vector<std::size_t> previous(items.size(), kNone);
    std::vector<bool> seen(items.size(), false);
    std::deque<std::size_t> queue;
    queue.push_back(from);
    while (!queue.empty()) {
        const std::size_t current = queue.front();
        queue.pop_front();
        for (const std::size_t next : items[current].dependencies) {
            if (!inComponent[next]) {
                continue;
            }
            if (next == from) {
                std::vector<std::size_t> path{from};
                std::vector<std::size_t> back;
                for (std::size_t at = current; at != from; at = previous[at]) {
                    back.push_back(at);
                }
                path.insert(path.end(), back.rbegin(), back.rend());
                path.push_back(from);
                return path;
            }
            if (!seen[next]) {
                seen[next] = true;
                previous[next] = current;
                queue.push_back(next);
            }
        }
    }
    return {from, from};
}

} // namespace

const MeasureResult* MeasureResolution::find(std::string_view name) const {
    const std::string key = foldMeasureName(name);
    for (const MeasureResult& result : results) {
        if (foldMeasureName(result.name) == key) {
            return &result;
        }
    }
    return nullptr;
}

MeasureLookup MeasureResolution::lookup() const {
    std::map<std::string, Quantity> known;
    for (const MeasureResult& result : results) {
        if (result.value) {
            known.emplace(foldMeasureName(result.name), *result.value);
        }
    }
    return [known = std::move(known)](std::string_view key) -> std::optional<Quantity> {
        const auto found = known.find(std::string(key));
        if (found == known.end()) {
            return std::nullopt;
        }
        return found->second;
    };
}

MeasureResolution resolveMeasures(const std::vector<MeasureDefinition>& definitions,
                                  double defaultUnitMillimetres) {
    const std::size_t count = definitions.size();
    std::vector<Item> items(count);
    MeasureResolution resolution;
    resolution.results.resize(count);

    // Names first: invalid and repeated names are errors on the measure itself.
    std::unordered_map<std::string, std::size_t> byName;
    for (std::size_t i = 0; i < count; ++i) {
        resolution.results[i].name = definitions[i].name;
        if (!isValidMeasureName(definitions[i].name)) {
            items[i].error =
                ExpressionError{ExpressionErrorCode::invalidName, {}, definitions[i].name, {}};
            continue;
        }
        const auto [position, inserted] = byName.emplace(foldMeasureName(definitions[i].name), i);
        if (!inserted) {
            items[i].error =
                ExpressionError{ExpressionErrorCode::duplicateName, {}, definitions[i].name, {}};
        }
    }

    // Formulas first, then who uses whom (a formula can use a measure defined further down).
    for (std::size_t i = 0; i < count; ++i) {
        if (items[i].error) {
            continue;
        }
        Outcome<Expression> parsed = Expression::parse(definitions[i].expression);
        if (!parsed) {
            items[i].error = parsed.error();
            continue;
        }
        items[i].expression = parsed.value();
    }
    for (std::size_t i = 0; i < count; ++i) {
        if (!items[i].expression) {
            continue;
        }
        for (const NameReference& reference : items[i].expression->references()) {
            const auto found = byName.find(foldMeasureName(reference.name));
            if (found == byName.end()) {
                continue; // unknown names are reported when evaluating
            }
            auto& dependencies = items[i].dependencies;
            if (std::find(dependencies.begin(), dependencies.end(), found->second) ==
                dependencies.end()) {
                dependencies.push_back(found->second);
                items[i].dependencyPlaces.push_back(reference.where);
            }
        }
    }

    // Cycles and the order of calculation.
    const auto components = Components(items).run();
    std::vector<bool> cyclic(count, false);
    for (const auto& component : components) {
        const bool selfLoop = component.size() == 1 &&
                              std::find(items[component[0]].dependencies.begin(),
                                        items[component[0]].dependencies.end(),
                                        component[0]) != items[component[0]].dependencies.end();
        if (component.size() == 1 && !selfLoop) {
            continue;
        }
        std::vector<bool> inComponent(count, false);
        for (const std::size_t member : component) {
            inComponent[member] = true;
        }
        for (const std::size_t member : component) {
            cyclic[member] = true;
            const std::vector<std::size_t> path = shortestCycle(items, member, inComponent);
            ExpressionError error;
            error.code = ExpressionErrorCode::cycle;
            error.subject = definitions[member].name;
            for (const std::size_t step : path) {
                error.path.push_back(definitions[step].name);
            }
            const std::size_t next = path.size() > 1 ? path[1] : member;
            const auto& dependencies = items[member].dependencies;
            const auto at = std::find(dependencies.begin(), dependencies.end(), next);
            if (at != dependencies.end()) {
                error.where =
                    items[member]
                        .dependencyPlaces[static_cast<std::size_t>(at - dependencies.begin())];
            }
            items[member].error = std::move(error);
        }
    }

    // Calculate, dependencies first. A measure that uses a broken one is broken as well.
    std::vector<std::vector<std::string>> usedBy(count);
    std::map<std::string, Quantity> known; // folded name -> value, grows as measures are done
    EvaluationContext context;
    context.defaultUnitMillimetres = defaultUnitMillimetres;
    context.lookup = [&known](std::string_view key) -> std::optional<Quantity> {
        const auto found = known.find(std::string(key));
        if (found == known.end()) {
            return std::nullopt;
        }
        return found->second;
    };
    for (const auto& component : components) {
        for (const std::size_t i : component) {
            resolution.order.push_back(i);
            MeasureResult& result = resolution.results[i];
            for (const std::size_t dependency : items[i].dependencies) {
                result.dependsOn.push_back(definitions[dependency].name);
                usedBy[dependency].push_back(definitions[i].name);
            }
            if (items[i].error) {
                result.error = items[i].error;
                continue;
            }
            bool broken = false;
            for (std::size_t d = 0; d < items[i].dependencies.size(); ++d) {
                const std::size_t dependency = items[i].dependencies[d];
                if (!resolution.results[dependency].value) {
                    result.error = ExpressionError{ExpressionErrorCode::brokenDependency,
                                                   items[i].dependencyPlaces[d],
                                                   definitions[dependency].name,
                                                   {}};
                    broken = true;
                    break;
                }
            }
            if (broken) {
                continue;
            }
            context.kind = definitions[i].kind;
            const Outcome<Quantity> value = items[i].expression->evaluate(context);
            if (value) {
                result.value = value.value();
                known.emplace(foldMeasureName(definitions[i].name), value.value());
            } else {
                result.error = value.error();
            }
        }
    }
    for (std::size_t i = 0; i < count; ++i) {
        resolution.results[i].usedBy = std::move(usedBy[i]);
    }
    return resolution;
}

} // namespace fission::core
