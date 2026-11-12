// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The closed areas of a sketch (AP-2.1g). Every curve becomes an edge on one plane; a big flat
// face that holds them all is split by those edges (`BRepAlgoAPI_Splitter`), which makes one face
// for every enclosed piece and one for the rest (the one with the corners of the big face, which
// is dropped). Each face is checked, measured exactly, and its boundary drawn as a polygon within
// `kSketchRegionDeflection`. Which curve each piece of a boundary lies on is read from the
// splitter's history, so that curves on top of each other all count.

#include "fission/geo/occt/sketch_regions.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <optional>

#include "fission/geo/error.h"

#include "sketch_region_face.h"
#include "sketch_spline_curve.h"
#include <BRepAdaptor_Curve.hxx>
#include <BRepAlgoAPI_Splitter.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <BRepGProp.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepTools.hxx>
#include <BRep_Tool.hxx>
#include <GCPnts_QuasiUniformDeflection.hxx>
#include <GProp_GProps.hxx>
#include <GeomAPI.hxx>
#include <GeomAPI_ProjectPointOnCurve.hxx>
#include <Geom_Curve.hxx>
#include <NCollection_DataMap.hxx>
#include <NCollection_List.hxx>
#include <Poly_Triangulation.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_Orientation.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <gp_Dir.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

namespace fission::geo::occt {

namespace {

constexpr double kTwoPi = 2.0 * std::numbers::pi;
constexpr double kJoin = 1e-5; // ends of boundary pieces nearer than this are one point

double distance(const Point2d& a, const Point2d& b) {
    return std::hypot(b.x - a.x, b.y - a.y);
}

[[noreturn]] void invalid(const char* op, const char* what) {
    throw GeometryError(GeometryErrorCode::invalidInput, std::string(op) + ": " + what, "region");
}

// One curve as an edge, with its curve kept for looking up directions.
struct Source {
    TopoDS_Edge edge;
    occ::handle<Geom_Curve> curve;
    double first = 0.0;
    double last = 0.0;
};

Source makeSource(const SketchCurve& curve, const char* op) {
    const std::vector<Point2d>& p = curve.points;
    for (const Point2d& point : p) {
        if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
            invalid(op, "a point is not finite");
        }
    }
    Source source;
    const auto fromEdge = [&source, op](const TopoDS_Edge& edge) {
        source.edge = edge;
        source.curve = BRep_Tool::Curve(edge, source.first, source.last);
        if (source.curve.IsNull()) {
            throw GeometryError(GeometryErrorCode::kernelFailure, std::string(op) + ": no curve");
        }
    };
    const auto lineOf = [&](const Point2d& a, const Point2d& b) {
        if (!(distance(a, b) > 0.0)) {
            invalid(op, "a line needs two different points");
        }
        fromEdge(BRepBuilderAPI_MakeEdge(gp_Pnt(a.x, a.y, 0.0), gp_Pnt(b.x, b.y, 0.0)).Edge());
    };
    switch (curve.kind) {
    case SketchCurveKind::line:
        if (p.size() != 2) {
            invalid(op, "a line needs two points");
        }
        lineOf(p[0], p[1]);
        break;
    case SketchCurveKind::circle:
    case SketchCurveKind::arc: {
        const std::size_t count = curve.kind == SketchCurveKind::circle ? 2 : 3;
        if (p.size() != count || !(distance(p[0], p[1]) > 0.0)) {
            invalid(op, "a circle or arc needs a radius");
        }
        const double radius = distance(p[0], p[1]);
        const gp_Dir toStart((p[1].x - p[0].x) / radius, (p[1].y - p[0].y) / radius, 0.0);
        const gp_Circ circle(gp_Ax2(gp_Pnt(p[0].x, p[0].y, 0.0), gp_Dir(0.0, 0.0, 1.0), toStart),
                             radius);
        if (curve.kind == SketchCurveKind::circle) {
            fromEdge(BRepBuilderAPI_MakeEdge(circle).Edge());
        } else {
            if (!(distance(p[0], p[2]) > 0.0)) {
                invalid(op, "an arc needs an end");
            }
            double sweep = std::atan2(p[2].y - p[0].y, p[2].x - p[0].x) -
                           std::atan2(p[1].y - p[0].y, p[1].x - p[0].x);
            sweep = std::fmod(sweep, kTwoPi);
            if (sweep < 0.0) {
                sweep += kTwoPi;
            }
            if (!(sweep > 1e-12)) {
                sweep = kTwoPi;
            }
            fromEdge(sweep >= kTwoPi - 1e-12 ? BRepBuilderAPI_MakeEdge(circle).Edge()
                                             : BRepBuilderAPI_MakeEdge(circle, 0.0, sweep).Edge());
        }
        break;
    }
    case SketchCurveKind::spline: {
        const InterpolatedSpline spline = interpolateSketchSpline(p);
        if (spline.curve.IsNull()) {
            lineOf(spline.points.front(), spline.points.back());
        } else {
            const occ::handle<Geom_Curve> curve3d =
                GeomAPI::To3d(spline.curve, gp_Pln(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0)));
            fromEdge(BRepBuilderAPI_MakeEdge(curve3d, spline.curve->FirstParameter(),
                                             spline.curve->LastParameter())
                         .Edge());
        }
        break;
    }
    }
    return source;
}

