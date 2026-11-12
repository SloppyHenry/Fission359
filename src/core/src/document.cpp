// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/core/document.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace fission::core {

void Document::setSettings(const DocumentSettings& settings) {
    if (settings_ == settings) {
        return;
    }
    settings_ = settings;
    touch();
}

namespace {

bool sameAssets(const AssetMap& a, const AssetMap& b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (auto left = a.begin(), right = b.begin(); left != a.end(); ++left, ++right) {
        if (left->first != right->first ||
            (left->second != right->second && *left->second != *right->second)) {
            return false;
        }
    }
    return true;
}

} // namespace

Document Document::fromParts(DocumentParts parts) {
    Document document;
    document.title = std::move(parts.title);
    document.settings_ = std::move(parts.settings);
    document.parameters_ = std::move(parts.parameters);
    document.features_ = std::move(parts.features);
    document.bodies_ = std::move(parts.bodies);
    document.lastFeatureNumber_ = parts.lastFeatureNumber;
    document.rollback_ = parts.rollback;
    document.assets_ = std::move(parts.assets);
    return document;
}

bool Document::sameContentAs(const Document& other) const {
    return title == other.title && settings_ == other.settings_ &&
           parameters_ == other.parameters_ && features_ == other.features_ &&
           bodies_ == other.bodies_ && lastFeatureNumber_ == other.lastFeatureNumber_ &&
           rollback() == other.rollback() && sameAssets(assets_, other.assets_);
}

// --- embedded files -----------------------------------------------------------------------

std::string Document::addAsset(std::string preferredName, Bytes bytes) {
    for (char& c : preferredName) {
        if (c == '/' || c == '\\' || c == '\0') {
            c = '_';
        }
    }
    while (!preferredName.empty() && preferredName.front() == '.') {
        preferredName.erase(preferredName.begin());
    }
    if (preferredName.empty()) {
        preferredName = "file";
    }
    std::string name = preferredName;
    const std::size_t dot = preferredName.rfind('.');
    const std::string stem =
        dot == std::string::npos ? preferredName : preferredName.substr(0, dot);
    const std::string extension = dot == std::string::npos ? "" : preferredName.substr(dot);
    for (int n = 2; assets_.contains(name); ++n) {
        name = stem + "-" + std::to_string(n) + extension;
    }
    assets_[name] = std::make_shared<const Bytes>(std::move(bytes));
    touch();
    return name;
}

bool Document::putAsset(const std::string& name, std::shared_ptr<const Bytes> content) {
    if (!content || name.empty()) {
        return false;
    }
    const auto found = assets_.find(name);
    if (found != assets_.end()) {
        return found->second == content || *found->second == *content;
    }
    assets_[name] = std::move(content);
    touch();
    return true;
}

bool Document::removeAsset(const std::string& name) {
    if (assets_.erase(name) == 0) {
        return false;
    }
    touch();
    return true;
}

std::vector<std::string> Document::usedAssetNames() const {
    std::vector<std::string> names;
    for (const Feature& feature : features_) {
        const auto found = feature.texts.find("asset");
        if (found != feature.texts.end() && assets_.contains(found->second) &&
            std::find(names.begin(), names.end(), found->second) == names.end()) {
            names.push_back(found->second);
        }
    }
    return names;
}

// --- named measures ----------------------------------------------------------------------

const Parameter* Document::findParameter(std::string_view name) const {
    const std::string key = foldMeasureName(name);
    const auto found =
        std::find_if(parameters_.begin(), parameters_.end(),
                     [&](const Parameter& p) { return foldMeasureName(p.name) == key; });
    return found == parameters_.end() ? nullptr : &*found;
}

void Document::addParameter(Parameter parameter) {
    parameters_.push_back(std::move(parameter));
    touch();
}

bool Document::setParameterExpression(std::string_view name, std::string expression) {
    const std::string key = foldMeasureName(name);
    for (Parameter& parameter : parameters_) {
        if (foldMeasureName(parameter.name) == key) {
            if (parameter.expression != expression) {
                parameter.expression = std::move(expression);
                touch();
            }
            return true;
        }
    }
    return false;
}

bool Document::renameParameter(std::string_view name, std::string newName) {
    const std::string key = foldMeasureName(name);
    for (Parameter& parameter : parameters_) {
        if (foldMeasureName(parameter.name) == key) {
            parameter.name = std::move(newName);
            touch();
            return true;
        }
    }
    return false;
}

