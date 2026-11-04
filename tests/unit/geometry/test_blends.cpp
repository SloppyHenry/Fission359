// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Rounding and bevelling edges through the kernel interface (AP-2.5): volumes from the formula,
// the faces keep their names, the new ones are named after the edge, a size that is too large is
// an error that says so, and tangential edges are taken along.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>
#include <set>

#include "fission/geo/occt/kernel.h"

using Catch::Approx;
namespace geo = fission::geo;

namespace {

constexpr double kPi = std::numbers::pi;

// The edge along z at the corner (0, 0) of a box that has its corner there.
geo::BlendEdge verticalEdgeAtOrigin(const geo::GeometryKernel& kernel, const geo::NamedSolid& box) {
    const geo::BodySignatures looks = kernel.signatures(box.solid);
    for (const geo::ElementSignature& edge : looks.edges) {
        if (edge.id != 0 && std::abs(edge.centroid.x) < 1e-6 && std::abs(edge.centroid.y) < 1e-6 &&
            std::abs(edge.direction.z) > 0.9) {
            return {edge.id, "origin"};
        }
    }
    FAIL("no vertical edge at the origin");
    return {};
}

} // namespace

TEST_CASE("blend: rounding one edge of a cube takes the formula's volume", "[geometry][blend]") {
    const auto kernel = geo::occt::makeKernel();
    const geo::NamedSolid cube = kernel->makeBox(20, 20, 20, {0, 0, 0}, 0.0);
    const geo::BlendEdge edge = verticalEdgeAtOrigin(*kernel, cube);

    const geo::NamedSolid made =
        kernel->blendEdges(cube, {edge}, {geo::BlendKind::round, 3.0, 0.0}, "F2/fillet/round");
    const geo::SolidReport report = kernel->measure(made.solid);
    CHECK(report.valid);
    CHECK(report.volume == Approx(8000.0 - 9.0 * (1.0 - kPi / 4.0) * 20.0).epsilon(1e-6));
    CHECK(report.faces == 7);

    // Every face has a name once; the six old ones keep theirs, the new one belongs to the edge.
    REQUIRE(made.faces.size() == 7);
    std::set<std::string> roles;
    for (const geo::NamedFace& face : made.faces) {
        roles.insert(face.role);
    }
    CHECK(roles.size() == 7);
    CHECK(roles.count("top") == 1);
    CHECK(roles.count("F2/fillet/round[origin]") == 1);
}

TEST_CASE("blend: a bevel takes half the square of its distance", "[geometry][blend]") {
    const auto kernel = geo::occt::makeKernel();
    const geo::NamedSolid cube = kernel->makeBox(20, 20, 20, {0, 0, 0}, 0.0);
    const geo::BlendEdge edge = verticalEdgeAtOrigin(*kernel, cube);

    const geo::NamedSolid even =
        kernel->blendEdges(cube, {edge}, {geo::BlendKind::chamfer, 2.0, 0.0}, "F2/chamfer/bevel");
    CHECK(kernel->measure(even.solid).volume == Approx(8000.0 - 0.5 * 2.0 * 2.0 * 20.0));
    CHECK(kernel->measure(even.solid).faces == 7);

    const geo::NamedSolid uneven =
        kernel->blendEdges(cube, {edge}, {geo::BlendKind::chamfer, 2.0, 3.0}, "F2/chamfer/bevel");
    CHECK(kernel->measure(uneven.solid).volume == Approx(8000.0 - 0.5 * 2.0 * 3.0 * 20.0));
    CHECK(kernel->measure(uneven.solid).valid);
}

TEST_CASE("blend: four vertical edges at once, corner patches stay named", "[geometry][blend]") {
    const auto kernel = geo::occt::makeKernel();
    const geo::NamedSolid cube = kernel->makeBox(20, 20, 20, {0, 0, 0}, 0.0);
    std::vector<geo::BlendEdge> edges;
    const geo::BodySignatures looks = kernel->signatures(cube.solid);
    for (const geo::ElementSignature& edge : looks.edges) {
        if (edge.id != 0 && std::abs(edge.direction.z) > 0.9) {
            edges.push_back({edge.id, "v" + std::to_string(edge.id)});
        }
    }
    REQUIRE(edges.size() == 4);
    const geo::NamedSolid made =
        kernel->blendEdges(cube, edges, {geo::BlendKind::round, 2.0, 0.0}, "F2/fillet/round");
    const geo::SolidReport report = kernel->measure(made.solid);
    CHECK(report.valid);
    CHECK(report.volume == Approx(8000.0 - 4.0 * 4.0 * (1.0 - kPi / 4.0) * 20.0).epsilon(1e-6));
    CHECK(report.faces == 10);
    CHECK(made.faces.size() == 10);
}

TEST_CASE("blend: a size that is too large is an error that says so", "[geometry][blend]") {
    const auto kernel = geo::occt::makeKernel();
    const geo::NamedSolid cube = kernel->makeBox(20, 20, 20, {0, 0, 0}, 0.0);
    const geo::BlendEdge edge = verticalEdgeAtOrigin(*kernel, cube);
    try {
        (void)kernel->blendEdges(cube, {edge}, {geo::BlendKind::round, 60.0, 0.0},
                                 "F2/fillet/round");
        FAIL("a rounding of 60 mm on a 20 mm cube cannot work");
    } catch (const geo::GeometryError& error) {
        CHECK(error.code() == geo::GeometryErrorCode::blendTooLarge);
    }
    CHECK_THROWS_AS(kernel->blendEdges(cube, {edge}, {geo::BlendKind::round, 0.0, 0.0}, "x"),
                    geo::GeometryError);
    CHECK_THROWS_AS(
        kernel->blendEdges(cube, {{999, "none"}}, {geo::BlendKind::round, 1.0, 0.0}, "x"),
        geo::GeometryError);
    CHECK_THROWS_AS(kernel->blendEdges(cube, {}, {geo::BlendKind::round, 1.0, 0.0}, "x"),
                    geo::GeometryError);
}

TEST_CASE("blend: edges that run on tangentially are taken along", "[geometry][blend]") {
    const auto kernel = geo::occt::makeKernel();
    // Rounded corners: the outline of the top face is straight pieces and quarter circles that
    // run into each other without a corner: eight edges in one chain.
    const geo::NamedSolid soft = kernel->makeBox(40, 40, 10, {0, 0, 0}, 5.0);
    const geo::BodySignatures looks = kernel->signatures(soft.solid);
    std::uint32_t onTop = 0;
    for (const geo::ElementSignature& edge : looks.edges) {
        if (edge.id != 0 && std::abs(edge.centroid.z - 10.0) < 1e-6 &&
            edge.shape == geo::SignatureShape::line) {
            onTop = edge.id;
            break;
        }
    }
    REQUIRE(onTop != 0);
    const std::vector<std::uint32_t> chain = kernel->tangentChain(soft.solid, onTop);
    CHECK(chain.size() == 8);
    CHECK(std::count(chain.begin(), chain.end(), onTop) == 1);

    // A box edge has no tangent neighbours.
    const geo::NamedSolid cube = kernel->makeBox(20, 20, 20, {0, 0, 0}, 0.0);
    const geo::BlendEdge edge = verticalEdgeAtOrigin(*kernel, cube);
    CHECK(kernel->tangentChain(cube.solid, edge.id) == std::vector<std::uint32_t>{edge.id});
}
