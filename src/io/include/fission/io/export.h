// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "fission/core/document.h"
#include "fission/geo/mesh.h"
#include "fission/geo/solid.h"

namespace fission::io {

/// How fine the mesh of an export is (docs/03-architektur.md §6). "Normal" keeps a circle within
/// 0.01 mm of the true shape (rule 12).
enum class ExportQuality { coarse, normal, fine };

/// Chord error and angle for a quality: coarse 0.05 mm / 10°, normal 0.01 mm / 5°,
/// fine 0.005 mm / 3°.
geo::TessellationParams exportTessellation(ExportQuality quality) noexcept;

/// One body ready to be written: its mesh (from `GeometryKernel::tessellate` with
/// `exportTessellation`), the name and the colour the person gave it. Millimetres.
struct MeshForExport {
    std::string name;
    core::Color color;
    geo::TriangleMesh mesh;
};

/// Writes all bodies into one binary STL file (little endian, mm). STL has no names and no
/// colours. The file is written atomically. False when it could not be written.
bool exportStl(const std::vector<MeshForExport>& bodies, const std::filesystem::path& path);

/// Writes a 3MF file: one object per body with its name, in millimetres, coloured by a base
/// material. Shared corners are merged, so every object is a closed mesh. Written atomically.
/// False when it could not be written.
bool export3mf(const std::vector<MeshForExport>& bodies, const std::filesystem::path& path);

} // namespace fission::io
