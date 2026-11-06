// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/analysis/material.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace fission::analysis {

const std::vector<Material>& materials() {
    static const std::vector<Material> table = {
        {"pla", 1.24}, {"petg", 1.27}, {"abs", 1.04}, {"asa", 1.07}, {"tpu", 1.21}};
    return table;
}

std::optional<Material> findMaterial(std::string_view id) {
    const auto& table = materials();
    const auto found =
        std::find_if(table.begin(), table.end(), [id](const Material& m) { return m.id == id; });
    return found == table.end() ? std::nullopt : std::optional<Material>(*found);
}

double massGrams(double volumeMm3, double densityGramsPerCm3) {
    return volumeMm3 / 1000.0 * densityGramsPerCm3; // 1 cm³ = 1000 mm³
}

double filamentLengthMm(double volumeMm3, double filamentDiameterMm) {
    if (!(filamentDiameterMm > 0.0) || !std::isfinite(filamentDiameterMm)) {
        return 0.0;
    }
    const double radius = filamentDiameterMm / 2.0;
    return volumeMm3 / (std::numbers::pi * radius * radius);
}

} // namespace fission::analysis
