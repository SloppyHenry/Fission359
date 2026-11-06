// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <filesystem>
#include <string>

#include "fission/geo/outline.h"

namespace fission::io {

/// The outline as an SVG drawing in millimetres (`width`/`height` in mm, so a laser cutter
/// program gets the true size): thin black lines, no fill, y turned so that the drawing looks as
/// it does from outside the body. Lines, circles and arcs stay exact (`<line>`, `<circle>`, arc
/// `<path>`); other curves are `<polyline>`s.
std::string svgText(const geo::Outline& outline);

/// The outline as a DXF file of the old, widely read kind (R12, "AC1009"): LINE, CIRCLE, ARC and
/// POLYLINE entities on layer 0, in millimetres, y up.
std::string dxfText(const geo::Outline& outline);

/// Write them atomically. False when the file could not be written.
bool exportSvg(const geo::Outline& outline, const std::filesystem::path& path);
bool exportDxf(const geo::Outline& outline, const std::filesystem::path& path);

} // namespace fission::io
