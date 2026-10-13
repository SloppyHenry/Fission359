// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "naming.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <set>
#include <utility>

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <GeomAbs_CurveType.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <NCollection_IndexedDataMap.hxx>
#include <NCollection_IndexedMap.hxx>
#include <NCollection_List.hxx>
#include <TopAbs_Orientation.hxx>
#include <TopExp.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <TopoDS.hxx>

namespace fission::test::naming {

namespace {

constexpr double kPi = 3.14159265358979323846;

const std::string kEmpty;

using ShapeMap = NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher>;

gp_Dir canonicalAxis(const gp_Dir& dir) {
    constexpr double eps = 1e-9;
    if (dir.X() < -eps || (std::abs(dir.X()) <= eps && dir.Y() < -eps) ||
        (std::abs(dir.X()) <= eps && std::abs(dir.Y()) <= eps && dir.Z() < 0.0)) {
        return dir.Reversed();
    }
    return dir;
}

std::string joinNames(const std::vector<std::string>& names, char separator) {
    std::string text;
    for (const std::string& name : names) {
        if (!text.empty()) {
            text += separator;
        }
        text += name;
    }
    return text;
}

std::vector<std::string> sortedUnique(std::vector<std::string> names) {
    std::sort(names.begin(), names.end());
    names.erase(std::unique(names.begin(), names.end()), names.end());
    return names;
}

} // namespace

Limits& limits() {
    static Limits current;
    return current;
}

// ---------------------------------------------------------------------------------------------
// Entry

const std::string& Entry::primary() const {
    return names.empty() ? kEmpty : names.front();
}

bool Entry::hasName(const std::string& name) const {
    return std::find(names.begin(), names.end(), name) != names.end();
}

void Entry::addName(const std::string& name) {
    if (name.empty() || hasName(name)) {
        return;
    }
    names.push_back(name);
    std::sort(names.begin(), names.end());
}

// ---------------------------------------------------------------------------------------------
// Body

Body::Body(const TopoDS_Shape& shape) : shape_(shape) {
    if (shape_.IsNull()) {
        return;
    }
    ShapeMap faceMap;
    ShapeMap edgeMap;
    TopExp::MapShapes(shape_, TopAbs_FACE, faceMap);
    TopExp::MapShapes(shape_, TopAbs_EDGE, edgeMap);
    for (int i = 1; i <= faceMap.Extent(); ++i) {
        faces_.push_back({faceMap(i), {}});
    }
    for (int i = 1; i <= edgeMap.Extent(); ++i) {
        edges_.push_back({edgeMap(i), {}});
    }

    edgeFaces_.assign(edges_.size(), {});
    faceEdges_.assign(faces_.size(), {});
    NCollection_IndexedDataMap<TopoDS_Shape, NCollection_List<TopoDS_Shape>,
                               TopTools_ShapeMapHasher>
        ancestors;
    TopExp::MapShapesAndAncestors(shape_, TopAbs_EDGE, TopAbs_FACE, ancestors);
    for (int e = 0; e < static_cast<int>(edges_.size()); ++e) {
        const int slot = ancestors.FindIndex(edges_[e].shape);
        if (slot == 0) {
            continue;
        }
        for (const TopoDS_Shape& face : ancestors(slot)) {
            const int f = index(Kind::face, face);
            if (f >= 0 &&
                std::find(edgeFaces_[e].begin(), edgeFaces_[e].end(), f) == edgeFaces_[e].end()) {
                edgeFaces_[e].push_back(f);
                faceEdges_[f].push_back(e);
            }
        }
    }

    Bnd_Box box;
    BRepBndLib::AddOptimal(shape_, box, /*useTriangulation=*/false, /*useShapeTolerance=*/false);
    if (!box.IsVoid()) {
        double x0 = 0.0, y0 = 0.0, z0 = 0.0, x1 = 0.0, y1 = 0.0, z1 = 0.0;
        box.Get(x0, y0, z0, x1, y1, z1);
        diagonal_ =
            std::sqrt((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0) + (z1 - z0) * (z1 - z0));
    }
}

const std::vector<Entry>& Body::entries(Kind kind) const {
    return kind == Kind::face ? faces_ : edges_;
}

int Body::index(Kind kind, const TopoDS_Shape& sub) const {
    const std::vector<Entry>& list = entries(kind);
    for (int i = 0; i < static_cast<int>(list.size()); ++i) {
        if (list[i].shape.IsSame(sub)) {
            return i;
        }
    }
    return -1;
}

std::vector<int> Body::find(Kind kind, const std::string& name) const {
    std::vector<int> hits;
    if (name.empty()) {
        return hits;
    }
    const std::vector<Entry>& list = entries(kind);
    for (int i = 0; i < static_cast<int>(list.size()); ++i) {
        if (list[i].hasName(name)) {
            hits.push_back(i);
        }
    }
    return hits;
}

std::vector<std::string> Body::neighbourNames(Kind kind, int index) const {
    std::vector<std::string> names;
    if (kind == Kind::edge) {
        for (const int f : edgeFaces_[index]) {
            if (!faces_[f].names.empty()) {
                names.push_back(faces_[f].primary());
            }
        }
    } else {
        for (const int e : faceEdges_[index]) {
            for (const int f : edgeFaces_[e]) {
                if (f != index && !faces_[f].names.empty()) {
                    names.push_back(faces_[f].primary());
                }
            }
        }
    }
    return sortedUnique(std::move(names));
}

void Body::deriveNames(const std::string& featureId) {
    // Faces first (a new fillet corner patch, a face the history knows nothing about) ...
    std::vector<std::string> derived(faces_.size());
    for (int f = 0; f < static_cast<int>(faces_.size()); ++f) {
        if (!faces_[f].names.empty()) {
            continue;
        }
        const std::vector<std::string> around = neighbourNames(Kind::face, f);
        if (!around.empty()) {
            derived[f] = featureId + "/face[" + joinNames(around, '|') + "]";
        }
    }
    for (int f = 0; f < static_cast<int>(faces_.size()); ++f) {
        faces_[f].addName(derived[f]);
    }
    // ... then edges: an edge is named after the faces it joins.
    for (int e = 0; e < static_cast<int>(edges_.size()); ++e) {
        std::vector<std::string> around;
        bool complete = !edgeFaces_[e].empty();
        for (const int f : edgeFaces_[e]) {
            if (faces_[f].names.empty()) {
                complete = false;
            } else {
                around.push_back(faces_[f].primary());
            }
        }
        edges_[e].names.clear();
        if (complete) {
            edges_[e].addName("edge[" + joinNames(sortedUnique(std::move(around)), '|') + "]");
        }
    }
}

Signature Body::signatureOf(Kind kind, int idx) const {
    Signature sig;
    sig.kind = kind;
    const TopoDS_Shape& shape = entries(kind)[idx].shape;
    GProp_GProps props;
    if (kind == Kind::face) {
        const TopoDS_Face face = TopoDS::Face(shape);
        BRepGProp::SurfaceProperties(face, props);
        sig.centroid = props.CentreOfMass();
        sig.size = props.Mass();
        const BRepAdaptor_Surface surface(face);
        sig.geomType = static_cast<int>(surface.GetType());
        if (surface.GetType() == GeomAbs_Plane) {
            gp_Dir normal = surface.Plane().Axis().Direction();
            if (face.Orientation() == TopAbs_REVERSED) {
                normal.Reverse();
            }
            sig.direction = normal;
            sig.hasDirection = true;
            sig.directed = true;
        } else if (surface.GetType() == GeomAbs_Cylinder) {
            sig.direction = canonicalAxis(surface.Cylinder().Axis().Direction());
            sig.hasDirection = true;
            sig.radius = surface.Cylinder().Radius();
        }
    } else {
        const TopoDS_Edge edge = TopoDS::Edge(shape);
        BRepGProp::LinearProperties(edge, props);
        sig.centroid = props.CentreOfMass();
        sig.size = props.Mass();
        const BRepAdaptor_Curve curve(edge);
        sig.geomType = static_cast<int>(curve.GetType());
        if (curve.GetType() == GeomAbs_Line) {
            sig.direction = canonicalAxis(curve.Line().Direction());
            sig.hasDirection = true;
        } else if (curve.GetType() == GeomAbs_Circle) {
            sig.direction = canonicalAxis(curve.Circle().Axis().Direction());
            sig.hasDirection = true;
            sig.radius = curve.Circle().Radius();
        }
    }
    sig.neighbours = neighbourNames(kind, idx);
    return sig;
}

Reference makeReference(const Body& body, const TopoDS_Shape& sub, Kind kind) {
    Reference ref;
    ref.kind = kind;
    const int idx = body.index(kind, sub);
    if (idx < 0) {
        return ref;
    }
    ref.name = body.entries(kind)[idx].primary();
    ref.signature = body.signatureOf(kind, idx);
    return ref;
}

// ---------------------------------------------------------------------------------------------
// Resolution

namespace {

double angleBetweenDegrees(const Signature& a, const Signature& b) {
    if (!a.hasDirection || !b.hasDirection) {
        return 0.0;
    }
    double cosine = a.direction.X() * b.direction.X() + a.direction.Y() * b.direction.Y() +
                    a.direction.Z() * b.direction.Z();
    cosine = std::clamp(cosine, -1.0, 1.0);
    double degrees = std::acos(cosine) * 180.0 / kPi;
    if (!(a.directed && b.directed)) {
        degrees = std::min(degrees, 180.0 - degrees); // axes have no sign
    }
    return degrees;
}

double ratio(double a, double b) {
    if (a <= 0.0 || b <= 0.0) {
        return 1.0;
    }
    return std::min(a, b) / std::max(a, b);
}

// 1 = same place, 0 = further away than `positionRadiusFactor` of the body diagonal.
double positionScore(const Signature& a, const Signature& b, double diagonal) {
    const double scale = std::max(limits().positionRadiusFactor * diagonal, 1e-9);
    return std::max(0.0, 1.0 - a.centroid.Distance(b.centroid) / scale);
}

struct Similarity {
    bool comparable = false; // same kind and geometry type, direction within the gate
    double total = 0.0;
};

Similarity similarity(const Signature& ref, const Signature& cand, double diagonal,
                      const std::set<std::string>& liveNames) {
    Similarity result;
    if (ref.kind != cand.kind || ref.geomType != cand.geomType) {
        return result;
    }
    const double angle = angleBetweenDegrees(ref, cand);
    if (angle > limits().directionGateDegrees) {
        return result;
    }
    if (ref.radius > 0.0 && cand.radius > 0.0 && ratio(ref.radius, cand.radius) < 0.8) {
        return result;
    }
    result.comparable = true;

    const double position = positionScore(ref, cand, diagonal);
    const double direction = 1.0 - angle / std::max(limits().directionGateDegrees, 1e-9);
    const double size =
        0.5 * ratio(ref.size, cand.size) + 0.5 * ((ref.radius > 0.0 && cand.radius > 0.0)
                                                      ? ratio(ref.radius, cand.radius)
                                                      : ratio(ref.size, cand.size));

    // Neighbours count only as far as their names still exist; if a whole feature was replaced
    // the old names are gone and carry no information (neutral score).
    std::vector<std::string> expected;
    for (const std::string& name : ref.neighbours) {
        if (liveNames.count(name) != 0) {
            expected.push_back(name);
        }
    }
    double neighbours = 0.75;
    if (!expected.empty()) {
        std::vector<std::string> common;
        std::set_intersection(expected.begin(), expected.end(), cand.neighbours.begin(),
                              cand.neighbours.end(), std::back_inserter(common));
        std::vector<std::string> all;
        std::set_union(expected.begin(), expected.end(), cand.neighbours.begin(),
                       cand.neighbours.end(), std::back_inserter(all));
        neighbours = static_cast<double>(common.size()) / static_cast<double>(all.size());
    }
    result.total = 0.35 * position + 0.15 * direction + 0.20 * size + 0.30 * neighbours;
    return result;
}

// How much `cand` looks like the original *by place and direction only* (used to detect that
// two equal elements swapped places; neighbours and size are allowed to change).
double placeLook(const Signature& ref, const Signature& cand, double diagonal) {
    if (ref.kind != cand.kind || ref.geomType != cand.geomType) {
        return 0.0;
    }
    const double angle = angleBetweenDegrees(ref, cand);
    if (angle > limits().directionGateDegrees) {
        return 0.0;
    }
    return 0.7 * positionScore(ref, cand, diagonal) +
           0.3 * ((ref.radius > 0.0 && cand.radius > 0.0) ? ratio(ref.radius, cand.radius) : 1.0);
}

std::set<std::string> liveNamesOf(const Body& body) {
    std::set<std::string> names;
    for (const Entry& face : body.faces()) {
        names.insert(face.names.begin(), face.names.end());
    }
    return names;
}

struct Ranked {
    int index = -1;
    double score = 0.0;
};

// Best and second best comparable candidate among `pool` (all entries when empty).
void rank(const Body& body, const Reference& ref, const std::vector<int>& pool, Ranked& best,
          Ranked& second) {
    const std::set<std::string> live = liveNamesOf(body);
    const std::vector<Entry>& entries = body.entries(ref.kind);
    const auto consider = [&](int i) {
        const Signature cand = body.signatureOf(ref.kind, i);
        const Similarity sim = similarity(ref.signature, cand, body.diagonal(), live);
        if (!sim.comparable) {
            return;
        }
        if (sim.total > best.score) {
            second = best;
            best = {i, sim.total};
        } else if (sim.total > second.score) {
            second = {i, sim.total};
        }
    };
    if (pool.empty()) {
        for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
            consider(i);
        }
    } else {
        for (const int i : pool) {
            consider(i);
        }
    }
}

Resolution help(std::string reason) {
    Resolution r;
    r.outcome = Outcome::needsHelp;
    r.reason = std::move(reason);
    return r;
}

Resolution take(const Body& body, Kind kind, int idx, Outcome outcome, double score,
                std::string reason) {
    Resolution r;
    r.outcome = outcome;
    r.shape = body.entries(kind)[idx].shape;
    r.index = idx;
    r.score = score;
    r.reason = std::move(reason);
    return r;
}

// Picks the winner of a similarity search under the rules of `strictness`.
Resolution decide(const Body& body, const Reference& ref, const std::vector<int>& pool,
                  Strictness strictness, const std::string& context) {
    Ranked best;
    Ranked second;
    rank(body, ref, pool, best, second);
    if (best.index < 0) {
        return help(context + ": nothing similar enough");
    }
    const Limits& lim = limits();
    const bool strictOk =
        best.score >= lim.strictScore && best.score - second.score >= lim.strictMargin;
    if (strictOk) {
        return take(body, ref.kind, best.index, Outcome::fallback, best.score,
                    context + ": clear winner by similarity");
    }
    const bool lenientOk =
        best.score >= lim.lenientScore && best.score - second.score >= lim.lenientMargin;
    if (strictness == Strictness::lenient && lenientOk) {
        Resolution r = take(body, ref.kind, best.index, Outcome::fallback, best.score,
                            context + ": probable match, please check");
        r.checkPlease = true;
        return r;
    }
    return help(context + ": no clear winner");
}

} // namespace

