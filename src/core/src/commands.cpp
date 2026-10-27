// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/core/commands.h"

#include <functional>
#include <utility>

namespace fission::core {

namespace {

// "Set a property of the document to a new value": the pattern behind most commands.
// Two of them with the same key aimed at the same property merge inside a transaction.
template <class T> class SetCommand final : public Command {
public:
    using Read = std::function<std::optional<T>(const Document&)>;
    using Write = std::function<bool(Document&, const T&)>;
    using Label = std::function<CommandDescription(const Document&, const T&)>;

    SetCommand(std::string key, T after, Read read, Write write, Label label)
        : key_(std::move(key)), after_(std::move(after)), read_(std::move(read)),
          write_(std::move(write)), label_(std::move(label)) {}

    ApplyResult apply(Document& document) override {
        if (!known_) {
            const std::optional<T> current = read_(document);
            if (!current) {
                return ApplyResult::refused;
            }
            before_ = *current;
            description_ = label_(document, after_);
            known_ = true;
            if (before_ == after_) {
                return ApplyResult::unchanged;
            }
        }
        return write_(document, after_) ? ApplyResult::applied : ApplyResult::refused;
    }

    void revert(Document& document) override { write_(document, before_); }

    CommandDescription describe() const override { return description_; }

    bool absorb(const Command& next) override {
        const auto* other = dynamic_cast<const SetCommand*>(&next);
        if (other == nullptr || other->key_ != key_) {
            return false;
        }
        after_ = other->after_;
        description_ = other->description_;
        return true;
    }

    bool changesNothing() const override { return known_ && before_ == after_; }

private:
    std::string key_;
    T before_{};
    T after_;
    bool known_ = false;
    Read read_;
    Write write_;
    Label label_;
    CommandDescription description_;
};

CommandDescription featureLabel(const Document& document, const FeatureId& id, std::string key) {
    CommandDescription description{std::move(key), {}};
    if (const Feature* feature = document.findFeature(id)) {
        description.values["type"] = feature->type;
        description.values["number"] = std::to_string(feature->number);
    }
    return description;
}

CommandDescription bodyLabel(const Document& document, const BodyId& id, std::string key) {
    CommandDescription description{std::move(key), {}};
    if (const Body* body = document.findBody(id)) {
        description.values["name"] = body->name;
        if (const Feature* maker = document.findFeature(body->createdBy)) {
            description.values["type"] = maker->type;
            description.values["number"] = std::to_string(maker->number);
        }
    }
    return description;
}

class RemoveFeatureCommand final : public Command {
public:
    explicit RemoveFeatureCommand(FeatureId id) : id_(std::move(id)) {}

    ApplyResult apply(Document& document) override {
        description_ = featureLabel(document, id_, "removeFeature");
        taken_ = document.takeFeature(id_);
        return taken_ ? ApplyResult::applied : ApplyResult::refused;
    }
    void revert(Document& document) override {
        if (taken_) {
            document.restoreFeature(std::move(*taken_));
            taken_.reset();
        }
    }
    CommandDescription describe() const override { return description_; }

private:
    FeatureId id_;
    std::optional<RemovedFeature> taken_;
    CommandDescription description_;
};

class AddParameterCommand final : public Command {
public:
    explicit AddParameterCommand(Parameter parameter) : parameter_(std::move(parameter)) {}

    ApplyResult apply(Document& document) override {
        index_ = document.parameters().size();
        document.addParameter(parameter_);
        return ApplyResult::applied;
    }
    void revert(Document& document) override { document.removeParameterAt(index_); }
    CommandDescription describe() const override {
        return {"addMeasure", {{"name", parameter_.name}}};
    }

private:
    Parameter parameter_;
    std::size_t index_ = 0;
};

class RemoveParameterCommand final : public Command {
public:
    explicit RemoveParameterCommand(std::string name) : name_(std::move(name)) {}

