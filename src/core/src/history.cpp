// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/core/history.h"

#include <algorithm>
#include <utility>

namespace fission::core {

namespace {

// Several changes that were made as one step. They were applied one by one while the
// transaction was open; redo applies them again in the same order.
class GroupCommand final : public Command {
public:
    GroupCommand(CommandDescription label, std::vector<std::unique_ptr<Command>> children)
        : label_(std::move(label)), children_(std::move(children)) {}

    ApplyResult apply(Document& document) override {
        for (const auto& child : children_) {
            child->apply(document);
        }
        return ApplyResult::applied;
    }
    void revert(Document& document) override {
        for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
            (*it)->revert(document);
        }
    }
    CommandDescription describe() const override { return label_; }

private:
    CommandDescription label_;
    std::vector<std::unique_ptr<Command>> children_;
};

} // namespace

History::History(Document document) : document_(std::move(document)) {}

bool History::execute(std::unique_ptr<Command> command) {
    if (!command) {
        return false;
    }
    switch (command->apply(document_)) {
    case ApplyResult::refused:
        return false;
    case ApplyResult::unchanged:
        return true;
    case ApplyResult::applied:
        break;
    }
    if (inTransaction()) {
        if (pending_.empty() || !pending_.back()->absorb(*command)) {
            pending_.push_back(std::move(command));
        }
    } else {
        pushStep(std::move(command));
    }
    return true;
}

std::optional<FeatureId> History::addFeature(FeatureDraft draft,
                                             std::optional<std::size_t> position) {
    auto command = std::make_unique<AddFeatureCommand>(std::move(draft), position);
    const AddFeatureCommand* added = command.get();
    if (!execute(std::move(command))) {
        return std::nullopt;
    }
    return added->result(); // the step is kept (never merged), so the pointer is valid
}

void History::pushStep(std::unique_ptr<Command> command) {
    // A new change ends the line of things that could be redone.
    steps_.erase(steps_.begin() + static_cast<std::ptrdiff_t>(position_), steps_.end());
    if (clean_ && *clean_ > position_) {
        clean_.reset(); // the saved state was in the part that is gone
    }
    steps_.push_back(std::move(command));
    position_ = steps_.size();
}

bool History::undo() {
    if (!canUndo()) {
        return false;
    }
    steps_[position_ - 1]->revert(document_);
    --position_;
    return true;
}

bool History::redo() {
    if (!canRedo()) {
        return false;
    }
    steps_[position_]->apply(document_);
    ++position_;
    return true;
}

std::optional<CommandDescription> History::nextUndo() const {
    if (!canUndo()) {
        return std::nullopt;
    }
    return steps_[position_ - 1]->describe();
}

std::optional<CommandDescription> History::nextRedo() const {
    if (!canRedo()) {
        return std::nullopt;
    }
    return steps_[position_]->describe();
}

std::vector<CommandDescription> History::steps() const {
    std::vector<CommandDescription> list;
    list.reserve(steps_.size());
    for (const auto& step : steps_) {
        list.push_back(step->describe());
    }
    return list;
}

bool History::goTo(std::size_t position) {
    if (inTransaction() || position > steps_.size()) {
        return false;
    }
    while (position_ > position) {
        undo();
    }
    while (position_ < position) {
        redo();
    }
    return true;
}

void History::beginTransaction(CommandDescription label) {
    if (depth_ == 0) {
        label_ = std::move(label);
        pending_.clear();
    }
    ++depth_;
}

void History::commitTransaction() {
    if (depth_ == 0) {
        return;
    }
    if (--depth_ > 0) {
        return;
    }
    std::erase_if(pending_, [](const auto& command) { return command->changesNothing(); });
    if (pending_.empty()) {
        return;
    }
    if (pending_.size() == 1 && label_.key == "setValue") {
        // A drag of one value: the step says what changed. A step with a name of its own
        // ("Box moved") keeps it, even when only one value changed.
        pushStep(std::move(pending_.front()));
    } else {
        pushStep(std::make_unique<GroupCommand>(std::move(label_), std::move(pending_)));
    }
    pending_.clear();
}

void History::cancelTransaction() {
    if (depth_ == 0) {
        return;
    }
    for (auto it = pending_.rbegin(); it != pending_.rend(); ++it) {
        (*it)->revert(document_);
    }
    pending_.clear();
    depth_ = 0;
}

void History::reset(Document document) {
    document_ = std::move(document);
    steps_.clear();
    pending_.clear();
    position_ = 0;
    depth_ = 0;
    clean_ = 0;
}

} // namespace fission::core
