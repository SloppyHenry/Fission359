// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// A random but reproducible sequence of changes of every kind the commands know. Shared by the
// history tests (undo) and the file tests (save and load).

#include <cstdint>
#include <memory>
#include <optional>
#include <random>
#include <string>

#include "fission/core/commands.h"

namespace fission::core::testing {

inline FeatureDraft boxDraft(std::string width = "20") {
    FeatureDraft draft;
    draft.type = "box";
    draft.params["width"] = {std::move(width), QuantityKind::length};
    draft.params["depth"] = {"10", QuantityKind::length};
    draft.newBodies = 1;
    return draft;
}

// A random but reproducible sequence of changes of every kind the commands know.
class RandomEditor {
public:
    explicit RandomEditor(std::uint64_t seed) : random_(seed) {}

    // Builds a command that makes sense for the current document, or one that is refused.
    std::unique_ptr<Command> next(const Document& document) {
        const auto& features = document.features();
        const auto& bodies = document.bodies();
        const auto& parameters = document.parameters();
        switch (pick(15)) {
        case 0:
        case 1: { // add (sometimes in the middle, sometimes working on an earlier body)
            FeatureDraft draft = boxDraft(std::to_string(pick(50)));
            std::optional<std::size_t> position;
            if (!features.empty() && pick(3) == 0) {
                position = pick(features.size() + 1);
            }
            if (!bodies.empty() && pick(2) == 0) {
                const std::size_t at = position.value_or(features.size());
                for (const Body& body : bodies) {
                    const auto maker = document.indexOf(body.createdBy);
                    if (maker && *maker < at) {
                        draft.inputs.push_back(body.id);
                        break;
                    }
                }
            }
            return std::make_unique<AddFeatureCommand>(std::move(draft), position);
        }
        case 2:
            if (!features.empty()) {
                return makeRemoveFeature(features[pick(features.size())].id);
            }
            break;
        case 3:
        case 4:
            if (!features.empty()) {
                static const char* const values[] = {"1", "2,5", "3 cm", "Breite + 1", "7", "-4"};
                static const char* const names[] = {"width", "depth", "nonsense"};
                return makeSetFeatureValue(features[pick(features.size())].id, names[pick(3)],
                                           values[pick(6)]);
            }
            break;
        case 5:
            if (!features.empty()) {
                return makeSetFeatureLabel(features[pick(features.size())].id,
                                           pick(2) == 0 ? "" : "Name " + std::to_string(pick(5)));
            }
            break;
        case 6:
            if (!features.empty()) {
                return makeSetFeatureSuppressed(features[pick(features.size())].id, pick(2) == 0);
            }
            break;
        case 7:
            if (!bodies.empty()) {
                return makeSetBodyName(bodies[pick(bodies.size())].id,
                                       "Teil " + std::to_string(pick(4)));
            }
            break;
        case 8:
            if (!bodies.empty()) {
                return makeSetBodyColor(bodies[pick(bodies.size())].id,
                                        Color{static_cast<std::uint8_t>(pick(256)), 20,
                                              static_cast<std::uint8_t>(pick(256))});
            }
            break;
        case 9:
            if (!bodies.empty()) {
                return makeSetBodyVisible(bodies[pick(bodies.size())].id, pick(2) == 0);
            }
            break;
        case 10: // names repeat on purpose: mistakes are not refused (document.h)
            return makeAddParameter({"M" + std::to_string(pick(3)), std::to_string(pick(100)),
                                     QuantityKind::length, ""});
        case 11:
            if (!parameters.empty()) {
                const std::string& name = parameters[pick(parameters.size())].name;
                return pick(2) == 0 ? makeRemoveParameter(name)
                                    : makeSetParameterExpression(name, std::to_string(pick(100)));
            }
            break;
        case 12:
            if (!parameters.empty()) {
                return makeRenameParameter(parameters[pick(parameters.size())].name,
                                           "M" + std::to_string(pick(3)));
            }
            break;
        default: // the time marker anywhere, or back at the end
            return makeSetRollback(pick(3) == 0
                                       ? std::nullopt
                                       : std::optional<std::size_t>(pick(features.size() + 1)));
        }
        return makeSetFeatureValue(FeatureId{"not-there"}, "width", "1"); // refused
    }

    std::size_t pick(std::size_t count) {
        return std::uniform_int_distribution<std::size_t>(0, count - 1)(random_);
    }

private:
    std::mt19937_64 random_;
};

} // namespace fission::core::testing
