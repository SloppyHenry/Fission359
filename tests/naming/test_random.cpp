// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Random trial of AP-0.6: many random changes to small models. The truth for every reference
// comes from a geometric query on the model parameters ("the face of the box that looks up, at
// z = height"; "the side face that stands on the segment from A to B"), never from the naming
// system. Hard criterion: no silently wrong assignment. Soft measures (how often the system
// finds the element by itself) are only reported.
//
// Two kinds of models: a box, and an extruded polygon (sketch). Both get holes and fillets.
// Each chain is reproducible: the seed is printed with every failure. The number of chains can
// be raised with FISSION_NAMING_CHAINS (default 24 per model kind, 10 edits each).
// FISSION_NAMING_VERBOSE=1 lists every unnecessary "needs help" with the candidate scores.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <random>
#include <sstream>

#include "scenario_support.h"

using namespace fission::test::naming;

namespace {

struct Intent {
    enum class Type { boxFace, boxEdge, segmentFace, segmentTopEdge, holeWall, holeRim };
    Type type = Type::boxFace;
    Side a = Side::posZ;
    Side b = Side::posX;
    int segment = 0;  // segment id for the sketch intents
    std::string hole; // feature id for holeWall / holeRim
};

struct HoleInfo {
    Side side = Side::posZ; // box world
    int segment = 0;        // extrude world
};

struct World {
    bool extrude = false;
    BoxDims dims;                    // box world; dz is the extrusion height in the other
    std::vector<SketchPoint> sketch; // extrude world
    Recipe recipe;
    std::map<std::string, std::vector<Intent>> intents; // feature id -> intent per reference
    std::map<std::string, HoleInfo> holes;
    int nextId = 2;
    int nextSegment = 0;

