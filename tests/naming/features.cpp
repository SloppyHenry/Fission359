// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "features.h"

#include <cmath>
#include <utility>

#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeShape.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <GProp_GProps.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <NCollection_List.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_Orientation.hxx>
#include <TopoDS.hxx>
#include <gp_Ax2.hxx>
#include <gp_Vec.hxx>

namespace fission::test::naming {

Feature makeBoxFeature(const std::string& id, double dx, double dy, double dz,
                       const gp_Pnt& origin) {
    Feature f;
    f.id = id;
    f.type = FeatureType::box;
    f.dx = dx;
    f.dy = dy;
    f.dz = dz;
    f.origin = origin;
    return f;
}

Feature makeExtrudeFeature(const std::string& id, std::vector<SketchPoint> sketch, double height,
                           double z) {
    Feature f;
    f.id = id;
    f.type = FeatureType::extrude;
    f.sketch = std::move(sketch);
    f.height = height;
    f.origin = gp_Pnt(0.0, 0.0, z);
    return f;
}

Feature makeHoleFeature(const std::string& id, const Reference& face, const gp_Pnt& center,
                        double radius, double depth) {
    Feature f;
    f.id = id;
    f.type = FeatureType::hole;
    f.refs = {face};
    f.center = center;
    f.radius = radius;
    f.depth = depth;
    return f;
}

Feature makeFilletFeature(const std::string& id, std::vector<Reference> edges, double radius) {
    Feature f;
    f.id = id;
    f.type = FeatureType::fillet;
    f.refs = std::move(edges);
    f.filletRadius = radius;
    return f;
}

namespace {

// The results of `op` for one element of the input: modified / generated pieces, or the
// element itself if the operation does not mention it.
std::vector<TopoDS_Shape> successors(BRepBuilderAPI_MakeShape& op, const TopoDS_Shape& shape,
                                     Kind kind) {
    std::vector<TopoDS_Shape> targets;
    if (op.IsDeleted(shape)) {
        return targets;
    }
    for (const TopoDS_Shape& modified : op.Modified(shape)) {
        targets.push_back(modified);
    }
    if (kind == Kind::face) {
        for (const TopoDS_Shape& generated : op.Generated(shape)) {
            targets.push_back(generated);
        }
    }
    if (targets.empty()) {
        targets.push_back(shape);
    }
    return targets;
}

// Passes the names of `input` to the result of `op` through the kernel history. Faces that the
// operation does not mention are unchanged (the same face lives on in the result).
void inherit(Body& result, const Body& input, BRepBuilderAPI_MakeShape& op) {
    result.tombstones().insert(result.tombstones().end(), input.tombstones().begin(),
                               input.tombstones().end());
    for (const Entry& face : input.faces()) {
        if (face.names.empty()) {
            continue;
        }
        for (const TopoDS_Shape& target : successors(op, face.shape, Kind::face)) {
            const int idx = result.index(Kind::face, target);
            if (idx < 0) {
                continue;
            }
            for (const std::string& name : face.names) {
                result.faces()[static_cast<std::size_t>(idx)].addName(name);
            }
        }
    }
}

// Remembers which named elements of `input` have no namesake in `result` any more. The kernel
// history is not reliable for edges (neighbours of a rounded edge are rebuilt), so "consumed"
// is decided by the names, after the derived names of `result` exist.
void recordTombstones(Body& result, const Body& input, const std::string& featureId) {
    for (const Kind kind : {Kind::face, Kind::edge}) {
        const std::vector<Entry>& entries = input.entries(kind);
        for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
            const Entry& entry = entries[static_cast<std::size_t>(i)];
            if (entry.names.empty() || !result.find(kind, entry.primary()).empty()) {
                continue;
            }
            result.tombstones().push_back(
                {kind, entry.primary(), input.signatureOf(kind, i), featureId});
        }
    }
}

// Names the six faces of a freshly made box by the direction they look at.
void nameBoxFaces(Body& body, const std::string& prefix) {
    for (Entry& entry : body.faces()) {
        const TopoDS_Face face = TopoDS::Face(entry.shape);
        const BRepAdaptor_Surface surface(face);
        gp_Dir normal = surface.Plane().Axis().Direction();
        if (face.Orientation() == TopAbs_REVERSED) {
            normal.Reverse();
        }
        const char* name = "face:?";
        if (normal.X() > 0.5) {
            name = "face:+X";
        } else if (normal.X() < -0.5) {
            name = "face:-X";
        } else if (normal.Y() > 0.5) {
            name = "face:+Y";
        } else if (normal.Y() < -0.5) {
            name = "face:-Y";
        } else if (normal.Z() > 0.5) {
            name = "face:+Z";
        } else {
            name = "face:-Z";
        }
        entry.addName(prefix + "/box/" + name);
    }
}

struct Step {
    bool ok = false;
    std::string message;
    Body body;
};

Step failure(std::string message) {
    Step step;
    step.message = std::move(message);
    return step;
}

// Adds a new body (box / extrusion) to the model: it becomes the model if there is none,
// otherwise it is united with it.
Step combine(const Body& current, Body tool, const std::string& id) {
    if (current.isNull()) {
        tool.deriveNames(id);
        return {true, {}, std::move(tool)};
    }
    BRepAlgoAPI_Fuse fuse;
    NCollection_List<TopoDS_Shape> arguments;
    arguments.Append(current.shape());
    NCollection_List<TopoDS_Shape> tools;
    tools.Append(tool.shape());
    fuse.SetArguments(arguments);
    fuse.SetTools(tools);
    fuse.SetRunParallel(false);
    fuse.Build();
    if (!fuse.IsDone() || fuse.HasErrors()) {
        return failure("the union failed in the kernel");
    }
    Body result(fuse.Shape());
    inherit(result, current, fuse);
    inherit(result, tool, fuse);
    result.deriveNames(id);
    recordTombstones(result, current, id);
    return {true, {}, std::move(result)};
}

Step applyBox(const Feature& f, const Body& current) {
    BRepPrimAPI_MakeBox box(f.origin, f.dx, f.dy, f.dz);
    box.Build();
    if (!box.IsDone()) {
        return failure("box failed");
    }
    Body body(box.Shape());
    nameBoxFaces(body, f.id);
    return combine(current, std::move(body), f.id);
}

Step applyExtrude(const Feature& f, const Body& current) {
    const std::size_t count = f.sketch.size();
    if (count < 3) {
        return failure("a sketch needs at least three points");
    }
    std::vector<TopoDS_Vertex> vertices;
    for (const SketchPoint& p : f.sketch) {
        vertices.push_back(BRepBuilderAPI_MakeVertex(
            gp_Pnt(p.x + f.origin.X(), p.y + f.origin.Y(), f.origin.Z())));
    }
    BRepBuilderAPI_MakeWire wire;
    std::vector<TopoDS_Edge> edges;
    for (std::size_t i = 0; i < count; ++i) {
        BRepBuilderAPI_MakeEdge edge(vertices[i], vertices[(i + 1) % count]);
        if (!edge.IsDone()) {
            return failure("two sketch points coincide");
        }
        edges.push_back(edge.Edge());
        wire.Add(edge.Edge());
    }
    if (!wire.IsDone()) {
        return failure("the sketch outline is not closed");
    }
    BRepBuilderAPI_MakeFace face(wire.Wire(), /*OnlyPlane=*/true);
    if (!face.IsDone()) {
        return failure("the sketch outline is not flat");
    }
    BRepPrimAPI_MakePrism prism(face.Face(), gp_Vec(0.0, 0.0, f.height));
    prism.Build();
    if (!prism.IsDone()) {
        return failure("extrusion failed");
    }
    Body body(prism.Shape());
    for (std::size_t i = 0; i < count; ++i) {
        for (const TopoDS_Shape& side : prism.Generated(edges[i])) {
            const int idx = body.index(Kind::face, side);
            if (idx >= 0) {
                body.faces()[idx].addName(f.id + "/extrude/side[S" +
                                          std::to_string(f.sketch[i].segment) + "]");
            }
        }
    }
    const int bottom = body.index(Kind::face, prism.FirstShape());
    const int top = body.index(Kind::face, prism.LastShape());
    if (bottom >= 0) {
        body.faces()[bottom].addName(f.id + "/extrude/cap:bottom");
    }
    if (top >= 0) {
        body.faces()[top].addName(f.id + "/extrude/cap:top");
    }
    return combine(current, std::move(body), f.id);
}

bool outwardNormal(const TopoDS_Shape& shape, gp_Pnt& origin, gp_Dir& normal) {
    const TopoDS_Face face = TopoDS::Face(shape);
    const BRepAdaptor_Surface surface(face);
    if (surface.GetType() != GeomAbs_Plane) {
        return false;
    }
    const gp_Pln plane = surface.Plane();
    origin = plane.Location();
    normal = plane.Axis().Direction();
    if (face.Orientation() == TopAbs_REVERSED) {
        normal.Reverse();
    }
    return true;
}

Step applyHole(const Feature& f, const Body& current, const Resolution& face) {
    gp_Pnt planePoint;
    gp_Dir normal;
    if (!outwardNormal(face.shape, planePoint, normal)) {
        return failure("a hole can only be placed on a flat face");
    }
    // Project the center onto the face plane; start 1 mm outside so the cut is never coplanar.
    const gp_Vec toCenter(planePoint, f.center);
    const double along = toCenter.Dot(gp_Vec(normal));
    const gp_Pnt onPlane = f.center.Translated(-along * gp_Vec(normal));
    const double margin = 1.0;
    const gp_Pnt start = onPlane.Translated(margin * gp_Vec(normal));
    const double length = (f.depth > 0.0 ? f.depth : 2.0 * current.diagonal()) + margin;

    const gp_Ax2 axis(start, normal.Reversed());
    BRepPrimAPI_MakeCylinder cylinder(axis, f.radius, length);
    cylinder.Build();
    if (!cylinder.IsDone()) {
        return failure("hole tool failed");
    }
    Body tool(cylinder.Shape());
    for (Entry& entry : tool.faces()) {
        const BRepAdaptor_Surface surface(TopoDS::Face(entry.shape));
        if (surface.GetType() == GeomAbs_Cylinder) {
            entry.addName(f.id + "/hole/wall");
        } else {
            GProp_GProps props;
            BRepGProp::SurfaceProperties(entry.shape, props);
            const double depthOfFace =
                gp_Vec(start, props.CentreOfMass()).Dot(gp_Vec(normal.Reversed()));
            entry.addName(f.id + (depthOfFace < 0.5 * length ? "/hole/cap:start" : "/hole/floor"));
        }
    }

    BRepAlgoAPI_Cut cut;
    NCollection_List<TopoDS_Shape> arguments;
    arguments.Append(current.shape());
    NCollection_List<TopoDS_Shape> tools;
    tools.Append(tool.shape());
    cut.SetArguments(arguments);
    cut.SetTools(tools);
    cut.SetRunParallel(false);
    cut.Build();
    if (!cut.IsDone() || cut.HasErrors()) {
        return failure("cutting the hole failed in the kernel");
    }
    Body result(cut.Shape());
    inherit(result, current, cut);
    inherit(result, tool, cut);
    result.deriveNames(f.id);
    recordTombstones(result, current, f.id);
    return {true, {}, std::move(result)};
}

Step applyFillet(const Feature& f, const Body& current, const std::vector<Resolution>& refs) {
    BRepFilletAPI_MakeFillet fillet(current.shape());
    std::vector<TopoDS_Shape> edges;
    std::vector<std::string> edgeNames;
    for (const Resolution& r : refs) {
        edges.push_back(r.shape);
        edgeNames.push_back(current.edges()[r.index].primary());
        fillet.Add(f.filletRadius, TopoDS::Edge(r.shape));
    }
    fillet.Build();
    if (!fillet.IsDone()) {
        return failure("the fillet failed in the kernel");
    }
    Body result(fillet.Shape());
    inherit(result, current, fillet);
    for (std::size_t i = 0; i < edges.size(); ++i) {
        for (const TopoDS_Shape& generated : fillet.Generated(edges[i])) {
            const int idx = result.index(Kind::face, generated);
            if (idx >= 0 && !edgeNames[i].empty()) {
                result.faces()[idx].addName(f.id + "/fillet[" + edgeNames[i] + "]");
            }
        }
    }
    result.deriveNames(f.id);
    recordTombstones(result, current, f.id);
    return {true, {}, std::move(result)};
}

} // namespace

