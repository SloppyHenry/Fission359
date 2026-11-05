// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// STEP and IGES in, STEP out (AP-2.13). The files in tests/fixtures/step are described in the
// README there.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <numbers>
#include <set>
#include <string>
#include <vector>

#include "fission/geo/error.h"
#include "fission/geo/exchange.h"
#include "fission/geo/kernel.h"
#include "fission/geo/occt/kernel.h"
#include "fission/geo/occt/solids.h"

using Catch::Approx;
using namespace fission::geo;

namespace {

constexpr double kPi = std::numbers::pi;

Approx exactly(double expected) {
    return Approx(expected).epsilon(1e-6);
}

std::vector<std::uint8_t> fixture(const char* name) {
    const std::filesystem::path path =
        std::filesystem::path(FISSION_TEST_FIXTURES_DIR) / "step" / name;
    std::ifstream stream(path, std::ios::binary);
    REQUIRE(stream);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

// Colours in a file are fractions: 0.1 of 255 is 25.5, so a channel may be one off.
bool closeTo(const FileColor& color, int red, int green, int blue) {
    return std::abs(color.red - red) <= 1 && std::abs(color.green - green) <= 1 &&
           std::abs(color.blue - blue) <= 1;
}

struct Kernel {
    std::unique_ptr<GeometryKernel> kernel = occt::makeKernel();
};

void checkNamedCompletely(const GeometryKernel& kernel, const ImportedBody& body) {
    const SolidReport report = kernel.measure(body.body.solid);
    CHECK(report.valid);
    REQUIRE(body.body.faces.size() == static_cast<std::size_t>(report.faces));
    std::set<std::string> roles;
    std::set<std::uint32_t> ids;
    for (const NamedFace& face : body.body.faces) {
        roles.insert(face.role);
        ids.insert(face.faceId);
    }
    CHECK(roles.size() == body.body.faces.size()); // no name twice
    CHECK(ids.size() == body.body.faces.size());
    CHECK(roles.contains("face1"));
}

} // namespace

TEST_CASE("a STEP file with one body gives its volume, faces, name and colour", "[exchange]") {
    Kernel k;
    const ImportResult result =
        k.kernel->importFile(ImportFormat::step, fixture("block-with-hole.step"), {});
    REQUIRE(result.bodies.size() == 1);
    CHECK(result.skipped == 0);
    CHECK_FALSE(result.assembly);
    CHECK_FALSE(result.unitAssumed);
    const ImportedBody& body = result.bodies.front();
    const SolidReport report = k.kernel->measure(body.body.solid);
    CHECK(report.volume == exactly(20.0 * 10.0 * 5.0 - kPi * 2.0 * 2.0 * 5.0));
    CHECK(report.faces == 7);
    CHECK(report.boxMin.x == Approx(0.0).margin(1e-6));
    CHECK(report.boxMax.x == exactly(20.0));
    CHECK(body.name == "Block with hole");
    REQUIRE(body.color);
    CHECK(closeTo(*body.color, 204, 51, 26));
    CHECK_FALSE(body.repaired);
    checkNamedCompletely(*k.kernel, body);
}

TEST_CASE("a file written in inches comes in as millimetres", "[exchange]") {
    Kernel k;
    const ImportResult result =
        k.kernel->importFile(ImportFormat::step, fixture("cube-one-inch.step"), {});
    REQUIRE(result.bodies.size() == 1);
    const SolidReport report = k.kernel->measure(result.bodies.front().body.solid);
    CHECK(report.boxMax.x - report.boxMin.x == exactly(25.4));
    CHECK(report.volume == exactly(25.4 * 25.4 * 25.4));
}

TEST_CASE("the unit factor scales a file whose unit was guessed wrong", "[exchange]") {
    Kernel k;
    ImportOptions inches;
    inches.lengthFactor = 25.4;
    const ImportResult result =
        k.kernel->importFile(ImportFormat::step, fixture("block-with-hole.step"), inches);
    REQUIRE(result.bodies.size() == 1);
    const SolidReport report = k.kernel->measure(result.bodies.front().body.solid);
    CHECK(report.boxMax.x == exactly(20.0 * 25.4));
    CHECK(report.volume ==
          exactly((20.0 * 10.0 * 5.0 - kPi * 2.0 * 2.0 * 5.0) * 25.4 * 25.4 * 25.4));
    CHECK(report.valid);
    for (const double bad : {0.0, -1.0, std::numeric_limits<double>::quiet_NaN()}) {
        ImportOptions wrong;
        wrong.lengthFactor = bad;
        CHECK_THROWS_AS(
            k.kernel->importFile(ImportFormat::step, fixture("block-with-hole.step"), wrong),
            GeometryError);
    }
}

TEST_CASE("an assembly is read flat, every part a body with its place and name", "[exchange]") {
    Kernel k;
    const ImportResult result =
        k.kernel->importFile(ImportFormat::step, fixture("assembly-two-parts.step"), {});
    CHECK(result.assembly);
    REQUIRE(result.bodies.size() == 2);
    std::set<std::string> names;
    for (const ImportedBody& body : result.bodies) {
        names.insert(body.name);
        checkNamedCompletely(*k.kernel, body);
        const SolidReport report = k.kernel->measure(body.body.solid);
        if (body.name == "Rod") {
            CHECK(report.volume == exactly(kPi * 4.0 * 4.0 * 12.0));
            CHECK(report.boxMin.x == exactly(26.0)); // moved by 30 in the assembly
            REQUIRE(body.color);
            CHECK(closeTo(*body.color, 26, 77, 230));
        } else {
            CHECK(report.volume == exactly(1000.0));
            CHECK(report.boxMin.x == Approx(0.0).margin(1e-6));
        }
    }
    CHECK(names == std::set<std::string>{"Cube", "Rod"});
}

TEST_CASE("a file with only a surface has no body and says so", "[exchange]") {
    Kernel k;
    try {
        k.kernel->importFile(ImportFormat::step, fixture("sheet-only.step"), {});
        FAIL("expected an error");
    } catch (const GeometryError& error) {
        CHECK(error.code() == GeometryErrorCode::importEmpty);
    }
}

TEST_CASE("bytes that are no STEP or IGES file are refused cleanly", "[exchange]") {
    Kernel k;
    const std::vector<std::vector<std::uint8_t>> junk = {
        {}, {'h', 'e', 'l', 'l', 'o'}, std::vector<std::uint8_t>(5000, 0xFF)};
    for (const auto& bytes : junk) {
        for (const ImportFormat format : {ImportFormat::step, ImportFormat::iges}) {
            try {
                k.kernel->importFile(format, bytes, {});
                FAIL("expected an error");
            } catch (const GeometryError& error) {
                CHECK((error.code() == GeometryErrorCode::importUnreadable ||
                       error.code() == GeometryErrorCode::importEmpty));
            }
        }
    }
    // A cut-off file never gives a wrong body: either an error or valid bodies.
    std::vector<std::uint8_t> cut = fixture("block-with-hole.step");
    cut.resize(cut.size() / 2);
    try {
        const ImportResult result = k.kernel->importFile(ImportFormat::step, cut, {});
        for (const ImportedBody& body : result.bodies) {
            CHECK(k.kernel->measure(body.body.solid).valid);
        }
    } catch (const GeometryError&) {
        SUCCEED();
    }
}

TEST_CASE("an IGES file gives the same body", "[exchange]") {
    Kernel k;
    const ImportResult result =
        k.kernel->importFile(ImportFormat::iges, fixture("block-with-hole.iges"), {});
    REQUIRE(result.bodies.size() == 1);
    CHECK_FALSE(result.unitAssumed);
    const SolidReport report = k.kernel->measure(result.bodies.front().body.solid);
    CHECK(report.volume == exactly(20.0 * 10.0 * 5.0 - kPi * 2.0 * 2.0 * 5.0));
    CHECK(report.faces == 7);
    checkNamedCompletely(*k.kernel, result.bodies.front());
}

TEST_CASE("an IGES file with only surfaces has no body and says so", "[exchange]") {
    Kernel k;
    try {
        k.kernel->importFile(ImportFormat::iges, fixture("block-faces-only.iges"), {});
        FAIL("expected an error");
    } catch (const GeometryError& error) {
        CHECK(error.code() == GeometryErrorCode::importEmpty);
    }
}

TEST_CASE("export then import keeps volume, faces, names and colours", "[exchange]") {
    Kernel k;
    const NamedSolid box = occt::makeNamedBox(30.0, 20.0, 10.0, {5.0, 5.0, 0.0}, 3.0);
    const NamedSolid cylinder = occt::makeNamedCylinder(6.0, 25.0, {-20.0, 0.0, 0.0});
    const std::vector<ExportBody> out = {
        {box.solid, "Rounded plate", FileColor{10, 200, 30}},
        {cylinder.solid, "Pin", std::nullopt},
    };
    const std::string text = k.kernel->exportStep(out);
    CHECK(text.starts_with("ISO-10303-21;"));
    const ImportResult back = k.kernel->importFile(
        ImportFormat::step, std::vector<std::uint8_t>(text.begin(), text.end()), {});
    REQUIRE(back.bodies.size() == 2);
    for (const ImportedBody& body : back.bodies) {
        const bool isPlate = body.name == "Rounded plate";
        const NamedSolid& original = isPlate ? box : cylinder;
        const SolidReport before = k.kernel->measure(original.solid);
        const SolidReport after = k.kernel->measure(body.body.solid);
        CHECK(after.valid);
        CHECK(after.volume == Approx(before.volume).epsilon(1e-9));
        CHECK(after.area == Approx(before.area).epsilon(1e-9));
        CHECK(after.faces == before.faces);
        CHECK(after.boxMin.x == Approx(before.boxMin.x).margin(1e-7));
        CHECK(after.boxMax.z == Approx(before.boxMax.z).margin(1e-7));
        if (isPlate) {
            REQUIRE(body.color);
            CHECK(*body.color == FileColor{10, 200, 30});
        } else {
            CHECK(body.name == "Pin");
            CHECK_FALSE(body.color);
        }
    }
}

TEST_CASE("names with umlauts survive a STEP round trip", "[exchange]") {
    Kernel k;
    const NamedSolid box = occt::makeNamedBox(10.0, 10.0, 10.0);
    const std::string name = "Gro\xC3\x9F"
                             "es Geh\xC3\xA4use \xE2\x80\x93 \xE6\x97\xA5\xE6\x9C\xAC";
    const std::string text = k.kernel->exportStep({{box.solid, name, std::nullopt}});
    const ImportResult back = k.kernel->importFile(
        ImportFormat::step, std::vector<std::uint8_t>(text.begin(), text.end()), {});
    REQUIRE(back.bodies.size() == 1);
    CHECK(back.bodies.front().name == name);
}

TEST_CASE("exporting a body of another kernel is refused", "[exchange]") {
    Kernel k;
    CHECK_THROWS_AS(k.kernel->exportStep({{Solid{}, "x", std::nullopt}}), GeometryError);
}
