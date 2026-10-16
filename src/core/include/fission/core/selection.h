// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "fission/core/element_names.h"
#include "fission/core/id.h"

namespace fission::core {

/// One thing a person has chosen: a body, or a face, edge or corner of a body.
///
/// `number` is the number in the current tessellation; `name` is what the element is called in
/// the recipe (see `ElementNames`). After a change the number is looked up again by the name.
struct SelectionItem {
    BodyId body;
    SelectableKind kind = SelectableKind::body;
    std::uint32_t number = 0; ///< 0 for a body
    std::string name;         ///< empty for a body, and for an element that has no name

    /// Two items are the same thing when body, kind and number agree.
    bool operator==(const SelectionItem& other) const noexcept {
        return body == other.body && kind == other.kind && number == other.number;
    }
};

/// Which kinds of things can be chosen right now. The tool decides (docs/02-ux-konzept.md §2.1):
/// "Passende Elemente leuchten beim Überfahren auf, unpassende reagieren nicht."
struct SelectionFilter {
    bool bodies = true;
    bool faces = true;
    bool edges = true;
    bool vertices = true;

    bool allows(SelectableKind kind) const noexcept;
    static SelectionFilter all() noexcept { return {}; }
    static SelectionFilter only(SelectableKind kind) noexcept;
    bool operator==(const SelectionFilter&) const = default;
};

/// What is chosen, in the order it was chosen. Not part of the document and not undoable (a
/// choice is not a change); it survives a recalculation as far as the elements still exist.
///
/// The model knows nothing of the mouse. The view tells it what was clicked, and the model says
/// what the choice becomes: click replaces, ⇧-click adds or removes, a click on nothing clears.
class Selection {
public:
    const std::vector<SelectionItem>& items() const noexcept { return items_; }
    bool empty() const noexcept { return items_.empty(); }
    std::size_t size() const noexcept { return items_.size(); }
    bool contains(const SelectionItem& item) const noexcept;
    /// Changes whenever the choice changes; the view redraws when it sees a new number.
    std::uint64_t revision() const noexcept { return revision_; }

    SelectionFilter filter() const noexcept { return filter_; }
    /// Chooses what can be selected; things already chosen that no longer fit are let go.
    /// Returns true if the choice changed.
    bool setFilter(SelectionFilter filter);

    /// A click: `hit` is what is under the pointer (nothing: the background). Without `extend`
    /// the hit becomes the only chosen thing, and a click on the background clears. With
    /// `extend` (⇧) the hit is added, or removed if it was already chosen. A hit that the
    /// filter does not allow counts as a click on the background. Returns true if anything
    /// changed.
    bool click(const std::optional<SelectionItem>& hit, bool extend);
    /// A double click chooses the whole body of the hit (with `extend`: adds or removes it).
    bool doubleClick(const std::optional<SelectionItem>& hit, bool extend);
    /// A rectangle was dragged: `items` is everything inside. Without `extend` they replace the
    /// choice, with it they are added.
    bool rubberBand(const std::vector<SelectionItem>& items, bool extend);

    bool clear();
    /// Directly sets the choice (restoring, tests). Items the filter forbids are left out.
    bool set(std::vector<SelectionItem> items);

    /// The model was recalculated: finds each chosen element again by its name and updates its
    /// number. What cannot be found for certain (body gone, name gone or never given) is let
    /// go, never moved to another element. Returns the number of items let go.
    std::size_t reconcile(const std::map<BodyId, ElementNames>& bodies);

private:
    bool assign(std::vector<SelectionItem> items);

    std::vector<SelectionItem> items_;
    SelectionFilter filter_;
    std::uint64_t revision_ = 0;
};

} // namespace fission::core
