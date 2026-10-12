// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Helpers for the kernel robustness catalog (AP-0.5). Like the smoke tests, these tests use
// OpenCASCADE directly on purpose: they study the kernel itself. Product code reaches it only
// through src/geometry/occt.

#pragma once

#include <chrono>
#include <functional>
#include <string>
#include <vector>

#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Pnt.hxx>

namespace fission::test::geo {

// Everything a golden test checks about a body.
struct ShapeStats {
    double volume = 0.0;
    double area = 0.0;
    double sizeX = 0.0;
    double sizeY = 0.0;
    double sizeZ = 0.0;
    int solids = 0;
    int faces = 0;
    int edges = 0;
    int vertices = 0;
    bool valid = false; // BRepCheck_Analyzer
};

ShapeStats measure(const TopoDS_Shape& shape);

// Result of a boolean operation, with the time the kernel needed.
struct BoolResult {
    TopoDS_Shape shape;
    bool done = false;
    bool hasErrors = false;
    double seconds = 0.0;
};

// Boxes and cylinders (axis along +Z) with the corner / base center at (x, y, z).
TopoDS_Shape makeBox(double dx, double dy, double dz, double x = 0.0, double y = 0.0,
                     double z = 0.0);
TopoDS_Shape makeCylinder(double radius, double height, double x = 0.0, double y = 0.0,
                          double z = 0.0);

// `fuzzy` is the optional OpenCASCADE fuzzy value in mm (0 = exact).
BoolResult fuse(const TopoDS_Shape& a, const TopoDS_Shape& b, double fuzzy = 0.0);
BoolResult cut(const TopoDS_Shape& a, const TopoDS_Shape& b, double fuzzy = 0.0);
BoolResult common(const TopoDS_Shape& a, const TopoDS_Shape& b, double fuzzy = 0.0);
BoolResult cutMany(const TopoDS_Shape& a, const std::vector<TopoDS_Shape>& tools,
                   double fuzzy = 0.0);

// Edges / faces of a body that satisfy a predicate (for picking the "right" edge in a test).
std::vector<TopoDS_Edge> edgesWhere(const TopoDS_Shape& shape,
                                    const std::function<bool(const TopoDS_Edge&)>& predicate);
std::vector<TopoDS_Face> facesWhere(const TopoDS_Shape& shape,
                                    const std::function<bool(const TopoDS_Face&)>& predicate);
gp_Pnt edgeMidpoint(const TopoDS_Edge& edge);
gp_Pnt faceCenter(const TopoDS_Face& face);
bool isCircle(const TopoDS_Edge& edge);
bool isPlane(const TopoDS_Face& face);
bool edgeTouches(const TopoDS_Edge& edge, const gp_Pnt& point, double tolerance = 1e-7);

// Rounds (fillet) or bevels (chamfer) the given edges. Failures, including exceptions of the
// kernel, are reported through `done` / `hasErrors` instead of being thrown.
BoolResult fillet(const TopoDS_Shape& shape, const std::vector<TopoDS_Edge>& edges, double radius);
BoolResult chamfer(const TopoDS_Shape& shape, const std::vector<TopoDS_Edge>& edges,
                   double distance);

// How the offset surfaces are joined at convex edges: `arc` rounds them, `intersection` extends
// and intersects the neighbouring surfaces.
enum class OffsetJoin { arc, intersection };

// Hollows a body: removes `openFaces` and keeps a wall of `thickness` on the inside.
BoolResult hollow(const TopoDS_Shape& shape, const std::vector<TopoDS_Face>& openFaces,
                  double thickness, double tolerance = 1e-3, OffsetJoin join = OffsetJoin::arc);

// Repairs a shape with OpenCASCADE's shape healing (ShapeFix_Shape).
TopoDS_Shape healShape(const TopoDS_Shape& shape);

// Merges faces that lie in the same surface (what the user expects after a union).
TopoDS_Shape unifyFaces(const TopoDS_Shape& shape);

// Adds a key/value pair to the row of the running test case in the result table.
void note(const std::string& key, const std::string& value);
void note(const std::string& key, double value);

// Adds kernel time (seconds) to the running test case.
void addKernelSeconds(double seconds);

// Measures the wall time of a callable.
template <typename Function> double timed(Function&& function) {
    const auto start = std::chrono::steady_clock::now();
    function();
    const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;
    return elapsed.count();
}

// Absolute path of the test font (tests/fixtures/fonts/DejaVuSans.ttf).
std::string testFontPath();

} // namespace fission::test::geo
