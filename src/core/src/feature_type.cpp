// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/core/feature_type.h"

#include <algorithm>
#include <set>

namespace fission::core {

bool FeatureRegistry::add(std::unique_ptr<const FeatureType> type) {
    if (type == nullptr) {
        return false;
    }
    std::string id(type->id());
    if (id.empty() || types_.contains(id)) {
        return false;
    }
    types_.emplace(std::move(id), std::move(type));
    return true;
}

const FeatureType* FeatureRegistry::find(std::string_view id) const {
    const auto found = types_.find(id);
    return found == types_.end() ? nullptr : found->second.get();
}

std::vector<std::string> FeatureRegistry::ids() const {
    std::vector<std::string> result;
    result.reserve(types_.size());
    for (const auto& entry : types_) {
        result.push_back(entry.first);
    }
    return result;
}

std::vector<ParamSpec> turnParameters() {
    return {{"rotX", QuantityKind::angle, "0"},
            {"rotY", QuantityKind::angle, "0"},
            {"rotZ", QuantityKind::angle, "0"}};
}

geo::Point3d FeatureType::bodyCentre(const ResolvedParams& params) const {
    const auto value = [&params](const char* name) {
        const auto found = params.find(name);
        return found != params.end() ? found->second.value : 0.0;
    };
    return {value("x"), value("y"), value("z") + value("height") / 2.0};
}

geo::Angles FeatureType::turnOf(const ResolvedParams& params) {
    const auto value = [&params](const char* name) {
        const auto found = params.find(name);
        return found != params.end() ? found->second.value : 0.0;
    };
    return {value("rotX"), value("rotY"), value("rotZ")};
}

std::vector<HandleSpec> FeatureType::placedHandles(const ResolvedParams& params) const {
    std::vector<HandleSpec> out = handles(params);
    const geo::Angles angles = turnOf(params);
    if (angles.isZero()) {
        return out;
    }
    const geo::Point3d centre = bodyCentre(params);
    for (HandleSpec& handle : out) {
        handle.origin = geo::rotatedAbout(handle.origin, centre, angles);
        handle.direction = geo::rotatedDirection(handle.direction, angles);
        handle.riseDirection = geo::rotatedDirection(handle.riseDirection, angles);
    }
    return out;
}

FeatureDraft defaultDraft(const FeatureType& type) {
    FeatureDraft draft;
    draft.type = std::string(type.id());
    draft.typeVersion = type.version();
    draft.newBodies = type.newBodies();
    for (const ParamSpec& spec : type.parameters()) {
        draft.params[spec.name] = Value{spec.defaultExpression, spec.kind};
    }
    for (const TextFieldSpec& spec : type.texts()) {
        draft.texts[spec.name] = spec.defaultValue;
    }
    return draft;
}

std::string elementName(const Feature& feature, std::string_view role) {
    std::string name = "F" + std::to_string(feature.number) + "/" + feature.type + "/";
    name += role;
    return name;
}

std::optional<FeatureIssue> checkNamingContract(const FeatureOutput& output,
                                                const geo::SolidReport& report) {
    const auto violation = [](std::string detail) {
        FeatureIssue issue;
        issue.code = FeatureIssueCode::namingContractViolated;
        issue.detail = std::move(detail);
        return std::optional<FeatureIssue>(std::move(issue));
    };
    if (output.faces.size() != static_cast<std::size_t>(report.faces)) {
        return violation("the body has " + std::to_string(report.faces) + " faces, " +
                         std::to_string(output.faces.size()) + " are named");
    }
    std::set<std::uint32_t> ids;
    std::set<std::string> roles;
    for (const geo::NamedFace& face : output.faces) {
        if (face.faceId < 1 || face.faceId > static_cast<std::uint32_t>(report.faces)) {
            return violation("face number " + std::to_string(face.faceId) + " does not exist");
        }
        if (face.role.empty()) {
            return violation("a face has an empty role");
        }
        if (!ids.insert(face.faceId).second) {
            return violation("face " + std::to_string(face.faceId) + " is named twice");
        }
        if (!roles.insert(face.role).second) {
            return violation("role '" + face.role + "' is used twice");
        }
    }
    return std::nullopt;
}

} // namespace fission::core
