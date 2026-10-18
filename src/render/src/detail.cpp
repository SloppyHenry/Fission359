// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/render/detail.h"

#include <algorithm>
#include <cmath>

namespace fission::render {

namespace {

constexpr double kBaseChord = 0.01;         // mm, also what the export uses
constexpr double kBaseAngle = 0.0872664626; // 5°
constexpr double kWantedErrorPixels = 0.25;
constexpr double kHysteresis = 1.35; // going finer at x, coarser below x / 1.35

int levelForScale(double pixelsPerMillimetre) {
    if (pixelsPerMillimetre <= 0.0) {
        return 0;
    }
    // chord(level) * pixelsPerMillimetre <= wanted error
    const double level =
        std::ceil(std::log2(kBaseChord * pixelsPerMillimetre / kWantedErrorPixels));
    return std::clamp(static_cast<int>(level), 0, kMaxDetailLevel);
}

} // namespace

geo::TessellationParams tessellationForDetail(int level, double size) {
    const int clamped = std::clamp(level, 0, kMaxDetailLevel);
    geo::TessellationParams params;
    const double base =
        std::isfinite(size) ? std::max(kBaseChord, kRelativeChord * size) : kBaseChord;
    params.chordTolerance = base / std::exp2(clamped);
    // The angle shrinks more slowly: it decides how many segments a whole circle gets.
    params.angleTolerance = kBaseAngle / std::exp2(0.5 * clamped);
    return params;
}

int detailLevelFor(float pixelsPerMillimetre, int current) {
    const int wanted = levelForScale(static_cast<double>(pixelsPerMillimetre));
    if (wanted >= current) {
        return wanted;
    }
    // Coarser only when the scale is clearly below the one the current level was chosen for.
    const int stillOk = levelForScale(static_cast<double>(pixelsPerMillimetre) * kHysteresis);
    return std::max(wanted, std::min(current, stillOk));
}

} // namespace fission::render