Resolution resolve(const Body& body, const Reference& ref, Strictness strictness) {
    if (body.isNull()) {
        return help("no body");
    }
    const std::vector<int> hits = body.find(ref.kind, ref.name);

    if (hits.size() == 1) {
        const int idx = hits.front();
        const Signature now = body.signatureOf(ref.kind, idx);
        if (now.geomType != ref.signature.geomType) {
            return help("the name now points to a different kind of surface or curve");
        }
        if (angleBetweenDegrees(ref.signature, now) > limits().plausibleDegrees) {
            return help("the name points to an element that turned around");
        }
        // Name and look must not contradict each other: if another equal element sits where the
        // original was and the named one moved away, the two probably changed places.
        const double named = placeLook(ref.signature, now, body.diagonal());
        const std::vector<Entry>& entries = body.entries(ref.kind);
        for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
            if (i == idx) {
                continue;
            }
            const double other =
                placeLook(ref.signature, body.signatureOf(ref.kind, i), body.diagonal());
            if (other >= 0.9 && named <= 0.5 && other - named >= 0.4) {
                return help("the name points away from where the element used to be, and an "
                            "equal element sits there now (places swapped?)");
            }
        }
        return take(body, ref.kind, idx, Outcome::found, 1.0, "by name");
    }

    if (hits.size() > 1) {
        // The name is not unique any more: the element was split.
        if (strictness == Strictness::strict) {
            return help("the name now matches several elements (it was split)");
        }
        Resolution r = decide(body, ref, hits, strictness, "split element");
        return r;
    }

    // Name unknown (the feature that created it was replaced, or the element vanished).
    return decide(body, ref, {}, strictness, "name not found");
}

void refresh(Reference& reference, const Body& body, const Resolution& resolution) {
    if (resolution.index < 0 || resolution.checkPlease) {
        return;
    }
    const std::vector<Entry>& entries = body.entries(reference.kind);
    reference.name = entries[resolution.index].primary();
    reference.signature = body.signatureOf(reference.kind, resolution.index);
}

} // namespace fission::test::naming
