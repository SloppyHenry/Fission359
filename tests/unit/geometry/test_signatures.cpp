// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// What bodies look like to persistent references (AP-2.10a, ADR-0003): golden values of the
// signatures, and the history of names that booleans report.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <numbers>
#include <string>

#include "fission/geo/occt/solids.h"

using Catch::Approx;
using namespace fission::geo;

namespace {

constexpr double kPi = std::numbers::pi;

NamedSolid called(NamedSolid solid, const std::string& prefix) {
    for (NamedFace& face : solid.faces) {
        face.role = prefix + "/" + face.role;
    }
    return solid;
}

NamedSolid plate() {
    return called(occt::makeNamedBox(40.0, 40.0, 10.0, {-20.0, -20.0, 0.0}), "F1/box");
}

// The face the creator called `role`.
const ElementSignature& faceOf(const NamedSolid& solid, const BodySignatures& sigs,
                               const std::string& role) {
    for (const NamedFace& face : solid.faces) {
        if (face.role == role) {
            return sigs.faces.at(face.faceId - 1);
        }
    }
    FAIL("no face " << role);
    return sigs.faces.front();
}

bool contains(const std::vector<std::string>& list, const std::string& value) {
    return std::find(list.begin(), list.end(), value) != list.end();
}

} // namespace

TEST_CASE("signatures: golden values of a box", "[signatures][golden]") {
    const NamedSolid box = plate();
    const BodySignatures sigs = occt::signatures(box.solid);
    REQUIRE(sigs.faces.size() == 6);
    CHECK(sigs.edges.size() == 12);
    CHECK(sigs.diagonal == Approx(std::sqrt(40.0 * 40.0 * 2.0 + 100.0)).epsilon(1e-6));

    const ElementSignature& top = faceOf(box, sigs, "F1/box/top");
    CHECK(top.kind == SignatureKind::face);
    CHECK(top.shape == SignatureShape::plane);
    CHECK(top.centroid.x == Approx(0.0).margin(1e-9));
    CHECK(top.centroid.y == Approx(0.0).margin(1e-9));
    CHECK(top.centroid.z == Approx(10.0));
    CHECK(top.hasDirection);
    CHECK(top.directed);
    CHECK(top.direction.z == Approx(1.0));
    CHECK(top.size == Approx(1600.0));
    CHECK(top.radius == 0.0);
    CHECK(top.faces.size() == 4); // all four sides, not the bottom

    const ElementSignature& bottom = faceOf(box, sigs, "F1/box/bottom");
    CHECK(bottom.direction.z == Approx(-1.0));
    const ElementSignature& right = faceOf(box, sigs, "F1/box/right");
    CHECK(right.direction.x == Approx(1.0));
    CHECK(right.size == Approx(400.0));
    CHECK(right.centroid.x == Approx(20.0));

    for (const ElementSignature& edge : sigs.edges) {
        CHECK(edge.kind == SignatureKind::edge);
        CHECK(edge.shape == SignatureShape::line);
        CHECK(edge.faces.size() == 2);
        CHECK_FALSE(edge.directed);
        CHECK((edge.size == Approx(40.0) || edge.size == Approx(10.0)));
    }
}

TEST_CASE("signatures: a cylinder has round faces and a seam", "[signatures][golden]") {
    const NamedSolid pin = occt::makeNamedCylinder(3.0, 20.0, {0.0, 0.0, 0.0});
    const BodySignatures sigs = occt::signatures(pin.solid);
    REQUIRE(sigs.faces.size() == 3);
    const ElementSignature& side = faceOf(pin, sigs, "side");
    CHECK(side.shape == SignatureShape::cylinder);
    CHECK(side.radius == Approx(3.0));
    CHECK(side.size == Approx(2.0 * kPi * 3.0 * 20.0));
    CHECK(side.hasDirection);
    CHECK_FALSE(side.directed);
    CHECK(side.direction.z == Approx(1.0)); // an axis points towards +Z, never -Z
    CHECK(side.faces.size() == 2);

    int circles = 0;
    int seams = 0;
    for (const ElementSignature& edge : sigs.edges) {
        if (edge.shape == SignatureShape::circle) {
            ++circles;
            CHECK(edge.radius == Approx(3.0));
            CHECK(edge.size == Approx(2.0 * kPi * 3.0));
            CHECK(edge.faces.size() == 2);
        } else {
            ++seams;
            CHECK(edge.shape == SignatureShape::line);
            CHECK(edge.faces.size() == 1); // the same face on both sides
        }
    }
    CHECK(circles == 2);
    CHECK(seams == 1);
}