    std::string newId() { return "F" + std::to_string(nextId++); }
};

// The standard distributions are not the same in every standard library; this generator makes
// the chains identical on macOS and Linux (std::mt19937 itself is fully specified).
struct Rng {
    std::mt19937 gen;
    explicit Rng(unsigned seed) : gen(seed) {}
    double uniform(double lo, double hi) {
        return lo + (hi - lo) * (static_cast<double>(gen()) / 4294967296.0);
    }
    int below(int n) { return static_cast<int>(gen() % static_cast<unsigned>(n)); }
};

// ---- geometry of the sides of a box ----

gp_Pnt projectToSide(const gp_Pnt& p, Side side, const BoxDims& d) {
    switch (side) {
    case Side::negX:
        return gp_Pnt(0, p.Y(), p.Z());
    case Side::posX:
        return gp_Pnt(d.dx, p.Y(), p.Z());
    case Side::negY:
        return gp_Pnt(p.X(), 0, p.Z());
    case Side::posY:
        return gp_Pnt(p.X(), d.dy, p.Z());
    case Side::negZ:
        return gp_Pnt(p.X(), p.Y(), 0);
    case Side::posZ:
        return gp_Pnt(p.X(), p.Y(), d.dz);
    }
    return p;
}

// Extent of the face in its two in-plane axes and the thickness behind it.
void faceExtent(Side side, const BoxDims& d, double& eu, double& ev, double& thickness) {
    switch (side) {
    case Side::negX:
    case Side::posX:
        eu = d.dy;
        ev = d.dz;
        thickness = d.dx;
        break;
    case Side::negY:
    case Side::posY:
        eu = d.dx;
        ev = d.dz;
        thickness = d.dy;
        break;
    default:
        eu = d.dx;
        ev = d.dy;
        thickness = d.dz;
        break;
    }
}

gp_Pnt centerOn(Side side, const BoxDims& d, double u, double v) {
    switch (side) {
    case Side::negX:
        return gp_Pnt(0, u, v);
    case Side::posX:
        return gp_Pnt(d.dx, u, v);
    case Side::negY:
        return gp_Pnt(u, 0, v);
    case Side::posY:
        return gp_Pnt(u, d.dy, v);
    case Side::negZ:
        return gp_Pnt(u, v, 0);
    case Side::posZ:
        return gp_Pnt(u, v, d.dz);
    }
    return gp_Pnt();
}

bool opposite(Side a, Side b) {
    return (a == Side::negX && b == Side::posX) || (a == Side::posX && b == Side::negX) ||
           (a == Side::negY && b == Side::posY) || (a == Side::posY && b == Side::negY) ||
           (a == Side::negZ && b == Side::posZ) || (a == Side::posZ && b == Side::negZ);
}

constexpr Side kSides[] = {Side::negX, Side::posX, Side::negY, Side::posY, Side::negZ, Side::posZ};

// ---- geometry of the sketch segments ----

const SketchPoint* segmentStart(const World& w, int segment, const SketchPoint** end) {
    for (std::size_t i = 0; i < w.sketch.size(); ++i) {
        if (w.sketch[i].segment == segment) {
            *end = &w.sketch[(i + 1) % w.sketch.size()];
            return &w.sketch[i];
        }
    }
    return nullptr;
}

// Plane of the side face standing on a segment: a point on it and the outward normal.
bool segmentPlane(const World& w, int segment, gp_Pnt& point, gp_Dir& normal) {
    const SketchPoint* end = nullptr;
    const SketchPoint* start = segmentStart(w, segment, &end);
    if (start == nullptr) {
        return false;
    }
    point = gp_Pnt(start->x, start->y, 0.0);
    normal = gp_Dir(end->y - start->y, -(end->x - start->x), 0.0);
    return true;
}

double segmentLength(const World& w, int segment) {
    const SketchPoint* end = nullptr;
    const SketchPoint* start = segmentStart(w, segment, &end);
    return start == nullptr ? 0.0 : std::hypot(end->x - start->x, end->y - start->y);
}

gp_Pnt pointOnSegment(const World& w, int segment, double t, double z) {
    const SketchPoint* end = nullptr;
    const SketchPoint* start = segmentStart(w, segment, &end);
    return gp_Pnt(start->x + t * (end->x - start->x), start->y + t * (end->y - start->y), z);
}

// Simple polygon: no two non-adjacent segments cross.
bool isSimple(const std::vector<SketchPoint>& pts) {
    const std::size_t n = pts.size();
    const auto cross = [](const SketchPoint& o, const SketchPoint& a, const SketchPoint& b) {
        return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
    };
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = i + 1; j < n; ++j) {
            if (j == i + 1 || (i == 0 && j == n - 1)) {
                continue;
            }
            const SketchPoint& a = pts[i];
            const SketchPoint& b = pts[(i + 1) % n];
            const SketchPoint& c = pts[j];
            const SketchPoint& d = pts[(j + 1) % n];
            if (cross(a, b, c) * cross(a, b, d) < 0.0 && cross(c, d, a) * cross(c, d, b) < 0.0) {
                return false;
            }
        }
    }
    double area = 0.0; // must stay counter-clockwise
    for (std::size_t i = 0; i < n; ++i) {
        area += pts[i].x * pts[(i + 1) % n].y - pts[(i + 1) % n].x * pts[i].y;
    }
    return area > 1.0;
}

// ---- truth ----

const Feature* findFeature(const World& w, const std::string& id) {
    for (const Feature& f : w.recipe) {
        if (f.id == id) {
            return &f;
        }
    }
    return nullptr;
}

bool holeAxis(const World& w, const std::string& id, gp_Pnt& onPlane, gp_Dir& axis,
              double& radius) {
    const Feature* hole = findFeature(w, id);
    if (hole == nullptr) {
        return false;
    }
    radius = hole->radius;
    const HoleInfo& info = w.holes.at(id);
    if (!w.extrude) {
        onPlane = projectToSide(hole->center, info.side, w.dims);
        axis = sideNormal(info.side);
        return true;
    }
    gp_Pnt point;
    gp_Dir normal;
    if (!segmentPlane(w, info.segment, point, normal)) {
        return false; // the segment is gone: the hole has no face to be in
    }
    const double along = gp_Vec(point, hole->center).Dot(gp_Vec(normal));
    onPlane = hole->center.Translated(-along * gp_Vec(normal));
    axis = normal;
    return true;
}