// The area that holds everything, so that the pieces that curves enclose are all inside it.
struct Frame {
    TopoDS_Face face;
    std::vector<Point2d> corners;
};

Frame makeFrame(const std::vector<SketchCurve>& curves) {
    double minX = 0.0;
    double maxX = 0.0;
    double minY = 0.0;
    double maxY = 0.0;
    bool first = true;
    const auto add = [&](double x, double y, double reach) {
        if (first) {
            minX = x - reach;
            maxX = x + reach;
            minY = y - reach;
            maxY = y + reach;
            first = false;
        }
        minX = std::min(minX, x - reach);
        maxX = std::max(maxX, x + reach);
        minY = std::min(minY, y - reach);
        maxY = std::max(maxY, y + reach);
    };
    for (const SketchCurve& curve : curves) {
        const bool round =
            curve.kind == SketchCurveKind::circle || curve.kind == SketchCurveKind::arc;
        const double reach =
            round && curve.points.size() >= 2 ? distance(curve.points[0], curve.points[1]) : 0.0;
        if (round && !curve.points.empty()) {
            add(curve.points[0].x, curve.points[0].y, reach);
        } else {
            for (const Point2d& point : curve.points) {
                add(point.x, point.y, 0.0);
            }
        }
    }
    const double margin = 10.0 + 0.5 * std::hypot(maxX - minX, maxY - minY);
    Frame frame;
    minX -= margin;
    minY -= margin;
    maxX += margin;
    maxY += margin;
    frame.face = BRepBuilderAPI_MakeFace(gp_Pln(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0)), minX,
                                         maxX, minY, maxY)
                     .Face();
    frame.corners = {{minX, minY}, {maxX, minY}, {maxX, maxY}, {minX, maxY}};
    return frame;
}

using SourceMap =
    NCollection_DataMap<TopoDS_Shape, std::vector<std::size_t>, TopTools_ShapeMapHasher>;

// The edge as points in the direction of its orientation.
std::vector<Point2d> polylineOf(const TopoDS_Edge& edge) {
    BRepAdaptor_Curve adaptor(edge);
    std::vector<Point2d> points;
    const GCPnts_QuasiUniformDeflection sampler(adaptor, kSketchRegionDeflection,
                                                adaptor.FirstParameter(), adaptor.LastParameter());
    if (!sampler.IsDone() || sampler.NbPoints() < 2) {
        throw GeometryError(GeometryErrorCode::kernelFailure,
                            "sketchRegions: no points of an edge");
    }
    for (int i = 1; i <= sampler.NbPoints(); ++i) {
        const gp_Pnt point = sampler.Value(i);
        points.push_back({point.X(), point.Y()});
    }
    if (edge.Orientation() == TopAbs_REVERSED) {
        std::ranges::reverse(points);
    }
    return points;
}

struct Stretch {
    std::vector<Point2d> points;
    std::vector<std::size_t> curves; ///< the curves it lies on
};

double signedArea(const std::vector<Point2d>& polygon) {
    double sum = 0.0;
    for (std::size_t i = 0; i + 1 < polygon.size(); ++i) {
        sum += polygon[i].x * polygon[i + 1].y - polygon[i + 1].x * polygon[i].y;
    }
    return 0.5 * sum;
}

// Which side of curve `index` the region is on, from the direction the boundary runs in at
// `at` (the region is on the left of the boundary).
bool onLeftOf(const Source& source, const Point2d& at, const Point2d& run) {
    GeomAPI_ProjectPointOnCurve projection(gp_Pnt(at.x, at.y, 0.0), source.curve, source.first,
                                           source.last);
    if (projection.NbPoints() == 0) {
        throw GeometryError(GeometryErrorCode::kernelFailure, "sketchRegions: no direction");
    }
    gp_Pnt point;
    gp_Vec way;
    source.curve->D1(projection.LowerDistanceParameter(), point, way);
    return way.X() * run.x + way.Y() * run.y >= 0.0;
}