bool Document::removeParameter(std::string_view name) {
    const std::string key = foldMeasureName(name);
    const auto found =
        std::find_if(parameters_.begin(), parameters_.end(),
                     [&](const Parameter& p) { return foldMeasureName(p.name) == key; });
    if (found == parameters_.end()) {
        return false;
    }
    parameters_.erase(found);
    touch();
    return true;
}

std::optional<std::size_t> Document::indexOfParameter(std::string_view name) const {
    const std::string key = foldMeasureName(name);
    for (std::size_t i = 0; i < parameters_.size(); ++i) {
        if (foldMeasureName(parameters_[i].name) == key) {
            return i;
        }
    }
    return std::nullopt;
}

bool Document::removeParameterAt(std::size_t index) {
    if (index >= parameters_.size()) {
        return false;
    }
    parameters_.erase(parameters_.begin() + static_cast<std::ptrdiff_t>(index));
    touch();
    return true;
}

bool Document::setParameterNameAt(std::size_t index, std::string newName) {
    if (index >= parameters_.size()) {
        return false;
    }
    parameters_[index].name = std::move(newName);
    touch();
    return true;
}

void Document::insertParameter(std::size_t index, Parameter parameter) {
    index = std::min(index, parameters_.size());
    parameters_.insert(parameters_.begin() + static_cast<std::ptrdiff_t>(index),
                       std::move(parameter));
    touch();
}

std::vector<MeasureDefinition> Document::measureDefinitions() const {
    std::vector<MeasureDefinition> definitions;
    definitions.reserve(parameters_.size());
    for (const Parameter& parameter : parameters_) {
        definitions.push_back({parameter.name, parameter.expression, parameter.kind});
    }
    return definitions;
}

bool SliderRange::valid() const {
    return std::isfinite(min) && std::isfinite(max) && std::isfinite(step) && min < max &&
           step > 0.0;
}

bool Document::setParameterCommentAt(std::size_t index, std::string comment) {
    if (index >= parameters_.size()) {
        return false;
    }
    if (parameters_[index].comment != comment) {
        parameters_[index].comment = std::move(comment);
        touch();
    }
    return true;
}

bool Document::setParameterSliderAt(std::size_t index, std::optional<SliderRange> slider) {
    if (index >= parameters_.size() || (slider && !slider->valid())) {
        return false;
    }
    if (parameters_[index].slider == slider) {
        return true;
    }
    parameters_[index].slider = slider;
    touch();
    return true;
}

namespace {

bool usesName(const std::string& formula, const std::string& foldedName) {
    const Outcome<Expression> parsed = Expression::parse(formula);
    if (!parsed) {
        return false;
    }
    const auto& references = parsed.value().references();
    return std::any_of(references.begin(), references.end(), [&](const NameReference& reference) {
        return foldMeasureName(reference.name) == foldedName;
    });
}

} // namespace

Document::MeasureUse Document::usesOfParameter(std::string_view name) const {
    MeasureUse use;
    const std::string folded = foldMeasureName(name);
    for (std::size_t i = 0; i < features_.size(); ++i) {
        const bool used = std::any_of(
            features_[i].params.begin(), features_[i].params.end(),
            [&](const auto& entry) { return usesName(entry.second.expression, folded); });
        if (used) {
            use.steps.push_back(i);
        }
    }
    for (std::size_t i = 0; i < parameters_.size(); ++i) {
        if (usesName(parameters_[i].expression, folded)) {
            use.measures.push_back(i);
        }
    }
    return use;
}

std::vector<Document::FormulaEdit> Document::renameInFormulas(std::string_view oldName,
                                                              std::string_view newName) {
    std::vector<FormulaEdit> edits;
    for (std::size_t i = 0; i < features_.size(); ++i) {
        for (auto& [field, value] : features_[i].params) {
            std::string changed = renameInExpression(value.expression, oldName, newName);
            if (changed != value.expression) {
                edits.push_back({false, i, field, value.expression});
                value.expression = std::move(changed);
            }
        }
    }
    for (std::size_t i = 0; i < parameters_.size(); ++i) {
        std::string changed = renameInExpression(parameters_[i].expression, oldName, newName);
        if (changed != parameters_[i].expression) {
            edits.push_back({true, i, {}, parameters_[i].expression});
            parameters_[i].expression = std::move(changed);
        }
    }
    if (!edits.empty()) {
        touch();
    }
    return edits;
}

