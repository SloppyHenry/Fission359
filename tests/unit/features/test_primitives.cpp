// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <memory>
#include <optional>

#include "fission/core/document.h"
#include "fission/core/feature_type.h"
#include "fission/features/builtin.h"
#include "fission/geo/occt/kernel.h"

using Catch::Approx;
using namespace fission::core;

namespace {

constexpr double kPi = 3.14159265358979323846;

struct Setup {
    FeatureRegistry registry;
    std::unique_ptr<fission::geo::GeometryKernel> kernel = fission::geo::occt::makeKernel();

    Setup() { fission::features::registerBuiltinFeatures(registry); }
};

ResolvedParams params(std::map<std::string, double> values) {
    ResolvedParams result;
    for (const auto& [name, value] : values) {
        result[name] = Quantity{value, {1, 0}};
    }
    return result;
}

struct Made {
    Document document{5};
    FeatureId id;
    FeatureResult result;
    fission::geo::SolidReport report;
    fission::geo::TriangleMesh mesh;
};

Made make(Setup& setup, const char* type, std::map<std::string, double> values) {
    Made made;
    const FeatureType* featureType = setup.registry.find(type);
    REQUIRE(featureType != nullptr);
    made.id = *made.document.addFeature(defaultDraft(*featureType));
    const Feature& feature = *made.document.findFeature(made.id);
    const ResolvedParams resolved = params(std::move(values));
    REQUIRE_FALSE(featureType->validate(resolved).has_value());
    const std::vector<FeatureOutput> none;
    const ExecuteContext context{*setup.kernel, {}};
    made.result = featureType->execute({feature, resolved, none}, context);
    REQUIRE(made.result.outputs.size() == 1);
    const FeatureOutput& output = made.result.outputs[0];
    made.report = setup.kernel->measure(output.solid);
    made.mesh = setup.kernel->tessellate(output.solid, {});
    return made;
}

// The unit normal shared by all vertices of the triangles of one face, or nothing when they differ.
std::optional<std::array<double, 3>> faceNormal(const fission::geo::TriangleMesh& mesh,
                                                std::uint32_t faceId) {
    std::optional<std::array<double, 3>> result;
    for (std::size_t t = 0; t < mesh.faceIds.size(); ++t) {
        if (mesh.faceIds[t] != faceId) {
            continue;
        }
        for (int corner = 0; corner < 3; ++corner) {
            const auto& n = mesh.normals[mesh.indices[t * 3 + static_cast<std::size_t>(corner)]];
            const std::array<double, 3> normal{n.x, n.y, n.z};
            if (!result) {
                result = normal;
            } else if (std::abs((*result)[0] - normal[0]) > 1e-4 ||
                       std::abs((*result)[1] - normal[1]) > 1e-4 ||
                       std::abs((*result)[2] - normal[2]) > 1e-4) {
                return std::nullopt;
            }
        }
    }
    return result;
}

std::uint32_t faceOf(const FeatureOutput& output, const std::string& role) {
    for (const auto& face : output.faces) {
        if (face.role == role) {
            return face.faceId;
        }
    }
    FAIL("no face with the role " << role);
    return 0;
}

} // namespace

TEST_CASE("the built-in feature types are registered", "[features]") {
    Setup setup;
    CHECK(setup.registry.ids() == std::vector<std::string>{"box", "cylinder"});
}

TEST_CASE("box: size, place, faces and their names", "[features]") {
    Setup setup;
    const Made made = make(
        setup, "box", {{"width", 20}, {"depth", 30}, {"height", 40}, {"x", 5}, {"y", 0}, {"z", 2}});
    const FeatureOutput& output = made.result.outputs[0];

    CHECK(made.report.valid);
    CHECK(made.report.volume == Approx(20.0 * 30.0 * 40.0).epsilon(1e-9));
    CHECK(made.report.boxMin.x == Approx(-5.0));
    CHECK(made.report.boxMax.x == Approx(15.0));
    CHECK(made.report.boxMin.y == Approx(-15.0));
    CHECK(made.report.boxMax.y == Approx(15.0));
    CHECK(made.report.boxMin.z == Approx(2.0));
    CHECK(made.report.boxMax.z == Approx(42.0));
    CHECK(made.report.faces == 6);

    // The step names every face it made (naming contract) ...
    CHECK_FALSE(checkNamingContract(output, made.report).has_value());

    // ... and the names are right: each role points to the face that looks that way.
    const std::map<std::string, std::array<double, 3>> expected{
        {"bottom", {0, 0, -1}}, {"top", {0, 0, 1}},   {"front", {0, -1, 0}},
        {"back", {0, 1, 0}},    {"left", {-1, 0, 0}}, {"right", {1, 0, 0}},
    };
    for (const auto& [role, normal] : expected) {
        INFO(role);
        const auto found = faceNormal(made.mesh, faceOf(output, role));
        REQUIRE(found.has_value());
        CHECK((*found)[0] == Approx(normal[0]).margin(1e-4));
        CHECK((*found)[1] == Approx(normal[1]).margin(1e-4));
        CHECK((*found)[2] == Approx(normal[2]).margin(1e-4));
    }
    CHECK(output.body == made.document.findFeature(made.id)->outputs.at(0));
    CHECK(elementName(*made.document.findFeature(made.id), "top") == "F1/box/top");
}