// Joins the stretches of one wire into one closed loop; false when they do not close.
bool chain(std::vector<Stretch> stretches, bool wantCounterClockwise,
           const std::vector<Source>& sources, SketchRegionLoop& loop) {
    if (stretches.empty()) {
        return false;
    }
    std::vector<Stretch> order;
    order.push_back(std::move(stretches.front()));
    stretches.erase(stretches.begin());
    while (!stretches.empty()) {
        const Point2d end = order.back().points.back();
        bool found = false;
        for (std::size_t i = 0; i < stretches.size(); ++i) {
            Stretch& candidate = stretches[i];
            if (distance(candidate.points.front(), end) <= kJoin) {
                found = true;
            } else if (distance(candidate.points.back(), end) <= kJoin) {
                std::ranges::reverse(candidate.points);
                found = true;
            }
            if (found) {
                order.push_back(std::move(candidate));
                stretches.erase(stretches.begin() + static_cast<std::ptrdiff_t>(i));
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    if (distance(order.front().points.front(), order.back().points.back()) > kJoin) {
        return false;
    }
    // The seam of a circle is no corner: following stretches on the same curves are one piece.
    for (Stretch& stretch : order) {
        std::ranges::sort(stretch.curves);
    }
    std::vector<Stretch> merged;
    for (Stretch& stretch : order) {
        if (!merged.empty() && merged.back().curves == stretch.curves) {
            merged.back().points.insert(merged.back().points.end(), stretch.points.begin() + 1,
                                        stretch.points.end());
        } else {
            merged.push_back(std::move(stretch));
        }
    }
    if (merged.size() > 1 && merged.front().curves == merged.back().curves) {
        merged.back().points.insert(merged.back().points.end(), merged.front().points.begin() + 1,
                                    merged.front().points.end());
        merged.erase(merged.begin());
    }
    order = std::move(merged);
    std::vector<Point2d> all;
    for (const Stretch& stretch : order) {
        all.insert(all.end(), stretch.points.begin(), stretch.points.end());
    }
    if ((signedArea(all) > 0.0) != wantCounterClockwise) {
        std::ranges::reverse(order);
        for (Stretch& stretch : order) {
            std::ranges::reverse(stretch.points);
        }
    }
    loop = {};
    for (const Stretch& stretch : order) {
        SketchRegionPiece piece;
        piece.firstPoint = loop.polygon.empty() ? 0 : loop.polygon.size() - 1;
        if (loop.polygon.empty()) {
            loop.polygon.push_back(stretch.points.front());
        } else {
            loop.polygon.back() = stretch.points.front(); // the ends are one point
        }
        loop.polygon.insert(loop.polygon.end(), stretch.points.begin() + 1, stretch.points.end());
        piece.lastPoint = loop.polygon.size() - 1;
        const std::size_t middle = (piece.firstPoint + piece.lastPoint + 1) / 2;
        const Point2d at = loop.polygon[middle];
        const Point2d before = loop.polygon[middle - 1];
        const Point2d run{at.x - before.x, at.y - before.y};
        for (const std::size_t curve : stretch.curves) {
            piece.sides.push_back({curve, onLeftOf(sources[curve], at, run)});
        }
        loop.pieces.push_back(std::move(piece));
    }
    loop.polygon.back() = loop.polygon.front();
    return true;
}

// A point inside the region: the middle of the widest stretch of a few horizontal cuts.
Point2d insidePoint(const SketchRegion& region, const Point2d& centroid) {
    double minY = region.outer.polygon.front().y;
    double maxY = minY;
    for (const Point2d& point : region.outer.polygon) {
        minY = std::min(minY, point.y);
        maxY = std::max(maxY, point.y);
    }
    std::vector<double> cuts{centroid.y};
    for (int i = 1; i <= 9; ++i) {
        cuts.push_back(minY + (maxY - minY) * i / 10.0);
    }
    Point2d best = centroid;
    double widest = -1.0;
    for (const double y : cuts) {
        std::vector<double> xs;
        const auto cross = [&](const SketchRegionLoop& loop) {
            for (std::size_t i = 0; i + 1 < loop.polygon.size(); ++i) {
                const Point2d& a = loop.polygon[i];
                const Point2d& b = loop.polygon[i + 1];
                if ((a.y <= y) != (b.y <= y)) {
                    xs.push_back(a.x + (y - a.y) / (b.y - a.y) * (b.x - a.x));
                }
            }
        };
        cross(region.outer);
        for (const SketchRegionLoop& hole : region.holes) {
            cross(hole);
        }
        std::ranges::sort(xs);
        for (std::size_t i = 0; i + 1 < xs.size(); i += 2) {
            if (xs[i + 1] - xs[i] > widest) {
                widest = xs[i + 1] - xs[i];
                best = {0.5 * (xs[i] + xs[i + 1]), y};
            }
        }
    }
    return best;
}

// The triangles that shade the face; false when the kernel makes none.
bool shade(const TopoDS_Face& face, SketchRegion& region) {
    BRepMesh_IncrementalMesh mesh(face, kSketchRegionDeflection);
    TopLoc_Location location;
    const occ::handle<Poly_Triangulation> triangulation = BRep_Tool::Triangulation(face, location);
    if (triangulation.IsNull() || triangulation->NbTriangles() == 0) {
        return false;
    }
    for (int i = 1; i <= triangulation->NbTriangles(); ++i) {
        int a = 0;
        int b = 0;
        int c = 0;
        triangulation->Triangle(i).Get(a, b, c);
        for (const int node : {a, b, c}) {
            const gp_Pnt point = triangulation->Node(node).Transformed(location.Transformation());
            region.triangles.push_back({point.X(), point.Y()});
        }
    }
    return true;
}

bool atCorner(const TopoDS_Face& face, const std::vector<Point2d>& corners) {
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> vertices;
    TopExp::MapShapes(face, TopAbs_VERTEX, vertices);
    for (int i = 1; i <= vertices.Extent(); ++i) {
        const gp_Pnt point = BRep_Tool::Pnt(TopoDS::Vertex(vertices(i)));
        for (const Point2d& corner : corners) {
            if (std::hypot(point.X() - corner.x, point.Y() - corner.y) < kJoin) {
                return true;
            }
        }
    }
    return false;
}

// The loop of one wire of a face; false when it does not close.
bool loopOf(const TopoDS_Wire& wire, const SourceMap& origin, const std::vector<Source>& sources,
            bool outer, SketchRegionLoop& loop) {
    std::vector<TopoDS_Edge> edges;
    for (TopExp_Explorer it(wire, TopAbs_EDGE); it.More(); it.Next()) {
        edges.push_back(TopoDS::Edge(it.Current()));
    }
    std::vector<Stretch> stretches;
    for (const TopoDS_Edge& edge : edges) {
        if (edge.Orientation() == TopAbs_INTERNAL || edge.Orientation() == TopAbs_EXTERNAL) {
            continue; // a loose end inside the area
        }
        const bool twice = std::ranges::count_if(edges, [&](const TopoDS_Edge& other) {
                               return other.IsSame(edge);
                           }) > 1;
        if (twice) {
            continue; // a spur: walked there and back
        }
        Stretch stretch;
        stretch.points = polylineOf(edge);
        if (const std::vector<std::size_t>* have = origin.Seek(edge)) {
            stretch.curves = *have;
        }
        stretches.push_back(std::move(stretch));
    }
    if (stretches.empty() && !outer) {
        loop = {}; // only loose ends: no hole
        return true;
    }
    return chain(std::move(stretches), outer, sources, loop);
}

// The first half of `sketchRegions` and `regionFaceAt`, shared: one edge per curve, the big
// frame face split by those edges, and the map from every piece of an edge back to the indices
// of the curves it lies on. `op` names the calling operation in error messages.
void splitSketch(const std::vector<SketchCurve>& curves, const char* op,
                 std::vector<Source>& sources, Frame& frame, BRepAlgoAPI_Splitter& splitter,
                 SourceMap& origin) {
    sources.reserve(curves.size());
    for (const SketchCurve& curve : curves) {
        sources.push_back(makeSource(curve, op));
    }
    frame = makeFrame(curves);

    NCollection_List<TopoDS_Shape> arguments;
    arguments.Append(frame.face);
    NCollection_List<TopoDS_Shape> tools;
    for (const Source& source : sources) {
        tools.Append(source.edge);
    }
    splitter.SetArguments(arguments);
    splitter.SetTools(tools);
    splitter.Build();
    if (!splitter.IsDone() || splitter.HasErrors()) {
        throw GeometryError(GeometryErrorCode::kernelFailure, std::string(op) + ": split failed");
    }

    for (std::size_t i = 0; i < sources.size(); ++i) {
        const NCollection_List<TopoDS_Shape>& made = splitter.Modified(sources[i].edge);
        std::vector<TopoDS_Shape> pieces(made.begin(), made.end());
        if (pieces.empty() && !splitter.IsDeleted(sources[i].edge)) {
            pieces.push_back(sources[i].edge);
        }
        for (const TopoDS_Shape& piece : pieces) {
            if (std::vector<std::size_t>* have = origin.ChangeSeek(piece)) {
                have->push_back(i);
            } else {
                origin.Bind(piece, std::vector<std::size_t>{i});
            }
        }
    }
}

} // namespace

SketchRegionResult sketchRegions(const std::vector<SketchCurve>& curves) {
    SketchRegionResult result;
    if (curves.empty()) {
        return result;
    }
    try {
        std::vector<Source> sources;
        Frame frame;
        BRepAlgoAPI_Splitter splitter;
        SourceMap origin;
        splitSketch(curves, "sketchRegions", sources, frame, splitter, origin);

        for (TopExp_Explorer it(splitter.Shape(), TopAbs_FACE); it.More(); it.Next()) {
            const TopoDS_Face face = TopoDS::Face(it.Current());
            if (atCorner(face, frame.corners)) {
                continue; // the rest of the plane
            }
            GProp_GProps properties;
            BRepGProp::SurfaceProperties(face, properties);
            const double area = std::abs(properties.Mass());
            if (!(area >= kSketchRegionMinArea)) {
                continue;
            }
            if (!BRepCheck_Analyzer(face).IsValid()) {
                ++result.rejected;
                continue;
            }
            SketchRegion region;
            region.area = area;
            region.centroid = {properties.CentreOfMass().X(), properties.CentreOfMass().Y()};
            const TopoDS_Wire outerWire = BRepTools::OuterWire(face);
            bool closed = true;
            for (TopExp_Explorer wires(face, TopAbs_WIRE); wires.More() && closed; wires.Next()) {
                const TopoDS_Wire wire = TopoDS::Wire(wires.Current());
                SketchRegionLoop loop;
                const bool isOuter = wire.IsSame(outerWire);
                closed = loopOf(wire, origin, sources, isOuter, loop);
                if (closed) {
                    if (isOuter) {
                        region.outer = std::move(loop);
                    } else if (!loop.polygon.empty()) {
                        region.holes.push_back(std::move(loop));
                    }
                }
            }
            if (!closed || region.outer.polygon.empty()) {
                ++result.rejected;
                continue;
            }
            region.inside = insidePoint(region, region.centroid);
            if (!shade(face, region)) {
                ++result.rejected;
                continue;
            }
            result.regions.push_back(std::move(region));
        }
    } catch (const Standard_Failure&) {
        throw GeometryError(GeometryErrorCode::kernelFailure, "sketchRegions: the kernel failed");
    }
    std::ranges::sort(result.regions, [](const SketchRegion& a, const SketchRegion& b) {
        if (std::abs(a.area - b.area) > 1e-9) {
            return a.area < b.area;
        }
        if (std::abs(a.centroid.x - b.centroid.x) > 1e-9) {
            return a.centroid.x < b.centroid.x;
        }
        return a.centroid.y < b.centroid.y;
    });
    return result;
}

std::optional<RegionFace> regionFaceAt(const std::vector<SketchCurve>& curves, Point2d inside) {
    if (curves.empty()) {
        return std::nullopt;
    }
    try {
        std::vector<Source> sources;
        Frame frame;
        BRepAlgoAPI_Splitter splitter;
        SourceMap origin;
        splitSketch(curves, "extrude", sources, frame, splitter, origin);

        for (TopExp_Explorer it(splitter.Shape(), TopAbs_FACE); it.More(); it.Next()) {
            const TopoDS_Face face = TopoDS::Face(it.Current());
            if (atCorner(face, frame.corners)) {
                continue; // the rest of the plane
            }
            GProp_GProps properties;
            BRepGProp::SurfaceProperties(face, properties);
            if (!(std::abs(properties.Mass()) >= kSketchRegionMinArea)) {
                continue;
            }
            BRepClass_FaceClassifier classifier(face, gp_Pnt(inside.x, inside.y, 0.0), 1e-7);
            if (classifier.State() == TopAbs_IN) {
                RegionFace region;
                region.face = face;
                for (TopExp_Explorer e(face, TopAbs_EDGE); e.More(); e.Next()) {
                    const TopoDS_Edge edge = TopoDS::Edge(e.Current());
                    if (const std::vector<std::size_t>* indices = origin.Seek(edge)) {
                        region.edgeCurves.push_back({edge, *indices});
                    }
                }
                return region;
            }
        }
    } catch (const Standard_Failure&) {
        throw GeometryError(GeometryErrorCode::kernelFailure, "extrude: the kernel failed");
    }
    return std::nullopt;
}

} // namespace fission::geo::occt
