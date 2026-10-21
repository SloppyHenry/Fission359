// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/core/cuts.h"

#include <algorithm>
#include <vector>

#include "fission/core/commands.h"
#include "fission/geo/error.h"

namespace fission::core {

namespace {

std::string wordOf(const Feature& step, const char* name) {
    const auto found = step.texts.find(name);
    return found != step.texts.end() ? found->second : std::string();
}

// A cut the program keeps up to date for holes.
bool isAutoCut(const Feature& step) {
    return step.type == "combine" && !step.suppressed && wordOf(step, "mode") == "subtract" &&
           wordOf(step, "auto") == "yes" && step.inputs.size() >= 2;
}

geo::NamedSolid namedSolid(const BodyResult& body) {
    geo::NamedSolid named;
    named.solid = body.solid;
    for (const BodyFace& face : body.faces) {
        named.faces.push_back({face.faceId, face.name});
    }
    return named;
}

bool boxesOverlap(const geo::Bounds& a, const geo::Bounds& b) {
    constexpr double kMargin = 1e-7;
    return a.min.x <= b.max.x + kMargin && b.min.x <= a.max.x + kMargin &&
           a.min.y <= b.max.y + kMargin && b.min.y <= a.max.y + kMargin &&
           a.min.z <= b.max.z + kMargin && b.min.z <= a.max.z + kMargin;
}

// Does the hole `hole` reach the body `target`? A hole that goes all the way through also reaches
// what it stands on, so it is tested made long as well, but only if the hole itself touches the
// target's box (a hole floating above does not cut). A kernel that cannot tell says no: no cut is
// made on a guess (rule 6).
struct Knowledge {
    const Document& document;
    const FeatureRegistry& registry;
    const geo::GeometryKernel& kernel;
    const RecomputeResult& result;
};

bool intersects(const geo::GeometryKernel& kernel, const geo::NamedSolid& a,
                const geo::NamedSolid& b) {
    try {
        kernel.boolean(geo::BooleanOp::intersect, a, {b});
        return true;
    } catch (const geo::GeometryError&) {
        return false;
    }
}

bool touches(const Knowledge& know, const BodyId& holeId, const BodyResult& hole,
             const BodyResult& target) {
    try {
        const geo::Bounds holeBox = know.kernel.bounds(hole.solid);
        const geo::Bounds targetBox = know.kernel.bounds(target.solid);
        if (!boxesOverlap(holeBox, targetBox)) {
            return false;
        }
        const geo::NamedSolid targetSolid = namedSolid(target);
        if (intersects(know.kernel, namedSolid(hole), targetSolid)) {
            return true;
        }
        const Body* body = know.document.findBody(holeId);
        InputStep step;
        step.feature = body != nullptr ? know.document.findFeature(body->createdBy) : nullptr;
        step.type = step.feature != nullptr ? know.registry.find(step.feature->type) : nullptr;
        if (const FeatureState* made =
                step.feature != nullptr ? know.result.state(step.feature->id) : nullptr) {
            step.params = made->params;
        }
        const ExecuteContext context{know.kernel, {}};
        const double reach = targetBox.diagonal() + holeBox.diagonal();
        if (const auto longer = throughTool(step, reach, context)) {
            return intersects(know.kernel, *longer, targetSolid);
        }
        return false;
    } catch (const geo::GeometryError&) {
        return false;
    }
}

const BodyResult* resultOf(const RecomputeResult& result, const BodyId& id) {
    const auto found = result.bodies.find(id);
    return found != result.bodies.end() && !found->second.stale ? &found->second : nullptr;
}

// Is `tool` a tool of one of the combine steps that lead to `head`?
bool chainContains(const Document& document, BodyId head, const BodyId& tool) {
    for (;;) {
        const Body* body = document.findBody(head);
        const Feature* step = body != nullptr ? document.findFeature(body->createdBy) : nullptr;
        if (step == nullptr || step->type != "combine" || step->inputs.empty()) {
            return false;
        }
        if (std::find(step->inputs.begin() + 1, step->inputs.end(), tool) != step->inputs.end()) {
            return true;
        }
        head = step->inputs.front();
    }
}

// The body at the start of a chain of combine steps: the one that was cut or merged into first.
BodyId rootOf(const Document& document, BodyId body) {
    for (;;) {
        const Body* found = document.findBody(body);
        const Feature* step = found != nullptr ? document.findFeature(found->createdBy) : nullptr;
        if (step == nullptr || step->type != "combine" || step->inputs.empty()) {
            return body;
        }
        body = step->inputs.front();
    }
}

bool isException(const Document& document, const Body& hole, const BodyId& head) {
    const BodyId root = rootOf(document, head);
    return std::find(hole.cutExceptions.begin(), hole.cutExceptions.end(), root) !=
           hole.cutExceptions.end();
}

// The bodies on show that are not holes, with what a hole is tested against.
struct Head {
    BodyId body;             // the body as it is now (the end of a chain of cuts)
    const BodyResult* shape; // what a hole is tested against
};

std::vector<Head> headsOf(const Document& document, const RecomputeResult& result) {
    std::vector<Head> heads;
    for (const Body& body : document.bodies()) {
        const BodyResult* shape = resultOf(result, body.id);
        if (!body.hole && body.visible && shape != nullptr && !document.consumerOf(body.id)) {
            heads.push_back({body.id, shape});
        }
    }
    return heads;
}

// Takes a cut away: whatever used its result uses the body it cut again.
void removeCut(History& history, const FeatureId& cutId) {
    const Document& document = history.document();
    const Feature* cut = document.findFeature(cutId);
    if (cut == nullptr || cut->outputs.empty()) {
        return;
    }
    const BodyId result = cut->outputs.front();
    const BodyId target = cut->inputs.front();
    std::vector<std::pair<FeatureId, std::vector<BodyId>>> rewired;
    for (const Feature& step : document.features()) {
        if (step.id != cutId &&
            std::find(step.inputs.begin(), step.inputs.end(), result) != step.inputs.end()) {
            std::vector<BodyId> inputs = step.inputs;
            std::replace(inputs.begin(), inputs.end(), result, target);
            rewired.emplace_back(step.id, std::move(inputs));
        }
    }
    for (auto& [id, inputs] : rewired) {
        history.execute(makeSetFeatureInputs(id, std::move(inputs)));
    }
    history.execute(makeRemoveFeature(cutId));
}

} // namespace

bool reconcileCuts(History& history, const FeatureRegistry& registry,
                   const geo::GeometryKernel& kernel, RecomputeCache& cache) {
    const Document& document = history.document();
    const bool hasCuttingParts =
        std::any_of(document.bodies().begin(), document.bodies().end(),
                    [](const Body& body) { return body.hole; }) ||
        std::any_of(document.features().begin(), document.features().end(), isAutoCut);
    if (!hasCuttingParts) {
        return false;
    }
    const std::optional<RecomputeResult> calculated = recompute(document, registry, kernel, cache);
    if (!calculated) {
        return false;
    }
    const RecomputeResult& result = *calculated;
    const Knowledge know{document, registry, kernel, result};
    const std::uint64_t revisionBefore = document.revision();

    // 1. Holes that left, or are none any more, are taken out of their cuts.
    std::vector<FeatureId> cuts;
    for (const Feature& step : document.features()) {
        if (isAutoCut(step)) {
            cuts.push_back(step.id);
        }
    }
    for (const FeatureId& id : cuts) {
        const Feature* cut = document.findFeature(id);
        if (cut == nullptr) {
            continue;
        }
        const BodyResult* target = resultOf(result, cut->inputs.front());
        std::vector<BodyId> kept{cut->inputs.front()};
        for (std::size_t i = 1; i < cut->inputs.size(); ++i) {
            const Body* tool = document.findBody(cut->inputs[i]);
            const BodyResult* toolResult = resultOf(result, cut->inputs[i]);
            // Without geometry to judge by, the cut stays as it is.
            const bool stays = tool != nullptr && tool->hole &&
                               !isException(document, *tool, cut->inputs.front()) &&
                               (target == nullptr || toolResult == nullptr ||
                                touches(know, cut->inputs[i], *toolResult, *target));
            if (stays) {
                kept.push_back(cut->inputs[i]);
            }
        }
        if (kept.size() == cut->inputs.size()) {
            continue;
        }
        if (kept.size() == 1) {
            removeCut(history, id);
        } else {
            history.execute(makeSetFeatureInputs(id, std::move(kept)));
        }
    }

    // 2. Holes that touch a body they do not cut yet get a cut.
    std::vector<Head> heads = headsOf(document, result);
    std::vector<BodyId> holes;
    for (const Body& body : document.bodies()) {
        if (body.hole && body.visible && resultOf(result, body.id) != nullptr) {
            holes.push_back(body.id);
        }
    }
    for (const BodyId& hole : holes) {
        const BodyResult& tool = *resultOf(result, hole);
        for (Head& head : heads) {
            if (chainContains(document, head.body, hole) ||
                isException(document, *document.findBody(hole), head.body) ||
                !touches(know, hole, tool, *head.shape)) {
                continue;
            }
            const Body headBody = *document.findBody(head.body);
            FeatureDraft draft;
            draft.type = "combine";
            draft.typeVersion = 1;
            draft.texts = {{"mode", "subtract"}, {"auto", "yes"}};
            draft.inputs = {head.body, hole};
            draft.newBodies = 1;
            const auto added = history.addFeature(std::move(draft));
            const Feature* step = added ? document.findFeature(*added) : nullptr;
            if (step == nullptr || step->outputs.empty()) {
                continue;
            }
            const BodyId made = step->outputs.front();
            if (!headBody.name.empty()) {
                history.execute(makeSetBodyName(made, headBody.name));
            }
            history.execute(makeSetBodyColor(made, headBody.color));
            head.body = made; // later holes cut the result; they are tested against the same shape
        }
    }
    return document.revision() != revisionBefore;
}

std::vector<CutTarget> cutTargetsOf(const Document& document, const RecomputeResult& result,
                                    const FeatureRegistry& registry,
                                    const geo::GeometryKernel& kernel, const BodyId& hole) {
    const Knowledge know{document, registry, kernel, result};
    std::vector<CutTarget> targets;
    const BodyResult* tool = resultOf(result, hole);
    const Body* body = document.findBody(hole);
    if (tool == nullptr || body == nullptr || !body->hole) {
        return targets;
    }
    for (const Head& head : headsOf(document, result)) {
        const bool cutNow = chainContains(document, head.body, hole);
        if (cutNow || touches(know, hole, *tool, *head.shape)) {
            targets.push_back({head.body, rootOf(document, head.body), cutNow});
        }
    }
    return targets;
}

} // namespace fission::core
