// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "fission/core/cancel.h"
#include "fission/core/document.h"
#include "fission/core/expression.h"
#include "fission/geo/kernel.h"
#include "fission/geo/rotation.h"

namespace fission::core {

/// What is wrong with a step. Codes and values only: the interface turns them into sentences
/// through the message catalogue (rule 5), and no raw kernel text ever reaches a person.
enum class FeatureIssueCode {
    unknownType,            ///< No feature type of that name is registered.
    parameterInvalid,       ///< A value's formula has an error; `expression` says which.
    parameterOutOfRange,    ///< Understandable, but not usable (size zero or negative).
    inputMissing,           ///< A body the step works on does not exist (any more).
    inputFailed,            ///< An earlier step it depends on has an error.
    namingContractViolated, ///< The step did not name every face it created (a bug).
    kernelRejected,         ///< The kernel could not use the input.
    kernelResultInvalid,    ///< The kernel's result failed the validity check.
    kernelFailed,           ///< The kernel could not compute it.
    cancelled,              ///< A newer change replaced this calculation.
    textGlyphsMissing,      ///< The font has no character for part of a text; `subject` lists them.
    fontNotFound,           ///< The font is not on this computer; `subject` is its name.
    fontStyleMissing,       ///< The font has no such style (bold, italic); `subject` is its name.
    combineEmpty,           ///< Combining left nothing (an overlap of bodies that do not overlap).
};

struct FeatureIssue {
    FeatureIssueCode code = FeatureIssueCode::kernelFailed;
    std::string subject; ///< The parameter concerned, if any.
    std::optional<ExpressionError> expression;
    std::string detail; ///< For the log only, never shown.
};

/// What a feature type says about a value it takes.
struct ParamSpec {
    std::string name;
    QuantityKind kind = QuantityKind::length;
    std::string defaultExpression;
};

/// A word-valued field of a step (a text, the name of a font) with its default.
struct TextFieldSpec {
    std::string name;
    std::string defaultValue;
};

/// Values of a step after formulas and named measures were worked out (mm, degrees).
using ResolvedParams = std::map<std::string, Quantity>;

/// The step as a recipe sentence, without words: the interface owns the text (rule 4) and
/// builds "Box 20 × 20 × 20 mm" from key `box` and its three values.
struct Description {
    std::string key;
    std::vector<Quantity> values;
};

enum class HandleKind { distance, radius };

/// What a handle is for, which decides how loudly it is shown: a main size of the shape (with its
/// measure band and label), a detail that refines it (a wall, an inner radius: its label shows
/// when the pointer is on it), or a rounding (a detail with the quarter-circle grip).
enum class HandleRole { size, detail, rounding };

/// A handle in the 3D view that changes one value of the step (rule 1). The handle changes the
/// value, never scales the geometry (rule 11).
struct HandleSpec {
    std::string id;
    HandleKind kind = HandleKind::distance;
    HandleRole role = HandleRole::size;
    std::string parameter;  ///< The value it changes.
    geo::Point3d origin;    ///< Where it sits (mm).
    geo::Point3d direction; ///< Unit vector along which it is dragged.
    double sign = 1.0;      ///< +1: dragging along `direction` increases the value, -1: decreases.
    /// How far the handle moves (mm) when the value grows by 1 mm. 1 for a handle on a face that
    /// grows alone; 0.5 for a face of a shape that grows to both sides around its middle (the
    /// width of a box): the face then follows the pointer when the value changes twice as much.
    double travel = 1.0;
    /// How far (mm) the handle also rises when the value grows by 1 mm. A sphere stands on its
    /// plane: as it grows, its middle rises, and a handle on its equator moves out and up at once.
    /// Dragging follows that slanted way, so the dot stays under the pointer.
    double rise = 0.0;
    /// The way it rises (unit vector): up, unless the body is turned.
    geo::Point3d riseDirection{0.0, 0.0, 1.0};
    /// The length (mm) this handle measures, along `direction` and ending at `origin`: the
    /// interface draws it as a measure band from `origin − direction × extent` to `origin`.
    double extent = 0.0;
};

/// One body a step made or changed, with the role of each of its faces.
struct FeatureOutput {
    BodyId body;
    geo::Solid solid;
    std::vector<geo::NamedFace> faces; ///< Roles, as the feature type reports them.
};

struct FeatureResult {
    std::vector<FeatureOutput> outputs;
};

struct ExecuteInput {
    const Feature& feature;
    const ResolvedParams& params;
    /// Results for `feature.inputs`, in the same order. Their face roles are full names
    /// ("F1/box/top"), so a step can pass them on.
    const std::vector<FeatureOutput>& inputs;
};

struct ExecuteContext {
    const geo::GeometryKernel& kernel;
    CancelToken cancel; ///< Long steps look at it and give up when asked to.
};

/// A kind of step (box, cylinder, later extrude, fillet ...). Feature types are plug-ins: a new
/// tool is a new type, the document and the recompute do not change (docs/03-architektur.md §3.2).
///
/// Naming contract (ADR-0003): `execute` names every face of every body it makes and passes on
/// the names of faces it keeps. The recompute checks this after each call and turns a violation
/// into an error on the step; a type that cannot keep the contract is not added.
class FeatureType {
public:
    FeatureType() = default;
    FeatureType(const FeatureType&) = delete;
    FeatureType& operator=(const FeatureType&) = delete;
    virtual ~FeatureType() = default;

