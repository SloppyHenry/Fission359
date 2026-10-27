// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Practical test for AP-0.6: the same scenarios with OpenCASCADE's own naming framework
// (OCAF / TNaming), built the way the OCCT sample modeller does it: the features register the
// shapes they create and their history on labels, a TNaming_Selector holds the reference.
// The numbers and the effort are compared with the prototype in
// docs/spikes/persistente-referenzen.md.

#include <cstdio>

#include "scenario_support.h"
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeShape.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <NCollection_IndexedMap.hxx>
#include <NCollection_List.hxx>
#include <NCollection_Map.hxx>
#include <TDF_ChildIterator.hxx>
#include <TDF_Data.hxx>
#include <TDF_Label.hxx>
#include <TNaming_Builder.hxx>
#include <TNaming_NamedShape.hxx>
#include <TNaming_Selector.hxx>
#include <TNaming_Tool.hxx>
#include <TopExp.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <TopoDS.hxx>
#include <gp_Ax2.hxx>
#include <gp_Vec.hxx>

using namespace fission::test::naming;

// OpenCASCADE 8.0.1 leaks a few bytes in TNaming_Builder::Select (reached from
// TNaming_Selector::Solve, which this test uses); the leaked block keeps its shapes alive. It is
// a defect of the library, not of the prototype, so LeakSanitizer is told to ignore it. Finding
// it by name needs the full stack, hence the slow unwinder for this executable.
#if defined(__SANITIZE_ADDRESS__)
#define FISSION_TEST_ASAN 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define FISSION_TEST_ASAN 1
#endif
#endif
#ifdef FISSION_TEST_ASAN
extern "C" {
const char* __lsan_default_suppressions();
const char* __lsan_default_suppressions() {
    return "leak:TNaming_Builder::Select\n";
}
const char* __asan_default_options();
const char* __asan_default_options() {
    return "fast_unwind_on_malloc=0";
}
}
#endif

namespace {

// ---------------------------------------------------------------------------------------------
// A tiny modeller on top of OCAF, the way the OCCT sample modeller does it. Every feature owns
// a label. Primitive shapes are registered with Generated(); an operation is recorded with
// Modify / Generated / Delete on child labels (a NamedShape has one evolution kind each).

using Shapes = std::vector<TopoDS_Shape>;

Shapes subShapes(const TopoDS_Shape& shape, TopAbs_ShapeEnum type) {
    Shapes list;
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> map;
    TopExp::MapShapes(shape, type, map);
    for (int i = 1; i <= map.Extent(); ++i) {
        list.push_back(map(i));
    }
    return list;
}

Shapes facesAndEdges(const TopoDS_Shape& shape) {
    Shapes list = subShapes(shape, TopAbs_FACE);
    const Shapes edges = subShapes(shape, TopAbs_EDGE);
    list.insert(list.end(), edges.begin(), edges.end());
    return list;
}

// Records the history of `op` (argument solid `from`, result `to`) below `label`.
void recordOperation(const TDF_Label& label, BRepBuilderAPI_MakeShape& op, const TopoDS_Shape& from,
                     const TopoDS_Shape& to) {
    {
        TNaming_Builder b(label);
        b.Modify(from, to);
    }
    TNaming_Builder modified(label.FindChild(1));
    TNaming_Builder generated(label.FindChild(2));
    TNaming_Builder deleted(label.FindChild(3));
    for (const TopoDS_Shape& old : facesAndEdges(from)) {
        if (op.IsDeleted(old)) {
            deleted.Delete(old);
            continue;
        }
        for (const TopoDS_Shape& piece : op.Modified(old)) {
            modified.Modify(old, piece);
        }
        for (const TopoDS_Shape& piece : op.Generated(old)) {
            generated.Generated(old, piece);
        }
    }
}

// Registers a solid and its faces below `label` (tags 1..n).
void registerPrimitive(const TDF_Label& label, const TopoDS_Shape& solid) {
    {
        TNaming_Builder b(label);
        b.Generated(solid);
    }
    int tag = 1;
    for (const TopoDS_Shape& face : subShapes(solid, TopAbs_FACE)) {
        TNaming_Builder b(label.FindChild(tag++));
        b.Generated(face);
    }
}

void addValid(NCollection_Map<TDF_Label>& valid, const TDF_Label& label) {
    valid.Add(label);
    for (TDF_ChildIterator it(label, true); it.More(); it.Next()) {
        valid.Add(it.Value());
    }
}

Resolution asResolution(bool solved, const TopoDS_Shape& shape) {
    Resolution r;
    if (solved && !shape.IsNull() && shape.ShapeType() != TopAbs_COMPOUND) {
        r.outcome = Outcome::found;
        r.shape = shape;
    } else {
        r.outcome = Outcome::needsHelp;
    }
    return r;
}

struct Ocaf {
    occ::handle<TDF_Data> data = new TDF_Data;
    // The attributes reference each other in cycles; a document has to be cleared by hand,
    // otherwise OpenCASCADE's reference counting never frees it.
    ~Ocaf() { data->Root().ForgetAllAttributes(true); }
    TDF_Label label(int tag) const { return data->Root().FindChild(tag); }
};

// One box, one selection, optional fillet / hole after it.
struct BoxModel : Ocaf {
    TopoDS_Shape solid;

