// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/ui/clipboard_payload.h"

#include <utility>

#include "fission/core/serialize.h"

namespace fission::ui {

std::string clipboardToJson(const std::vector<ClipboardPart>& parts) {
    core::DocumentParts document;
    document.title = "clipboard";
    int number = 0;
    for (const ClipboardPart& part : parts) {
        const std::string index = std::to_string(++number);
        core::Feature feature;
        feature.id = core::FeatureId{"clip-step-" + index};
        feature.number = number;
        feature.type = part.draft.type;
        feature.typeVersion = part.draft.typeVersion;
        feature.label = part.draft.label;
        feature.params = part.draft.params;
        feature.texts = part.draft.texts;
        feature.outputs = {core::BodyId{"clip-body-" + index}};
        core::Body body;
        body.id = feature.outputs.front();
        body.name = part.name;
        body.color = part.color;
        body.hole = part.hole;
        body.createdBy = feature.id;
        document.features.push_back(std::move(feature));
        document.bodies.push_back(std::move(body));
    }
    document.lastFeatureNumber = number;
    return core::toJson(core::Document::fromParts(std::move(document)));
}

std::vector<ClipboardPart> clipboardFromJson(std::string_view text) {
    const core::LoadResult loaded = core::fromJson(text);
    std::vector<ClipboardPart> parts;
    if (!loaded.document || loaded.hasFatalProblem()) {
        return parts;
    }
    for (const core::Feature& feature : loaded.document->features()) {
        // Only steps that stand alone: ones that work on other bodies need those bodies.
        if (!feature.inputs.empty() || feature.outputs.size() != 1) {
            return {};
        }
        const core::Body* body = loaded.document->findBody(feature.outputs.front());
        if (body == nullptr) {
            return {};
        }
        ClipboardPart part;
        part.draft.type = feature.type;
        part.draft.typeVersion = feature.typeVersion;
        part.draft.label = feature.label;
        part.draft.params = feature.params;
        part.draft.texts = feature.texts;
        part.draft.newBodies = 1;
        part.name = body->name;
        part.color = body->color;
        part.hole = body->hole;
        parts.push_back(std::move(part));
    }
    return parts;
}

} // namespace fission::ui
