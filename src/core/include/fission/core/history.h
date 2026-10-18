// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

#include "fission/core/command.h"
#include "fission/core/commands.h"
#include "fission/core/document.h"

namespace fission::core {

/// Owns a document and everything that happened to it: the one place where a document
/// changes (architecture rule 3). Whoever holds the history can only look at the document
/// (`document()`); changing it means executing a command, which makes it undoable.
///
/// Steps are a straight line: after going back, a new change throws away what could have been
/// redone. The stack is only limited by memory and is not saved with the file.
/// Not thread-safe; the interface thread owns it. The recompute works on a copy of the document.
class History {
public:
    explicit History(Document document = {});

    const Document& document() const noexcept { return document_; }

    /// Applies the command and records it as one step (or as part of the open transaction).
    /// False when the command was refused. A command that changes nothing is accepted but
    /// leaves no step.
    bool execute(std::unique_ptr<Command> command);

    /// Adds a step and tells its id; nullopt when refused.
    std::optional<FeatureId> addFeature(FeatureDraft draft,
                                        std::optional<std::size_t> position = std::nullopt);

    // --- undo and redo -------------------------------------------------------------------
    bool canUndo() const noexcept { return !inTransaction() && position_ > 0; }
    bool canRedo() const noexcept { return !inTransaction() && position_ < steps_.size(); }
    bool undo();
    bool redo();
    /// Description of what `undo` / `redo` would do next.
    std::optional<CommandDescription> nextUndo() const;
    std::optional<CommandDescription> nextRedo() const;

    /// All steps in the order they happened; the first `position()` are done, the rest can be
    /// redone.
    std::vector<CommandDescription> steps() const;
    std::size_t position() const noexcept { return position_; }
    /// Goes to the state after `position` steps (0 = the beginning), undoing or redoing as
    /// many steps as needed. False when not possible (out of range, transaction open).
    bool goTo(std::size_t position);

    // --- transactions --------------------------------------------------------------------
    /// Everything executed until `commitTransaction` becomes one step, called `label` (when it
    /// was a single change under the plain label `setValue`, the step is described by that
    /// change). The
    /// changes are applied right away (so the 3D view follows the mouse). Calls may nest; the
    /// outermost one counts.
    void beginTransaction(CommandDescription label);
    /// Closes the transaction. Records one step, or none when nothing changed on balance.
    void commitTransaction();
    /// Closes the transaction and takes back everything it did; no step is recorded.
    void cancelTransaction();
    bool inTransaction() const noexcept { return depth_ > 0; }

    // --- saved state ---------------------------------------------------------------------
    /// Remembers the current state as the one on disk.
    void markClean() noexcept { clean_ = position_; }
    /// True when the document is in the state of the last `markClean`.
    bool isClean() const noexcept { return !inTransaction() && clean_ && *clean_ == position_; }

    /// Replaces the document (open file) and forgets all steps.
    void reset(Document document);

private:
    void pushStep(std::unique_ptr<Command> command);

    Document document_;
    std::vector<std::unique_ptr<Command>> steps_;
    std::size_t position_ = 0; // steps_[0 .. position_) are applied
    std::optional<std::size_t> clean_ = 0;

    int depth_ = 0;
    CommandDescription label_;
    std::vector<std::unique_ptr<Command>> pending_;
};

} // namespace fission::core