    void box(double dx, double dy, double dz) {
        BRepPrimAPI_MakeBox mk(gp_Pnt(0, 0, 0), dx, dy, dz);
        mk.Build();
        solid = mk.Shape();
        registerPrimitive(label(1), solid);
    }
};

// Extruded rectangle-like polygon: sketch edges live on label 1 (tag = segment id + 1), the
// solid on label 2 with one child per side (tag = segment id + 10).
struct SketchModel : Ocaf {
    TopoDS_Shape solid;
    std::vector<SketchPoint> sketch;
    double height = 10.0;
    std::vector<int> registered; // segment ids that have labels from an earlier build

    void build() {
        // Segments that are gone: their labels must not keep the old content (a real modeller
        // has to do this bookkeeping; without it OCAF would still "see" the old face).
        for (const int id : registered) {
            bool stillThere = false;
            for (const SketchPoint& p : sketch) {
                stillThere = stillThere || p.segment == id;
            }
            if (!stillThere) {
                label(1).FindChild(id + 1).ForgetAllAttributes(false);
                label(2).FindChild(id + 10).ForgetAllAttributes(false);
            }
        }
        registered.clear();
        for (const SketchPoint& p : sketch) {
            registered.push_back(p.segment);
        }
        std::vector<TopoDS_Vertex> vertices;
        for (const SketchPoint& p : sketch) {
            vertices.push_back(BRepBuilderAPI_MakeVertex(gp_Pnt(p.x, p.y, 0.0)));
        }
        BRepBuilderAPI_MakeWire wire;
        std::vector<TopoDS_Edge> edges;
        for (std::size_t i = 0; i < sketch.size(); ++i) {
            BRepBuilderAPI_MakeEdge e(vertices[i], vertices[(i + 1) % sketch.size()]);
            edges.push_back(e.Edge());
            wire.Add(e.Edge());
            TNaming_Builder b(label(1).FindChild(sketch[i].segment + 1));
            b.Generated(e.Edge());
        }
        BRepBuilderAPI_MakeFace face(wire.Wire(), true);
        BRepPrimAPI_MakePrism prism(face.Face(), gp_Vec(0, 0, height));
        prism.Build();
        solid = prism.Shape();
        {
            TNaming_Builder b(label(2));
            b.Generated(solid);
        }
        for (std::size_t i = 0; i < sketch.size(); ++i) {
            TNaming_Builder b(label(2).FindChild(sketch[i].segment + 10));
            for (const TopoDS_Shape& side : prism.Generated(edges[i])) {
                b.Generated(edges[i], side);
            }
        }
        {
            TNaming_Builder b(label(2).FindChild(1));
            b.Generated(face.Face(), prism.FirstShape());
        }
        {
            TNaming_Builder b(label(2).FindChild(2));
            b.Generated(face.Face(), prism.LastShape());
        }
    }

