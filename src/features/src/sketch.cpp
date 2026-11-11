// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/core/sketch.h"

#include <cmath>
#include <memory>

#include "fission/core/sketch_solve.h"
#include "fission/features/builtin.h"
#include "fission/geo/error.h"

#include "primitives.h"

namespace fission::features {

namespace {

using namespace fission::core;

// A sketch (ADR-0013): a plane and the elements drawn on it. It makes no body; its result is the
// plane and the elements (`SketchResult`), which the later steps that build bodies from sketches
// (AP-2.3) take their curves from. The plane is one of the three base planes (word `basePlane`),
// moved by x, y, z and turned by rotX, rotY, rotZ; or a flat face of a body (reference `plane`,
// the body is the step's input): the plane is worked out from the face every time, so it follows
// the body (AP-2.1b). A face that cannot be found again safely is not calculated at all (rule 6).
class SketchType final : public FeatureType {
public:
    std::string_view id() const override { return "sketch"; }
    int version() const override { return 1; }
    int newBodies() const override { return 0; }
    bool carriesSketch() const override { return true; }

    // The face the sketch lies on is looked up in the first input; an edge taken over in the body
    // its element names (AP-2.1f). An edge no element uses any more is left alone.
    std::optional<std::size_t> refInput(const Feature& feature, const std::string& slot,
                                        std::size_t index) const override {
        if (slot != kSketchEdgeSlot) {
            return std::size_t{0};
        }
        if (feature.sketch) {
            for (const SketchElement& element : feature.sketch->elements) {
                if (element.link && element.link->ref == index) {
                    return element.link->input;
                }
            }
        }
        return std::nullopt;
    }

    std::vector<ParamSpec> parameters() const override { return withPlace({}); }

    std::vector<TextFieldSpec> texts() const override {
        return {{"basePlane", std::string(basePlaneWord(BasePlane::xy))}};
    }

    std::optional<FeatureIssue> validate(const ResolvedParams& params) const override {
        for (const auto& [name, quantity] : params) {
            if (!std::isfinite(quantity.value)) {
                FeatureIssue issue;
                issue.code = FeatureIssueCode::parameterOutOfRange;
                issue.subject = name;
                return issue;
            }
        }
        return std::nullopt;
    }

    FeatureResult execute(const ExecuteInput& input, const ExecuteContext& context) const override {
        SketchResult sketch;
        if (input.feature.sketch) {
            sketch.data = *input.feature.sketch;
        }
        if (!sketch.data.consistent()) {
            throw geo::GeometryError(geo::GeometryErrorCode::invalidInput,
                                     "sketch: the elements are not usable", "sketch");
        }
        const auto onFace = input.feature.refs.find("plane");
        if (onFace != input.feature.refs.end() && !onFace->second.empty()) {
            const auto found = input.refs.find("plane");
            if (input.inputs.empty() || found == input.refs.end() || found->second.empty() ||
                found->second.front().kind != ElementKind::face) {
                throw geo::GeometryError(geo::GeometryErrorCode::invalidInput,
                                         "sketch: the face to draw on is not there", "plane");
            }
            // Throws for a face that is not flat.
            const geo::FlatFace face =
                context.kernel.flatFace(input.inputs.front().solid, found->second.front().id);
            sketch.plane = planeOnFace(face.centre, face.normal, face.longestEdge);
        } else {
            const auto base = basePlaneFromWord(textOf(input.feature, "basePlane", "xy"));
            if (!base) {
                throw geo::GeometryError(geo::GeometryErrorCode::invalidInput,
                                         "sketch: unknown base plane", "basePlane");
            }
            sketch.plane = placedBasePlane(*base,
                                           {valueOf(input.params, "x"), valueOf(input.params, "y"),
                                            valueOf(input.params, "z")},
                                           turnOf(input.params));
        }
        followEdges(input, context, sketch);
        // The measures were evaluated by the recompute (a formula may use a named measure, as in
        // "2 * width"): solve the sketch before its result is used. Only a status of ok writes the
        // solved coordinates back; conflict and failed keep them - a contradiction is no error
        // here, AP-2.2d explains it.
        // A sketch with rules but no measures is solved too (no values to evaluate then).
        static const SketchMeasureValues noValues;
        const SketchMeasureValues& values =
            input.sketchMeasures != nullptr ? *input.sketchMeasures : noValues;
        if (context.sketchSolver != nullptr &&
            (!sketch.data.rules.empty() || !sketch.data.measures.empty())) {
            for (SketchMeasure& measure : sketch.data.measures) {
                const auto found = values.find(measure.id);
                if (found != values.end()) {
                    measure.lastValue = found->second; // the label shows the current number
                }
            }
            const geo::SolveResult solved = solveSketch(sketch.data, values, *context.sketchSolver);
            if (solved.status == geo::SolveStatus::ok) {
                sketch.movable = movableElements(sketch.data, solved);
                sketch.dof = solved.dof;
            } else {
                // conflict or failed: the coordinates stay, every element that does not follow a
                // body edge counts as free
                for (const SketchElement& element : sketch.data.elements) {
                    if (!element.link) {
                        sketch.movable.push_back(element.id);
                    }
                }
            }
        } else {
            // no rules and no measures: nothing was solved, every element that does not follow a
            // body edge can still move
            for (const SketchElement& element : sketch.data.elements) {
                if (!element.link) {
                    sketch.movable.push_back(element.id);
                }
            }
        }
        FeatureResult result;
        result.sketch = std::move(sketch);
        return result;
    }