// The elements of `shape` that are geometrically the intended one.
truth::Shapes truthOf(const World& w, const Intent& intent, const TopoDS_Shape& shape) {
    switch (intent.type) {
    case Intent::Type::boxFace:
        return boxFace(shape, w.dims, intent.a);
    case Intent::Type::boxEdge:
        return boxEdge(shape, w.dims, intent.a, intent.b);
    case Intent::Type::segmentFace:
        return segmentFace(shape, w.sketch, intent.segment);
    case Intent::Type::segmentTopEdge:
        return segmentTopEdge(shape, w.sketch, intent.segment, w.dims.dz);
    case Intent::Type::holeWall:
    case Intent::Type::holeRim: {
        gp_Pnt onPlane;
        gp_Dir axis;
        double radius = 0.0;
        if (!holeAxis(w, intent.hole, onPlane, axis, radius)) {
            return {}; // deleted hole or vanished segment: nothing is the right answer
        }
        return intent.type == Intent::Type::holeWall
                   ? truth::cylinderFaces(shape, onPlane, axis, radius)
                   : truth::circleEdges(shape, onPlane, axis, radius);
    }
    }
    return {};
}

// ---- building and changing the model ----

Feature baseFeature(const World& w, const std::string& id) {
    return w.extrude ? makeExtrudeFeature(id, w.sketch, w.dims.dz)
                     : makeBoxFeature(id, w.dims.dx, w.dims.dy, w.dims.dz);
}

Body bodyBefore(const World& w, std::size_t index) {
    Recipe before(w.recipe.begin(), w.recipe.begin() + static_cast<std::ptrdiff_t>(index));
    return evaluate(before).body;
}

// Tries to add a hole at position `index`. Returns false if nothing sensible could be picked.
bool addHole(World& w, Rng& rng, std::size_t index) {
    const Body body = bodyBefore(w, index);
    HoleInfo info;
    Intent faceIntent;
    gp_Pnt center;
    double radius = 0.0;
    double thickness = 0.0;
    truth::Shapes faces;
    if (!w.extrude) {
        info.side = kSides[rng.below(6)];
        faceIntent.type = Intent::Type::boxFace;
        faceIntent.a = info.side;
        faces = truthOf(w, faceIntent, body.shape());
        double eu = 0, ev = 0;
        faceExtent(info.side, w.dims, eu, ev, thickness);
        radius = rng.uniform(1.5, std::min(6.0, 0.4 * std::min(eu, ev)));
        center = centerOn(info.side, w.dims, rng.uniform(radius + 1.0, eu - radius - 1.0),
                          rng.uniform(radius + 1.0, ev - radius - 1.0));
    } else {
        info.segment =
            w.sketch[static_cast<std::size_t>(rng.below(static_cast<int>(w.sketch.size())))]
                .segment;
        faceIntent.type = Intent::Type::segmentFace;
        faceIntent.segment = info.segment;
        faces = truthOf(w, faceIntent, body.shape());
        const double length = segmentLength(w, info.segment);
        radius = rng.uniform(1.5, std::min(5.0, 0.4 * std::min(length, w.dims.dz)));
        const double margin = (radius + 1.0) / length;
        if (margin >= 0.5) {
            return false;
        }
        center = pointOnSegment(w, info.segment, rng.uniform(margin, 1.0 - margin),
                                rng.uniform(radius + 1.0, w.dims.dz - radius - 1.0));
        thickness = 20.0;
    }
    if (faces.size() != 1) {
        return false;
    }
    const double depth = rng.below(3) == 0 ? 0.0 : rng.uniform(2.0, 0.7 * thickness);
    const std::string id = w.newId();
    w.recipe.insert(
        w.recipe.begin() + static_cast<std::ptrdiff_t>(index),
        makeHoleFeature(id, makeReference(body, faces.front(), Kind::face), center, radius, depth));
    w.intents[id] = {faceIntent};
    w.holes[id] = info;
    return true;
}

