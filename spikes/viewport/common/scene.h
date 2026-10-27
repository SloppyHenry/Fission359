// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Spike AP-0.4: test scenes for the viewport prototypes. The same OpenCASCADE bodies feed
// both prototypes: prototype A draws the triangles below, prototype B displays the shape.
// Spike code, not product code: it may use OpenCASCADE directly.

#pragma once

#include <TopoDS_Shape.hxx>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace fission::spike {

struct TriMesh {
    // One vertex = position (3), normal (3), face id (1 float, exact up to 16 million).
    std::vector<float> vertices;
    std::vector<std::uint32_t> indices;
    // Line segments of the B-rep edges: pairs of xyz points.
    std::vector<float> edgePoints;

    std::array<float, 3> boundsMin{0.0F, 0.0F, 0.0F};
    std::array<float, 3> boundsMax{0.0F, 0.0F, 0.0F};

    static constexpr std::size_t kFloatsPerVertex = 7;
    std::size_t vertexCount() const { return vertices.size() / kFloatsPerVertex; }
    std::size_t triangleCount() const { return indices.size() / 3; }
};

struct Scene {
    std::string name;
    TriMesh mesh;
    TopoDS_Shape shape; // the same body as B-rep (for prototype B)
    int faceCount = 0;  // face ids run from 1 to faceCount
};

// Names: "part" (small typical part), "bench0.5", "bench2", "bench5" (millions of triangles).
// Returns an empty scene (faceCount == 0) for unknown names.
Scene makeScene(const std::string& name);

} // namespace fission::spike
