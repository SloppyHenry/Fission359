// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// What the faces and edges of a body look like (ADR-0003). The measures follow the prototype
// in tests/naming/, whose thresholds were measured with exactly these values.

#include <algorithm>
#include <cmath>
#include <vector>

#include "fission/geo/error.h"
#include "fission/geo/occt/solids.h"

#include "occt_build.h"
#include <BRepAdaptor_Curve.hxx>
#include <BRepGProp.hxx>
#include <BRep_Tool.hxx>
#include <GeomAbs_CurveType.hxx>
#include <NCollection_IndexedDataMap.hxx>
#include <NCollection_List.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS_Edge.hxx>

namespace fission::geo::occt {

namespace {

constexpr double kAxisEpsilon = 1e-9;

// Axes and line directions have no sign: pick the one that points towards +X, then +Y, then +Z.
gp_Dir canonicalAxis(const gp_Dir& dir) {
    if (dir.X() < -kAxisEpsilon || (std::abs(dir.X()) <= kAxisEpsilon && dir.Y() < -kAxisEpsilon) ||
        (std::abs(dir.X()) <= kAxisEpsilon && std::abs(dir.Y()) <= kAxisEpsilon && dir.Z() < 0.0)) {
        return dir.Reversed();
    }
    return dir;
}

Point3d toPoint(const gp_Pnt& point) {
    return {point.X(), point.Y(), point.Z()};
}

Point3d toPoint(const gp_Dir& dir) {
    return {dir.X(), dir.Y(), dir.Z()};
}

SignatureShape shapeOfSurface(GeomAbs_SurfaceType type) {
    switch (type) {
    case GeomAbs_Plane:
        return SignatureShape::plane;
    case GeomAbs_Cylinder:
        return SignatureShape::cylinder;
    case GeomAbs_Cone:
        return SignatureShape::cone;
    case GeomAbs_Sphere:
        return SignatureShape::sphere;
    case GeomAbs_Torus:
        return SignatureShape::torus;
    default:
        return SignatureShape::freeform;
    }
}

SignatureShape shapeOfCurve(GeomAbs_CurveType type) {
    switch (type) {
    case GeomAbs_Line:
        return SignatureShape::line;
    case GeomAbs_Circle:
        return SignatureShape::circle;
    default:
        return SignatureShape::curve;
    }
}

ElementSignature describeFace(const TopoDS_Face& face, std::uint32_t id) {
    ElementSignature sig;
    sig.id = id;
    sig.kind = SignatureKind::face;
    GProp_GProps props;
    BRepGProp::SurfaceProperties(face, props);
    sig.centroid = toPoint(props.CentreOfMass());
    sig.size = props.Mass();
    const BRepAdaptor_Surface surface(face);
    sig.shape = shapeOfSurface(surface.GetType());
    if (surface.GetType() == GeomAbs_Plane) {
        sig.direction = toPoint(outwardNormal(face));
        sig.hasDirection = true;
        sig.directed = true;
    } else if (surface.GetType() == GeomAbs_Cylinder) {
        sig.direction = toPoint(canonicalAxis(surface.Cylinder().Axis().Direction()));
        sig.hasDirection = true;
        sig.radius = surface.Cylinder().Radius();
    }
    return sig;
}

ElementSignature describeEdge(const TopoDS_Edge& edge, std::uint32_t id) {
    ElementSignature sig;
    sig.id = id;
    sig.kind = SignatureKind::edge;
    GProp_GProps props;
    BRepGProp::LinearProperties(edge, props);
    sig.centroid = toPoint(props.CentreOfMass());
    sig.size = props.Mass();
    const BRepAdaptor_Curve curve(edge);
    sig.shape = shapeOfCurve(curve.GetType());
    if (curve.GetType() == GeomAbs_Line) {
        sig.direction = toPoint(canonicalAxis(curve.Line().Direction()));
        sig.hasDirection = true;
    } else if (curve.GetType() == GeomAbs_Circle) {
        sig.direction = toPoint(canonicalAxis(curve.Circle().Axis().Direction()));
        sig.hasDirection = true;
        sig.radius = curve.Circle().Radius();
    }
    return sig;
}

void sortUnique(std::vector<std::uint32_t>& numbers) {
    std::sort(numbers.begin(), numbers.end());
    numbers.erase(std::unique(numbers.begin(), numbers.end()), numbers.end());
}

} // namespace

BodySignatures signatures(const Solid& solid) {
    const auto* data = dynamic_cast<const OcctSolidData*>(solid.data());
    if (data == nullptr) {
        throw GeometryError(GeometryErrorCode::invalidInput,
                            "signatures: a body is empty or was not created by this kernel");
    }
    try {
        const TopoDS_Shape& shape = data->shape();
        BodySignatures result;
        result.diagonal = bounds(solid).diagonal();

        ShapeMap faces;
        TopExp::MapShapes(shape, TopAbs_FACE, faces);
        ShapeMap edges;
        TopExp::MapShapes(shape, TopAbs_EDGE, edges);
        NCollection_IndexedDataMap<TopoDS_Shape, NCollection_List<TopoDS_Shape>,
                                   TopTools_ShapeMapHasher>
            facesOfEdge;
        TopExp::MapShapesAndAncestors(shape, TopAbs_EDGE, TopAbs_FACE, facesOfEdge);

        for (int i = 1; i <= faces.Extent(); ++i) {
            result.faces.push_back(
                describeFace(TopoDS::Face(faces(i)), static_cast<std::uint32_t>(i)));
        }
        for (int i = 1; i <= edges.Extent(); ++i) {
            const TopoDS_Edge edge = TopoDS::Edge(edges(i));
            if (BRep_Tool::Degenerated(edge)) {
                continue;
            }
            ElementSignature sig = describeEdge(edge, static_cast<std::uint32_t>(i));
            for (const TopoDS_Shape& face : facesOfEdge.FindFromKey(edge)) {
                const int number = faces.FindIndex(face);
                if (number > 0) {
                    sig.faces.push_back(static_cast<std::uint32_t>(number));
                }
            }
            sortUnique(sig.faces);
            // The neighbours of a face are the other faces on its edges.
            for (const std::uint32_t a : sig.faces) {
                for (const std::uint32_t b : sig.faces) {
                    if (a != b) {
                        result.faces[a - 1].faces.push_back(b);
                    }
                }
            }
            result.edges.push_back(std::move(sig));
        }
        for (ElementSignature& face : result.faces) {
            sortUnique(face.faces);
        }
        return result;
    } catch (const Standard_Failure&) {
        throw GeometryError(GeometryErrorCode::kernelFailure, "signatures: the kernel failed");
    }
}

} // namespace fission::geo::occt