    virtual std::string_view id() const = 0;
    virtual int version() const = 0;

    /// How many new bodies a new step of this type makes.
    virtual int newBodies() const { return 1; }

    /// The values the step takes, with their defaults.
    virtual std::vector<ParamSpec> parameters() const = 0;

    /// The words the step takes (most steps take none), with their defaults. They are read from
    /// `Feature::texts` in `execute`.
    virtual std::vector<TextFieldSpec> texts() const { return {}; }

    /// Checks the resolved values before the kernel is asked (cheap, no geometry).
    virtual std::optional<FeatureIssue> validate(const ResolvedParams& params) const = 0;

    /// Makes the geometry. May throw `geo::GeometryError`; the recompute turns it into an issue.
    virtual FeatureResult execute(const ExecuteInput& input,
                                  const ExecuteContext& context) const = 0;

    virtual Description describe(const ResolvedParams& params) const = 0;

    /// Like `describe`, for a step whose sentence depends on its words or inputs (a combine says
    /// how many tools it has). The default asks `describe`.
    virtual Description describeStep(const Feature& feature, const ResolvedParams& params) const {
        (void)feature;
        return describe(params);
    }

    /// True for a type whose result keeps the full names of its input faces ("F1/box/top"): the
    /// roles it reports are final and not prefixed with this step's number and type.
    virtual bool passesOnNames() const { return false; }

    /// The handles of the step, in the order they are shown. A value that can be changed from
    /// several equal places (both ends of a box, all around a cylinder) is listed once for
    /// each place, with the same `parameter`: the interface picks the one facing the viewer, so
    /// the handle is always where one expects it.
    virtual std::vector<HandleSpec> handles(const ResolvedParams& params) const = 0;

    /// The middle of the box round the body as the step makes it before it is turned (mm): the
    /// point it is turned about (ADR-0010). The default fits a body that stands on the plane z,
    /// centred on (x, y), with a value `height`; a type with another shape says so.
    virtual geo::Point3d bodyCentre(const ResolvedParams& params) const;

    /// The turn of the body: the values `rotX`, `rotY`, `rotZ` (degrees), none for a type that
    /// has no such values.
    static geo::Angles turnOf(const ResolvedParams& params);

    /// The handles where they stand once the body is turned: `handles` turned about
    /// `bodyCentre` by `turnOf`. This is what the interface shows.
    std::vector<HandleSpec> placedHandles(const ResolvedParams& params) const;

    /// The values that are sizes of the shape (AP-1.6b2): they all grow by the same factor when
    /// the body is made larger or smaller in all directions alike, so that it keeps its form (a
    /// box with 5 mm corners at 200 % has 10 mm corners). The default is every length but the
    /// place (`x`, `y`, `z`); counts (the points of a star) and angles are no sizes.
    virtual std::vector<std::string> sizeParameters() const;
};

/// The three values of the turn (degrees, default 0), to be listed by every type that puts a
/// body in a place.
std::vector<ParamSpec> turnParameters();

/// The feature types a program knows. One per program (or per test); no global instance.
class FeatureRegistry {
public:
    /// False (and nothing added) when a type with that id is already there.
    bool add(std::unique_ptr<const FeatureType> type);
    const FeatureType* find(std::string_view id) const;
    std::vector<std::string> ids() const;

private:
    std::map<std::string, std::unique_ptr<const FeatureType>, std::less<>> types_;
};

/// A new step of this type with every value at its default and the bodies it will create.
FeatureDraft defaultDraft(const FeatureType& type);

/// The name of a face as a reference stores it: "F5/box/top".
std::string elementName(const Feature& feature, std::string_view role);

/// Checks the naming contract for a result: every face of the body has exactly one role.
std::optional<FeatureIssue> checkNamingContract(const FeatureOutput& output,
                                                const geo::SolidReport& report);

} // namespace fission::core