bool addFillet(World& w, Rng& rng, std::size_t index) {
    const Body body = bodyBefore(w, index);
    Intent intent;
    // Half of the fillets go to a hole rim (if there is one before this position).
    std::vector<std::string> holes;
    for (std::size_t i = 0; i < index; ++i) {
        if (w.recipe[i].type == FeatureType::hole) {
            holes.push_back(w.recipe[i].id);
        }
    }
    if (!holes.empty() && rng.below(2) == 0) {
        intent.type = Intent::Type::holeRim;
        intent.hole = holes[static_cast<std::size_t>(rng.below(static_cast<int>(holes.size())))];
    } else if (!w.extrude) {
        intent.type = Intent::Type::boxEdge;
        intent.a = kSides[rng.below(6)];
        intent.b = kSides[rng.below(6)];
        if (intent.a == intent.b || opposite(intent.a, intent.b)) {
            return false;
        }
    } else {
        intent.type = Intent::Type::segmentTopEdge;
        intent.segment =
            w.sketch[static_cast<std::size_t>(rng.below(static_cast<int>(w.sketch.size())))]
                .segment;
    }
    const truth::Shapes picks = truthOf(w, intent, body.shape());
    if (picks.size() != 1) {
        return false;
    }
    const std::string id = w.newId();
    w.recipe.insert(w.recipe.begin() + static_cast<std::ptrdiff_t>(index),
                    makeFilletFeature(id, {makeReference(body, picks.front(), Kind::edge)},
                                      rng.uniform(0.4, 1.4)));
    w.intents[id] = {intent};
    return true;
}

void syncBase(World& w) {
    w.recipe[0] = baseFeature(w, w.recipe[0].id);
}

// One random change. Returns a short description (for failure messages), empty if nothing
// happened.
std::string randomEdit(World& w, Rng& rng) {
    const int n = static_cast<int>(w.recipe.size());
    switch (rng.below(10)) {
    case 0:
    case 1: { // resize the box / change the height
        const int axis = w.extrude ? 2 : rng.below(3);
        double& value = axis == 0 ? w.dims.dx : (axis == 1 ? w.dims.dy : w.dims.dz);
        value = std::max(12.0, value * rng.uniform(0.7, 1.4));
        syncBase(w);
        return "resize";
    }
    case 2: { // move a hole
        std::vector<std::size_t> holes;
        for (std::size_t i = 0; i < w.recipe.size(); ++i) {
            if (w.recipe[i].type == FeatureType::hole) {
                holes.push_back(i);
            }
        }
        if (holes.empty()) {
            return {};
        }
        Feature& hole =
            w.recipe[holes[static_cast<std::size_t>(rng.below(static_cast<int>(holes.size())))]];
        const HoleInfo& info = w.holes.at(hole.id);
        if (!w.extrude) {
            double eu = 0, ev = 0, t = 0;
            faceExtent(info.side, w.dims, eu, ev, t);
            const double r = std::min(hole.radius, 0.4 * std::min(eu, ev));
            hole.center = centerOn(info.side, w.dims, rng.uniform(r + 1.0, eu - r - 1.0),
                                   rng.uniform(r + 1.0, ev - r - 1.0));
        } else {
            const double length = segmentLength(w, info.segment);
            if (length <= 0.0) {
                return {};
            }
            const double margin = std::min(0.45, (hole.radius + 1.0) / length);
            hole.center = pointOnSegment(w, info.segment, rng.uniform(margin, 1.0 - margin),
                                         rng.uniform(1.0, std::max(2.0, w.dims.dz - 1.0)));
        }
        return "move hole " + hole.id;
    }
    case 3: { // resize a hole or change its depth
        for (int tries = 0; tries < 6; ++tries) {
            Feature& f = w.recipe[static_cast<std::size_t>(rng.below(n))];
            if (f.type != FeatureType::hole) {
                continue;
            }
            if (rng.below(2) == 0) {
                f.radius = rng.uniform(1.5, 6.0);
            } else {
                f.depth = rng.below(2) == 0 ? 0.0 : rng.uniform(2.0, 15.0);
            }
            return "change hole " + f.id;
        }
        return {};
    }
    case 4: { // change a fillet radius
        for (int tries = 0; tries < 6; ++tries) {
            Feature& f = w.recipe[static_cast<std::size_t>(rng.below(n))];
            if (f.type == FeatureType::fillet) {
                f.filletRadius = rng.uniform(0.4, 1.6);
                return "change fillet " + f.id;
            }
        }
        return {};
    }
    case 5: // insert a hole in front of something
        return addHole(w, rng, static_cast<std::size_t>(1 + rng.below(n))) ? "insert hole" : "";
    case 6: // insert a fillet
        return addFillet(w, rng, static_cast<std::size_t>(1 + rng.below(n))) ? "insert fillet" : "";
    case 7: { // delete a feature (not the base)
        if (n < 3) {
            return {};
        }
        const std::size_t index = static_cast<std::size_t>(1 + rng.below(n - 1));
        const std::string id = w.recipe[index].id;
        w.recipe.erase(w.recipe.begin() + static_cast<std::ptrdiff_t>(index));
        return "delete " + id;
    }
    case 8: { // sketch edits (extrude world), otherwise replace the base like below
        if (!w.extrude) {
            break;
        }
        std::vector<SketchPoint> sketch = w.sketch;
        const std::size_t i = static_cast<std::size_t>(rng.below(static_cast<int>(sketch.size())));
        std::string what;
        switch (rng.below(3)) {
        case 0: // move a point
            sketch[i].x += rng.uniform(-6.0, 6.0);
            sketch[i].y += rng.uniform(-6.0, 6.0);
            what = "move sketch point";
            break;
        case 1: { // new point behind point i, a bit off the segment
            const SketchPoint& next = sketch[(i + 1) % sketch.size()];
            const double mx = 0.5 * (sketch[i].x + next.x) + rng.uniform(-5.0, 5.0);
            const double my = 0.5 * (sketch[i].y + next.y) + rng.uniform(-5.0, 5.0);
            insertPoint(sketch, i, mx, my, w.nextSegment++);
            what = "insert sketch point";
            break;
        }
        default: // remove a point
            if (sketch.size() <= 3) {
                return {};
            }
            removePoint(sketch, i);
            what = "remove sketch point";
            break;
        }
        if (!isSimple(sketch)) {
            return {};
        }
        w.sketch = sketch;
        syncBase(w);
        return what;
    }
    default:
        break;
    }
    // Replace the base feature by a new one with the same geometry (new id, new segment ids).
    const std::string id = w.newId();
    if (w.extrude) {
        std::map<int, int> renamed;
        for (SketchPoint& point : w.sketch) {
            const int fresh = w.nextSegment++;
            renamed[point.segment] = fresh;
            point.segment = fresh;
        }
        for (auto& [featureId, intents] : w.intents) {
            for (Intent& intent : intents) {
                if (intent.type == Intent::Type::segmentFace ||
                    intent.type == Intent::Type::segmentTopEdge) {
                    intent.segment = renamed[intent.segment];
                }
            }
        }
        for (auto& [holeId, info] : w.holes) {
            const auto found = renamed.find(info.segment);
            info.segment = found == renamed.end() ? -1 : found->second;
        }
    }
    w.recipe[0] = baseFeature(w, id);
    return "replace base by " + id;
}

