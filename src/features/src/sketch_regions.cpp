// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/features/sketch_regions.h"

#include <algorithm>
#include <map>

namespace fission::features {

namespace {

using core::SketchElement;
using core::SketchElementKind;

geo::SketchCurve curveOf(const SketchElement& element) {
    geo::SketchCurve curve;
    switch (element.kind) {
    case SketchElementKind::line:
        curve.kind = geo::SketchCurveKind::line;
        break;
    case SketchElementKind::circle:
        curve.kind = geo::SketchCurveKind::circle;
        break;
    case SketchElementKind::arc:
        curve.kind = geo::SketchCurveKind::arc;
        break;
    case SketchElementKind::point:
    case SketchElementKind::spline:
        curve.kind = geo::SketchCurveKind::spline;
        break;
    }
    for (const core::SketchPoint& point : element.points) {
        curve.points.push_back({point.u, point.v});
    }
    return curve;
}

bool insidePolygon(const std::vector<geo::Point2d>& polygon, const core::SketchPoint& at) {
    bool inside = false;
    for (std::size_t i = 0; i + 1 < polygon.size(); ++i) {
        const geo::Point2d& a = polygon[i];
        const geo::Point2d& b = polygon[i + 1];
        if ((a.y <= at.v) != (b.y <= at.v) &&
            at.u < a.x + (at.v - a.y) / (b.y - a.y) * (b.x - a.x)) {
            inside = !inside;
        }
    }
    return inside;
}

// Letters of the sides of each element, for the elements in `names` order.
std::string sidesOf(const geo::SketchRegion& shape, const std::vector<std::size_t>& curveOfName,
                    std::size_t names) {
    std::vector<int> seen(names, 0); // bit 1: left, bit 2: right
    const auto note = [&](const geo::SketchRegionLoop& loop) {
        for (const geo::SketchRegionPiece& piece : loop.pieces) {
            for (const geo::SketchRegionSide& side : piece.sides) {
                const auto it = std::ranges::find(curveOfName, side.curve);
                if (it != curveOfName.end()) {
                    seen[static_cast<std::size_t>(it - curveOfName.begin())] |= side.left ? 1 : 2;
                }
            }
        }
    };
    note(shape.outer);
    for (const geo::SketchRegionLoop& hole : shape.holes) {
        note(hole);
    }
    std::string letters;
    for (const int bits : seen) {
        letters += bits == 1 ? 'l' : bits == 2 ? 'r' : 'b';
    }
    return letters;
}

} // namespace

SketchCurves sketchCurvesOf(const std::vector<SketchElement>& elements) {
    SketchCurves out;
    for (const SketchElement& element : elements) {
        if (element.kind == SketchElementKind::point || element.construction) {
            continue;
        }
        out.curves.push_back(curveOf(element));
        out.ids.push_back(element.id);
    }
    return out;
}

SketchRegionSet computeSketchRegions(const geo::GeometryKernel& kernel,
                                     const std::vector<SketchElement>& elements) {
    const SketchCurves built = sketchCurvesOf(elements);
    const std::vector<geo::SketchCurve>& curves = built.curves;
    const std::vector<std::string>& ids = built.ids; // id of curve i
    SketchRegionSet set;
    if (curves.empty()) {
        return set;
    }
    geo::SketchRegionResult found = kernel.sketchRegions(curves);
    set.rejected = found.rejected;

    struct Draft {
        SketchRegionInfo info;
        std::vector<std::size_t> curveOfName; // for each name, one curve index
        std::string base;
    };
    std::vector<Draft> drafts;
    for (geo::SketchRegion& shape : found.regions) {
        Draft draft;
        std::map<std::string, std::size_t> names; // id -> curve
        const auto collect = [&](const geo::SketchRegionLoop& loop) {
            for (const geo::SketchRegionPiece& piece : loop.pieces) {
                for (const geo::SketchRegionSide& side : piece.sides) {
                    names.emplace(ids[side.curve], side.curve);
                }
            }
        };
        collect(shape.outer);
        for (const geo::SketchRegionLoop& hole : shape.holes) {
            collect(hole);
        }
        for (const auto& [name, curve] : names) {
            draft.info.elements.push_back(name);
            draft.curveOfName.push_back(curve);
            draft.base += (draft.base.empty() ? "" : "+") + name;
        }
        // the sides are per curve index; elements are unique per curve, so this is per element
        draft.info.id = draft.base;
        draft.info.shape = std::move(shape);
        drafts.push_back(std::move(draft));
    }
    std::map<std::string, int> shared;
    for (const Draft& draft : drafts) {
        ++shared[draft.base];
    }
    std::map<std::string, int> used;
    for (Draft& draft : drafts) {
        if (shared[draft.base] > 1) {
            draft.info.id = draft.base + "/" +
                            sidesOf(draft.info.shape, draft.curveOfName, draft.curveOfName.size());
        }
        ++used[draft.info.id];
    }
    std::map<std::string, int> counter;
    for (Draft& draft : drafts) {
        if (used[draft.info.id] > 1 && ++counter[draft.info.id] > 1) {
            draft.info.id += "#" + std::to_string(counter[draft.info.id]);
        }
    }
    for (Draft& draft : drafts) {
        set.regions.push_back(std::move(draft.info));
    }
    return set;
}

bool regionContains(const geo::SketchRegion& region, const core::SketchPoint& at) {
    if (!insidePolygon(region.outer.polygon, at)) {
        return false;
    }
    return std::ranges::none_of(region.holes, [&](const geo::SketchRegionLoop& hole) {
        return insidePolygon(hole.polygon, at);
    });
}

int regionAt(const SketchRegionSet& set, const core::SketchPoint& at) {
    int best = -1;
    for (std::size_t i = 0; i < set.regions.size(); ++i) {
        if (regionContains(set.regions[i].shape, at) &&
            (best < 0 ||
             set.regions[i].shape.area < set.regions[static_cast<std::size_t>(best)].shape.area)) {
            best = static_cast<int>(i);
        }
    }
    return best;
}

} // namespace fission::features
