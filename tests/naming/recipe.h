// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Four mini features for the naming prototype (AP-0.6): box, extruded polyline, hole, fillet.
// Each one names what it creates and passes the names on through the kernel history. A recipe
// is a list of features; evaluating it recomputes everything from scratch, like the real
// recompute engine will (docs/03-architektur.md section 3.3), but without cache or threads.

#pragma once

#include <string>
#include <vector>

#include "naming.h"

namespace fission::test::naming {

enum class FeatureType { box, extrude, hole, fillet };

enum class FeatureStatus {
    ok,
    needsHelp, // a reference could not be resolved safely: the feature is not applied
    error,     // the kernel refused (or the input was unusable); the feature is not applied
};

// A point of the sketch outline. `segment` is the stable id of the segment that starts here
// (up to the next point, the outline is closed).
struct SketchPoint {
    double x = 0.0;
    double y = 0.0;
    int segment = 0;
};

struct Feature {
    std::string id; // stable and never reused: "F1", "F2" ...
    FeatureType type = FeatureType::box;

    // box: corner `origin`, size dx, dy, dz. extrude: sketch plane z = origin.z, +Z by `height`.
    gp_Pnt origin{0.0, 0.0, 0.0};
    double dx = 0.0;
    double dy = 0.0;
    double dz = 0.0;
    std::vector<SketchPoint> sketch;
    double height = 0.0;

    // hole: drilled into the referenced planar face at `center` (projected onto the face),
    // `depth` <= 0 means through.
    gp_Pnt center{0.0, 0.0, 0.0};
    double radius = 0.0;
    double depth = 0.0;

    // fillet: all edges get the same radius.
    double filletRadius = 0.0;

    // hole: one face. fillet: the edges.
    std::vector<Reference> refs;
};

using Recipe = std::vector<Feature>;

Feature makeBoxFeature(const std::string& id, double dx, double dy, double dz,
                       const gp_Pnt& origin = gp_Pnt(0.0, 0.0, 0.0));
Feature makeExtrudeFeature(const std::string& id, std::vector<SketchPoint> sketch, double height,
                           double z = 0.0);
Feature makeHoleFeature(const std::string& id, const Reference& face, const gp_Pnt& center,
                        double radius, double depth = 0.0);
Feature makeFilletFeature(const std::string& id, std::vector<Reference> edges, double radius);

struct FeatureOutcome {
    FeatureStatus status = FeatureStatus::ok;
    std::string message;
    Body input;                   // the body this feature was applied to (null for the first)
    std::vector<Resolution> refs; // one per reference of the feature
};

struct Evaluation {
    Body body;                            // result after the last feature
    std::vector<FeatureOutcome> features; // one per feature of the recipe
};

// Recomputes the recipe. References that were resolved safely are refreshed in `recipe`
// (they then point to the current name and look), as the document will do after a recompute.
Evaluation evaluate(Recipe& recipe, Strictness strictness = Strictness::strict);

// Convenience for tests: the current body after evaluating `upTo` features (all if negative).
Body bodyAfter(Recipe recipe, int upTo = -1, Strictness strictness = Strictness::strict);

} // namespace fission::test::naming