const char* groupOf(const World& w, const Feature& f, const Intent& intent) {
    if (f.type == FeatureType::hole) {
        return w.extrude ? "random-sketch-hole-on-side" : "random-box-hole-on-face";
    }
    if (intent.type == Intent::Type::holeRim) {
        return w.extrude ? "random-sketch-fillet-hole-rim" : "random-box-fillet-hole-rim";
    }
    return w.extrude ? "random-sketch-fillet-top-edge" : "random-box-fillet-edge";
}

std::string describe(const std::string& name, const Signature& s) {
    std::ostringstream out;
    out << name << " type " << s.geomType << " at (" << s.centroid.X() << ", " << s.centroid.Y()
        << ", " << s.centroid.Z() << ") r " << s.radius << " size " << s.size << " nb "
        << s.neighbours.size();
    return out.str();
}

struct ChainStats {
    int edits = 0;
    int references = 0;
    int kernelErrors = 0;
};

// `w` is changed: where the system asked for help and there is one clear right element, the
// simulated user picks it again (the real user would do the same when the feature shows
// "needs help"), so one unsafe reference is counted once and not at every later edit.
void judge(World& w, const Evaluation& ev, Strictness mode, unsigned seed, int step,
           const std::string& what, ChainStats& stats, bool requireSafe) {
    const bool verbose = std::getenv("FISSION_NAMING_VERBOSE") != nullptr;
    for (std::size_t i = 0; i < w.recipe.size(); ++i) {
        Feature& f = w.recipe[i];
        const FeatureOutcome& outcome = ev.features[i];
        if (outcome.status == FeatureStatus::error) {
            ++stats.kernelErrors;
        }
        const auto found = w.intents.find(f.id);
        if (f.refs.empty() || found == w.intents.end()) {
            continue;
        }
        for (std::size_t j = 0; j < outcome.refs.size(); ++j) {
            const Intent& intent = found->second[j];
            const truth::Shapes expected = truthOf(w, intent, outcome.input.shape());
            const Verdict verdict = record(groupOf(w, f, intent), mode, outcome.refs[j], expected);
            ++stats.references;
            INFO("seed " << seed << (w.extrude ? " (sketch)" : " (box)") << ", step " << step
                         << " (" << what << "), feature " << f.id << ": " << outcome.refs[j].reason
                         << ", verdict " << toString(verdict) << ", truth elements "
                         << expected.size() << "\n  saved ref: "
                         << describe(f.refs[j].name, f.refs[j].signature) << "\n  chosen: "
                         << (outcome.refs[j].index >= 0
                                 ? describe(outcome.input
                                                .entries(f.refs[j].kind)[static_cast<std::size_t>(
                                                    outcome.refs[j].index)]
                                                .primary(),
                                            outcome.input.signatureOf(f.refs[j].kind,
                                                                      outcome.refs[j].index))
                                 : std::string("-")));
            if (requireSafe) {
                CHECK(verdict != Verdict::silentlyWrong);
            }
            if (verbose && verdict == Verdict::unnecessaryHelp) {
                std::fprintf(stderr, "[%s] seed %u step %d (%s) %s: %s\n  saved: %s\n%s",
                             groupOf(w, f, intent), seed, step, what.c_str(), f.id.c_str(),
                             outcome.refs[j].reason.c_str(),
                             describe(f.refs[j].name, f.refs[j].signature).c_str(),
                             explain(outcome.input, f.refs[j]).c_str());
            }
            if (verdict == Verdict::unnecessaryHelp) {
                f.refs[j] = makeReference(outcome.input, expected.front(), f.refs[j].kind);
            }
        }
    }
}

