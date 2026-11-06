// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Further file formats (AP-2.13): OBJ and STEP out, the outline of a flat face as SVG and DXF.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <numbers>
#include <sstream>
#include <string>
#include <unistd.h>
#include <variant>
#include <vector>

#include "fission/geo/error.h"
#include "fission/geo/kernel.h"
#include "fission/geo/occt/kernel.h"
#include "fission/geo/occt/solids.h"
#include "fission/io/drawing.h"
#include "fission/io/export.h"

using Catch::Approx;
using namespace fission::geo;
namespace io = fission::io;
namespace fs = std::filesystem;

namespace {

constexpr double kPi = std::numbers::pi;

struct Folder {
    fs::path path;
    Folder() {
        static int counter = 0;
        path = fs::temp_directory_path() / ("fission-formats-test-" + std::to_string(::getpid()) +
                                            "-" + std::to_string(++counter));
        fs::create_directories(path);
    }
    ~Folder() {
        std::error_code ignored;
        fs::remove_all(path, ignored);
    }
    Folder(const Folder&) = delete;
    Folder& operator=(const Folder&) = delete;
};

std::string slurp(const fs::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

std::uint32_t faceWithRole(const NamedSolid& solid, const std::string& ending) {
    for (const NamedFace& face : solid.faces) {
        if (face.role.size() >= ending.size() &&
            face.role.compare(face.role.size() - ending.size(), ending.size(), ending) == 0) {
            return face.faceId;
        }
    }
    FAIL("no face with the role " << ending);
    return 0;
}

// A plate 40 x 20 x 5 with a hole of radius 3 whose middle is 10 mm left of and 5 mm behind
// (+y) the middle of the plate.
NamedSolid plateWithHole(double cornerRadius = 0.0) {
    const NamedSolid plate = occt::makeNamedBox(40.0, 20.0, 5.0, {-20.0, -10.0, 0.0}, cornerRadius);
    const NamedSolid hole = occt::makeNamedCylinder(3.0, 9.0, {-10.0, 5.0, -2.0});
    return occt::booleanNamed(BooleanOp::subtract, plate, {hole});
}

template <class T> std::vector<T> shapesOf(const Outline& outline) {
    std::vector<T> found;
    for (const OutlineShape& shape : outline.shapes) {
        if (const auto* one = std::get_if<T>(&shape)) {
            found.push_back(*one);
        }
    }
    return found;
}

// Middle of the box round the straight edges.
Point2d middleOf(const Outline& outline) {
    double minX = 1e9;
    double maxX = -1e9;
    double minY = 1e9;
    double maxY = -1e9;
    for (const OutlineLine& line : shapesOf<OutlineLine>(outline)) {
        for (const Point2d& p : {line.from, line.to}) {
            minX = std::min(minX, p.x);
            maxX = std::max(maxX, p.x);
            minY = std::min(minY, p.y);
            maxY = std::max(maxY, p.y);
        }
    }
    return {(minX + maxX) / 2.0, (minY + maxY) / 2.0};
}

std::size_t countLines(const std::string& text, const std::string& line) {
    std::size_t count = 0;
    std::istringstream stream(text);
    for (std::string one; std::getline(stream, one);) {
        count += one == line ? 1 : 0;
    }
    return count;
}

} // namespace

TEST_CASE("outline: a plate with a hole is four lines and a circle", "[formats][outline]") {
    const auto kernel = occt::makeKernel();
    const NamedSolid plate = plateWithHole();
    const Outline outline = kernel->faceOutline(plate.solid, faceWithRole(plate, "top"));

    const auto lines = shapesOf<OutlineLine>(outline);
    const auto circles = shapesOf<OutlineCircle>(outline);
    CHECK(lines.size() == 4);
    CHECK(circles.size() == 1);
    CHECK(outline.shapes.size() == 5);
    CHECK(circles.front().radius == Approx(3.0).epsilon(1e-9));

    double total = 0.0;
    double minX = 1e9;
    double maxX = -1e9;
    for (const OutlineLine& line : lines) {
        total += std::hypot(line.to.x - line.from.x, line.to.y - line.from.y);
        minX = std::min({minX, line.from.x, line.to.x});
        maxX = std::max({maxX, line.from.x, line.to.x});
    }
    CHECK(total == Approx(120.0).epsilon(1e-9)); // 2 x 40 + 2 x 20
    CHECK(maxX - minX == Approx(40.0).epsilon(1e-9));
    // Seen from above: the hole is left and up.
    CHECK(circles.front().centre.x - middleOf(outline).x == Approx(-10.0).margin(1e-9));
    CHECK(circles.front().centre.y - middleOf(outline).y == Approx(5.0).margin(1e-9));
}

TEST_CASE("outline: seen from outside, so the underside is turned over", "[formats][outline]") {
    const auto kernel = occt::makeKernel();
    const NamedSolid plate = plateWithHole();
    const Outline outline = kernel->faceOutline(plate.solid, faceWithRole(plate, "bottom"));
    const auto circles = shapesOf<OutlineCircle>(outline);
    REQUIRE(circles.size() == 1);
    // Looking up at the underside with the front edge up: left stays left, up and down swap.
    CHECK(circles.front().centre.x - middleOf(outline).x == Approx(-10.0).margin(1e-9));
    CHECK(circles.front().centre.y - middleOf(outline).y == Approx(-5.0).margin(1e-9));
}

TEST_CASE("outline: rounded corners stay arcs of the right size", "[formats][outline]") {
    const auto kernel = occt::makeKernel();
    const NamedSolid plate = occt::makeNamedBox(40.0, 20.0, 5.0, {0.0, 0.0, 0.0}, 4.0);
    const Outline outline = kernel->faceOutline(plate.solid, faceWithRole(plate, "top"));
    const auto arcs = shapesOf<OutlineArc>(outline);
    CHECK(shapesOf<OutlineLine>(outline).size() == 4);
    REQUIRE(arcs.size() == 4);
    for (const OutlineArc& arc : arcs) {
        CHECK(arc.radius == Approx(4.0).epsilon(1e-9));
        CHECK(arc.endAngle - arc.startAngle == Approx(90.0).epsilon(1e-6));
    }
}

TEST_CASE("outline: what is not a flat face is refused", "[formats][outline]") {
    const auto kernel = occt::makeKernel();
    const NamedSolid cylinder = occt::makeNamedCylinder(5.0, 10.0);
    const Outline round = kernel->faceOutline(cylinder.solid, faceWithRole(cylinder, "top"));
    REQUIRE(round.shapes.size() == 1);
    CHECK(std::holds_alternative<OutlineCircle>(round.shapes.front()));
    CHECK_THROWS_AS(kernel->faceOutline(cylinder.solid, faceWithRole(cylinder, "side")),
                    GeometryError);
    CHECK_THROWS_AS(kernel->faceOutline(cylinder.solid, 0), GeometryError);
    CHECK_THROWS_AS(kernel->faceOutline(cylinder.solid, 99), GeometryError);
    CHECK_THROWS_AS(kernel->faceOutline(Solid{}, 1), GeometryError);
}

TEST_CASE("svg: true size in millimetres, lines, circles and arcs stay exact", "[formats][svg]") {
    Outline outline;
    outline.shapes.push_back(OutlineLine{{0, 0}, {40, 0}});
    outline.shapes.push_back(OutlineLine{{40, 0}, {40, 20}});
    outline.shapes.push_back(OutlineCircle{{10, 10}, 3});
    outline.shapes.push_back(OutlineArc{{0, 0}, 4, 0, 90});
    outline.shapes.push_back(OutlinePolyline{{{0, 0}, {1.5, 2}, {3, 0}}});
    const std::string svg = io::svgText(outline);
    CHECK(svg.starts_with("<?xml"));
    CHECK(svg.find("width=\"40mm\" height=\"20mm\"") != std::string::npos);
    CHECK(svg.find("viewBox=\"0 0 40 20\"") != std::string::npos);
    CHECK(svg.find("<line x1=\"0\" y1=\"20\" x2=\"40\" y2=\"20\"/>") != std::string::npos);
    CHECK(svg.find("<circle cx=\"10\" cy=\"10\" r=\"3\"/>") != std::string::npos);
    // The arc from (4, 0) up to (0, 4): counter-clockwise on the page, small.
    CHECK(svg.find("<path d=\"M 4 20 A 4 4 0 0 0 0 16\"/>") != std::string::npos);
    CHECK(svg.find("<polyline points=\"0,20 1.5,18 3,20\"/>") != std::string::npos);
    CHECK(svg.ends_with("</svg>\n"));
}

TEST_CASE("svg: an empty outline is still a valid drawing", "[formats][svg]") {
    const std::string svg = io::svgText({});
    CHECK(svg.find("width=\"0mm\"") != std::string::npos);
    CHECK(svg.ends_with("</svg>\n"));
}

TEST_CASE("dxf: the entities of a plate with a hole", "[formats][dxf]") {
    const auto kernel = occt::makeKernel();
    const NamedSolid plate = plateWithHole(4.0);
    const std::string dxf =
        io::dxfText(kernel->faceOutline(plate.solid, faceWithRole(plate, "top")));
    CHECK(dxf.starts_with("0\nSECTION\n2\nHEADER\n"));
    CHECK(dxf.ends_with("0\nEOF\n"));
    CHECK(countLines(dxf, "LINE") == 4);
    CHECK(countLines(dxf, "ARC") == 4);
    CHECK(countLines(dxf, "CIRCLE") == 1);
    CHECK(countLines(dxf, "ENDSEC") == 3);
    // The hole: radius 3, 10 mm left of the middle of a plate 40 wide with its corner at -20.
    CHECK(dxf.find("40\n3\n") != std::string::npos);
}

TEST_CASE("dxf: arcs run counter-clockwise from start to end angle", "[formats][dxf]") {
    Outline outline;
    outline.shapes.push_back(OutlineArc{{1, 2}, 5, 270, 360});
    outline.shapes.push_back(OutlinePolyline{{{0, 0}, {1, 1}}});
    const std::string dxf = io::dxfText(outline);
    CHECK(dxf.find("0\nARC\n8\n0\n10\n1\n20\n2\n40\n5\n50\n270\n51\n0\n") != std::string::npos);
    CHECK(countLines(dxf, "POLYLINE") == 1);
    CHECK(countLines(dxf, "VERTEX") == 2);
    CHECK(countLines(dxf, "SEQEND") == 1);
}

TEST_CASE("svg and dxf files are written whole", "[formats]") {
    Folder folder;
    Outline outline;
    outline.shapes.push_back(OutlineCircle{{0, 0}, 5});
    REQUIRE(io::exportSvg(outline, folder.path / "a.svg"));
    REQUIRE(io::exportDxf(outline, folder.path / "a.dxf"));
    CHECK(slurp(folder.path / "a.svg") == io::svgText(outline));
    CHECK(slurp(folder.path / "a.dxf") == io::dxfText(outline));
    CHECK_FALSE(io::exportSvg(outline, folder.path / "missing-folder" / "a.svg"));
}

namespace {

struct Wavefront {
    std::vector<std::array<double, 3>> points;
    std::vector<std::array<int, 3>> faces;
    std::vector<std::string> objects;
    double volume() const {
        double sum = 0.0;
        for (const auto& f : faces) {
            const auto& a = points[f[0] - 1];
            const auto& b = points[f[1] - 1];
            const auto& c = points[f[2] - 1];
            sum += a[0] * (b[1] * c[2] - b[2] * c[1]) - a[1] * (b[0] * c[2] - b[2] * c[0]) +
                   a[2] * (b[0] * c[1] - b[1] * c[0]);
        }
        return sum / 6.0;
    }
};

Wavefront parseObj(const std::string& text) {
    Wavefront parsed;
    std::istringstream stream(text);
    for (std::string line; std::getline(stream, line);) {
        std::istringstream words(line);
        std::string tag;
        words >> tag;
        if (tag == "v") {
            std::array<double, 3> p{};
            words >> p[0] >> p[1] >> p[2];
            parsed.points.push_back(p);
        } else if (tag == "f") {
            std::array<int, 3> f{};
            words >> f[0] >> f[1] >> f[2];
            parsed.faces.push_back(f);
        } else if (tag == "o") {
            parsed.objects.push_back(line.substr(2));
        }
    }
    return parsed;
}

} // namespace

TEST_CASE("obj: objects with names, colours in a .mtl, volumes that match", "[formats][obj]") {
    Folder folder;
    const auto kernel = occt::makeKernel();
    const NamedSolid box = occt::makeNamedBox(40.0, 20.0, 5.0, {0.0, 0.0, 0.0});
    const NamedSolid cylinder = occt::makeNamedCylinder(6.0, 25.0, {100.0, 0.0, 0.0});
    const TessellationParams params = io::exportTessellation(io::ExportQuality::normal);
    const std::vector<io::MeshForExport> bodies = {
        {"Plate", fission::core::Color{200, 100, 50}, kernel->tessellate(box.solid, params)},
        {"Pin\nwith break", fission::core::Color{0, 255, 0},
         kernel->tessellate(cylinder.solid, params)},
    };
    const fs::path file = folder.path / "model.obj";
    REQUIRE(io::exportObj(bodies, file));

    const std::string text = slurp(file);
    CHECK(text.find("mtllib model.mtl") != std::string::npos);
    const Wavefront parsed = parseObj(text);
    REQUIRE(parsed.objects.size() == 2);
    CHECK(parsed.objects[0] == "Plate");
    CHECK(parsed.objects[1] == "Pin with break"); // no line break inside a name
    // The corners are shared: fewer than the mesh of the kernel repeats.
    CHECK(parsed.points.size() < bodies[0].mesh.positions.size() + bodies[1].mesh.positions.size());
    CHECK(parsed.points.size() >= 8 + 2 * 3);
    for (const auto& f : parsed.faces) {
        for (const int index : f) {
            REQUIRE(index >= 1);
            REQUIRE(index <= static_cast<int>(parsed.points.size()));
        }
    }
    const double expected = 40.0 * 20.0 * 5.0 + kPi * 36.0 * 25.0;
    CHECK(parsed.volume() == Approx(expected).epsilon(2e-4)); // the cylinder is a mesh

    const std::string materials = slurp(folder.path / "model.mtl");
    CHECK(materials.find("newmtl colour0\nKd 0.7843137 0.3921569 0.1960784\n") !=
          std::string::npos);
    CHECK(materials.find("newmtl colour1\nKd 0 1 0\n") != std::string::npos);
    CHECK(fs::directory_iterator(folder.path) != fs::directory_iterator());
    CHECK(std::distance(fs::directory_iterator(folder.path), fs::directory_iterator()) == 2);
}

TEST_CASE("step: the kernel's text is written to a file as it is", "[formats][step]") {
    Folder folder;
    const auto kernel = occt::makeKernel();
    const NamedSolid box = occt::makeNamedBox(10.0, 10.0, 10.0);
    const std::string text = kernel->exportStep({{box.solid, "Cube", FileColor{1, 2, 3}}});
    REQUIRE(io::writeTextFile(text, folder.path / "cube.step"));
    CHECK(slurp(folder.path / "cube.step") == text);
    CHECK(std::distance(fs::directory_iterator(folder.path), fs::directory_iterator()) == 1);
}
