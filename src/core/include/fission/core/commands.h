// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "fission/core/command.h"
#include "fission/core/document.h"

namespace fission::core {

/// The standard commands. Each `make...` returns a command for `History::execute`.
/// Everything that changes a document goes through one of these (rule 3).

/// Adds a step (at the end, or before `position`).
class AddFeatureCommand final : public Command {
public:
    explicit AddFeatureCommand(FeatureDraft draft,
                               std::optional<std::size_t> position = std::nullopt);

    ApplyResult apply(Document& document) override;
    void revert(Document& document) override;
    CommandDescription describe() const override;

    /// The id of the new step once the command was applied.
    const std::optional<FeatureId>& result() const noexcept { return id_; }

private:
    FeatureDraft draft_;
    std::optional<std::size_t> position_;
    std::optional<FeatureId> id_;
    std::optional<RemovedFeature> taken_; // while undone
    int numberBefore_ = 0;
    int numberAfter_ = 0;
    std::string type_;
    int number_ = 0;
};

/// One body of an imported file: the step that reads it, and what the body is called and how it
/// looks (taken from the file).
struct ImportedPart {
    FeatureDraft draft;
    std::string bodyName;
    std::optional<Color> color;
};

/// Embeds a file and adds one step per body in it, as **one** step of the history (AP-2.13).
/// The steps name the embedded file in their word `asset`; the command fills that in.
/// Description key `importFile`, values `file` and `count`.
std::unique_ptr<Command> makeImportFile(std::string fileName, Bytes bytes,
                                        std::vector<ImportedPart> parts);

/// Removes a step and the bodies it made.
std::unique_ptr<Command> makeRemoveFeature(FeatureId id);

/// Sets a value of a step (a number or a formula).
std::unique_ptr<Command> makeSetFeatureValue(FeatureId id, std::string name,
                                             std::string expression);
/// Sets a word-valued field of a step (the text of a lettering, the name of its font).
std::unique_ptr<Command> makeSetFeatureText(FeatureId id, std::string name, std::string text);
/// Replaces the bodies a step works on (a combine step gets another tool).
std::unique_ptr<Command> makeSetFeatureInputs(FeatureId id, std::vector<BodyId> inputs);
std::unique_ptr<Command> makeSetFeatureLabel(FeatureId id, std::string label);
/// Switches a step off or on ("ausblenden").
std::unique_ptr<Command> makeSetFeatureSuppressed(FeatureId id, bool suppressed);

/// Pulls the time marker back to `count` steps (nullopt: all of them count, "jetzt" at the end).
std::unique_ptr<Command> makeSetRollback(std::optional<std::size_t> count);

std::unique_ptr<Command> makeSetBodyName(BodyId id, std::string name);
std::unique_ptr<Command> makeSetBodyColor(BodyId id, Color color);
std::unique_ptr<Command> makeSetBodyVisible(BodyId id, bool visible);
/// Marks a body as a hole ("Als Loch") or takes the mark away.
std::unique_ptr<Command> makeSetBodyHole(BodyId id, bool hole);

/// Sets the bodies a hole must not cut (the choices of the label "cuts out: ...").
std::unique_ptr<Command> makeSetBodyCutExceptions(BodyId id, std::vector<BodyId> exceptions);

// Named measures
std::unique_ptr<Command> makeAddParameter(Parameter parameter);
std::unique_ptr<Command> makeRemoveParameter(std::string name);
std::unique_ptr<Command> makeSetParameterExpression(std::string name, std::string expression);
std::unique_ptr<Command> makeRenameParameter(std::string name, std::string newName);
/// Renames a measure **and** every formula of steps and other measures that uses it, as one step
/// of the history (AP-2.9). Refused when the new name is not a valid name or another measure has
/// it already. Description key `renameMeasure`.
std::unique_ptr<Command> makeRenameMeasureEverywhere(std::string name, std::string newName);
/// Changes the comment of a measure. Description key `setMeasureComment`.
std::unique_ptr<Command> makeSetParameterComment(std::string name, std::string comment);
/// Sets (or, with nothing, takes away) the slider range of a measure. Description key
/// `setMeasureSlider`.
std::unique_ptr<Command> makeSetParameterSlider(std::string name,
                                                std::optional<SliderRange> slider);

} // namespace fission::core