int chainCount() {
    if (const char* text = std::getenv("FISSION_NAMING_CHAINS")) {
        const int value = std::atoi(text);
        if (value > 0) {
            return value;
        }
    }
    return 24;
}

// `requireSafe`: a silently wrong assignment fails the test (the sensitivity runs only count).

World makeWorld(Rng& rng, bool extrude) {
    World w;
    w.extrude = extrude;
    w.dims = {rng.uniform(30, 80), rng.uniform(25, 60), rng.uniform(14, 40)};
    if (extrude) {
        // A rounded-off rectangle with 4 to 6 points, always simple and counter-clockwise.
        const double wx = w.dims.dx;
        const double wy = w.dims.dy;
        std::vector<SketchPoint> pts = {{0, 0, 0}, {wx, 0, 0}, {wx, wy, 0}, {0, wy, 0}};
        if (rng.below(2) == 0) {
            pts.insert(pts.begin() + 2, {wx + rng.uniform(5, 12), 0.5 * wy, 0});
        }
        if (rng.below(2) == 0) {
            pts.push_back({-rng.uniform(5, 12), 0.5 * wy, 0});
        }
        for (SketchPoint& point : pts) {
            point.segment = w.nextSegment++;
        }
        w.sketch = pts;
    }
    w.recipe.push_back(baseFeature(w, "F1"));
    const int target = 2 + rng.below(3);
    for (int k = 0; k < target * 3 && static_cast<int>(w.recipe.size()) < 1 + target; ++k) {
        if (rng.below(2) == 0) {
            addHole(w, rng, w.recipe.size());
        } else {
            addFillet(w, rng, w.recipe.size());
        }
    }
    return w;
}