void Document::restoreFormulas(const std::vector<FormulaEdit>& edits) {
    for (const FormulaEdit& edit : edits) {
        if (edit.inMeasure) {
            if (edit.index < parameters_.size()) {
                parameters_[edit.index].expression = edit.before;
            }
        } else if (edit.index < features_.size()) {
            const auto found = features_[edit.index].params.find(edit.field);
            if (found != features_[edit.index].params.end()) {
                found->second.expression = edit.before;
            }
        }
    }
    if (!edits.empty()) {
        touch();
    }
}

// --- recipe -------------------------------------------------------------------------------

std::optional<std::size_t> Document::rollback() const noexcept {
    if (rollback_ && *rollback_ < features_.size()) {
        return rollback_;
    }
    return std::nullopt;
}

bool Document::setRollback(std::optional<std::size_t> count) {
    if (count && *count >= features_.size()) {
        count.reset();
    }
    if (rollback() == count) {
        return false;
    }
    rollback_ = count;
    touch();
    return true;
}

const Feature* Document::findFeature(const FeatureId& id) const {
    const auto found = std::find_if(features_.begin(), features_.end(),
                                    [&](const Feature& f) { return f.id == id; });
    return found == features_.end() ? nullptr : &*found;
}

Feature* Document::mutableFeature(const FeatureId& id) {
    const auto found = std::find_if(features_.begin(), features_.end(),
                                    [&](const Feature& f) { return f.id == id; });
    return found == features_.end() ? nullptr : &*found;
}

std::optional<std::size_t> Document::indexOf(const FeatureId& id) const {
    for (std::size_t i = 0; i < features_.size(); ++i) {
        if (features_[i].id == id) {
            return i;
        }
    }
    return std::nullopt;
}

std::optional<FeatureId> Document::addFeature(FeatureDraft draft,
                                              std::optional<std::size_t> position) {
    // With the marker pulled back a new step goes in at the marker and the marker moves behind it.
    const bool atMarker = !position && rollback().has_value();
    const std::size_t at =
        std::min(position.value_or(atMarker ? *rollback() : features_.size()), features_.size());

    if (draft.sketch && !draft.sketch->consistent()) {
        return std::nullopt; // repeated ids, a counter behind an id, unusable points
    }
    // An input must be made by a step that comes earlier in the recipe: no cycles possible.
    for (const BodyId& input : draft.inputs) {
        const Body* body = findBody(input);
        const auto maker = body != nullptr ? indexOf(body->createdBy) : std::nullopt;
        if (!maker || *maker >= at) {
            return std::nullopt;
        }
    }

    Feature feature;
    feature.id = FeatureId{ids_.nextUuid()};
    feature.number = ++lastFeatureNumber_;
    feature.type = std::move(draft.type);
    feature.typeVersion = draft.typeVersion;
    feature.label = std::move(draft.label);
    feature.params = std::move(draft.params);
    feature.texts = std::move(draft.texts);
    feature.refs = std::move(draft.refs);
    feature.inputs = std::move(draft.inputs);
    feature.sketch = std::move(draft.sketch);
    for (int i = 0; i < draft.newBodies; ++i) {
        Body body;
        body.id = BodyId{ids_.nextUuid()};
        body.createdBy = feature.id;
        feature.outputs.push_back(body.id);
        bodies_.push_back(std::move(body));
    }

    const FeatureId id = feature.id;
    features_.insert(features_.begin() + static_cast<std::ptrdiff_t>(at), std::move(feature));
    if (rollback_ && (at < *rollback_ || atMarker)) {
        ++*rollback_;
    }
    touch();
    return id;
}

bool Document::removeFeature(const FeatureId& id) {
    const auto index = indexOf(id);
    if (!index) {
        return false;
    }
    std::erase_if(bodies_, [&](const Body& body) { return body.createdBy == id; });
    features_.erase(features_.begin() + static_cast<std::ptrdiff_t>(*index));
    if (rollback_ && *index < *rollback_) {
        --*rollback_;
    }
    touch();
    return true;
}

std::optional<RemovedFeature> Document::takeFeature(const FeatureId& id) {
    const auto index = indexOf(id);
    if (!index) {
        return std::nullopt;
    }
    RemovedFeature removed;
    removed.index = *index;
    removed.feature = std::move(features_[*index]);
    for (std::size_t i = 0; i < bodies_.size(); ++i) {
        if (bodies_[i].createdBy == id) {
            removed.bodies.emplace_back(i, bodies_[i]);
        }
    }
    std::erase_if(bodies_, [&](const Body& body) { return body.createdBy == id; });
    features_.erase(features_.begin() + static_cast<std::ptrdiff_t>(*index));
    if (rollback_ && *index < *rollback_) {
        --*rollback_;
        removed.movedMarker = true;
    }
    touch();
    return removed;
}

