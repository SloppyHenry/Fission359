// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// An edge of a body taken over into a sketch (AP-2.1f): projected perpendicularly onto the sketch
// plane, exactly. A straight edge stays a line; a circle or arc whose axis stands perpendicular to
// the plane stays a circle or arc. Everything else (a tilted circle, which would be an ellipse, a
// free-form edge) is reported as "not exact": a curve through projected points would look right
// and be wrong.

#include "fission/geo/occt/sketch_edge.h"

#include <cmath>
#include <numbers>

#include "fission/geo/error.h"

#include "occt_build.h"
#include <BRepAdaptor_Curve.hxx>
#include <BRep_Tool.hxx>
#include <GeomAbs_CurveType.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS_Edge.hxx>
#include <gp_Circ.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>

namespace fission::geo::occt {

namespace {

constexpr double kMinLength = 1e-9; // a projection shorter than this is a point

double dot(const Point3d& a, const Point3d& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Point3d cross(const Point3d& a, const Point3d& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

bool usable(const SketchFrame& frame) {
    const auto unit = [](const Point3d& v) {
        return isFinite(v) && std::abs(dot(v, v) - 1.0) < 1e-6;
    };
    return isFinite(frame.origin) && unit(frame.u) && unit(frame.v) &&
           std::abs(dot(frame.u, frame.v)) < 1e-6;
}

Point2d toPlane(const SketchFrame& frame, const gp_Pnt& point) {
    const Point3d d{point.X() - frame.origin.x, point.Y() - frame.origin.y,
                    point.Z() - frame.origin.z};
    return {dot(d, frame.u), dot(d, frame.v)};
}

} // namespace

SketchEdgeShape projectEdgeToSketch(const Solid& solid, std::uint32_t edgeId,
                                    const SketchFrame& frame) {
    const auto* data = dynamic_cast<const OcctSolidData*>(solid.data());
    if (data == nullptr) {
        throw GeometryError(
            GeometryErrorCode::invalidInput,
            "projectEdgeToSketch: a body is empty or was not created by this kernel", "edge");
    }
    if (!usable(frame)) {
        throw GeometryError(GeometryErrorCode::invalidInput,
                            "projectEdgeToSketch: the plane is not usable", "plane");
    }
    try {
        ShapeMap edges;
        TopExp::MapShapes(data->shape(), TopAbs_EDGE, edges);
        if (edgeId < 1 || static_cast<int>(edgeId) > edges.Extent()) {
            throw GeometryError(GeometryErrorCode::invalidInput,
                                "projectEdgeToSketch: the edge is not there", "edge");
        }
        const TopoDS_Edge edge = TopoDS::Edge(edges(static_cast<int>(edgeId)));
        SketchEdgeShape shape;
        if (BRep_Tool::Degenerated(edge)) {
            return shape; // notExact
        }
        const BRepAdaptor_Curve curve(edge);
        const Point3d normal = cross(frame.u, frame.v);
        const double first = curve.FirstParameter();
        const double last = curve.LastParameter();
        switch (curve.GetType()) {
        case GeomAbs_Line: {
            const Point2d from = toPlane(frame, curve.Value(first));
            const Point2d to = toPlane(frame, curve.Value(last));
            if (std::hypot(to.x - from.x, to.y - from.y) < kMinLength) {
                shape.status = SketchEdgeStatus::perpendicular;
                return shape;
            }
            shape.status = SketchEdgeStatus::ok;
            shape.kind = SketchCurveKind::line;
            shape.points = {from, to};
            return shape;
        }
        case GeomAbs_Circle: {
            const gp_Circ circle = curve.Circle();
            const gp_Dir axis = circle.Axis().Direction();
            const Point3d way{axis.X(), axis.Y(), axis.Z()};
            const double alongNormal = dot(way, normal);
            const Point3d sideways = cross(way, normal);
            if (std::sqrt(dot(sideways, sideways)) > kSketchEdgeAxisTolerance) {
                return shape; // notExact: an ellipse (or an edge-on circle)
            }
            const Point2d middle = toPlane(frame, circle.Location());
            const Point2d begin = toPlane(frame, curve.Value(first));
            const Point2d finish = toPlane(frame, curve.Value(last));
            shape.status = SketchEdgeStatus::ok;
            if (std::abs((last - first) - 2.0 * std::numbers::pi) < 1e-9) {
                shape.kind = SketchCurveKind::circle;
                shape.points = {middle, begin};
            } else {
                shape.kind = SketchCurveKind::arc;
                // The parameter runs counter-clockwise about the circle's own axis; on the plane
                // that is the other way round when the axis looks against the plane's normal.
                shape.points = alongNormal > 0.0 ? std::vector<Point2d>{middle, begin, finish}
                                                 : std::vector<Point2d>{middle, finish, begin};
            }
            return shape;
        }
        default:
            return shape; // notExact
        }
    } catch (const Standard_Failure&) {
        throw GeometryError(GeometryErrorCode::kernelFailure,
                            "projectEdgeToSketch: the kernel failed");
    }
}

} // namespace fission::geo::occt
