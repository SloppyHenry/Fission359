// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/geo/occt/solids.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "fission/geo/error.h"

#include "occt_build.h"
#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_Copy.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepLProp_SLProps.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <GCPnts_TangentialDeflection.hxx>
#include <GProp_GProps.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <NCollection_IndexedDataMap.hxx>
#include <NCollection_IndexedMap.hxx>
#include <NCollection_List.hxx>
#include <Poly_Triangulation.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopLoc_Location.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Vertex.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <gp_Dir.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

namespace fission::geo::occt {

namespace {

using AncestorMap = NCollection_IndexedDataMap<TopoDS_Shape, NCollection_List<TopoDS_Shape>,
                                               TopTools_ShapeMapHasher>;

const TopoDS_Shape& shapeOf(const Solid& solid) {
    const auto* data = dynamic_cast<const OcctSolidData*>(solid.data());
    if (data == nullptr) {
        throw GeometryError(GeometryErrorCode::invalidInput,
                            "the body is empty or was not created by the OpenCASCADE kernel");
    }
    return data->shape();
}

Point3f toFloat(const gp_Pnt& point) {
    return {static_cast<float>(point.X()), static_cast<float>(point.Y()),
            static_cast<float>(point.Z())};
}

Point3f toFloat(const gp_Dir& direction) {
    return {static_cast<float>(direction.X()), static_cast<float>(direction.Y()),
            static_cast<float>(direction.Z())};
}

// Unit normal of a triangle (counter-clockwise corners), used where the surface has none.
Point3f triangleNormal(const gp_Pnt& a, const gp_Pnt& b, const gp_Pnt& c) {
    const gp_Vec ab(a, b);
    const gp_Vec ac(a, c);
    gp_Vec normal = ab.Crossed(ac);
    if (normal.Magnitude() <= 1e-300) {
        return {0.0F, 0.0F, 1.0F};
    }
    normal.Normalize();
    return {static_cast<float>(normal.X()), static_cast<float>(normal.Y()),
            static_cast<float>(normal.Z())};
}

void appendFace(const TopoDS_Face& face, std::uint32_t faceId, TriangleMesh& mesh) {
    TopLoc_Location location;
    const occ::handle<Poly_Triangulation>& triangulation = BRep_Tool::Triangulation(face, location);
    if (triangulation.IsNull() || triangulation->NbTriangles() == 0) {
        throw GeometryError(GeometryErrorCode::kernelFailure, "a face could not be tessellated");
    }

    const bool reversed = face.Orientation() == TopAbs_REVERSED;
    const gp_Trsf transform = location.Transformation();
    const BRepAdaptor_Surface surface(face);
    const bool hasUv = triangulation->HasUVNodes();
    const auto base = static_cast<std::uint32_t>(mesh.positions.size());

    for (int i = 1; i <= triangulation->NbNodes(); ++i) {
        const gp_Pnt point = triangulation->Node(i).Transformed(transform);
        mesh.positions.push_back(toFloat(point));

        // Rounded surfaces: the true surface normal at the node, so the shading is smooth.
        // Flat or degenerate spots use the normal of the first triangle that touches the node
        // (filled in below).
        Point3f normal{0.0F, 0.0F, 0.0F};
        if (hasUv) {
            const gp_Pnt2d uv = triangulation->UVNode(i);
            BRepLProp_SLProps properties(surface, uv.X(), uv.Y(), 1, 1e-7);
            if (properties.IsNormalDefined()) {
                gp_Dir direction = properties.Normal();
                if (reversed) {
                    direction.Reverse();
                }
                normal = toFloat(direction);
            }
        }
        mesh.normals.push_back(normal);
    }

    for (int t = 1; t <= triangulation->NbTriangles(); ++t) {
        int n1 = 0;
        int n2 = 0;
        int n3 = 0;
        triangulation->Triangle(t).Get(n1, n2, n3);
        if (reversed) {
            std::swap(n2, n3); // keep counter-clockwise seen from outside
        }
        const auto a = static_cast<std::uint32_t>(n1 - 1);
        const auto b = static_cast<std::uint32_t>(n2 - 1);
        const auto c = static_cast<std::uint32_t>(n3 - 1);
        mesh.indices.insert(mesh.indices.end(), {base + a, base + b, base + c});
        mesh.faceIds.push_back(faceId);

        // Fill normals that the surface could not provide from the triangle.
        const Point3f flat = triangleNormal(triangulation->Node(n1).Transformed(transform),
                                            triangulation->Node(n2).Transformed(transform),
                                            triangulation->Node(n3).Transformed(transform));
        for (const std::uint32_t index : {base + a, base + b, base + c}) {
            Point3f& normal = mesh.normals[index];
            if (normal.x == 0.0F && normal.y == 0.0F && normal.z == 0.0F) {
                normal = flat;
            }
        }
    }
}

} // namespace

namespace {

// A box whose four vertical edges are rounded: the outline (straight pieces and quarter circles,
// counter-clockwise) pulled up. The radius stays a true radius whatever the sides are (rule 11).
TopoDS_Shape roundedBoxShape(double sizeX, double sizeY, double sizeZ, const Point3d& corner,
                             double radius) {
    const double x0 = corner.x;
    const double y0 = corner.y;
    const double x1 = corner.x + sizeX;
    const double y1 = corner.y + sizeY;
    const double z = corner.z;
    constexpr double kHalfPi = 1.57079632679489661923;
    constexpr double kMinPiece = 1e-9;

    BRepBuilderAPI_MakeWire outline;
    const auto line = [&](double ax, double ay, double bx, double by) {
        if (std::hypot(bx - ax, by - ay) > kMinPiece) {
            outline.Add(BRepBuilderAPI_MakeEdge(gp_Pnt(ax, ay, z), gp_Pnt(bx, by, z)).Edge());
        }
    };
    const auto arc = [&](double cx, double cy, double from) {
        const gp_Circ circle(gp_Ax2(gp_Pnt(cx, cy, z), gp_Dir(0.0, 0.0, 1.0)), radius);
        outline.Add(BRepBuilderAPI_MakeEdge(circle, from, from + kHalfPi).Edge());
    };
    line(x0 + radius, y0, x1 - radius, y0);
    arc(x1 - radius, y0 + radius, -kHalfPi);
    line(x1, y0 + radius, x1, y1 - radius);
    arc(x1 - radius, y1 - radius, 0.0);
    line(x1 - radius, y1, x0 + radius, y1);
    arc(x0 + radius, y1 - radius, kHalfPi);
    line(x0, y1 - radius, x0, y0 + radius);
    arc(x0 + radius, y0 + radius, 2.0 * kHalfPi);

    const TopoDS_Face base = BRepBuilderAPI_MakeFace(outline.Wire(), /*OnlyPlane=*/true).Face();
    return BRepPrimAPI_MakePrism(base, gp_Vec(0.0, 0.0, sizeZ)).Shape();
}

} // namespace

NamedSolid makeNamedBox(double sizeX, double sizeY, double sizeZ, Point3d corner,
                        double cornerRadius) {
    if (!isPositiveFinite(sizeX) || !isPositiveFinite(sizeY) || !isPositiveFinite(sizeZ) ||
        !isFinite(corner)) {
        throw GeometryError(GeometryErrorCode::invalidInput, "box: sizes must be positive numbers");
    }
    if (!std::isfinite(cornerRadius) || cornerRadius < 0.0 ||
        cornerRadius > std::min(sizeX, sizeY) / 2.0 * (1.0 + 1e-12)) {
        throw GeometryError(GeometryErrorCode::invalidInput,
                            "box: the corner radius must be between 0 and half the shorter side");
    }
    try {
        const bool rounded = cornerRadius > 0.0;
        const TopoDS_Shape shape =
            rounded ? roundedBoxShape(sizeX, sizeY, sizeZ, corner, cornerRadius)
                    : BRepPrimAPI_MakeBox(gp_Pnt(corner.x, corner.y, corner.z), sizeX, sizeY, sizeZ)
                          .Shape();

        // The kernel has its own idea of front and left. Ours: looking at the box from the front
        // we look along +Y, so front is -Y, left is -X (as in the view cube).
        const double midX = corner.x + sizeX / 2.0;
        const double midY = corner.y + sizeY / 2.0;
        return nameFaces(shape, "box", [&](const TopoDS_Face& face) -> std::string {
            if (!isFlat(face)) {
                // A rounded corner: named after the quarter of the plan it sits in.
                const gp_Pnt centre = faceCentre(face);
                return std::string("corner") + (centre.Y() < midY ? "Front" : "Back") +
                       (centre.X() < midX ? "Left" : "Right");
            }
            const gp_Dir normal = outwardNormal(face);
            if (normal.Z() > 0.5) {
                return "top";
            }
            if (normal.Z() < -0.5) {
                return "bottom";
            }
            if (normal.Y() > 0.5) {
                return "back";
            }
            if (normal.Y() < -0.5) {
                return "front";
            }
            return normal.X() > 0.5 ? "right" : "left";
        });
    } catch (const Standard_Failure&) {
        throw GeometryError(GeometryErrorCode::kernelFailure, "box: the kernel failed");
    }
}

NamedSolid makeNamedCylinder(double radius, double height, Point3d base) {
    if (!isPositiveFinite(radius) || !isPositiveFinite(height) || !isFinite(base)) {
        throw GeometryError(GeometryErrorCode::invalidInput,
                            "cylinder: radius and height must be positive numbers");
    }
    try {
        const gp_Ax2 axis(gp_Pnt(base.x, base.y, base.z), gp_Dir(0.0, 0.0, 1.0));
        BRepPrimAPI_MakeCylinder maker(axis, radius, height);
        const TopoDS_Shape shape = maker.Shape();

        // The maker does not hand out its faces: tell them apart by what they are. The side is
        // the cylindrical face; of the two flat ones the lower is the bottom.
        return nameFaces(shape, "cylinder", [&](const TopoDS_Face& face) -> std::string {
            if (!isFlat(face)) {
                return "side";
            }
            return faceCentre(face).Z() < base.z + height / 2.0 ? "bottom" : "top";
        });
    } catch (const Standard_Failure&) {
        throw GeometryError(GeometryErrorCode::kernelFailure, "cylinder: the kernel failed");
    }
}

NamedSolid rotateNamed(const NamedSolid& solid, Point3d centre, const Angles& angles) {
    if (!isFinite(centre) || !std::isfinite(angles.x) || !std::isfinite(angles.y) ||
        !std::isfinite(angles.z)) {
        throw GeometryError(GeometryErrorCode::invalidInput, "rotate: not a number");
    }
    if (angles.isZero()) {
        return solid;
    }
    try {
        // About the three axes of the bed through the centre, X first. The copy carries the
        // turned geometry itself (no hidden location), and the faces keep their order.
        const gp_Pnt middle(centre.x, centre.y, centre.z);
        const auto about = [&](const gp_Dir& axis, double degrees) {
            gp_Trsf turn;
            turn.SetRotation(gp_Ax1(middle, axis), degrees / kDegrees);
            return turn;
        };
        gp_Trsf all = about(gp_Dir(0, 0, 1), angles.z);
        all.Multiply(about(gp_Dir(0, 1, 0), angles.y));
        all.Multiply(about(gp_Dir(1, 0, 0), angles.x));
        const TopoDS_Shape turned =
            BRepBuilderAPI_Transform(shapeOf(solid.solid), all, /*Copy=*/true).Shape();
        NamedSolid result;
        result.solid = validated(turned, "rotate");
        result.faces = solid.faces;
        return result;
    } catch (const Standard_Failure&) {
        throw GeometryError(GeometryErrorCode::kernelFailure, "rotate: the kernel failed");
    }
}

Solid makeBox(double sizeX, double sizeY, double sizeZ, Point3d corner, double cornerRadius) {
    return makeNamedBox(sizeX, sizeY, sizeZ, corner, cornerRadius).solid;
}

Solid makeCylinder(double radius, double height, Point3d base) {
    return makeNamedCylinder(radius, height, base).solid;
}

Bounds bounds(const Solid& solid) {
    Bounds result;
    Bnd_Box box;
    BRepBndLib::Add(shapeOf(solid), box, /*useTriangulation=*/false);
    if (!box.IsVoid()) {
        box.Get(result.min.x, result.min.y, result.min.z, result.max.x, result.max.y, result.max.z);
    }
    return result;
}

SolidReport measure(const Solid& solid) {
    const TopoDS_Shape& shape = shapeOf(solid);
    SolidReport report;
    try {
        GProp_GProps volume;
        BRepGProp::VolumeProperties(shape, volume);
        report.volume = volume.Mass();
        GProp_GProps surface;
        BRepGProp::SurfaceProperties(shape, surface);
        report.area = surface.Mass();

        Bnd_Box box;
        BRepBndLib::AddOptimal(shape, box, /*useTriangulation=*/false,
                               /*useShapeTolerance=*/false);
        if (!box.IsVoid()) {
            box.Get(report.boxMin.x, report.boxMin.y, report.boxMin.z, report.boxMax.x,
                    report.boxMax.y, report.boxMax.z);
        }

        const auto count = [&shape](TopAbs_ShapeEnum type) {
            ShapeMap map;
            TopExp::MapShapes(shape, type, map);
            return map.Extent();
        };
        report.solids = count(TopAbs_SOLID);
        report.faces = count(TopAbs_FACE);
        report.edges = count(TopAbs_EDGE);
        report.vertices = count(TopAbs_VERTEX);
        report.valid = BRepCheck_Analyzer(shape).IsValid();
    } catch (const Standard_Failure&) {
        throw GeometryError(GeometryErrorCode::kernelFailure, "measure: the kernel failed");
    }
    return report;
}

TriangleMesh tessellate(const Solid& solid, const TessellationParams& params) {
    if (!isPositiveFinite(params.chordTolerance) || !isPositiveFinite(params.angleTolerance)) {
        throw GeometryError(GeometryErrorCode::invalidInput,
                            "tessellation: tolerances must be positive numbers");
    }
    const TopoDS_Shape& original = shapeOf(solid);
    TriangleMesh mesh;
    try {
        // The kernel stores the triangles inside the shape. Work on a copy so that the body
        // other threads may be reading stays untouched.
        const TopoDS_Shape shape = BRepBuilderAPI_Copy(original, /*copyGeom=*/false).Shape();
        BRepMesh_IncrementalMesh mesher(shape, params.chordTolerance, /*isRelative=*/false,
                                        params.angleTolerance, /*isInParallel=*/true);
        if (!mesher.IsDone()) {
            throw GeometryError(GeometryErrorCode::kernelFailure, "tessellation failed");
        }

        ShapeMap faces;
        TopExp::MapShapes(shape, TopAbs_FACE, faces);
        for (int i = 1; i <= faces.Extent(); ++i) {
            appendFace(TopoDS::Face(faces(i)), static_cast<std::uint32_t>(i), mesh);
        }

        ShapeMap corners;
        TopExp::MapShapes(shape, TopAbs_VERTEX, corners);
        for (int i = 1; i <= corners.Extent(); ++i) {
            mesh.corners.push_back({static_cast<std::uint32_t>(i),
                                    toFloat(BRep_Tool::Pnt(TopoDS::Vertex(corners(i))))});
        }

        ShapeMap edges;
        TopExp::MapShapes(shape, TopAbs_EDGE, edges);
        AncestorMap facesOfEdge;
        TopExp::MapShapesAndAncestors(shape, TopAbs_EDGE, TopAbs_FACE, facesOfEdge);
        for (int i = 1; i <= edges.Extent(); ++i) {
            const TopoDS_Edge edge = TopoDS::Edge(edges(i));
            if (BRep_Tool::Degenerated(edge)) {
                continue;
            }
            const BRepAdaptor_Curve curve(edge);
            const GCPnts_TangentialDeflection points(curve, params.angleTolerance,
                                                     params.chordTolerance);
            EdgePolyline line;
            line.edgeId = static_cast<std::uint32_t>(i);
            // The faces beside the edge. A seam is listed with its one face only: it has it twice.
            int slot = 0;
            for (const TopoDS_Shape& face : facesOfEdge.FindFromKey(edge)) {
                if (slot < 2) {
                    line.faces[static_cast<std::size_t>(slot++)] =
                        static_cast<std::uint32_t>(faces.FindIndex(face));
                }
            }
            if (slot == 1) {
                line.faces[1] = line.faces[0];
            }
            TopoDS_Vertex first;
            TopoDS_Vertex last;
            TopExp::Vertices(edge, first, last);
            if (!first.IsNull()) {
                line.corners[0] = static_cast<std::uint32_t>(corners.FindIndex(first));
            }
            if (!last.IsNull()) {
                line.corners[1] = static_cast<std::uint32_t>(corners.FindIndex(last));
            }
            for (int p = 1; p <= points.NbPoints(); ++p) {
                line.points.push_back(toFloat(points.Value(p)));
            }
            mesh.edges.push_back(std::move(line));
        }
    } catch (const Standard_Failure&) {
        throw GeometryError(GeometryErrorCode::kernelFailure, "tessellation: the kernel failed");
    }
    return mesh;
}

} // namespace fission::geo::occt
