// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "fission/core/recompute.h"
#include "fission/geo/mesh.h"

namespace fission::core {

/// What a person can point at in the 3D view.
enum class SelectableKind {
    body,  ///< Körper
    face,  ///< Fläche
    edge,  ///< Kante
    vertex ///< Ecke
};

/// The names of the faces, edges and corners of one body (docs/03-architektur.md §3.4,
/// ADR-0003). The numbers in a tessellation only mean something for that tessellation; the names
/// stay the same when a value higher up in the recipe changes, so a selection (and later a
/// reference from another step) can find its element again.
///
/// - A face is named by the step that made it: `F1/box/top`.
/// - An edge is named after the two faces it joins: `edge[F1/box/left|F1/box/top]`.
/// - A corner is named after the faces that meet in it: `corner[F1/box/bottom|...|...]`.
///
/// An element whose name would not be unique in the body (or that has an unnamed neighbour) gets
/// no name at all. It can be pointed at, but nothing may hold on to it across a change: better
/// to lose it than to land on a different one silently (rule 6).
class ElementNames {
public:
    ElementNames() = default;

    /// `faces` are the names the steps gave (`BodyResult::faces`), `mesh` tells which faces meet
    /// in which edge and corner. The numbers are those of the mesh.
    static ElementNames build(const std::vector<BodyFace>& faces, const geo::TriangleMesh& mesh);

    /// Empty for a number without a name.
    const std::string& faceName(std::uint32_t faceId) const;
    const std::string& edgeName(std::uint32_t edgeId) const;
    const std::string& cornerName(std::uint32_t cornerId) const;

    std::optional<std::uint32_t> findFace(const std::string& name) const;
    std::optional<std::uint32_t> findEdge(const std::string& name) const;
    std::optional<std::uint32_t> findCorner(const std::string& name) const;

    /// The number of the element with this name, or nothing.
    std::optional<std::uint32_t> find(SelectableKind kind, const std::string& name) const;
    /// The name of an element of the given kind (empty for `body`).
    const std::string& name(SelectableKind kind, std::uint32_t number) const;

    std::size_t faceCount() const noexcept { return faces_.size(); }
    std::size_t edgeCount() const noexcept { return edges_.size(); }
    std::size_t cornerCount() const noexcept { return corners_.size(); }

private:
    using Table = std::map<std::uint32_t, std::string>;
    static std::optional<std::uint32_t> lookup(const Table& table, const std::string& name);
    static const std::string& nameIn(const Table& table, std::uint32_t number);

    Table faces_;
    Table edges_;
    Table corners_;
};

} // namespace fission::core