    ApplyResult apply(Document& document) override {
        const auto index = document.indexOfParameter(name_);
        if (!index) {
            return ApplyResult::refused;
        }
        index_ = *index;
        taken_ = document.parameters()[*index];
        document.removeParameterAt(*index);
        return ApplyResult::applied;
    }
    void revert(Document& document) override { document.insertParameter(index_, taken_); }
    CommandDescription describe() const override { return {"removeMeasure", {{"name", name_}}}; }

private:
    std::string name_;
    std::size_t index_ = 0;
    Parameter taken_;
};

class RenameParameterCommand final : public Command {
public:
    RenameParameterCommand(std::string name, std::string newName)
        : name_(std::move(name)), newName_(std::move(newName)) {}

    ApplyResult apply(Document& document) override {
        if (!known_) {
            const auto index = document.indexOfParameter(name_);
            if (!index) {
                return ApplyResult::refused;
            }
            index_ = *index;
            oldName_ = document.parameters()[*index].name;
            known_ = true;
            if (oldName_ == newName_) {
                return ApplyResult::unchanged;
            }
        }
        return document.setParameterNameAt(index_, newName_) ? ApplyResult::applied
                                                             : ApplyResult::refused;
    }
    void revert(Document& document) override { document.setParameterNameAt(index_, oldName_); }
    CommandDescription describe() const override {
        return {"renameMeasure", {{"name", oldName_}, {"value", newName_}}};
    }

private:
    std::string name_;
    std::string newName_;
    std::string oldName_;
    std::size_t index_ = 0;
    bool known_ = false;
};

} // namespace

// --- AddFeatureCommand --------------------------------------------------------------------

AddFeatureCommand::AddFeatureCommand(FeatureDraft draft, std::optional<std::size_t> position)
    : draft_(std::move(draft)), position_(position) {}

ApplyResult AddFeatureCommand::apply(Document& document) {
    if (taken_) { // redo: same ids, same place, same number
        document.restoreFeature(std::move(*taken_));
        taken_.reset();
        document.setLastFeatureNumber(numberAfter_);
        return ApplyResult::applied;
    }
    numberBefore_ = document.lastFeatureNumber();
    const std::string type = draft_.type;
    const auto added = document.addFeature(draft_, position_);
    if (!added) {
        return ApplyResult::refused;
    }
    id_ = added;
    numberAfter_ = document.lastFeatureNumber();
    type_ = type;
    number_ = numberAfter_;
    return ApplyResult::applied;
}

void AddFeatureCommand::revert(Document& document) {
    if (id_) {
        taken_ = document.takeFeature(*id_);
        document.setLastFeatureNumber(numberBefore_);
    }
}

CommandDescription AddFeatureCommand::describe() const {
    return {"addFeature", {{"type", type_}, {"number", std::to_string(number_)}}};
}

// --- factories ----------------------------------------------------------------------------

std::unique_ptr<Command> makeRemoveFeature(FeatureId id) {
    return std::make_unique<RemoveFeatureCommand>(std::move(id));
}

std::unique_ptr<Command> makeSetFeatureValue(FeatureId id, std::string name,
                                             std::string expression) {
    std::string key = "value/" + id.value + "/" + name;
    return std::make_unique<SetCommand<std::string>>(
        std::move(key), std::move(expression),
        [id, name](const Document& document) -> std::optional<std::string> {
            const Feature* feature = document.findFeature(id);
            if (feature == nullptr) {
                return std::nullopt;
            }
            const auto found = feature->params.find(name);
            if (found == feature->params.end()) {
                return std::nullopt;
            }
            return found->second.expression;
        },
        [id, name](Document& document, const std::string& value) {
            return document.setFeatureValue(id, name, value);
        },
        [id, name](const Document& document, const std::string& value) {
            CommandDescription description = featureLabel(document, id, "setValue");
            description.values["field"] = name;
            description.values["value"] = value;
            return description;
        });
}

std::unique_ptr<Command> makeSetFeatureText(FeatureId id, std::string name, std::string text) {
    std::string key = "text/" + id.value + "/" + name;
    return std::make_unique<SetCommand<std::string>>(
        std::move(key), std::move(text),
        [id, name](const Document& document) -> std::optional<std::string> {
            const Feature* feature = document.findFeature(id);
            if (feature == nullptr) {
                return std::nullopt;
            }
            const auto found = feature->texts.find(name);
            if (found == feature->texts.end()) {
                return std::nullopt;
            }
            return found->second;
        },
        [id, name](Document& document, const std::string& value) {
            return document.setFeatureText(id, name, value);
        },
        [id, name](const Document& document, const std::string& value) {
            CommandDescription description = featureLabel(document, id, "setText");
            description.values["field"] = name;
            description.values["value"] = value;
            return description;
        });
}

std::unique_ptr<Command> makeSetFeatureInputs(FeatureId id, std::vector<BodyId> inputs) {
    std::string key = "inputs/" + id.value;
    return std::make_unique<SetCommand<std::vector<BodyId>>>(
        std::move(key), std::move(inputs),
        [id](const Document& document) -> std::optional<std::vector<BodyId>> {
            const Feature* feature = document.findFeature(id);
            return feature != nullptr ? std::optional(feature->inputs) : std::nullopt;
        },
        [id](Document& document, const std::vector<BodyId>& value) {
            return document.setFeatureInputs(id, value);
        },
        [id](const Document& document, const std::vector<BodyId>&) {
            return featureLabel(document, id, "setInputs");
        });
}

std::unique_ptr<Command> makeSetFeatureLabel(FeatureId id, std::string label) {
    std::string key = "label/" + id.value;
    return std::make_unique<SetCommand<std::string>>(
        std::move(key), std::move(label),
        [id](const Document& document) -> std::optional<std::string> {
            const Feature* feature = document.findFeature(id);
            return feature != nullptr ? std::optional<std::string>(feature->label) : std::nullopt;
        },
        [id](Document& document, const std::string& value) {
            return document.setFeatureLabel(id, value);
        },
        [id](const Document& document, const std::string& value) {
            CommandDescription description = featureLabel(document, id, "setLabel");
            description.values["name"] = value;
            return description;
        });
}

std::unique_ptr<Command> makeSetFeatureSuppressed(FeatureId id, bool suppressed) {
    std::string key = "suppressed/" + id.value;
    return std::make_unique<SetCommand<bool>>(
        std::move(key), suppressed,
        [id](const Document& document) -> std::optional<bool> {
            const Feature* feature = document.findFeature(id);
            return feature != nullptr ? std::optional<bool>(feature->suppressed) : std::nullopt;
        },
        [id](Document& document, const bool& value) {
            return document.setFeatureSuppressed(id, value);
        },
        [id](const Document& document, const bool& value) {
            return featureLabel(document, id, value ? "suppress" : "unsuppress");
        });
}

std::unique_ptr<Command> makeSetRollback(std::optional<std::size_t> count) {
    return std::make_unique<SetCommand<std::optional<std::size_t>>>(
        "rollback", count,
        [](const Document& document) -> std::optional<std::optional<std::size_t>> {
            return document.rollback();
        },
        [](Document& document, const std::optional<std::size_t>& value) {
            document.setRollback(value);
            return true;
        },
        [](const Document&, const std::optional<std::size_t>& value) {
            CommandDescription description{"setRollback", {}};
            description.values["count"] = value ? std::to_string(*value) : std::string();
            return description;
        });
}

std::unique_ptr<Command> makeSetBodyName(BodyId id, std::string name) {
    std::string key = "bodyName/" + id.value;
    return std::make_unique<SetCommand<std::string>>(
        std::move(key), std::move(name),
        [id](const Document& document) -> std::optional<std::string> {
            const Body* body = document.findBody(id);
            return body != nullptr ? std::optional<std::string>(body->name) : std::nullopt;
        },
        [id](Document& document, const std::string& value) {
            return document.setBodyName(id, value);
        },
        [id](const Document& document, const std::string& value) {
            CommandDescription description = bodyLabel(document, id, "nameBody");
            description.values["value"] = value;
            return description;
        });
}

std::unique_ptr<Command> makeSetBodyColor(BodyId id, Color color) {
    std::string key = "bodyColor/" + id.value;
    return std::make_unique<SetCommand<Color>>(
        std::move(key), color,
        [id](const Document& document) -> std::optional<Color> {
            const Body* body = document.findBody(id);
            return body != nullptr ? std::optional<Color>(body->color) : std::nullopt;
        },
        [id](Document& document, const Color& value) { return document.setBodyColor(id, value); },
        [id](const Document& document, const Color&) {
            return bodyLabel(document, id, "colorBody");
        });
}

std::unique_ptr<Command> makeSetBodyVisible(BodyId id, bool visible) {
    std::string key = "bodyVisible/" + id.value;
    return std::make_unique<SetCommand<bool>>(
        std::move(key), visible,
        [id](const Document& document) -> std::optional<bool> {
            const Body* body = document.findBody(id);
            return body != nullptr ? std::optional<bool>(body->visible) : std::nullopt;
        },
        [id](Document& document, const bool& value) { return document.setBodyVisible(id, value); },
        [id](const Document& document, const bool& value) {
            return bodyLabel(document, id, value ? "showBody" : "hideBody");
        });
}

std::unique_ptr<Command> makeSetBodyHole(BodyId id, bool hole) {
    std::string key = "bodyHole/" + id.value;
    return std::make_unique<SetCommand<bool>>(
        std::move(key), hole,
        [id](const Document& document) -> std::optional<bool> {
            const Body* body = document.findBody(id);
            return body != nullptr ? std::optional<bool>(body->hole) : std::nullopt;
        },
        [id](Document& document, const bool& value) { return document.setBodyHole(id, value); },
        [id](const Document& document, const bool& value) {
            return bodyLabel(document, id, value ? "holeBody" : "solidBody");
        });
}

std::unique_ptr<Command> makeSetBodyCutExceptions(BodyId id, std::vector<BodyId> exceptions) {
    std::string key = "bodyCutExceptions/" + id.value;
    return std::make_unique<SetCommand<std::vector<BodyId>>>(
        std::move(key), std::move(exceptions),
        [id](const Document& document) -> std::optional<std::vector<BodyId>> {
            const Body* body = document.findBody(id);
            return body != nullptr ? std::optional(body->cutExceptions) : std::nullopt;
        },
        [id](Document& document, const std::vector<BodyId>& value) {
            return document.setBodyCutExceptions(id, value);
        },
        [id](const Document& document, const std::vector<BodyId>&) {
            return bodyLabel(document, id, "cutTargets");
        });
}

std::unique_ptr<Command> makeAddParameter(Parameter parameter) {
    return std::make_unique<AddParameterCommand>(std::move(parameter));
}

std::unique_ptr<Command> makeRemoveParameter(std::string name) {
    return std::make_unique<RemoveParameterCommand>(std::move(name));
}

std::unique_ptr<Command> makeSetParameterExpression(std::string name, std::string expression) {
    std::string key = "measure/" + name;
    return std::make_unique<SetCommand<std::string>>(
        std::move(key), std::move(expression),
        [name](const Document& document) -> std::optional<std::string> {
            const Parameter* parameter = document.findParameter(name);
            return parameter != nullptr ? std::optional<std::string>(parameter->expression)
                                        : std::nullopt;
        },
        [name](Document& document, const std::string& value) {
            return document.setParameterExpression(name, value);
        },
        [name](const Document&, const std::string& value) {
            return CommandDescription{"setMeasure", {{"name", name}, {"value", value}}};
        });
}

std::unique_ptr<Command> makeRenameParameter(std::string name, std::string newName) {
    return std::make_unique<RenameParameterCommand>(std::move(name), std::move(newName));
}

} // namespace fission::core
