// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/interaction/align.h"

#include <algorithm>

namespace fission::interaction {

std::vector<Vec3> alignOffsets(const std::vector<Box3>& boxes, int axis, AlignTo to) {
    std::vector<Vec3> offsets(boxes.size());
    if (boxes.size() < 2 || axis < 0 || axis > 2) {
        return offsets;
    }
    const auto a = static_cast<std::size_t>(axis);
    double low = boxes.front().low[a];
    double high = boxes.front().high[a];
    for (const Box3& box : boxes) {
        low = std::min(low, box.low[a]);
        high = std::max(high, box.high[a]);
    }
    for (std::size_t i = 0; i < boxes.size(); ++i) {
        const Box3& box = boxes[i];
        double by = 0.0;
        switch (to) {
        case AlignTo::min:
            by = low - box.low[a];
            break;
        case AlignTo::max:
            by = high - box.high[a];
            break;
        case AlignTo::centre:
            by = (low + high) / 2.0 - (box.low[a] + box.high[a]) / 2.0;
            break;
        }
        offsets[i][a] = cleanedCoordinate(by);
    }
    return offsets;
}

} // namespace fission::interaction