    NCollection_Map<TDF_Label> valid() const {
        NCollection_Map<TDF_Label> map;
        addValid(map, label(1));
        addValid(map, label(2));
        return map;
    }
};

// Drills a hole (cylinder along -X from x = 40 + 1) with history recorded on `where`.
TopoDS_Shape drill(const TDF_Label& where, const TopoDS_Shape& body, const gp_Pnt& start,
                   const gp_Dir& dir, double radius, double length) {
    BRepPrimAPI_MakeCylinder cylinder(gp_Ax2(start, dir), radius, length);
    cylinder.Build();
    BRepAlgoAPI_Cut cut;
    NCollection_List<TopoDS_Shape> args;
    args.Append(body);
    NCollection_List<TopoDS_Shape> tools;
    tools.Append(cylinder.Shape());
    cut.SetArguments(args);
    cut.SetTools(tools);
    cut.SetRunParallel(false);
    cut.Build();
    recordOperation(where, cut, body, cut.Shape());
    return cut.Shape();
}

} // namespace

// ---------------------------------------------------------------------------------------------

TEST_CASE("ocaf: box, round one edge, change the height", "[naming][ocaf]") {
    BoxModel model;
    model.box(40, 20, 10);
    const TopoDS_Shape edge =
        only(boxEdge(model.solid, BoxDims{40, 20, 10}, Side::posX, Side::posZ));

    TNaming_Selector selector(model.label(10));
    REQUIRE(selector.Select(edge, model.solid));
    BRepFilletAPI_MakeFillet mk(model.solid);
    mk.Add(2.0, TopoDS::Edge(edge));
    mk.Build();
    recordOperation(model.label(3), mk, model.solid, mk.Shape());

    for (const double height : {25.0, 4.0, 10.0}) {
        model.box(40, 20, height);
        NCollection_Map<TDF_Label> valid;
        addValid(valid, model.label(1));
        const bool solved = selector.Solve(valid);
        const TopoDS_Shape shape =
            solved ? TNaming_Tool::GetShape(selector.NamedShape()) : TopoDS_Shape();
        const truth::Shapes expected =
            boxEdge(model.solid, BoxDims{40, 20, height}, Side::posX, Side::posZ);
        const Verdict verdict = record("ocaf: box fillet, height change", Strictness::strict,
                                       asResolution(solved, shape), expected);
        CHECK(verdict == Verdict::correct);
    }
}

TEST_CASE("ocaf: sketch, extrude, extend the sketch by a segment", "[naming][ocaf]") {
    SketchModel model;
    model.sketch = rectangleSketch(40, 20);
    model.build();
    const TopoDS_Shape side = only(segmentFace(model.solid, model.sketch, 1));
    TNaming_Selector selector(model.label(10));
    REQUIRE(selector.Select(side, model.solid));

    SECTION("a new point in the top segment") {
        insertPoint(model.sketch, 2, 20, 30, 4);
        model.build();
        NCollection_Map<TDF_Label> valid = model.valid();
        const bool solved = selector.Solve(valid);
        const TopoDS_Shape shape =
            solved ? TNaming_Tool::GetShape(selector.NamedShape()) : TopoDS_Shape();
        const Verdict verdict =
            record("ocaf: sketch extended", Strictness::strict, asResolution(solved, shape),
                   segmentFace(model.solid, model.sketch, 1));
        CHECK(verdict == Verdict::correct);
    }

    SECTION("a new point in the selected segment itself") {
        insertPoint(model.sketch, 1, 46, 10, 4);
        model.build();
        NCollection_Map<TDF_Label> valid = model.valid();
        const bool solved = selector.Solve(valid);
        const TopoDS_Shape shape =
            solved ? TNaming_Tool::GetShape(selector.NamedShape()) : TopoDS_Shape();
        const Verdict verdict =
            record("ocaf: sketch extended", Strictness::strict, asResolution(solved, shape),
                   segmentFace(model.solid, model.sketch, 1));
        CHECK(verdict == Verdict::correct);
    }
}

