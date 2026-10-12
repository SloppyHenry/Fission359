// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Kernel robustness catalog, group G: the same boolean cases on triangle meshes with
// Manifold, compared with the exact OpenCASCADE result.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdlib>
#include <numbers>
#include <vector>

#include "support.h"
#include <BRepBuilderAPI_Copy.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Tool.hxx>
#include <Poly_Triangulation.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS.hxx>
#include <manifold/manifold.h>

using Catch::Approx;
using namespace fission::test::geo;

namespace {

constexpr double kPi = std::numbers::pi;

// Triangulates a body (0.01 mm sag) and hands the triangles to Manifold.
manifold::Manifold toManifold(const TopoDS_Shape& original, double angleDegrees) {
    // A fresh copy: OpenCASCADE keeps triangulations on shapes, which would falsify timings.
    const TopoDS_Shape shape =
        BRepBuilderAPI_Copy(original, /*copyGeom=*/true, /*copyMesh=*/false).Shape();
    BRepMesh_IncrementalMesh mesher(shape, 0.01, false, angleDegrees * kPi / 180.0, false);
    REQUIRE(mesher.IsDone());

    manifold::MeshGL64 mesh;
    mesh.numProp = 3;
    for (TopExp_Explorer faces(shape, TopAbs_FACE); faces.More(); faces.Next()) {
        const TopoDS_Face face = TopoDS::Face(faces.Current());
        TopLoc_Location location;
        const occ::handle<Poly_Triangulation> triangulation =
            BRep_Tool::Triangulation(face, location);
        if (triangulation.IsNull()) {
            continue;
        }
        const bool reversed = face.Orientation() == TopAbs_REVERSED;
        const uint64_t offset = mesh.vertProperties.size() / 3;
        for (int node = 1; node <= triangulation->NbNodes(); ++node) {
            const gp_Pnt p = triangulation->Node(node).Transformed(location.Transformation());
            mesh.vertProperties.push_back(p.X());
            mesh.vertProperties.push_back(p.Y());
            mesh.vertProperties.push_back(p.Z());
        }
        for (int index = 1; index <= triangulation->NbTriangles(); ++index) {
            int a = 0, b = 0, c = 0;
            triangulation->Triangle(index).Get(a, b, c);
            if (reversed) {
                std::swap(b, c);
            }
            mesh.triVerts.push_back(offset + static_cast<uint64_t>(a - 1));
            mesh.triVerts.push_back(offset + static_cast<uint64_t>(b - 1));
            mesh.triVerts.push_back(offset + static_cast<uint64_t>(c - 1));
        }
    }
    mesh.Merge(); // faces were meshed separately: join the vertices along shared edges
    return manifold::Manifold(mesh);
}

enum class Operation { fuse, cut };

struct Comparison {
    double exactVolume = 0.0;
    double meshVolume = 0.0;
    double exactSeconds = 0.0;
    double meshSeconds = 0.0;
    bool meshValid = false;
};

Comparison compare(Operation operation, const TopoDS_Shape& a,
                   const std::vector<TopoDS_Shape>& tools, double angleDegrees = 5.0) {
    Comparison result;
    BoolResult exact;
    if (operation == Operation::fuse) {
        exact = fuse(a, tools.front());
    } else {
        exact = cutMany(a, tools);
    }
    REQUIRE(exact.done);
    result.exactVolume = measure(exact.shape).volume;
    result.exactSeconds = exact.seconds;

    manifold::Manifold left = toManifold(a, angleDegrees);
    std::vector<manifold::Manifold> right;
    for (const TopoDS_Shape& tool : tools) {
        right.push_back(toManifold(tool, angleDegrees));
    }
    REQUIRE(left.Status() == manifold::Manifold::Error::NoError);

    manifold::Manifold combined;
    result.meshSeconds = timed([&] {
        if (operation == Operation::fuse) {
            combined = left + right.front();
        } else {
            combined = left - manifold::Manifold::BatchBoolean(right, manifold::OpType::Add);
        }
        combined.NumTri(); // forces the lazy evaluation
    });
    addKernelSeconds(result.meshSeconds);
    result.meshValid = combined.Status() == manifold::Manifold::Error::NoError;
    result.meshVolume = combined.Volume();
    return result;
}

void report(const Comparison& comparison) {
    note("exact volume", comparison.exactVolume);
    note("mesh volume", comparison.meshVolume);
    note("exact seconds", comparison.exactSeconds);
    note("mesh seconds", comparison.meshSeconds);
}

} // namespace

TEST_CASE("G1 mesh boolean: round hole through a plate", "[geometry][manifold]") {
    const Comparison c =
        compare(Operation::cut, makeBox(40, 40, 10), {makeCylinder(5, 12, 20, 20, -1)});
    report(c);
    CHECK(c.meshValid);
    CHECK(c.meshVolume == Approx(c.exactVolume).epsilon(1e-3));
}

TEST_CASE("G2 mesh boolean: grid of 100 holes", "[geometry][manifold]") {
    std::vector<TopoDS_Shape> holes;
    for (int row = 0; row < 10; ++row) {
        for (int column = 0; column < 10; ++column) {
            holes.push_back(makeCylinder(3, 7, 5.0 + 10.0 * column, 5.0 + 10.0 * row, -1.0));
        }
    }
    const Comparison c = compare(Operation::cut, makeBox(100, 100, 5), holes);
    report(c);
    CHECK(c.meshValid);
    CHECK(c.meshVolume == Approx(c.exactVolume).epsilon(1e-3));

    // With the strict 0.5 degree limit of the export defaults the mesh is about ten times
    // finer. Only on request, because it takes many seconds.
    if (std::getenv("FISSION_GEOMETRY_SLOW") != nullptr) {
        const Comparison fine = compare(Operation::cut, makeBox(100, 100, 5), holes, 0.5);
        note("0.5 deg mesh seconds", fine.meshSeconds);
        CHECK(fine.meshValid);
    }
}

TEST_CASE("G3 mesh boolean: two boxes sharing a whole face", "[geometry][manifold]") {
    const Comparison c =
        compare(Operation::fuse, makeBox(20, 20, 10), {makeBox(20, 20, 10, 0, 0, 10)});
    report(c);
    CHECK(c.meshValid);
    CHECK(c.meshVolume == Approx(8000.0).epsilon(1e-9));
}

TEST_CASE("G4 mesh boolean: hole that touches the wall", "[geometry][manifold]") {
    const Comparison c =
        compare(Operation::cut, makeBox(40, 40, 10), {makeCylinder(5, 12, 5, 20, -1)});
    report(c);
    CHECK(c.meshValid);
    CHECK(c.meshVolume == Approx(c.exactVolume).epsilon(1e-3));
}

TEST_CASE("G5 mesh boolean: boxes overlapping by 0.00001 mm", "[geometry][manifold]") {
    const Comparison c =
        compare(Operation::fuse, makeBox(10, 10, 10), {makeBox(10, 10, 10, 0, 0, 9.99999)});
    report(c);
    CHECK(c.meshValid);
    CHECK(c.meshVolume == Approx(c.exactVolume).epsilon(1e-6));
}
