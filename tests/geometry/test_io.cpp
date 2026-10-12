// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Kernel robustness catalog, group F: files. STEP round trip and export tessellation.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <numbers>
#include <string>
#include <vector>

#include "support.h"
#include <BRepAdaptor_Surface.hxx>
#include <BRepBuilderAPI_Copy.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Tool.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <Poly_Triangulation.hxx>
#include <STEPControl_Reader.hxx>
#include <STEPControl_Writer.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS.hxx>
#include <gp_Pnt.hxx>

using Catch::Approx;
using namespace fission::test::geo;

namespace {

constexpr double kPi = std::numbers::pi;

// A typical small part: plate with a hole pattern (two rows of up to ten) and a boss.
TopoDS_Shape typicalPart(int holeCount = 20) {
    std::vector<TopoDS_Shape> holes;
    for (int index = 0; index < holeCount; ++index) {
        holes.push_back(
            makeCylinder(2.0, 7.0, 8.0 + 8.0 * (index % 10), 10.0 + 40.0 * (index / 10), -1.0));
    }
    const BoolResult plate = cutMany(makeBox(100, 60, 5), holes);
    REQUIRE(plate.done);
    const BoolResult withBoss = fuse(plate.shape, makeCylinder(8.0, 12.0, 50.0, 30.0, 5.0));
    REQUIRE(withBoss.done);
    return withBoss.shape;
}

struct MeshStats {
    int triangles = 0;
    double volume = 0.0;          // from the triangles (signed tetrahedra)
    double maxSagOnCylinders = 0; // largest gap between a triangle edge and a cylinder surface
};

// Facet sag on cylinders around +Z through (cx, cy): distance of edge midpoints from the true
// radius, only for cylindrical faces.
MeshStats measureMesh(const TopoDS_Shape& shape) {
    MeshStats result;
    for (TopExp_Explorer faces(shape, TopAbs_FACE); faces.More(); faces.Next()) {
        const TopoDS_Face face = TopoDS::Face(faces.Current());
        TopLoc_Location location;
        const occ::handle<Poly_Triangulation> mesh = BRep_Tool::Triangulation(face, location);
        if (mesh.IsNull()) {
            continue;
        }
        const BRepAdaptor_Surface surface(face);
        const bool cylinder = surface.GetType() == GeomAbs_Cylinder;
        const gp_Pnt axisPoint = cylinder ? surface.Cylinder().Location() : gp_Pnt();
        const double radius = cylinder ? surface.Cylinder().Radius() : 0.0;
        const bool reversed = face.Orientation() == TopAbs_REVERSED;

        for (int index = 1; index <= mesh->NbTriangles(); ++index) {
            int a = 0, b = 0, c = 0;
            mesh->Triangle(index).Get(a, b, c);
            if (reversed) {
                std::swap(b, c);
            }
            const gp_Pnt p[3] = {mesh->Node(a).Transformed(location.Transformation()),
                                 mesh->Node(b).Transformed(location.Transformation()),
                                 mesh->Node(c).Transformed(location.Transformation())};
            ++result.triangles;
            const gp_Vec va(p[0].XYZ()), vb(p[1].XYZ()), vc(p[2].XYZ());
            result.volume += va.Dot(vb.Crossed(vc)) / 6.0;
            if (cylinder) {
                for (int edge = 0; edge < 3; ++edge) {
                    const gp_Pnt& s = p[edge];
                    const gp_Pnt& e = p[(edge + 1) % 3];
                    const double mx = 0.5 * (s.X() + e.X()) - axisPoint.X();
                    const double my = 0.5 * (s.Y() + e.Y()) - axisPoint.Y();
                    result.maxSagOnCylinders =
                        std::max(result.maxSagOnCylinders, radius - std::hypot(mx, my));
                }
            }
        }
    }
    return result;
}

} // namespace

