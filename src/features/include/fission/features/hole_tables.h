// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <span>
#include <string_view>

namespace fission::features {

/// Clearance fit classes for through holes (ISO 273): fine, medium, coarse.
enum class ClearanceFit { fine, medium, coarse };

/// Tabulated dimensions of a metric screw and its mating hole features, all in mm.
struct MetricScrew {
    std::string_view size;                                  // "M3"
    double nominal;                                         // 3.0 mm
    double pitch;                                           // coarse thread pitch, mm
    double tapDrill;                                        // core hole for cutting a thread, mm
    double clearanceFine, clearanceMedium, clearanceCoarse; // ISO 273, mm
    double socketHeadDiameter, socketHeadHeight;            // ISO 4762 dk max, k max
    double countersinkDiameter;                             // ISO 10642 dk theoretical max, 90°
    double nutWidth, nutHeight;                             // ISO 4032 s, m max
};

/// All tabulated metric screws, sorted by nominal diameter.
/// Sources: ISO 273 (clearance holes), ISO 4762 (socket head cap screws), ISO 10642
/// (countersunk flat head screws), ISO 4032 (hexagon nuts); the tap drill follows
/// ISO 261/ISO 2306 as nominal − pitch, rounded as commonly tabulated. The values
/// are facts typed from those tables (decision AP-2.7, Haltepunkt 1).
std::span<const MetricScrew> metricScrews();

/// Looks up a screw by its size label, e.g. "M3"; nullptr if unknown.
const MetricScrew* findMetricScrew(std::string_view size);

/// Diameter of the clearance through hole for the given fit, mm.
double clearanceDiameter(const MetricScrew& screw, ClearanceFit fit);

/// Countersink angle behind the flat-head values, in degrees.
inline constexpr double kCountersinkAngleDegrees = 90.0;

/// Corner-to-corner width of a hexagon with width across flats `s`: s / cos(30°).
double hexCornerWidth(double acrossFlats);

} // namespace fission::features
