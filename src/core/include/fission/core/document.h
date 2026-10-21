// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "fission/core/expression.h"
#include "fission/core/id.h"
#include "fission/core/measures.h"
#include "fission/core/persistent_ref.h"

namespace fission::core {

/// A number as the person typed it. The calculated value is not stored here: it is the result
/// of a recompute and depends on the named measures at that moment.
struct Value {
    std::string expression;
    QuantityKind kind = QuantityKind::length;

    bool operator==(const Value&) const = default;
};

/// A named measure (benanntes Maß).
struct Parameter {
    std::string name;
    std::string expression;
    QuantityKind kind = QuantityKind::length;
    std::string comment;

    bool operator==(const Parameter&) const = default;
};

/// One step of the recipe. Everything a step does is described by its type and its values;
/// nothing is hidden in geometry (rule 3: everything stays changeable).
struct Feature {
    FeatureId id;
    int number = 0;      ///< "F5": counted up per document, never used twice (ADR-0003).
    std::string type;    ///< "box", "cylinder", ...
    int typeVersion = 1; ///< For migrations of one feature type.
    std::string label;   ///< Optional name given by the person.
    std::map<std::string, Value> params;
    std::map<std::string, std::string> texts; ///< Words the person typed (not formulas).
    std::map<std::string, std::vector<PersistentRef>> refs;
    std::vector<BodyId> inputs;  ///< Bodies this step works on (made by earlier steps).
    std::vector<BodyId> outputs; ///< Bodies this step creates or changes.
    bool suppressed = false;

    bool operator==(const Feature&) const = default;
};

enum class BodyKind { precise, free };

struct Color {
    std::uint8_t red = 233;
    std::uint8_t green = 234;
    std::uint8_t blue = 236;

    bool operator==(const Color&) const = default;
};

/// What is left of the steps: a body the person sees, names and colours. Its geometry is a
/// result of the recompute, not part of the document.
struct Body {
    BodyId id;
    std::string name; ///< Empty = the interface shows a default name for the type.
    Color color;
    bool visible = true;
    /// Marked "as hole" (Als Loch): the body is a tool that cuts out of the bodies it touches
    /// instead of being part. Shown as a ghost; the cutting itself arrives with AP-1.7.
    bool hole = false;
    BodyKind kind = BodyKind::precise;
    FeatureId createdBy;
    /// Bodies this hole must not cut although it touches them (the person took them off the label
    /// "cuts out: ..."). Named by the body they were when the hole met them, see `reconcileCuts`.
    std::vector<BodyId> cutExceptions;

    bool operator==(const Body&) const = default;
};

struct DocumentSettings {
    /// What a bare number in a length field means: 1.0 for millimetres, 25.4 for inches.
    double defaultUnitMillimetres = 1.0;

    bool operator==(const DocumentSettings&) const = default;
};

/// A new step, before the document gave it an id and a number.
struct FeatureDraft {
    std::string type;
    int typeVersion = 1;
    std::string label;
    std::map<std::string, Value> params;
    std::map<std::string, std::string> texts;
    std::map<std::string, std::vector<PersistentRef>> refs;
    std::vector<BodyId> inputs;
    int newBodies = 0; ///< How many new bodies the step creates.
};

/// A step taken out of the recipe, with everything needed to put it back exactly where it was
/// (same id, same number, same place, same bodies at the same places). Commands use this to
/// undo and redo adding and removing steps.
struct RemovedFeature {
    Feature feature;
    std::size_t index = 0;
    std::vector<std::pair<std::size_t, Body>> bodies; ///< Ascending by index.
};

/// The document: named measures, the recipe (features in order) and the bodies it makes.
///
/// A plain value: copies are independent, so the recompute works on a snapshot while the
/// interface goes on editing. There is no global state; any number of documents can be open.
/// The interface changes a document only through commands (AP-1.2, `History`); the member
/// functions that change it are what those commands call.
class Document {
public:
    Document() = default;
    /// Deterministic ids (tests).
    explicit Document(std::uint64_t idSeed) : ids_(idSeed) {}

    std::string title;
    const DocumentSettings& settings() const noexcept { return settings_; }
    void setSettings(const DocumentSettings& settings);

    /// Increases with every change; lets others see "did it change since I looked".
    std::uint64_t revision() const noexcept { return revision_; }

    /// True when both documents hold the same content (title, settings, measures, recipe, bodies).
    /// Revision and id generator state are not content. Used to prove that undo restores a state.
    bool sameContentAs(const Document& other) const;

