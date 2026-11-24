// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/features/hole_tables.h"

#include <array>
#include <numbers>

namespace fission::features {

namespace {

// Typed from ISO 273 / ISO 4762 / ISO 10642 / ISO 4032 (see hole_tables.h).
constexpr std::array<MetricScrew, 9> kMetricScrews{{
    // size    nominal pitch tapDrill fine medium coarse headDk headK cskDk  nutS  nutM
    {"M2", 2.0, 0.4, 1.6, 2.2, 2.4, 2.6, 3.8, 2.0, 4.4, 4.0, 1.6},
    {"M2.5", 2.5, 0.45, 2.05, 2.7, 2.9, 3.1, 4.5, 2.5, 5.5, 5.0, 2.0},
    {"M3", 3.0, 0.5, 2.5, 3.2, 3.4, 3.6, 5.5, 3.0, 6.72, 5.5, 2.4},
    {"M4", 4.0, 0.7, 3.3, 4.3, 4.5, 4.8, 7.0, 4.0, 8.96, 7.0, 3.2},
    {"M5", 5.0, 0.8, 4.2, 5.3, 5.5, 5.8, 8.5, 5.0, 11.2, 8.0, 4.7},
    {"M6", 6.0, 1.0, 5.0, 6.4, 6.6, 7.0, 10.0, 6.0, 13.44, 10.0, 5.2},
    {"M8", 8.0, 1.25, 6.8, 8.4, 9.0, 10.0, 13.0, 8.0, 17.92, 13.0, 6.8},
    {"M10", 10.0, 1.5, 8.5, 10.5, 11.0, 12.0, 16.0, 10.0, 22.4, 16.0, 8.4},
    {"M12", 12.0, 1.75, 10.2, 13.0, 13.5, 14.5, 18.0, 12.0, 26.88, 18.0, 10.8},
}};

} // namespace

std::span<const MetricScrew> metricScrews() {
    return kMetricScrews;
}

const MetricScrew* findMetricScrew(std::string_view size) {
    for (const MetricScrew& screw : kMetricScrews) {
        if (screw.size == size) {
            return &screw;
        }
    }
    return nullptr;
}

double clearanceDiameter(const MetricScrew& screw, ClearanceFit fit) {
    switch (fit) {
    case ClearanceFit::fine:
        return screw.clearanceFine;
    case ClearanceFit::medium:
        return screw.clearanceMedium;
    case ClearanceFit::coarse:
        return screw.clearanceCoarse;
    }
    return screw.clearanceMedium; // unreachable: the enum has no other values
}

double hexCornerWidth(double acrossFlats) {
    return acrossFlats / (std::numbers::sqrt3 / 2.0); // s / cos(30°)
}

} // namespace fission::features
