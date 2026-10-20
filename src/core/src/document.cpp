// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/core/document.h"

#include <algorithm>
#include <utility>

namespace fission::core {

void Document::setSettings(const DocumentSettings& settings) {
    if (settings_ == settings) {
        return;
    }
    settings_ = settings;
    touch();
}

bool Document::sameContentAs(const Document& other) const {
    return title == other.title && settings_ == other.settings_ &&
           parameters_ == other.parameters_ && features_ == other.features_ &&
           bodies_ == other.bodies_ && lastFeatureNumber_ == other.lastFeatureNumber_;
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

// --- recipe -------------------------------------------------------------------------------

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
    const std::size_t at = std::min(position.value_or(features_.size()), features_.size());

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
    for (int i = 0; i < draft.newBodies; ++i) {
        Body body;
        body.id = BodyId{ids_.nextUuid()};
        body.createdBy = feature.id;
        feature.outputs.push_back(body.id);
        bodies_.push_back(std::move(body));
    }

    const FeatureId id = feature.id;
    features_.insert(features_.begin() + static_cast<std::ptrdiff_t>(at), std::move(feature));
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
    touch();
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
        if (!feature.suppressed &&
            std::find(feature.inputs.begin(), feature.inputs.end(), id) != feature.inputs.end()) {
            return feature.id;
        }
    }
    return std::nullopt;
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