    // Elements taken over from a body (AP-2.1f) are worked out from their edge every time: the
    // line moves with the body. An edge that cannot be projected exactly any more (the body was
    // turned so that a circle would be an ellipse) is an error on the step, never a guess.
    static void followEdges(const ExecuteInput& input, const ExecuteContext& context,
                            SketchResult& sketch) {
        const auto edges = input.refs.find(std::string(kSketchEdgeSlot));
        for (SketchElement& element : sketch.data.elements) {
            if (!element.link) {
                continue;
            }
            const SketchLink link = *element.link;
            if (!sketch.plane || link.input >= input.inputs.size() || edges == input.refs.end() ||
                link.ref >= edges->second.size() || edges->second[link.ref].id == 0) {
                throw geo::GeometryError(geo::GeometryErrorCode::invalidInput,
                                         "sketch: the edge of " + element.id + " is not there",
                                         std::string(kSketchEdgeSlot));
            }
            const SketchPlane& plane = *sketch.plane;
            const geo::SketchFrame frame{plane.origin(), plane.uAxis(), plane.vAxis()};
            const geo::SketchEdgeShape shape = context.kernel.projectEdgeToSketch(
                input.inputs[link.input].solid, edges->second[link.ref].id, frame);
            const auto expected =
                element.kind == SketchElementKind::line     ? geo::SketchCurveKind::line
                : element.kind == SketchElementKind::circle ? geo::SketchCurveKind::circle
                                                            : geo::SketchCurveKind::arc;
            if (shape.status != geo::SketchEdgeStatus::ok || shape.kind != expected ||
                shape.points.size() != element.points.size()) {
                throw geo::GeometryError(geo::GeometryErrorCode::invalidInput,
                                         "sketch: the edge of " + element.id +
                                             " cannot be taken over exactly any more",
                                         std::string(kSketchEdgeSlot));
            }
            for (std::size_t i = 0; i < shape.points.size(); ++i) {
                element.points[i] = {shape.points[i].x, shape.points[i].y};
            }
        }
    }

    Description describe(const ResolvedParams&) const override { return {"sketch", {}}; }

    std::vector<HandleSpec> handles(const ResolvedParams&) const override { return {}; }

    // A sketch has no size to scale; its elements are edited in the sketch mode.
    std::vector<std::string> sizeParameters() const override { return {}; }
};

} // namespace

namespace detail {
std::unique_ptr<const core::FeatureType> makeSketchType() {
    return std::make_unique<SketchType>();
}
} // namespace detail

} // namespace fission::features
