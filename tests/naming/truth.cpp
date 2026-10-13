// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "truth.h"

#include <cmath>

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRep_Tool.hxx>
#include <GeomAbs_CurveType.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <NCollection_IndexedMap.hxx>
#include <TopAbs_Orientation.hxx>
#include <TopExp.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Vertex.hxx>
#include <gp_Lin.hxx>
#include <gp_Vec.hxx>

namespace fission::test::naming::truth {

namespace {

using ShapeMap = NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher>;

double distanceToPlane(const gp_Pnt& p, const gp_Pnt& point, const gp_Dir& normal) {
    return std::abs(gp_Vec(point, p).Dot(gp_Vec(normal)));
}

bool parallel(const gp_Dir& a, const gp_Dir& b) {
    return std::abs(std::abs(a.Dot(b)) - 1.0) < 1e-7;
}

} // namespace

Shapes planarFaces(const TopoDS_Shape& shape, const gp_Pnt& point, const gp_Dir& outward) {
    ShapeMap faces;
    TopExp::MapShapes(shape, TopAbs_FACE, faces);
    Shapes found;
    for (int i = 1; i <= faces.Extent(); ++i) {
        const TopoDS_Face face = TopoDS::Face(faces(i));
        const BRepAdaptor_Surface surface(face);
        if (surface.GetType() != GeomAbs_Plane) {
            continue;
        }
        gp_Dir normal = surface.Plane().Axis().Direction();
        if (face.Orientation() == TopAbs_REVERSED) {
            normal.Reverse();
        }
        if (normal.Dot(outward) > 1.0 - 1e-7 &&
            distanceToPlane(surface.Plane().Location(), point, outward) < kTolerance) {
            found.push_back(face);
        }
    }
    return found;
}

Shapes straightEdgesBetween(const TopoDS_Shape& shape, const gp_Pnt& point1, const gp_Dir& normal1,
                            const gp_Pnt& point2, const gp_Dir& normal2) {
    ShapeMap edges;
    TopExp::MapShapes(shape, TopAbs_EDGE, edges);
    Shapes found;
    for (int i = 1; i <= edges.Extent(); ++i) {
        const TopoDS_Edge edge = TopoDS::Edge(edges(i));
        if (BRepAdaptor_Curve(edge).GetType() != GeomAbs_Line) {
            continue;
        }
        TopoDS_Vertex first;
        TopoDS_Vertex last;
        TopExp::Vertices(edge, first, last);
        bool inBoth = true;
        for (const TopoDS_Vertex& vertex : {first, last}) {
            const gp_Pnt p = BRep_Tool::Pnt(vertex);
            inBoth = inBoth && distanceToPlane(p, point1, normal1) < kTolerance &&
                     distanceToPlane(p, point2, normal2) < kTolerance;
        }
        if (inBoth) {
            found.push_back(edge);
        }
    }
    return found;
}

Shapes cylinderFaces(const TopoDS_Shape& shape, const gp_Pnt& point, const gp_Dir& axis,
                     double radius) {
    ShapeMap faces;
    TopExp::MapShapes(shape, TopAbs_FACE, faces);
    const gp_Lin line(point, axis);
    Shapes found;
    for (int i = 1; i <= faces.Extent(); ++i) {
        const BRepAdaptor_Surface surface(TopoDS::Face(faces(i)));
        if (surface.GetType() != GeomAbs_Cylinder) {
            continue;
        }
        const gp_Cylinder cylinder = surface.Cylinder();
        if (parallel(cylinder.Axis().Direction(), axis) &&
            line.Distance(cylinder.Location()) < kTolerance &&
            std::abs(cylinder.Radius() - radius) < kTolerance) {
            found.push_back(faces(i));
        }
    }
    return found;
}

Shapes circleEdges(const TopoDS_Shape& shape, const gp_Pnt& center, const gp_Dir& axis,
                   double radius) {
    ShapeMap edges;
    TopExp::MapShapes(shape, TopAbs_EDGE, edges);
    Shapes found;
    for (int i = 1; i <= edges.Extent(); ++i) {
        const BRepAdaptor_Curve curve(TopoDS::Edge(edges(i)));
        if (curve.GetType() != GeomAbs_Circle) {
            continue;
        }
        const gp_Circ circle = curve.Circle();
        if (parallel(circle.Axis().Direction(), axis) &&
            circle.Location().Distance(center) < kTolerance &&
            std::abs(circle.Radius() - radius) < kTolerance) {
            found.push_back(edges(i));
        }
    }
    return found;
}

bool contains(const Shapes& shapes, const TopoDS_Shape& candidate) {
    for (const TopoDS_Shape& shape : shapes) {
        if (shape.IsSame(candidate)) {
            return true;
        }
    }
    return false;
}

} // namespace fission::test::naming::truth
