// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "scene.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRepBuilderAPI_Copy.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRepPrimAPI_MakeTorus.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <GCPnts_QuasiUniformDeflection.hxx>
#include <NCollection_IndexedMap.hxx>
#include <NCollection_List.hxx>
#include <Poly_Triangulation.hxx>
#include <TopExp.hxx>
#include <TopLoc_Location.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace fission::spike {

namespace {

using ShapeMap = NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher>;

TopoDS_Shape cylinderAt(double radius, double height, double x, double y, double z) {
    return BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(x, y, z), gp_Dir(0, 0, 1)), radius, height)
        .Shape();
}

// A plate with a hole pattern and a boss: a typical small part.
TopoDS_Shape makePart() {
    TopoDS_Shape plate = BRepPrimAPI_MakeBox(100.0, 60.0, 5.0).Shape();
    for (int index = 0; index < 20; ++index) {
        const TopoDS_Shape hole =
            cylinderAt(2.0, 7.0, 8.0 + 8.0 * (index % 10), 10.0 + 40.0 * (index / 10), -1.0);
        plate = BRepAlgoAPI_Cut(plate, hole).Shape();
    }
    return BRepAlgoAPI_Fuse(plate, cylinderAt(8.0, 12.0, 50.0, 30.0, 5.0)).Shape();
}

// One "knuckle": a sphere, a ring around it and a post, 5 faces in 3 solids. The bench scenes
// repeat it on a grid, which gives many faces and a controllable triangle count.
TopoDS_Shape makeKnuckle() {
    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    builder.Add(compound, BRepPrimAPI_MakeSphere(gp_Pnt(0, 0, 0), 10.0).Shape());
    builder.Add(compound,
                BRepPrimAPI_MakeTorus(gp_Ax2(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)), 18.0, 3.0).Shape());
    builder.Add(compound, cylinderAt(4.0, 30.0, 0.0, 0.0, -15.0));
    return compound;
}

std::size_t meshShape(const TopoDS_Shape& shape, double deflection, double angleRadians) {
    BRepMesh_IncrementalMesh mesher(shape, deflection, false, angleRadians, false);
    std::size_t triangles = 0;
    ShapeMap faces;
    TopExp::MapShapes(shape, TopAbs_FACE, faces);
    for (int index = 1; index <= faces.Extent(); ++index) {
        TopLoc_Location location;
        const auto triangulation = BRep_Tool::Triangulation(TopoDS::Face(faces(index)), location);
        if (!triangulation.IsNull()) {
            triangles += static_cast<std::size_t>(triangulation->NbTriangles());
        }
    }
    return triangles;
}

void appendEdges(const TopoDS_Shape& shape, TriMesh& mesh) {
    ShapeMap edges;
    TopExp::MapShapes(shape, TopAbs_EDGE, edges);
    for (int index = 1; index <= edges.Extent(); ++index) {
        const BRepAdaptor_Curve curve(TopoDS::Edge(edges(index)));
        GCPnts_QuasiUniformDeflection sampler(curve, 0.05);
        if (!sampler.IsDone() || sampler.NbPoints() < 2) {
            continue;
        }
        for (int point = 1; point < sampler.NbPoints(); ++point) {
            const gp_Pnt a = sampler.Value(point);
            const gp_Pnt b = sampler.Value(point + 1);
            for (const double value : {a.X(), a.Y(), a.Z(), b.X(), b.Y(), b.Z()}) {
                mesh.edgePoints.push_back(static_cast<float>(value));
            }
        }
    }
}