void Document::restoreFeature(RemovedFeature removed) {
    const std::size_t at = std::min(removed.index, features_.size());
    features_.insert(features_.begin() + static_cast<std::ptrdiff_t>(at),
                     std::move(removed.feature));
    for (auto& [index, body] : removed.bodies) { // ascending, so each insert lands in place
        const std::size_t place = std::min(index, bodies_.size());
        bodies_.insert(bodies_.begin() + static_cast<std::ptrdiff_t>(place), std::move(body));
    }
    if (removed.movedMarker && rollback_) {
        ++*rollback_;
    }
    touch();
}

namespace {

/// The step number in a reference name ("F5/hole/wall" -> 5); nullopt when the name does not
/// start with "F" and digits. Such a name is no reference to a step and is ignored.
std::optional<int> featureNumberInName(const std::string& name) {
    if (name.size() < 2 || name.front() != 'F') {
        return std::nullopt;
    }
    int number = 0;
    std::size_t i = 1;
    while (i < name.size() && name[i] >= '0' && name[i] <= '9') {
        number = number * 10 + (name[i] - '0');
        ++i;
    }
    if (i == 1) {
        return std::nullopt;
    }
    return number;
}

} // namespace

bool Document::orderIsValid(const std::vector<Feature>& order) const {
    // Where every step sits in this order, by id and by number ("F5" names the number).
    std::map<FeatureId, std::size_t> positionOfId;
    std::map<int, std::size_t> positionOfNumber;
    for (std::size_t p = 0; p < order.size(); ++p) {
        positionOfId[order[p].id] = p;
        positionOfNumber[order[p].number] = p;
    }
    for (std::size_t p = 0; p < order.size(); ++p) {
        const Feature& step = order[p];
        // Every input body must be made by an earlier step of this order. An input that is
        // already missing (its step was removed) reports itself elsewhere and blocks no move.
        for (const BodyId& input : step.inputs) {
            const Body* body = findBody(input);
            if (body == nullptr) {
                continue;
            }
            const auto maker = positionOfId.find(body->createdBy);
            if (maker != positionOfId.end() && maker->second >= p) {
                return false;
            }
        }
        // Every reference to another step ("F<n>/...") must point to an earlier step of this
        // order; a name without a step number is no such reference. The edges a sketch follows
        // (AP-2.1f) are references of the sketch step too.
        const auto pointsEarlier = [&](const std::string& name) {
            const std::optional<int> number = featureNumberInName(name);
            if (!number) {
                return true;
            }
            // A step that is gone already "needs help"; it blocks no move.
            const auto target = positionOfNumber.find(*number);
            return target == positionOfNumber.end() || target->second < p;
        };
        for (const auto& entry : step.refs) {
            for (const PersistentRef& ref : entry.second) {
                if (!pointsEarlier(ref.name)) {
                    return false;
                }
            }
        }
    }
    return true;
}

bool Document::moveFeature(const FeatureId& id, std::size_t newIndex) {
    const auto oldIndex = indexOf(id);
    if (!oldIndex || newIndex >= features_.size()) {
        return false;
    }
    if (*oldIndex == newIndex) {
        return true; // already there: nothing changes
    }
    // Try the new order on a copy first: a refused move must not change anything.
    std::vector<Feature> candidate = features_;
    const auto from = candidate.begin() + static_cast<std::ptrdiff_t>(*oldIndex);
    const auto to = candidate.begin() + static_cast<std::ptrdiff_t>(newIndex);
    if (*oldIndex < newIndex) {
        std::rotate(from, from + 1, to + 1); // the step lands at newIndex
    } else {
        std::rotate(to, from, from + 1);
    }
    if (!orderIsValid(candidate)) {
        return false;
    }
    features_ = std::move(candidate);
    // The time marker keeps its count of active steps (AP-1.9): only the order changed.
    touch();
    return true;
}

