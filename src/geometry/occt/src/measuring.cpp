// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Measuring corners, edges and faces (AP-2.11).

#include "fission/geo/occt/measuring.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "fission/geo/error.h"

#include "occt_build.h"
#include <BRepAdaptor_Curve.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepGProp.hxx>
#include <BRep_Tool.hxx>
#include <GProp_GProps.hxx>
#include <GeomAbs_CurveType.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Vertex.hxx>
#include <gp_Vec.hxx>

namespace fission::geo::occt {

namespace {

constexpr double kRadiansToDegrees = 180.0 / std::numbers::pi;

Point3d toPoint(const gp_Pnt& p) {
    return {p.X(), p.Y(), p.Z()};
}

TopoDS_Shape elementOf(const ElementRef& ref) {
    const auto* data = dynamic_cast<const OcctSolidData*>(ref.solid.data());
    if (data == nullptr) {
        throw GeometryError(GeometryErrorCode::invalidInput, "not a body of this kernel");
    }
    const TopAbs_ShapeEnum type = ref.kind == ElementKind::corner ? TopAbs_VERTEX
                                  : ref.kind == ElementKind::edge ? TopAbs_EDGE
                                                                  : TopAbs_FACE;
    ShapeMap map;
    TopExp::MapShapes(data->shape(), type, map);
    if (ref.number < 1 || ref.number > static_cast<std::uint32_t>(map.Extent())) {
        throw GeometryError(GeometryErrorCode::invalidInput, "no such element");
    }
    return map(static_cast<int>(ref.number));
}

// Angle in degrees between two directions, 0 to 180.
double between(const gp_Vec& a, const gp_Vec& b) {
    const double cosine = std::clamp(a.Normalized().Dot(b.Normalized()), -1.0, 1.0);
    return std::acos(cosine) * kRadiansToDegrees;
}

bool isStraight(const TopoDS_Shape& shape) {
    return shape.ShapeType() == TopAbs_EDGE &&
           BRepAdaptor_Curve(TopoDS::Edge(shape)).GetType() == GeomAbs_Line;
}

bool isFlatFace(const TopoDS_Shape& shape) {
    return shape.ShapeType() == TopAbs_FACE && isFlat(TopoDS::Face(shape));
}

gp_Vec directionOf(const TopoDS_Shape& edge) {
    const BRepAdaptor_Curve curve(TopoDS::Edge(edge));
    const gp_Vec along(curve.Value(curve.FirstParameter()), curve.Value(curve.LastParameter()));
    return along;
}

// Do the two faces have an edge in common (in one body)?
bool shareEdge(const TopoDS_Shape& a, const TopoDS_Shape& b) {
    ShapeMap edgesOfA;
    TopExp::MapShapes(a, TopAbs_EDGE, edgesOfA);
    for (TopExp_Explorer edges(b, TopAbs_EDGE); edges.More(); edges.Next()) {
        if (edgesOfA.Contains(edges.Current())) {
            return true;
        }
    }
    return false;
}

void single(const TopoDS_Shape& shape, Measurement& out) {
    if (shape.ShapeType() == TopAbs_VERTEX) {
        out.position = toPoint(BRep_Tool::Pnt(TopoDS::Vertex(shape)));
    } else if (shape.ShapeType() == TopAbs_EDGE) {
        const TopoDS_Edge edge = TopoDS::Edge(shape);
        GProp_GProps properties;
        BRepGProp::LinearProperties(edge, properties);
        out.length = properties.Mass();
        const BRepAdaptor_Curve curve(edge);
        if (curve.GetType() == GeomAbs_Circle) {
            out.radius = curve.Circle().Radius();
        }
        out.span = std::array<Point3d, 2>{toPoint(curve.Value(curve.FirstParameter())),
                                          toPoint(curve.Value(curve.LastParameter()))};
    } else {
        const TopoDS_Face face = TopoDS::Face(shape);
        GProp_GProps properties;
        BRepGProp::SurfaceProperties(face, properties);
        out.area = properties.Mass();
        const BRepAdaptor_Surface surface(face);
        if (surface.GetType() == GeomAbs_Cylinder) {
            out.radius = surface.Cylinder().Radius();
        } else if (surface.GetType() == GeomAbs_Sphere) {
            out.radius = surface.Sphere().Radius();
        }
    }
}

void pair(const TopoDS_Shape& a, const TopoDS_Shape& b, bool sameBody, Measurement& out) {
    BRepExtrema_DistShapeShape distance(a, b);
    if (!distance.IsDone() || distance.NbSolution() < 1) {
        throw GeometryError(GeometryErrorCode::kernelFailure, "the distance could not be found");
    }
    out.distance = distance.Value();
    out.span = std::array<Point3d, 2>{toPoint(distance.PointOnShape1(1)),
                                      toPoint(distance.PointOnShape2(1))};

    if (isStraight(a) && isStraight(b)) {
        const double angle = between(directionOf(a), directionOf(b));
        out.angle = std::min(angle, 180.0 - angle);
    } else if (isFlatFace(a) && isFlatFace(b)) {
        const gp_Vec na(outwardNormal(TopoDS::Face(a)));
        const gp_Vec nb(outwardNormal(TopoDS::Face(b)));
        const double apart = between(na, nb);
        out.angle = sameBody && shareEdge(a, b) ? 180.0 - apart : std::min(apart, 180.0 - apart);
    } else if ((isStraight(a) && isFlatFace(b)) || (isFlatFace(a) && isStraight(b))) {
        const TopoDS_Shape& edge = isStraight(a) ? a : b;
        const TopoDS_Shape& face = isStraight(a) ? b : a;
        const double toNormal =
            between(directionOf(edge), gp_Vec(outwardNormal(TopoDS::Face(face))));
        out.angle = std::abs(90.0 - std::min(toNormal, 180.0 - toNormal));
    }
}

} // namespace

Measurement measureElements(const std::vector<ElementRef>& elements) {
    Measurement out;
    if (elements.size() == 1) {
        single(elementOf(elements.front()), out);
    } else if (elements.size() == 2) {
        const TopoDS_Shape a = elementOf(elements[0]);
        const TopoDS_Shape b = elementOf(elements[1]);
        pair(a, b, elements[0].solid.data() == elements[1].solid.data(), out);
    } else {
        throw GeometryError(GeometryErrorCode::invalidInput, "measure one or two elements");
    }
    return out;
}

} // namespace fission::geo::occt