TEST_CASE("signatures: every shape and every combined body is described completely",
          "[signatures]") {
    std::vector<NamedSolid> bodies;
    bodies.push_back(occt::makeNamedBox(10.0, 20.0, 30.0, {}, 0.0));
    bodies.push_back(occt::makeNamedBox(10.0, 20.0, 30.0, {}, 3.0)); // rounded corners
    bodies.push_back(occt::makeNamedCylinder(5.0, 10.0));
    bodies.push_back(occt::makeNamedPrimitive(SphereSpec{}));
    bodies.push_back(occt::makeNamedPrimitive(ConeSpec{}));
    bodies.push_back(occt::makeNamedPrimitive(TorusSpec{}));
    bodies.push_back(occt::booleanNamed(
        BooleanOp::subtract, plate(),
        {called(occt::makeNamedCylinder(3.0, 20.0, {0, 0, -5}), "F2/cylinder")}));
    for (const NamedSolid& body : bodies) {
        const SolidReport report = occt::measure(body.solid);
        const BodySignatures sigs = occt::signatures(body.solid);
        REQUIRE(sigs.faces.size() == static_cast<std::size_t>(report.faces));
        for (std::size_t i = 0; i < sigs.faces.size(); ++i) {
            CHECK(sigs.faces[i].id == i + 1);
            CHECK(sigs.faces[i].size > 0.0);
            CHECK(sigs.faces[i].shape != SignatureShape::unknown);
        }
        CHECK_FALSE(sigs.edges.empty());
        for (const ElementSignature& edge : sigs.edges) {
            CHECK_FALSE(edge.faces.empty());
            for (const std::uint32_t face : edge.faces) {
                CHECK((face >= 1 && face <= sigs.faces.size()));
            }
        }
        CHECK(sigs.diagonal > 0.0);
    }
}

TEST_CASE("signatures: the same body always looks the same", "[signatures]") {
    const NamedSolid a = plate();
    const BodySignatures first = occt::signatures(a.solid);
    const BodySignatures second = occt::signatures(a.solid);
    REQUIRE(first.faces.size() == second.faces.size());
    for (std::size_t i = 0; i < first.faces.size(); ++i) {
        CHECK(first.faces[i].centroid.x == second.faces[i].centroid.x);
        CHECK(first.faces[i].faces == second.faces[i].faces);
    }
}

TEST_CASE("signatures: a body from another kernel is refused", "[signatures]") {
    CHECK_THROWS_AS(occt::signatures(Solid{}), GeometryError);
}

TEST_CASE("history: a hole leaves the plate and uses up the ends of the pin", "[history]") {
    const NamedSolid pin =
        called(occt::makeNamedCylinder(3.0, 20.0, {0.0, 0.0, -5.0}), "F2/cylinder");
    const NamedSolid cut = occt::booleanNamed(BooleanOp::subtract, plate(), {pin});
    CHECK(cut.history.modified.empty());
    CHECK(cut.history.deleted.size() == 2);
    CHECK(contains(cut.history.deleted, "F2/cylinder/top"));
    CHECK(contains(cut.history.deleted, "F2/cylinder/bottom"));
}

TEST_CASE("history: a face cut in two is reported as its pieces", "[history]") {
    // A slot across the whole width splits top, front and back.
    const NamedSolid slot =
        called(occt::makeNamedBox(10.0, 60.0, 20.0, {-5.0, -30.0, 5.0}), "F2/box");
    const NamedSolid cut = occt::booleanNamed(BooleanOp::subtract, plate(), {slot});
    const auto split = std::find_if(cut.history.modified.begin(), cut.history.modified.end(),
                                    [](const auto& entry) { return entry.first == "F1/box/top"; });
    REQUIRE(split != cut.history.modified.end());
    CHECK(split->second.size() == 2);
    CHECK(contains(split->second, "F1/box/top#1"));
    CHECK(contains(split->second, "F1/box/top#2"));
    CHECK(contains(cut.history.deleted, "F2/box/top"));
}

TEST_CASE("history: a face joined into a face of the target is gone", "[history]") {
    const NamedSolid other =
        called(occt::makeNamedBox(40.0, 40.0, 10.0, {10.0, -20.0, 0.0}), "F2/box");
    const NamedSolid merged = occt::booleanNamed(BooleanOp::merge, plate(), {other});
    CHECK(contains(merged.history.deleted, "F2/box/top"));
    CHECK(contains(merged.history.deleted, "F2/box/bottom"));
    CHECK_FALSE(contains(merged.history.deleted, "F1/box/top"));
}

TEST_CASE("history: bodies made from nothing and pure moves have none", "[history]") {
    CHECK(plate().history.empty());
    CHECK(occt::translateNamed(plate(), {5.0, 0.0, 0.0}).history.empty());
    CHECK(occt::rotateNamed(plate(), {}, {}).history.empty());
}