TEST_CASE("cylinder: round stays round, faces are named", "[features]") {
    Setup setup;
    const Made made =
        make(setup, "cylinder", {{"radius", 10}, {"height", 20}, {"x", 0}, {"y", 0}, {"z", 0}});
    const FeatureOutput& output = made.result.outputs[0];
    CHECK(made.report.valid);
    CHECK(made.report.volume == Approx(kPi * 10.0 * 10.0 * 20.0).epsilon(1e-9));
    CHECK(made.report.faces == 3);
    CHECK_FALSE(checkNamingContract(output, made.report).has_value());

    const auto bottom = faceNormal(made.mesh, faceOf(output, "bottom"));
    const auto top = faceNormal(made.mesh, faceOf(output, "top"));
    REQUIRE(bottom.has_value());
    REQUIRE(top.has_value());
    CHECK((*bottom)[2] == Approx(-1.0).margin(1e-4));
    CHECK((*top)[2] == Approx(1.0).margin(1e-4));
    // The side is curved: no single normal, and none of its triangles faces up or down.
    CHECK_FALSE(faceNormal(made.mesh, faceOf(output, "side")).has_value());
}

TEST_CASE("values that cannot make a body are stopped before the kernel", "[features]") {
    Setup setup;
    const FeatureType* box = setup.registry.find("box");
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const auto good = params({{"width", 1}, {"depth", 1}, {"height", 1}});

    CHECK_FALSE(box->validate(good).has_value());
    for (const auto& [name, value] :
         std::map<std::string, double>{{"width", 0}, {"depth", -3}, {"height", nan}}) {
        INFO(name);
        auto bad = good;
        bad[name].value = value;
        const auto issue = box->validate(bad);
        REQUIRE(issue.has_value());
        CHECK(issue->code == FeatureIssueCode::parameterOutOfRange);
        CHECK(issue->subject == name);
    }
}

TEST_CASE("steps describe themselves and offer handles that change values", "[features]") {
    Setup setup;
    const auto values =
        params({{"width", 20}, {"depth", 30}, {"height", 40}, {"x", 0}, {"y", 0}, {"z", 0}});

    const Description box = setup.registry.find("box")->describe(values);
    CHECK(box.key == "box");
    REQUIRE(box.values.size() == 3);
    CHECK(box.values[1].value == Approx(30.0));

    const auto handles = setup.registry.find("box")->handles(values);
    REQUIRE(handles.size() == 3);
    for (const HandleSpec& handle : handles) {
        // Every handle names the value it changes; none of them scales anything (rule 11).
        CHECK(values.contains(handle.parameter));
    }
    CHECK(handles[2].parameter == "height");
    CHECK(handles[2].origin.z == Approx(40.0));

    const auto cylinder = params({{"radius", 10}, {"height", 20}, {"x", 0}, {"y", 0}, {"z", 0}});
    const auto round = setup.registry.find("cylinder")->handles(cylinder);
    REQUIRE(round.size() == 2);
    CHECK(round[0].kind == HandleKind::radius);
}

TEST_CASE("a default draft has every value and its body", "[features]") {
    Setup setup;
    const FeatureDraft draft = defaultDraft(*setup.registry.find("box"));
    CHECK(draft.type == "box");
    CHECK(draft.newBodies == 1);
    CHECK(draft.params.size() == 6);
    CHECK(draft.params.at("width").expression == "20");
    CHECK(draft.params.at("x").kind == QuantityKind::length);
}