TEST_CASE("ocaf: evil cases", "[naming][ocaf][evil]") {
    SECTION("a face is split in two by a wide through hole") {
        BoxModel model;
        model.box(40, 20, 10);
        const TopoDS_Shape top = only(boxFace(model.solid, BoxDims{40, 20, 10}, Side::posZ));
        TNaming_Selector selector(model.label(10));
        REQUIRE(selector.Select(top, model.solid));
        const TopoDS_Shape drilled =
            drill(model.label(3), model.solid, gp_Pnt(20, 10, 11), gp_Dir(0, 0, -1), 14.0, 12.0);
        NCollection_Map<TDF_Label> valid;
        addValid(valid, model.label(1));
        addValid(valid, model.label(3));
        const bool solved = selector.Solve(valid);
        const TopoDS_Shape shape =
            solved ? TNaming_Tool::GetShape(selector.NamedShape()) : TopoDS_Shape();
        std::fprintf(stderr, "[ocaf] split: solved=%d null=%d type=%d\n", solved, shape.IsNull(),
                     shape.IsNull() ? -1 : static_cast<int>(shape.ShapeType()));
        const truth::Shapes expected = boxFace(drilled, BoxDims{40, 20, 10}, Side::posZ);
        REQUIRE(expected.size() == 2);
        record("ocaf: evil face splits", Strictness::strict, asResolution(solved, shape), expected);
    }

    SECTION("a sketch point is removed, the selected segment's face is gone") {
        SketchModel model;
        model.sketch = rectangleSketch(40, 20);
        model.build();
        const TopoDS_Shape side = only(segmentFace(model.solid, model.sketch, 2));
        TNaming_Selector selector(model.label(10));
        REQUIRE(selector.Select(side, model.solid));
        removePoint(model.sketch, 2);
        model.build();
        NCollection_Map<TDF_Label> valid = model.valid();
        const bool solved = selector.Solve(valid);
        const TopoDS_Shape shape =
            solved ? TNaming_Tool::GetShape(selector.NamedShape()) : TopoDS_Shape();
        std::fprintf(stderr, "[ocaf] vanish: solved=%d null=%d type=%d\n", solved, shape.IsNull(),
                     shape.IsNull() ? -1 : static_cast<int>(shape.ShapeType()));
        record("ocaf: evil face vanishes", Strictness::strict, asResolution(solved, shape),
               segmentFace(model.solid, model.sketch, 2));
    }

    SECTION("a mirrored sketch: the segment stands on the other side") {
        SketchModel model;
        model.sketch = rectangleSketch(40, 20);
        model.build();
        const TopoDS_Shape side = only(segmentFace(model.solid, model.sketch, 2));
        TNaming_Selector selector(model.label(10));
        REQUIRE(selector.Select(side, model.solid));
        for (SketchPoint& point : model.sketch) {
            point.y = 20.0 - point.y;
        }
        model.build();
        NCollection_Map<TDF_Label> valid = model.valid();
        const bool solved = selector.Solve(valid);
        const TopoDS_Shape shape =
            solved ? TNaming_Tool::GetShape(selector.NamedShape()) : TopoDS_Shape();
        std::fprintf(stderr, "[ocaf] mirrored: solved=%d null=%d type=%d\n", solved, shape.IsNull(),
                     shape.IsNull() ? -1 : static_cast<int>(shape.ShapeType()));
        // As in the prototype tests: the right answer is "needs help" (no unique right face).
        record("ocaf: evil places swap", Strictness::strict, asResolution(solved, shape), {});
    }
}
