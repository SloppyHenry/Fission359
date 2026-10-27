// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/render/pick.h"

#include <cmath>
#include <set>

namespace fission::render {

namespace {

constexpr unsigned kKindFace = 1;
constexpr unsigned kKindEdge = 2;
constexpr unsigned kKindCorner = 3;

unsigned codeOf(core::SelectableKind kind) noexcept {
    switch (kind) {
    case core::SelectableKind::face:
        return kKindFace;
    case core::SelectableKind::edge:
        return kKindEdge;
    case core::SelectableKind::vertex:
        return kKindCorner;
    case core::SelectableKind::body:
        break;
    }
    return 0;
}

} // namespace

std::uint32_t packPickId(core::SelectableKind kind, unsigned slot, std::uint32_t number) noexcept {
    const unsigned code = codeOf(kind);
    if (code == 0 || slot == 0 || slot >= kPickSlots || number == 0 || number > kPickMaxNumber) {
        return 0;
    }
    return (code << 30) | (static_cast<std::uint32_t>(slot) << 20) | number;
}

PickId unpackPickId(std::uint32_t id) noexcept {
    PickId result;
    const unsigned code = id >> 30;
    result.slot = (id >> 20) & (kPickSlots - 1);
    result.number = id & kPickMaxNumber;
    if (code == 0 || result.slot == 0 || result.number == 0) {
        return result;
    }
    result.kind = code == kKindFace   ? core::SelectableKind::face
                  : code == kKindEdge ? core::SelectableKind::edge
                                      : core::SelectableKind::vertex;
    result.valid = true;
    return result;
}

std::optional<PickedElement> choosePicked(std::span<const std::uint32_t> picture,
                                          const core::SelectionFilter& filter) {
    if (picture.size() != static_cast<std::size_t>(kPickSize) * kPickSize) {
        return std::nullopt;
    }

    struct Candidate {
        PickId id;
        float distance = 0.0F;
        float score = 0.0F;
    };
    std::optional<Candidate> best; // the nearest corner or edge the filter allows
    for (int y = 0; y < kPickSize; ++y) {
        for (int x = 0; x < kPickSize; ++x) {
            const PickId id = unpackPickId(picture[static_cast<std::size_t>(y * kPickSize + x)]);
            if (!id.valid || id.kind == core::SelectableKind::face) {
                continue;
            }
            const float distance = std::hypot(static_cast<float>(x - kPickRadius),
                                              static_cast<float>(y - kPickRadius));
            const bool corner = id.kind == core::SelectableKind::vertex;
            if (distance > (corner ? kCornerReach : kEdgeReach)) {
                continue;
            }
            const Candidate candidate{id, distance,
                                      corner ? distance - kCornerPreference : distance};
            if (filter.allows(id.kind) && (!best || candidate.score < best->score)) {
                best = candidate;
            }
        }
    }
    if (best) {
        return PickedElement{best->id.slot, best->id.kind, best->id.number, best->distance};
    }

    const PickId centre =
        unpackPickId(picture[static_cast<std::size_t>(kPickRadius * kPickSize + kPickRadius)]);
    if (centre.valid && centre.kind == core::SelectableKind::face) {
        if (filter.faces) {
            return PickedElement{centre.slot, core::SelectableKind::face, centre.number, 0.0F};
        }
        if (filter.bodies) {
            return PickedElement{centre.slot, core::SelectableKind::body, 0, 0.0F};
        }
        return std::nullopt;
    }
    return std::nullopt;
}

std::vector<PickedElement> chooseInRegion(std::span<const std::uint32_t> picture,
                                          const core::SelectionFilter& filter) {
    const core::SelectableKind wanted = filter.bodies     ? core::SelectableKind::body
                                        : filter.faces    ? core::SelectableKind::face
                                        : filter.edges    ? core::SelectableKind::edge
                                        : filter.vertices ? core::SelectableKind::vertex
                                                          : core::SelectableKind::body;
    if (!filter.bodies && !filter.faces && !filter.edges && !filter.vertices) {
        return {};
    }
    std::vector<PickedElement> found;
    std::set<std::uint64_t> seen;
    for (const std::uint32_t value : picture) {
        const PickId id = unpackPickId(value);
        if (!id.valid) {
            continue;
        }
        if (wanted != core::SelectableKind::body && id.kind != wanted) {
            continue;
        }
        const std::uint32_t number = wanted == core::SelectableKind::body ? 0 : id.number;
        const std::uint64_t key = (static_cast<std::uint64_t>(id.slot) << 32) | number;
        if (seen.insert(key).second) {
            found.push_back({id.slot, wanted, number, 0.0F});
        }
    }
    return found;
}

} // namespace fission::render
