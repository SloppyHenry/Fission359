// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/geo/occt/solids.h"

#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "fission/geo/error.h"

#include "occt_solid.h"
#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_Copy.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepLProp_SLProps.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
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
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

namespace fission::geo::occt {

namespace {

using ShapeMap = NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher>;
using AncestorMap = NCollection_IndexedDataMap<TopoDS_Shape, NCollection_List<TopoDS_Shape>,
                                               TopTools_ShapeMapHasher>;

bool isPositiveFinite(double value) {
    return std::isfinite(value) && value > 0.0;
}

bool isFinite(const Point3d& point) {
    return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
}

const TopoDS_Shape& shapeOf(const Solid& solid) {
    const auto* data = dynamic_cast<const OcctSolidData*>(solid.data());
    if (data == nullptr) {
        throw GeometryError(GeometryErrorCode::invalidInput,
                            "the body is empty or was not created by the OpenCASCADE kernel");
    }
    return data->shape();
}

// Results are validated before anybody sees them: an invalid body is an error, not a warning
// (master prompt, rule 8).
Solid validated(const TopoDS_Shape& shape, const char* what) {
    if (shape.IsNull() || !BRepCheck_Analyzer(shape).IsValid()) {
        throw GeometryError(GeometryErrorCode::invalidResult,
                            std::string(what) + ": the kernel returned an invalid body");
    }
    return wrap(shape);
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

// 1-based number of `face` among the faces of `shape`: the same numbering tessellate() uses.
std::uint32_t faceNumber(const TopoDS_Shape& shape, const TopoDS_Shape& face) {
    ShapeMap faces;
    TopExp::MapShapes(shape, TopAbs_FACE, faces);
    const int index = faces.FindIndex(face);
    if (index <= 0) {
        throw GeometryError(GeometryErrorCode::kernelFailure,
                            "a primitive face could not be found in its body");
    }
    return static_cast<std::uint32_t>(index);
}

// The roles must cover every face exactly once (naming contract, ADR-0003).
NamedSolid withRoles(const TopoDS_Shape& shape, const char* what,
                     const std::vector<std::pair<const char*, TopoDS_Shape>>& roles) {
    NamedSolid result;
    result.solid = validated(shape, what);
    ShapeMap faces;
    TopExp::MapShapes(shape, TopAbs_FACE, faces);
    if (static_cast<std::size_t>(faces.Extent()) != roles.size()) {
        throw GeometryError(GeometryErrorCode::kernelFailure,
                            std::string(what) + ": not every face got a role");
    }
    for (const auto& [role, face] : roles) {
        result.faces.push_back({faceNumber(shape, face), role});
    }
    return result;
}

} // namespace

NamedSolid makeNamedBox(double sizeX, double sizeY, double sizeZ, Point3d corner) {
    if (!isPositiveFinite(sizeX) || !isPositiveFinite(sizeY) || !isPositiveFinite(sizeZ) ||
        !isFinite(corner)) {
        throw GeometryError(GeometryErrorCode::invalidInput, "box: sizes must be positive numbers");
    }
    try {
        BRepPrimAPI_MakeBox maker(gp_Pnt(corner.x, corner.y, corner.z), sizeX, sizeY, sizeZ);
        const TopoDS_Shape shape = maker.Shape();

        // The kernel has its own idea of front and left. Ours: looking at the box from the front
        // we look along +Y, so front is -Y, left is -X (as in the view cube).
        ShapeMap faces;
        TopExp::MapShapes(shape, TopAbs_FACE, faces);
        std::vector<std::pair<const char*, TopoDS_Shape>> roles;
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
            const char* role = nullptr;
            if (normal.Z() > 0.5) {
                role = "top";
            } else if (normal.Z() < -0.5) {
                role = "bottom";
            } else if (normal.Y() > 0.5) {
                role = "back";
            } else if (normal.Y() < -0.5) {
                role = "front";
            } else if (normal.X() > 0.5) {
                role = "right";
            } else if (normal.X() < -0.5) {
                role = "left";
            }
            if (role != nullptr) {
                roles.emplace_back(role, faces(i));
            }
        }
        return withRoles(shape, "box", roles);
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
        ShapeMap faces;
        TopExp::MapShapes(shape, TopAbs_FACE, faces);
        std::vector<std::pair<const char*, TopoDS_Shape>> roles;
        for (int i = 1; i <= faces.Extent(); ++i) {
            const BRepAdaptor_Surface surface(TopoDS::Face(faces(i)));
            if (surface.GetType() == GeomAbs_Cylinder) {
                roles.emplace_back("side", faces(i));
            } else if (surface.GetType() == GeomAbs_Plane) {
                const bool lower = surface.Plane().Location().Z() < base.z + height / 2.0;
                roles.emplace_back(lower ? "bottom" : "top", faces(i));
            }
        }
        return withRoles(shape, "cylinder", roles);
    } catch (const Standard_Failure&) {
        throw GeometryError(GeometryErrorCode::kernelFailure, "cylinder: the kernel failed");
    }
}

Solid makeBox(double sizeX, double sizeY, double sizeZ, Point3d corner) {
    return makeNamedBox(sizeX, sizeY, sizeZ, corner).solid;
}

Solid makeCylinder(double radius, double height, Point3d base) {
    return makeNamedCylinder(radius, height, base).solid;
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
