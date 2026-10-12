// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "support.h"

#include <catch2/catch_test_case_info.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <utility>

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBndLib.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepFilletAPI_MakeChamfer.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepGProp.hxx>
#include <BRepOffsetAPI_MakeThickSolid.hxx>
#include <BRepOffset_Mode.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <GeomAbs_CurveType.hxx>
#include <GeomAbs_JoinType.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <NCollection_IndexedMap.hxx>
#include <NCollection_List.hxx>
#include <ShapeFix_Shape.hxx>
#include <ShapeUpgrade_UnifySameDomain.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Vertex.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>

namespace fission::test::geo {

namespace {

// State of the running test case; the listener below turns it into one row of the result table.
struct CaseState {
    double kernelSeconds = 0.0;
    std::vector<std::pair<std::string, std::string>> notes;
};

CaseState& state() {
    static CaseState current;
    return current;
}

int countOf(const TopoDS_Shape& shape, TopAbs_ShapeEnum type) {
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> map;
    TopExp::MapShapes(shape, type, map);
    return map.Extent();
}

template <typename Operation>
BoolResult runBoolean(Operation& operation, const TopoDS_Shape& a,
                      const std::vector<TopoDS_Shape>& tools, double fuzzy) {
    NCollection_List<TopoDS_Shape> arguments;
    arguments.Append(a);
    NCollection_List<TopoDS_Shape> toolList;
    for (const TopoDS_Shape& tool : tools) {
        toolList.Append(tool);
    }
    operation.SetArguments(arguments);
    operation.SetTools(toolList);
    operation.SetRunParallel(false); // reproducible, and kind to shared test machines
    if (fuzzy > 0.0) {
        operation.SetFuzzyValue(fuzzy);
    }

    BoolResult result;
    result.seconds = timed([&] { operation.Build(); });
    result.done = operation.IsDone();
    result.hasErrors = operation.HasErrors();
    if (result.done) {
        result.shape = operation.Shape();
    }
    addKernelSeconds(result.seconds);
    return result;
}

std::string csvQuote(const std::string& text) {
    std::string quoted = "\"";
    for (const char c : text) {
        quoted += (c == '"') ? "\"\"" : std::string(1, c);
    }
    return quoted + "\"";
}

// Appends one CSV row per finished test case to the file named by FISSION_GEOMETRY_REPORT
// (if set). scripts/tools/geometry_report.py turns the file into a table.
class ReportListener : public Catch::EventListenerBase {
public:
    using Catch::EventListenerBase::EventListenerBase;

    void testCaseStarting(const Catch::TestCaseInfo& /*info*/) override { state() = {}; }

