// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <optional>
#include <string_view>
#include <vector>

namespace fission::analysis {

/// A filament material for the weight of a part (AP-2.11).
struct Material {
    std::string_view id;  ///< "pla", "petg", "abs", "asa", "tpu".
    double density = 0.0; ///< g/cm³
};

/// The materials the program knows: PLA 1.24, PETG 1.27, ABS 1.04, ASA 1.07, TPU 1.21 g/cm³.
const std::vector<Material>& materials();

/// The material with that id, or nothing.
std::optional<Material> findMaterial(std::string_view id);

/// The common filament thicknesses (mm): 1.75 and 2.85.
constexpr double kFilament175 = 1.75;
constexpr double kFilament285 = 2.85;

/// The most a part of this volume (mm³) can weigh in grams: a solid body of that material. The
/// real weight depends on the infill and the walls in the slicer and is lower.
double massGrams(double volumeMm3, double densityGramsPerCm3);

/// The filament length (mm) that makes this volume (mm³), as a solid body, from a filament of
/// this diameter (mm). 0 for a diameter that is not positive.
double filamentLengthMm(double volumeMm3, double filamentDiameterMm);

} // namespace fission::analysis