    // --- named measures -----------------------------------------------------------------
    const std::vector<Parameter>& parameters() const noexcept { return parameters_; }
    const Parameter* findParameter(std::string_view name) const;
    /// Adds a measure at the end. Mistakes (repeated name, broken formula) are not refused:
    /// they show up as errors on the measure so that nothing a person typed is lost.
    void addParameter(Parameter parameter);
    bool setParameterExpression(std::string_view name, std::string expression);
    bool renameParameter(std::string_view name, std::string newName);
    bool removeParameter(std::string_view name);
    /// Position of a measure in the list, or nullopt.
    std::optional<std::size_t> indexOfParameter(std::string_view name) const;
    /// Removes the measure at a position (exact, even when names repeat).
    bool removeParameterAt(std::size_t index);
    /// Renames the measure at a position (exact, even when names repeat).
    bool setParameterNameAt(std::size_t index, std::string newName);
    /// Puts a measure back at a given place (undo of removing it).
    void insertParameter(std::size_t index, Parameter parameter);
    std::vector<MeasureDefinition> measureDefinitions() const;

    // --- recipe --------------------------------------------------------------------------
    const std::vector<Feature>& features() const noexcept { return features_; }
    const Feature* findFeature(const FeatureId& id) const;
    /// Index in the recipe, or nullopt.
    std::optional<std::size_t> indexOf(const FeatureId& id) const;

    /// Inserts a step (at the end, or before `position`). The new bodies it creates get ids.
    /// Refused (nullopt) when an input body does not exist or is made only by a later step.
    std::optional<FeatureId> addFeature(FeatureDraft draft,
                                        std::optional<std::size_t> position = std::nullopt);
    /// Removes the step and the bodies it created. Steps that used them stay and report a
    /// missing input; `dependentsOf` tells beforehand who that will be.
    bool removeFeature(const FeatureId& id);
    std::vector<FeatureId> dependentsOf(const FeatureId& id) const;
    /// Like `removeFeature`, but hands back what is needed to restore the step exactly.
    std::optional<RemovedFeature> takeFeature(const FeatureId& id);
    /// Puts a step taken out with `takeFeature` back where it was.
    void restoreFeature(RemovedFeature removed);
    /// The number the last new step got. Commands set it back on undo so that numbers stay
    /// gapless in what the person sees; removing a step alone never frees its number.
    int lastFeatureNumber() const noexcept { return lastFeatureNumber_; }
    void setLastFeatureNumber(int number) noexcept { lastFeatureNumber_ = number; }

    bool setFeatureValue(const FeatureId& id, std::string_view name, std::string expression);
    /// Sets a text of a step; false when the step has no text of that name.
    bool setFeatureText(const FeatureId& id, std::string_view name, std::string text);
    bool setFeatureLabel(const FeatureId& id, std::string label);
    /// Replaces the bodies a step works on. Refused (false) when one does not exist or is made by
    /// this step or a later one.
    bool setFeatureInputs(const FeatureId& id, std::vector<BodyId> inputs);
    /// The step that uses the body as an input, which is then "consumed": it is part of that
    /// step's result and not a body of its own any more (ADR-0011). Steps that are switched off
    /// consume nothing. nullopt when no step uses it.
    std::optional<FeatureId> consumerOf(const BodyId& id) const;
    bool setFeatureSuppressed(const FeatureId& id, bool suppressed);
    bool setFeatureRefs(const FeatureId& id, std::string_view name,
                        std::vector<PersistentRef> refs);

    // --- bodies --------------------------------------------------------------------------
    const std::vector<Body>& bodies() const noexcept { return bodies_; }
    const Body* findBody(const BodyId& id) const;
    bool setBodyName(const BodyId& id, std::string name);
    bool setBodyColor(const BodyId& id, Color color);
    bool setBodyVisible(const BodyId& id, bool visible);
    bool setBodyHole(const BodyId& id, bool hole);
    bool setBodyCutExceptions(const BodyId& id, std::vector<BodyId> exceptions);

private:
    Feature* mutableFeature(const FeatureId& id);
    Body* mutableBody(const BodyId& id);
    void touch() noexcept { ++revision_; }

    DocumentSettings settings_;
    std::vector<Parameter> parameters_;
    std::vector<Feature> features_;
    std::vector<Body> bodies_;
    int lastFeatureNumber_ = 0;
    std::uint64_t revision_ = 0;
    IdSource ids_;
};

} // namespace fission::core
