// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/core/element_names.h"

#include <algorithm>
#include <set>

namespace fission::core {

namespace {

const std::string kNoName;

// "edge[A|B]": the names joined in sorted order; empty if any is missing.
std::string joined(const char* prefix, const std::set<std::string>& names, bool anyMissing) {
    if (anyMissing || names.empty()) {
        return {};
    }
    std::string text = prefix;
    text += '[';
    bool first = true;
    for (const std::string& name : names) {
        if (!first) {
            text += '|';
        }
        first = false;
        text += name;
    }
    text += ']';
    return text;
}

// Names that occur more than once belong to nobody.
void dropDuplicates(std::map<std::uint32_t, std::string>& table) {
    std::map<std::string, int> uses;
    for (const auto& entry : table) {
        if (!entry.second.empty()) {
            ++uses[entry.second];
        }
    }
    for (auto& entry : table) {
        if (!entry.second.empty() && uses[entry.second] > 1) {
            entry.second.clear();
        }
    }
}

} // namespace

ElementNames ElementNames::build(const std::vector<BodyFace>& faces,
                                 const geo::TriangleMesh& mesh) {
    ElementNames names;
    for (const BodyFace& face : faces) {
        names.faces_[face.faceId] = face.name;
    }
    dropDuplicates(names.faces_);

    // The faces that touch each corner, collected from the edges that end in it.
    struct Touching {
        std::set<std::string> faces;
        bool missing = false;
    };
    std::map<std::uint32_t, Touching> atCorner;

    for (const geo::EdgePolyline& edge : mesh.edges) {
        std::set<std::string> beside;
        bool missing = false;
        for (std::uint32_t faceId : edge.faces) {
            const std::string& name = names.faceName(faceId);
            if (faceId == 0 || name.empty()) {
                missing = true;
            } else {
                beside.insert(name);
            }
        }
        names.edges_[edge.edgeId] = joined("edge", beside, missing);
        for (std::uint32_t cornerId : edge.corners) {
            if (cornerId == 0) {
                continue;
            }
            Touching& touching = atCorner[cornerId];
            touching.faces.insert(beside.begin(), beside.end());
            touching.missing = touching.missing || missing;
        }
    }
    dropDuplicates(names.edges_);

    for (const geo::CornerPoint& corner : mesh.corners) {
        const auto found = atCorner.find(corner.cornerId);
        names.corners_[corner.cornerId] =
            found == atCorner.end() ? std::string{}
                                    : joined("corner", found->second.faces, found->second.missing);
    }
    dropDuplicates(names.corners_);
    return names;
}

const std::string& ElementNames::nameIn(const Table& table, std::uint32_t number) {
    const auto found = table.find(number);
    return found == table.end() ? kNoName : found->second;
}

std::optional<std::uint32_t> ElementNames::lookup(const Table& table, const std::string& name) {
    if (name.empty()) {
        return std::nullopt;
    }
    for (const auto& entry : table) {
        if (entry.second == name) {
            return entry.first;
        }
    }
    return std::nullopt;
}

const std::string& ElementNames::faceName(std::uint32_t faceId) const {
    return nameIn(faces_, faceId);
}
const std::string& ElementNames::edgeName(std::uint32_t edgeId) const {
    return nameIn(edges_, edgeId);
}
const std::string& ElementNames::cornerName(std::uint32_t cornerId) const {
    return nameIn(corners_, cornerId);
}

std::optional<std::uint32_t> ElementNames::findFace(const std::string& name) const {
    return lookup(faces_, name);
}
std::optional<std::uint32_t> ElementNames::findEdge(const std::string& name) const {
    return lookup(edges_, name);
}
std::optional<std::uint32_t> ElementNames::findCorner(const std::string& name) const {
    return lookup(corners_, name);
}

std::optional<std::uint32_t> ElementNames::find(SelectableKind kind,
                                                const std::string& name) const {
    switch (kind) {
    case SelectableKind::face:
        return findFace(name);
    case SelectableKind::edge:
        return findEdge(name);
    case SelectableKind::vertex:
        return findCorner(name);
    case SelectableKind::body:
        break;
    }
    return std::nullopt;
}

const std::string& ElementNames::name(SelectableKind kind, std::uint32_t number) const {
    switch (kind) {
    case SelectableKind::face:
        return faceName(number);
    case SelectableKind::edge:
        return edgeName(number);
    case SelectableKind::vertex:
        return cornerName(number);
    case SelectableKind::body:
        break;
    }
    return kNoName;
}

} // namespace fission::core