std::vector<FeatureId> Document::dependentsOf(const FeatureId& id) const {
    const Feature* feature = findFeature(id);
    if (feature == nullptr) {
        return {};
    }
    // Everything that works on a body this step created, and everything that works on those.
    std::vector<BodyId> bodies;
    for (const Body& body : bodies_) {
        if (body.createdBy == id) {
            bodies.push_back(body.id);
        }
    }
    std::vector<FeatureId> result;
    for (const Feature& other : features_) {
        if (other.id == id) {
            continue;
        }
        const bool uses =
            std::any_of(other.inputs.begin(), other.inputs.end(), [&](const BodyId& b) {
                return std::find(bodies.begin(), bodies.end(), b) != bodies.end();
            });
        if (uses) {
            result.push_back(other.id);
            bodies.insert(bodies.end(), other.outputs.begin(), other.outputs.end());
        }
    }
    return result;
}

bool Document::setFeatureValue(const FeatureId& id, std::string_view name, std::string expression) {
    Feature* feature = mutableFeature(id);
    if (feature == nullptr) {
        return false;
    }
    const auto found = feature->params.find(std::string(name));
    if (found == feature->params.end()) {
        return false;
    }
    if (found->second.expression != expression) {
        found->second.expression = std::move(expression);
        touch();
    }
    return true;
}

bool Document::setFeatureText(const FeatureId& id, std::string_view name, std::string text) {
    Feature* feature = mutableFeature(id);
    if (feature == nullptr) {
        return false;
    }
    const auto found = feature->texts.find(std::string(name));
    if (found == feature->texts.end()) {
        return false;
    }
    if (found->second != text) {
        found->second = std::move(text);
        touch();
    }
    return true;
}

bool Document::setFeatureLabel(const FeatureId& id, std::string label) {
    Feature* feature = mutableFeature(id);
    if (feature == nullptr) {
        return false;
    }
    feature->label = std::move(label);
    touch();
    return true;
}

bool Document::setFeatureSuppressed(const FeatureId& id, bool suppressed) {
    Feature* feature = mutableFeature(id);
    if (feature == nullptr) {
        return false;
    }
    if (feature->suppressed != suppressed) {
        feature->suppressed = suppressed;
        touch();
    }
    return true;
}

bool Document::setFeatureRefs(const FeatureId& id, std::string_view name,
                              std::vector<PersistentRef> refs) {
    Feature* feature = mutableFeature(id);
    if (feature == nullptr) {
        return false;
    }
    feature->refs[std::string(name)] = std::move(refs);
    touch();
    return true;
}

// --- sketches -----------------------------------------------------------------------------

std::optional<std::string> Document::addSketchElement(const FeatureId& id, SketchElementKind kind,
                                                      std::vector<SketchPoint> points,
                                                      bool construction,
                                                      std::optional<SketchLink> link) {
    Feature* feature = mutableFeature(id);
    if (feature == nullptr || !feature->sketch || !sketchPointsUsable(kind, points)) {
        return std::nullopt;
    }
    SketchElement element;
    element.id = feature->sketch->allocateId(kind);
    element.kind = kind;
    element.points = std::move(points);
    element.construction = construction;
    element.link = link;
    std::string made = element.id;
    feature->sketch->elements.push_back(std::move(element));
    touch();
    return made;
}

std::optional<RemovedSketchElement> Document::takeSketchElement(const FeatureId& id,
                                                                std::string_view elementId) {
    Feature* feature = mutableFeature(id);
    if (feature == nullptr || !feature->sketch) {
        return std::nullopt;
    }
    const auto index = feature->sketch->indexOf(elementId);
    if (!index) {
        return std::nullopt;
    }
    auto& elements = feature->sketch->elements;
    RemovedSketchElement removed{std::move(elements[*index]), *index};
    elements.erase(elements.begin() + static_cast<std::ptrdiff_t>(*index));
    touch();
    return removed;
}

bool Document::restoreSketchElement(const FeatureId& id, RemovedSketchElement removed) {
    Feature* feature = mutableFeature(id);
    if (feature == nullptr || !feature->sketch || feature->sketch->find(removed.element.id)) {
        return false;
    }
    auto& elements = feature->sketch->elements;
    const std::size_t at = std::min(removed.index, elements.size());
    elements.insert(elements.begin() + static_cast<std::ptrdiff_t>(at), std::move(removed.element));
    touch();
    return true;
}

bool Document::setSketchLastNumber(const FeatureId& id, SketchElementKind kind, int number) {
    Feature* feature = mutableFeature(id);
    if (feature == nullptr || !feature->sketch || number < 0) {
        return false;
    }
    feature->sketch->lastNumber[static_cast<std::size_t>(kind)] = number;
    touch();
    return true;
}