// Appends the triangulation of every face (flat vertex data with unique face ids).
void appendFaces(const TopoDS_Shape& shape, TriMesh& mesh, int& nextFaceId) {
    ShapeMap faces;
    TopExp::MapShapes(shape, TopAbs_FACE, faces);
    for (int index = 1; index <= faces.Extent(); ++index) {
        const TopoDS_Face face = TopoDS::Face(faces(index));
        TopLoc_Location location;
        const auto triangulation = BRep_Tool::Triangulation(face, location);
        if (triangulation.IsNull()) {
            continue;
        }
        const gp_Trsf transform = location.Transformation();
        const bool reversed = face.Orientation() == TopAbs_REVERSED;
        const auto base = static_cast<std::uint32_t>(mesh.vertexCount());
        const float faceId = static_cast<float>(nextFaceId++);

        const int nodeCount = triangulation->NbNodes();
        std::vector<gp_Vec> normals(static_cast<std::size_t>(nodeCount), gp_Vec(0, 0, 0));
        std::vector<gp_Pnt> points(static_cast<std::size_t>(nodeCount));
        for (int node = 1; node <= nodeCount; ++node) {
            points[static_cast<std::size_t>(node - 1)] =
                triangulation->Node(node).Transformed(transform);
        }
        // Smooth normals per face from the triangles (area weighted).
        for (int tri = 1; tri <= triangulation->NbTriangles(); ++tri) {
            int a = 0, b = 0, c = 0;
            triangulation->Triangle(tri).Get(a, b, c);
            const gp_Pnt& pa = points[static_cast<std::size_t>(a - 1)];
            const gp_Pnt& pb = points[static_cast<std::size_t>(b - 1)];
            const gp_Pnt& pc = points[static_cast<std::size_t>(c - 1)];
            gp_Vec n = gp_Vec(pa, pb).Crossed(gp_Vec(pa, pc));
            if (reversed) {
                n.Reverse();
            }
            for (const int vertex : {a, b, c}) {
                normals[static_cast<std::size_t>(vertex - 1)] += n;
            }
        }
        for (int node = 0; node < nodeCount; ++node) {
            gp_Vec n = normals[static_cast<std::size_t>(node)];
            if (n.Magnitude() > 1e-12) {
                n.Normalize();
            }
            const gp_Pnt& p = points[static_cast<std::size_t>(node)];
            for (const float value : {static_cast<float>(p.X()), static_cast<float>(p.Y()),
                                      static_cast<float>(p.Z()), static_cast<float>(n.X()),
                                      static_cast<float>(n.Y()), static_cast<float>(n.Z()),
                                      faceId}) {
                mesh.vertices.push_back(value);
            }
        }
        for (int tri = 1; tri <= triangulation->NbTriangles(); ++tri) {
            int a = 0, b = 0, c = 0;
            triangulation->Triangle(tri).Get(a, b, c);
            if (reversed) {
                std::swap(b, c);
            }
            mesh.indices.push_back(base + static_cast<std::uint32_t>(a - 1));
            mesh.indices.push_back(base + static_cast<std::uint32_t>(b - 1));
            mesh.indices.push_back(base + static_cast<std::uint32_t>(c - 1));
        }
    }
}

void computeBounds(TriMesh& mesh) {
    constexpr float kHuge = std::numeric_limits<float>::max();
    mesh.boundsMin = {kHuge, kHuge, kHuge};
    mesh.boundsMax = {-kHuge, -kHuge, -kHuge};
    for (std::size_t v = 0; v < mesh.vertexCount(); ++v) {
        for (std::size_t axis = 0; axis < 3; ++axis) {
            const float value = mesh.vertices[v * TriMesh::kFloatsPerVertex + axis];
            mesh.boundsMin[axis] = std::min(mesh.boundsMin[axis], value);
            mesh.boundsMax[axis] = std::max(mesh.boundsMax[axis], value);
        }
    }
}

Scene partScene() {
    Scene scene;
    scene.name = "part";
    scene.shape = makePart();
    meshShape(scene.shape, 0.05, 10.0 * std::numbers::pi / 180.0);
    int faceId = 1;
    appendFaces(scene.shape, scene.mesh, faceId);
    appendEdges(scene.shape, scene.mesh);
    scene.faceCount = faceId - 1;
    computeBounds(scene.mesh);
    return scene;
}

Scene benchScene(const std::string& name, double targetMillions) {
    Scene scene;
    scene.name = name;
    const auto target = static_cast<std::size_t>(targetMillions * 1.0e6);

    // Tessellate one knuckle at a fixed quality; the copies share that triangulation.
    TopoDS_Shape knuckle = makeKnuckle();
    const std::size_t perKnuckle = meshShape(knuckle, 0.05, 0.14);
    const auto count = static_cast<int>((target + perKnuckle - 1) / std::max<std::size_t>(perKnuckle, 1));
    const int side = static_cast<int>(std::ceil(std::cbrt(static_cast<double>(count))));

    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    int placed = 0;
    int faceId = 1;
    for (int z = 0; z < side && placed < count; ++z) {
        for (int y = 0; y < side && placed < count; ++y) {
            for (int x = 0; x < side && placed < count; ++x, ++placed) {
                gp_Trsf move;
                move.SetTranslation(gp_Vec(x * 50.0, y * 50.0, z * 50.0));
                const TopoDS_Shape copy = knuckle.Moved(TopLoc_Location(move));
                builder.Add(compound, copy);
                appendFaces(copy, scene.mesh, faceId);
            }
        }
    }
    scene.shape = compound;
    scene.faceCount = faceId - 1;
    computeBounds(scene.mesh);
    return scene;
}

} // namespace

Scene makeScene(const std::string& name) {
    if (name == "part") {
        return partScene();
    }
    if (name == "bench0.5") {
        return benchScene(name, 0.5);
    }
    if (name == "bench2") {
        return benchScene(name, 2.0);
    }
    if (name == "bench5") {
        return benchScene(name, 5.0);
    }
    return {};
}

} // namespace fission::spike
