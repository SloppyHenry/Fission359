// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/core/selection.h"

#include <algorithm>
#include <utility>

namespace fission::core {

bool SelectionFilter::allows(SelectableKind kind) const noexcept {
    switch (kind) {
    case SelectableKind::body:
        return bodies;
    case SelectableKind::face:
        return faces;
    case SelectableKind::edge:
        return edges;
    case SelectableKind::vertex:
        return vertices;
    }
    return false;
}

SelectionFilter SelectionFilter::only(SelectableKind kind) noexcept {
    return {kind == SelectableKind::body, kind == SelectableKind::face,
            kind == SelectableKind::edge, kind == SelectableKind::vertex};
}

bool Selection::contains(const SelectionItem& item) const noexcept {
    return std::find(items_.begin(), items_.end(), item) != items_.end();
}

bool Selection::assign(std::vector<SelectionItem> items) {
    // Names may differ for the same element only by being filled in later: compare the identity.
    if (items == items_) {
        return false;
    }
    items_ = std::move(items);
    ++revision_;
    return true;
}

bool Selection::setFilter(SelectionFilter filter) {
    filter_ = filter;
    std::vector<SelectionItem> kept;
    for (const SelectionItem& item : items_) {
        if (filter_.allows(item.kind)) {
            kept.push_back(item);
        }
    }
    return assign(std::move(kept));
}

bool Selection::clear() {
    return assign({});
}

bool Selection::set(std::vector<SelectionItem> items) {
    std::vector<SelectionItem> kept;
    for (SelectionItem& item : items) {
        if (filter_.allows(item.kind) && std::find(kept.begin(), kept.end(), item) == kept.end()) {
            kept.push_back(std::move(item));
        }
    }
    return assign(std::move(kept));
}

bool Selection::click(const std::optional<SelectionItem>& hit, bool extend) {
    if (!hit || !filter_.allows(hit->kind)) {
        return extend ? false : clear();
    }
    std::vector<SelectionItem> next;
    if (extend) {
        next = items_;
        const auto found = std::find(next.begin(), next.end(), *hit);
        if (found != next.end()) {
            next.erase(found);
        } else {
            next.push_back(*hit);
        }
    } else {
        next.push_back(*hit);
    }
    return assign(std::move(next));
}

bool Selection::doubleClick(const std::optional<SelectionItem>& hit, bool extend) {
    if (!hit) {
        return click(hit, extend);
    }
    SelectionItem body;
    body.body = hit->body;
    return click(body, extend);
}

bool Selection::rubberBand(const std::vector<SelectionItem>& items, bool extend) {
    std::vector<SelectionItem> next;
    if (extend) {
        next = items_;
    }
    for (const SelectionItem& item : items) {
        if (filter_.allows(item.kind) && std::find(next.begin(), next.end(), item) == next.end()) {
            next.push_back(item);
        }
    }
    return assign(std::move(next));
}

std::size_t Selection::reconcile(const std::map<BodyId, ElementNames>& bodies) {
    std::vector<SelectionItem> next;
    std::size_t letGo = 0;
    for (const SelectionItem& item : items_) {
        const auto body = bodies.find(item.body);
        if (body == bodies.end()) {
            ++letGo;
            continue;
        }
        if (item.kind == SelectableKind::body) {
            next.push_back(item);
            continue;
        }
        // Without a name there is nothing to find it by.
        const std::optional<std::uint32_t> number = body->second.find(item.kind, item.name);
        if (!number) {
            ++letGo;
            continue;
        }
        SelectionItem found = item;
        found.number = *number;
        if (std::find(next.begin(), next.end(), found) == next.end()) {
            next.push_back(std::move(found));
        }
    }
    assign(std::move(next));
    return letGo;
}

} // namespace fission::core