    void testCaseEnded(const Catch::TestCaseStats& stats) override {
        const char* path = std::getenv("FISSION_GEOMETRY_REPORT");
        if (path == nullptr || *path == '\0') {
            return;
        }
        const Catch::TestCaseInfo& info = *stats.testInfo;
        std::string tags;
        for (const Catch::Tag& tag : info.tags) {
            tags += std::string(tag.original.data(), tag.original.size()) + " ";
        }
        // A failed check in a test marked [!mayfail] counts as "failed but ok" for Catch2.
        std::string outcome = "ok";
        if (stats.totals.assertions.failed > 0) {
            outcome = "failed";
        } else if (stats.totals.assertions.failedButOk > 0) {
            outcome = "known-problem";
        }
        std::ostringstream notes;
        for (const auto& [key, value] : state().notes) {
            notes << key << "=" << value << "; ";
        }

        std::ofstream out(path, std::ios::app);
        out << csvQuote(tags) << ',' << csvQuote(info.name) << ',' << outcome << ','
            << state().kernelSeconds << ',' << csvQuote(notes.str()) << '\n';
    }
};

} // namespace

CATCH_REGISTER_LISTENER(ReportListener)

ShapeStats measure(const TopoDS_Shape& shape) {
    ShapeStats stats;
    if (shape.IsNull()) {
        return stats;
    }
    GProp_GProps volume;
    BRepGProp::VolumeProperties(shape, volume);
    stats.volume = volume.Mass();
    GProp_GProps surface;
    BRepGProp::SurfaceProperties(shape, surface);
    stats.area = surface.Mass();

    Bnd_Box box;
    BRepBndLib::AddOptimal(shape, box, /*useTriangulation=*/false, /*useShapeTolerance=*/false);
    if (!box.IsVoid()) {
        double x0 = 0.0, y0 = 0.0, z0 = 0.0, x1 = 0.0, y1 = 0.0, z1 = 0.0;
        box.Get(x0, y0, z0, x1, y1, z1);
        stats.sizeX = x1 - x0;
        stats.sizeY = y1 - y0;
        stats.sizeZ = z1 - z0;
    }
    stats.solids = countOf(shape, TopAbs_SOLID);
    stats.faces = countOf(shape, TopAbs_FACE);
    stats.edges = countOf(shape, TopAbs_EDGE);
    stats.vertices = countOf(shape, TopAbs_VERTEX);
    stats.valid = BRepCheck_Analyzer(shape).IsValid();
    return stats;
}

TopoDS_Shape makeBox(double dx, double dy, double dz, double x, double y, double z) {
    return BRepPrimAPI_MakeBox(gp_Pnt(x, y, z), dx, dy, dz).Shape();
}

TopoDS_Shape makeCylinder(double radius, double height, double x, double y, double z) {
    const gp_Ax2 axis(gp_Pnt(x, y, z), gp_Dir(0.0, 0.0, 1.0));
    return BRepPrimAPI_MakeCylinder(axis, radius, height).Shape();
}

BoolResult fuse(const TopoDS_Shape& a, const TopoDS_Shape& b, double fuzzy) {
    BRepAlgoAPI_Fuse operation;
    return runBoolean(operation, a, {b}, fuzzy);
}

BoolResult cut(const TopoDS_Shape& a, const TopoDS_Shape& b, double fuzzy) {
    BRepAlgoAPI_Cut operation;
    return runBoolean(operation, a, {b}, fuzzy);
}

BoolResult common(const TopoDS_Shape& a, const TopoDS_Shape& b, double fuzzy) {
    BRepAlgoAPI_Common operation;
    return runBoolean(operation, a, {b}, fuzzy);
}

BoolResult cutMany(const TopoDS_Shape& a, const std::vector<TopoDS_Shape>& tools, double fuzzy) {
    BRepAlgoAPI_Cut operation;
    return runBoolean(operation, a, tools, fuzzy);
}

std::vector<TopoDS_Edge> edgesWhere(const TopoDS_Shape& shape,
                                    const std::function<bool(const TopoDS_Edge&)>& predicate) {
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> map;
    TopExp::MapShapes(shape, TopAbs_EDGE, map);
    std::vector<TopoDS_Edge> found;
    for (int index = 1; index <= map.Extent(); ++index) {
        const TopoDS_Edge edge = TopoDS::Edge(map(index));
        if (predicate(edge)) {
            found.push_back(edge);
        }
    }
    return found;
}

std::vector<TopoDS_Face> facesWhere(const TopoDS_Shape& shape,
                                    const std::function<bool(const TopoDS_Face&)>& predicate) {
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> map;
    TopExp::MapShapes(shape, TopAbs_FACE, map);
    std::vector<TopoDS_Face> found;
    for (int index = 1; index <= map.Extent(); ++index) {
        const TopoDS_Face face = TopoDS::Face(map(index));
        if (predicate(face)) {
            found.push_back(face);
        }
    }
    return found;
}

gp_Pnt edgeMidpoint(const TopoDS_Edge& edge) {
    const BRepAdaptor_Curve curve(edge);
    return curve.Value(0.5 * (curve.FirstParameter() + curve.LastParameter()));
}

gp_Pnt faceCenter(const TopoDS_Face& face) {
    GProp_GProps properties;
    BRepGProp::SurfaceProperties(face, properties);
    return properties.CentreOfMass();
}

bool isCircle(const TopoDS_Edge& edge) {
    return BRepAdaptor_Curve(edge).GetType() == GeomAbs_Circle;
}

bool isPlane(const TopoDS_Face& face) {
    return BRepAdaptor_Surface(face).GetType() == GeomAbs_Plane;
}

bool edgeTouches(const TopoDS_Edge& edge, const gp_Pnt& point, double tolerance) {
    TopoDS_Vertex first;
    TopoDS_Vertex last;
    TopExp::Vertices(edge, first, last);
    return BRep_Tool::Pnt(first).Distance(point) <= tolerance ||
           BRep_Tool::Pnt(last).Distance(point) <= tolerance;
}

namespace {

// Runs a kernel construction that reports through IsDone() and may throw.
template <typename Builder, typename Configure>
BoolResult runBuilder(Builder& builder, Configure&& configure) {
    BoolResult result;
    try {
        result.seconds = timed([&] {
            configure();
            builder.Build();
        });
        result.done = builder.IsDone();
        result.hasErrors = !result.done;
        if (result.done) {
            result.shape = builder.Shape();
        }
    } catch (const Standard_Failure& failure) {
        result.done = false;
        result.hasErrors = true;
        note("exception", std::string(failure.what()));
    }
    addKernelSeconds(result.seconds);
    return result;
}

} // namespace

BoolResult fillet(const TopoDS_Shape& shape, const std::vector<TopoDS_Edge>& edges, double radius) {
    BRepFilletAPI_MakeFillet builder(shape);
    return runBuilder(builder, [&] {
        for (const TopoDS_Edge& edge : edges) {
            builder.Add(radius, edge);
        }
    });
}

BoolResult chamfer(const TopoDS_Shape& shape, const std::vector<TopoDS_Edge>& edges,
                   double distance) {
    BRepFilletAPI_MakeChamfer builder(shape);
    return runBuilder(builder, [&] {
        for (const TopoDS_Edge& edge : edges) {
            builder.Add(distance, edge);
        }
    });
}

BoolResult hollow(const TopoDS_Shape& shape, const std::vector<TopoDS_Face>& openFaces,
                  double thickness, double tolerance, OffsetJoin join) {
    NCollection_List<TopoDS_Shape> closing;
    for (const TopoDS_Face& face : openFaces) {
        closing.Append(face);
    }
    BRepOffsetAPI_MakeThickSolid builder;
    return runBuilder(builder, [&] {
        // A negative offset grows the wall inwards, so the outer size stays as modeled.
        builder.MakeThickSolidByJoin(
            shape, closing, -thickness, tolerance, BRepOffset_Skin, /*Intersection=*/false,
            /*SelfInter=*/false, join == OffsetJoin::arc ? GeomAbs_Arc : GeomAbs_Intersection);
    });
}

TopoDS_Shape healShape(const TopoDS_Shape& shape) {
    ShapeFix_Shape fixer(shape);
    fixer.Perform();
    return fixer.Shape();
}

TopoDS_Shape unifyFaces(const TopoDS_Shape& shape) {
    ShapeUpgrade_UnifySameDomain unify(shape, /*UnifyEdges=*/true, /*UnifyFaces=*/true,
                                       /*ConcatBSplines=*/false);
    unify.Build();
    return unify.Shape();
}

void note(const std::string& key, const std::string& value) {
    state().notes.emplace_back(key, value);
}

void note(const std::string& key, double value) {
    char buffer[48];
    std::snprintf(buffer, sizeof buffer, "%.6g", value);
    note(key, std::string(buffer));
}

void addKernelSeconds(double seconds) {
    state().kernelSeconds += seconds;
}

std::string testFontPath() {
    return std::string(FISSION_TEST_FIXTURES_DIR) + "/fonts/DejaVuSans.ttf";
}

} // namespace fission::test::geo