Evaluation evaluate(Recipe& recipe, Strictness strictness) {
    Evaluation evaluation;
    Body current;
    for (Feature& feature : recipe) {
        FeatureOutcome outcome;
        outcome.input = current;

        // Resolve the references first. One unsafe reference stops the whole feature; it is
        // never applied "mostly".
        bool safe = true;
        for (const Reference& ref : feature.refs) {
            Resolution r = resolve(current, ref, strictness);
            if (r.outcome == Outcome::needsHelp) {
                safe = false;
            }
            outcome.refs.push_back(std::move(r));
        }
        if (!safe) {
            outcome.status = FeatureStatus::needsHelp;
            outcome.message = "a reference needs help";
            evaluation.features.push_back(std::move(outcome));
            continue;
        }

        Step step;
        try {
            switch (feature.type) {
            case FeatureType::box:
                step = applyBox(feature, current);
                break;
            case FeatureType::extrude:
                step = applyExtrude(feature, current);
                break;
            case FeatureType::hole:
                step = current.isNull() ? failure("no body to drill into")
                                        : applyHole(feature, current, outcome.refs.front());
                break;
            case FeatureType::fillet:
                step = current.isNull() ? failure("no body to round")
                                        : applyFillet(feature, current, outcome.refs);
                break;
            }
        } catch (const Standard_Failure& failureInKernel) {
            step = failure(std::string("kernel exception: ") + failureInKernel.what());
        }

        if (!step.ok) {
            outcome.status = FeatureStatus::error;
            outcome.message = step.message;
            evaluation.features.push_back(std::move(outcome));
            continue;
        }

        // Safe references now point to the current name and look.
        for (std::size_t i = 0; i < feature.refs.size(); ++i) {
            refresh(feature.refs[i], current, outcome.refs[i]);
        }
        current = std::move(step.body);
        evaluation.features.push_back(std::move(outcome));
    }
    evaluation.body = current;
    return evaluation;
}

Body bodyAfter(Recipe recipe, int upTo, Strictness strictness) {
    if (upTo >= 0 && upTo < static_cast<int>(recipe.size())) {
        recipe.resize(static_cast<std::size_t>(upTo));
    }
    return evaluate(recipe, strictness).body;
}

} // namespace fission::test::naming