void runChains(Strictness mode, bool extrude, bool requireSafe = true, int chains = 0) {
    constexpr int kEdits = 10;
    ChainStats stats;
    for (int chain = 0; chain < (chains > 0 ? chains : chainCount()); ++chain) {
        const unsigned seed = (extrude ? 5000u : 1000u) + static_cast<unsigned>(chain);
        Rng rng(seed);
        World w = makeWorld(rng, extrude);
        judge(w, evaluate(w.recipe, mode), mode, seed, 0, "initial", stats, requireSafe);
        for (int step = 1; step <= kEdits; ++step) {
            const World before = w;
            const std::string what = randomEdit(w, rng);
            if (what.empty()) {
                w = before;
                continue;
            }
            // The references are healed in w.recipe like the document would do.
            Recipe trial = w.recipe;
            const Evaluation ev = evaluate(trial, mode);
            if (ev.features[0].status != FeatureStatus::ok) {
                w = before; // the base itself failed (e.g. a degenerate sketch): not an edit
                continue;
            }
            w.recipe = trial;
            ++stats.edits;
            judge(w, ev, mode, seed, step, what, stats, requireSafe);
        }
    }
    INFO((extrude ? "sketch" : "box")
         << " world: edits " << stats.edits << ", references judged " << stats.references
         << ", kernel errors " << stats.kernelErrors);
    std::printf("random trial (%s model, %s): %d edits, %d references judged, %d kernel refusals\n",
                extrude ? "sketch" : "box", mode == Strictness::strict ? "strict" : "lenient",
                stats.edits, stats.references, stats.kernelErrors);
    REQUIRE(stats.references > 0);
    REQUIRE(stats.edits > 0);
}

} // namespace

TEST_CASE("random: box, strict mode never assigns silently wrong", "[naming][random]") {
    runChains(Strictness::strict, false);
}

TEST_CASE("random: box, lenient mode never assigns silently wrong", "[naming][random]") {
    runChains(Strictness::lenient, false);
}

TEST_CASE("random: sketch, strict mode never assigns silently wrong", "[naming][random]") {
    runChains(Strictness::strict, true);
}

TEST_CASE("random: sketch, lenient mode never assigns silently wrong", "[naming][random]") {
    runChains(Strictness::lenient, true);
}

// How much does each part of the design contribute? The same random chains run with parts
// switched off or with other limits. Only counted, nothing is required here except for the
// default setting. The numbers go into docs/spikes/persistente-referenzen.md.
TEST_CASE("sensitivity: effect of each design decision", "[naming][sensitivity][.]") {
    struct Variant {
        const char* name;
        void (*apply)(Limits&);
        Strictness mode = Strictness::strict;
    };
    const Variant variants[] = {
        {"sens: default", [](Limits&) {}},
        {"sens: without consumed-element memory", [](Limits& l) { l.useTombstones = false; }},
        {"sens: without neighbour recognition", [](Limits& l) { l.recogniseNeighbours = false; }},
        {"sens: without place dominance", [](Limits& l) { l.nearestRatio = 0.0; }},
        {"sens: looser (score 0.70, margin 0.10)",
         [](Limits& l) {
             l.strictScore = 0.70;
             l.strictMargin = 0.10;
         }},
        {"sens: very loose (score 0.45, margin 0.00)",
         [](Limits& l) {
             l.strictScore = 0.45;
             l.strictMargin = 0.0;
         }},
        {"sens: very tight (score 0.97, margin 0.50)",
         [](Limits& l) {
             l.strictScore = 0.97;
             l.strictMargin = 0.50;
         }},
        {"sens: tighter (score 0.90, margin 0.30)",
         [](Limits& l) {
             l.strictScore = 0.90;
             l.strictMargin = 0.30;
         }},
        {"sens: generous mode (default limits)", [](Limits&) {}, Strictness::lenient},
        {"sens: generous mode, wide (score 0.35, margin 0.00)",
         [](Limits& l) {
             l.lenientScore = 0.35;
             l.lenientMargin = 0.0;
         },
         Strictness::lenient},
        {"sens: turn limit 25 degrees", [](Limits& l) { l.plausibleDegrees = 25.0; }},
        {"sens: turn limit 90 degrees", [](Limits& l) { l.plausibleDegrees = 90.0; }},
    };
    const Limits saved = limits();
    for (const Variant& variant : variants) {
        limits() = saved;
        variant.apply(limits());
        setGroupOverride(variant.name);
        const bool isDefault = std::string(variant.name) == "sens: default";
        runChains(variant.mode, false, isDefault);
        runChains(variant.mode, true, isDefault);
    }
    setGroupOverride({});
    limits() = saved;
}