bool Document::setSketchPoints(const FeatureId& id, std::string_view elementId,
                               std::vector<SketchPoint> points) {
    Feature* feature = mutableFeature(id);
    SketchElement* element =
        feature != nullptr && feature->sketch ? feature->sketch->find(elementId) : nullptr;
    if (element == nullptr || !sketchPointsUsable(element->kind, points) ||
        (!sketchTakesMorePoints(element->kind) && points.size() != element->points.size())) {
        return false;
    }
    if (element->points != points) {
        element->points = std::move(points);
        touch();
    }
    return true;
}

bool Document::setSketchLink(const FeatureId& id, std::string_view elementId,
                             std::optional<SketchLink> link) {
    Feature* feature = mutableFeature(id);
    SketchElement* element =
        feature != nullptr && feature->sketch ? feature->sketch->find(elementId) : nullptr;
    if (element == nullptr) {
        return false;
    }
    if (element->link != link) {
        element->link = link;
        touch();
    }
    return true;
}

bool Document::setSketchConstruction(const FeatureId& id, std::string_view elementId,
                                     bool construction) {
    Feature* feature = mutableFeature(id);
    SketchElement* element =
        feature != nullptr && feature->sketch ? feature->sketch->find(elementId) : nullptr;
    if (element == nullptr) {
        return false;
    }
    if (element->construction != construction) {
        element->construction = construction;
        touch();
    }
    return true;
}

// --- bodies -------------------------------------------------------------------------------

bool Document::setFeatureInputs(const FeatureId& id, std::vector<BodyId> inputs) {
    const auto self = indexOf(id);
    if (!self) {
        return false;
    }
    for (const BodyId& input : inputs) {
        const Body* body = findBody(input);
        const auto maker = body != nullptr ? indexOf(body->createdBy) : std::nullopt;
        if (!maker || *maker >= *self) {
            return false;
        }
    }
    features_[*self].inputs = std::move(inputs);
    touch();
    return true;
}

std::optional<FeatureId> Document::consumerOf(const BodyId& id) const {
    for (const Feature& feature : features_) {
        // A sketch only reads a face of the body; it leaves the body as it is.
        if (feature.suppressed || feature.sketch) {
            continue;
        }
        const auto used = std::find(feature.inputs.begin(), feature.inputs.end(), id);
        if (used == feature.inputs.end()) {
            continue;
        }
        // A combine that keeps its tools only uses them: they stay bodies of their own.
        const auto keeps = feature.texts.find("keepTools");
        if (used != feature.inputs.begin() && keeps != feature.texts.end() &&
            keeps->second == "yes") {
            continue;
        }
        return feature.id;
    }
    return std::nullopt;
}

bool Document::setBodyCutExceptions(const BodyId& id, std::vector<BodyId> exceptions) {
    Body* body = mutableBody(id);
    if (body == nullptr) {
        return false;
    }
    body->cutExceptions = std::move(exceptions);
    touch();
    return true;
}

const Body* Document::findBody(const BodyId& id) const {
    const auto found =
        std::find_if(bodies_.begin(), bodies_.end(), [&](const Body& b) { return b.id == id; });
    return found == bodies_.end() ? nullptr : &*found;
}

Body* Document::mutableBody(const BodyId& id) {
    const auto found =
        std::find_if(bodies_.begin(), bodies_.end(), [&](const Body& b) { return b.id == id; });
    return found == bodies_.end() ? nullptr : &*found;
}

bool Document::setBodyName(const BodyId& id, std::string name) {
    Body* body = mutableBody(id);
    if (body == nullptr) {
        return false;
    }
    body->name = std::move(name);
    touch();
    return true;
}

bool Document::setBodyColor(const BodyId& id, Color color) {
    Body* body = mutableBody(id);
    if (body == nullptr) {
        return false;
    }
    body->color = color;
    touch();
    return true;
}

bool Document::setBodyVisible(const BodyId& id, bool visible) {
    Body* body = mutableBody(id);
    if (body == nullptr) {
        return false;
    }
    body->visible = visible;
    touch();
    return true;
}

bool Document::setBodyHole(const BodyId& id, bool hole) {
    Body* body = mutableBody(id);
    if (body == nullptr) {
        return false;
    }
    body->hole = hole;
    touch();
    return true;
}

} // namespace fission::core