TEST_CASE("F1 STEP round trip keeps volume, faces and validity", "[geometry][io]") {
    const TopoDS_Shape part = typicalPart();
    const ShapeStats before = measure(part);

    const std::filesystem::path file =
        std::filesystem::temp_directory_path() / "fission3d_geometry_roundtrip.step";
    STEPControl_Writer writer;
    REQUIRE(writer.Transfer(part, STEPControl_AsIs) == IFSelect_RetDone);
    REQUIRE(writer.Write(file.string().c_str()) == IFSelect_RetDone);

    STEPControl_Reader reader;
    REQUIRE(reader.ReadFile(file.string().c_str()) == IFSelect_RetDone);
    REQUIRE(reader.TransferRoots() > 0);
    const TopoDS_Shape loaded = reader.OneShape();
    std::filesystem::remove(file);
    const ShapeStats after = measure(loaded);

    note("faces", after.faces);
    note("volume before", before.volume);
    note("volume after", after.volume);
    CHECK(after.valid);
    CHECK(after.volume == Approx(before.volume).epsilon(1e-9));
    CHECK(after.area == Approx(before.area).epsilon(1e-9));
    CHECK(after.faces == before.faces);
    CHECK(after.sizeX == Approx(before.sizeX).epsilon(1e-9));
}

namespace {

struct Tessellation {
    int triangles = 0;
    double seconds = 0.0;
    double sag = 0.0;
    double volumeError = 0.0; // relative to the exact volume
};

// Meshes a copy of the part with the given settings and measures the result.
Tessellation tessellate(const TopoDS_Shape& part, double sagMm, double angleDegrees) {
    // A fresh copy, so that earlier meshes do not influence the measurement.
    const TopoDS_Shape copy =
        BRepBuilderAPI_Copy(part, /*copyGeom=*/true, /*copyMesh=*/false).Shape();
    Tessellation result;
    result.seconds = timed([&] {
        BRepMesh_IncrementalMesh mesher(copy, sagMm, /*isRelative=*/false,
                                        angleDegrees * kPi / 180.0, /*parallel=*/false);
        REQUIRE(mesher.IsDone());
    });
    addKernelSeconds(result.seconds);
    const MeshStats mesh = measureMesh(copy);
    result.triangles = mesh.triangles;
    result.sag = mesh.maxSagOnCylinders;
    result.volumeError = std::abs(mesh.volume - measure(part).volume) / measure(part).volume;
    return result;
}

} // namespace

TEST_CASE("F2 export tessellation: 0.01 mm sag, 5 degrees", "[geometry][io]") {
    const Tessellation mesh = tessellate(typicalPart(), 0.01, 5.0);

    note("triangles", mesh.triangles);
    note("seconds", mesh.seconds);
    note("largest sag mm", mesh.sag);
    note("volume error", mesh.volumeError);
    CHECK(mesh.triangles > 0);
    CHECK(mesh.sag <= 0.0101); // the requested 0.01 mm plus rounding
    CHECK(mesh.volumeError < 1e-3);
}

// Evidence for the default export settings: the angle limit decides the cost for small round
// features (a 2 mm hole needs 32 segments for 0.01 mm sag, but 720 for 0.5 degrees).
TEST_CASE("F4 export tessellation: effect of the angle limit", "[geometry][io]") {
    const TopoDS_Shape part = typicalPart(4);
    int previous = 0;
    for (const double angle : {20.0, 10.0, 5.0, 2.0, 0.5}) {
        const Tessellation mesh = tessellate(part, 0.01, angle);
        const std::string label = std::to_string(angle).substr(0, 4) + " deg ";
        note(label + "triangles", mesh.triangles);
        note(label + "seconds", mesh.seconds);
        note(label + "sag mm", mesh.sag);
        CHECK(mesh.sag <= 0.0101);
        CHECK(mesh.triangles >= previous); // a smaller angle never gives fewer triangles
        previous = mesh.triangles;
    }
    if (std::getenv("FISSION_GEOMETRY_SLOW") != nullptr) {
        const Tessellation mesh = tessellate(typicalPart(), 0.01, 0.5);
        note("20 holes, 0.5 deg triangles", mesh.triangles);
        note("20 holes, 0.5 deg seconds", mesh.seconds);
    }
}

TEST_CASE("F3 export tessellation of a single fine cylinder (0.001 mm)", "[geometry][io]") {
    const TopoDS_Shape cylinder = makeCylinder(25.0, 40.0);
    double seconds = timed([&] {
        BRepMesh_IncrementalMesh mesher(cylinder, 0.001, false, 0.5 * kPi / 180.0, false);
        REQUIRE(mesher.IsDone());
    });
    addKernelSeconds(seconds);
    const MeshStats mesh = measureMesh(cylinder);

    note("triangles", mesh.triangles);
    note("largest sag mm", mesh.maxSagOnCylinders);
    CHECK(mesh.maxSagOnCylinders <= 0.00101);
    CHECK(mesh.volume == Approx(kPi * 625.0 * 40.0).epsilon(1e-4));
}
