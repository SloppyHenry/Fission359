// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
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
    // Values the test does not mention keep their defaults.
    ResolvedParams resolved = params(std::move(values));
    for (const ParamSpec& spec : featureType->parameters()) {
        if (!resolved.contains(spec.name)) {
            resolved[spec.name] = Quantity{std::stod(spec.defaultExpression), {1, 0}};
        }
    }
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
    CHECK(setup.registry.ids() == std::vector<std::string>{"box", "cone", "cylinder", "hemisphere",
                                                           "prism", "pyramid", "sphere", "star",
                                                           "text", "torus", "tube", "wedge"});
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
    const auto good = params({{"width", 1}, {"depth", 1}, {"height", 1}, {"cornerRadius", 0}});

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
    const auto values = params({{"width", 20},
                                {"depth", 30},
                                {"height", 40},
                                {"cornerRadius", 3},
                                {"x", 0},
                                {"y", 0},
                                {"z", 0}});

    const Description box = setup.registry.find("box")->describe(values);
    CHECK(box.key == "box");
    REQUIRE(box.values.size() == 3);
    CHECK(box.values[1].value == Approx(30.0));

    const auto handles = setup.registry.find("box")->handles(values);
    // A value that can be taken from several equal places is listed once for each place: both
    // ends of the width and the depth, one for the height, one at each of the four corners.
    REQUIRE(handles.size() == 9);
    std::map<std::string, int> places;
    for (const HandleSpec& handle : handles) {
        // Every handle names the value it changes; none of them scales anything (rule 11).
        CHECK(values.contains(handle.parameter));
        ++places[handle.parameter];
        // The band it measures ends at the handle and is as long as the value says.
        if (handle.parameter != "cornerRadius") {
            CHECK(handle.extent == Approx(values.at(handle.parameter).value));
        } else {
            CHECK(handle.extent == Approx(3.0));
        }
    }
    CHECK(places == std::map<std::string, int>{
                        {"width", 2}, {"depth", 2}, {"height", 1}, {"cornerRadius", 4}});
    const auto height = std::find_if(handles.begin(), handles.end(), [](const HandleSpec& handle) {
        return handle.parameter == "height";
    });
    REQUIRE(height != handles.end());
    CHECK(height->origin.z == Approx(40.0));
    // The two ends of the width face opposite ways and sit half a width from the middle.
    std::vector<HandleSpec> width;
    std::copy_if(handles.begin(), handles.end(), std::back_inserter(width),
                 [](const HandleSpec& handle) { return handle.parameter == "width"; });
    REQUIRE(width.size() == 2);
    CHECK(width[0].direction.x == Approx(-width[1].direction.x));
    CHECK(width[0].origin.x == Approx(-width[1].origin.x));
    CHECK(std::abs(width[0].origin.x) == Approx(10.0));

    const auto cylinder = params({{"radius", 10}, {"height", 20}, {"x", 0}, {"y", 0}, {"z", 0}});
    const auto round = setup.registry.find("cylinder")->handles(cylinder);
    // Round: the radius can be taken at eight places all around, the height at the top.
    REQUIRE(round.size() == 9);
    CHECK(round[0].kind == HandleKind::radius);
    for (int k = 0; k < 8; ++k) {
        CHECK(round[static_cast<std::size_t>(k)].parameter == "radius");
        // Every place is 10 mm from the axis and measures from the axis to there.
        CHECK(std::hypot(round[static_cast<std::size_t>(k)].origin.x,
                         round[static_cast<std::size_t>(k)].origin.y) == Approx(10.0));
        CHECK(round[static_cast<std::size_t>(k)].extent == Approx(10.0));
    }
    CHECK(round[8].parameter == "height");
}

TEST_CASE("a default draft has every value and its body", "[features]") {
    Setup setup;
    const FeatureDraft draft = defaultDraft(*setup.registry.find("box"));
    CHECK(draft.type == "box");
    CHECK(draft.newBodies == 1);
    CHECK(draft.params.size() == 10); // four sizes, the place and the turn
    CHECK(draft.params.at("width").expression == "20");
    CHECK(draft.params.at("x").kind == QuantityKind::length);
}

TEST_CASE("box: rounded corners keep their radius when the box gets wider", "[features]") {
    // Ablauf D, step 1: the handle changes the width, never scales (rule 11).
    Setup setup;
    for (const double width : {40.0, 80.0}) {
        INFO("width " << width);
        const Made made = make(
            setup, "box", {{"width", width}, {"depth", 40}, {"height", 10}, {"cornerRadius", 5}});
        CHECK(made.report.valid);
        CHECK(made.report.volume ==
              Approx((width * 40.0 - (4.0 - kPi) * 25.0) * 10.0).epsilon(1e-6));
        CHECK_FALSE(checkNamingContract(made.result.outputs[0], made.report).has_value());
        CHECK(made.report.faces == 10);
    }
}

TEST_CASE("box: a corner radius that does not fit is stopped before the kernel", "[features]") {
    Setup setup;
    const FeatureType* box = setup.registry.find("box");
    auto values = params({{"width", 40}, {"depth", 10}, {"height", 5}, {"cornerRadius", 5}});
    CHECK_FALSE(box->validate(values).has_value()); // exactly half the shorter side is fine
    for (const double bad : {5.5, -1.0, std::numeric_limits<double>::quiet_NaN()}) {
        INFO("radius " << bad);
        values["cornerRadius"].value = bad;
        const auto issue = box->validate(values);
        REQUIRE(issue.has_value());
        CHECK(issue->code == FeatureIssueCode::parameterOutOfRange);
        CHECK(issue->subject == "cornerRadius");
    }
}

namespace {

struct ShapeCase {
    const char* type;
    std::map<std::string, double> values;
    double volume;
    int faces;
};

} // namespace

TEST_CASE("every basic shape is made, named and offers handles for its own values", "[features]") {
    Setup setup;
    const std::vector<ShapeCase> shapes{
        {"sphere", {{"radius", 10}}, 4.0 / 3.0 * kPi * 1000.0, 1},
        {"hemisphere", {{"radius", 10}}, 2.0 / 3.0 * kPi * 1000.0, 2},
        {"cone", {{"radius", 10}, {"topRadius", 0}, {"height", 24}}, kPi * 100.0 * 24.0 / 3.0, 2},
        {"torus", {{"radius", 15}, {"tubeRadius", 5}}, 2.0 * kPi * kPi * 15.0 * 25.0, 1},
        {"tube", {{"radius", 10}, {"wall", 2}, {"height", 30}}, kPi * 30.0 * 36.0, 4},
        {"prism",
         {{"sides", 6}, {"radius", 10}, {"height", 7}},
         0.5 * 6 * 100.0 * std::sin(kPi / 3.0) * 7.0,
         8},
        {"pyramid", {{"width", 30}, {"depth", 20}, {"height", 12}}, 30.0 * 20.0 * 12.0 / 3.0, 5},
        {"wedge", {{"width", 20}, {"depth", 30}, {"height", 10}}, 20.0 * 30.0 * 10.0 / 2.0, 5},
        {"star",
         {{"points", 5}, {"radius", 20}, {"innerRadius", 8}, {"height", 5}},
         5 * 20.0 * 8.0 * std::sin(kPi / 5.0) * 5.0,
         12},
    };
    for (const ShapeCase& shape : shapes) {
        INFO(shape.type);
        const FeatureType* type = setup.registry.find(shape.type);
        REQUIRE(type != nullptr);
        const Made made = make(setup, shape.type, shape.values);
        CHECK(made.report.valid);
        CHECK(made.report.volume == Approx(shape.volume).epsilon(1e-6));
        CHECK(made.report.faces == shape.faces);
        CHECK_FALSE(checkNamingContract(made.result.outputs[0], made.report).has_value());

        // Every handle names a value of the step; none of them scales anything (rule 11).
        const auto resolved = [&] {
            ResolvedParams all;
            for (const ParamSpec& spec : type->parameters()) {
                all[spec.name] = Quantity{std::stod(spec.defaultExpression), {1, 0}};
            }
            for (const auto& [name, value] : shape.values) {
                all[name].value = value;
            }
            return all;
        }();
        const auto handles = type->handles(resolved);
        CHECK_FALSE(handles.empty());
        for (const HandleSpec& handle : handles) {
            INFO(handle.parameter);
            CHECK(resolved.contains(handle.parameter));
            CHECK(handle.sign == 1.0);
        }
        CHECK(type->describe(resolved).key == shape.type);
        CHECK_FALSE(type->validate(resolved).has_value());
    }
}

TEST_CASE("shapes with impossible values are stopped before the kernel", "[features]") {
    Setup setup;
    const auto stopped = [&](const char* type, std::map<std::string, double> values,
                             const char* subject) {
        INFO(type << " " << subject);
        const FeatureType* featureType = setup.registry.find(type);
        ResolvedParams all;
        for (const ParamSpec& spec : featureType->parameters()) {
            all[spec.name] = Quantity{std::stod(spec.defaultExpression), {1, 0}};
        }
        for (const auto& [name, value] : values) {
            all[name].value = value;
        }
        const auto issue = featureType->validate(all);
        REQUIRE(issue.has_value());
        CHECK(issue->code == FeatureIssueCode::parameterOutOfRange);
        CHECK(issue->subject == subject);
    };
    stopped("sphere", {{"radius", 0}}, "radius");
    stopped("cone", {{"topRadius", -1}}, "topRadius");
    stopped("torus", {{"tubeRadius", 15}}, "tubeRadius");
    stopped("tube", {{"wall", 10}}, "wall");
    stopped("prism", {{"sides", 2}}, "sides");
    stopped("prism", {{"sides", 5.5}}, "sides");
    stopped("prism", {{"sides", 100}}, "sides");
    stopped("star", {{"innerRadius", 20}}, "innerRadius");
    stopped("star", {{"points", 2}}, "points");
    stopped("pyramid", {{"depth", 0}}, "depth");
    stopped("wedge", {{"height", -4}}, "height");
}

namespace {

ResolvedParams resolvedFor(const FeatureType& type, const std::map<std::string, double>& values) {
    ResolvedParams all;
    for (const ParamSpec& spec : type.parameters()) {
        all[spec.name] = Quantity{std::stod(spec.defaultExpression), {1, 0}};
    }
    for (const auto& [name, value] : values) {
        all[name].value = value;
    }
    return all;
}

// One of every kind of shape, with a place that is not the origin.
const std::vector<std::pair<const char*, std::map<std::string, double>>> kTurnCases{
    {"box", {{"width", 20}, {"depth", 30}, {"height", 40}, {"cornerRadius", 4}}},
    {"cylinder", {{"radius", 8}, {"height", 25}}},
    {"sphere", {{"radius", 10}}},
    {"hemisphere", {{"radius", 10}}},
    {"cone", {{"radius", 10}, {"topRadius", 4}, {"height", 24}}},
    {"torus", {{"radius", 15}, {"tubeRadius", 5}}},
    {"tube", {{"radius", 10}, {"wall", 2}, {"height", 30}}},
    {"prism", {{"sides", 5}, {"radius", 10}, {"height", 7}}},
    {"pyramid", {{"width", 30}, {"depth", 20}, {"height", 12}}},
    {"wedge", {{"width", 20}, {"depth", 30}, {"height", 10}}},
    {"star", {{"points", 5}, {"radius", 20}, {"innerRadius", 8}, {"height", 5}}},
};

} // namespace

TEST_CASE("turn: every shape turns about the middle of the box round it", "[features][turn]") {
    Setup setup;
    for (const auto& [name, values] : kTurnCases) {
        INFO(name);
        std::map<std::string, double> placed = values;
        placed["x"] = 12;
        placed["y"] = -7;
        placed["z"] = 3;
        const FeatureType& type = *setup.registry.find(name);
        const Made made = make(setup, name, placed);
        const fission::geo::Point3d centre = type.bodyCentre(resolvedFor(type, placed));
        // A star or a prism with an odd number of corners turns about its axis, which is not the
        // middle of its box in Y; there the point must still lie inside the box.
        const bool onAxis = std::string(name) == "prism" || std::string(name) == "star";
        CHECK(centre.x == Approx((made.report.boxMin.x + made.report.boxMax.x) / 2).margin(1e-6));
        if (onAxis) {
            CHECK(centre.y == Approx(-7.0).margin(1e-9));
            CHECK(centre.y > made.report.boxMin.y);
            CHECK(centre.y < made.report.boxMax.y);
        } else {
            CHECK(centre.y ==
                  Approx((made.report.boxMin.y + made.report.boxMax.y) / 2).margin(1e-6));
        }
        CHECK(centre.z == Approx((made.report.boxMin.z + made.report.boxMax.z) / 2).margin(1e-6));
    }
}

TEST_CASE("turn: a turned shape is the same body, valid, with the same faces and names",
          "[features][turn]") {
    Setup setup;
    for (const auto& [name, values] : kTurnCases) {
        INFO(name);
        const Made flat = make(setup, name, values);
        std::map<std::string, double> turnedValues = values;
        turnedValues["rotX"] = 30;
        turnedValues["rotY"] = 20;
        turnedValues["rotZ"] = 50;
        const Made turned = make(setup, name, turnedValues);
        CHECK(turned.report.valid);
        CHECK(turned.report.volume == Approx(flat.report.volume).epsilon(1e-6));
        CHECK(turned.report.area == Approx(flat.report.area).epsilon(1e-6));
        CHECK(turned.report.faces == flat.report.faces);
        CHECK_FALSE(checkNamingContract(turned.result.outputs[0], turned.report).has_value());
        const auto& a = flat.result.outputs[0].faces;
        const auto& b = turned.result.outputs[0].faces;
        REQUIRE(a.size() == b.size());
        for (std::size_t i = 0; i < a.size(); ++i) {
            CHECK(a[i].role == b[i].role);
        }
    }
}

TEST_CASE("turn: the top of a box that lies on its side faces sideways", "[features][turn]") {
    Setup setup;
    const Made lying =
        make(setup, "box", {{"width", 20}, {"depth", 20}, {"height", 40}, {"rotX", 90}});
    const FeatureOutput& output = lying.result.outputs[0];
    const auto normal = faceNormal(lying.mesh, faceOf(output, "top"));
    REQUIRE(normal.has_value());
    // Tipped 90 degrees about X, up becomes -Y.
    CHECK((*normal)[0] == Approx(0.0).margin(1e-4));
    CHECK((*normal)[1] == Approx(-1.0).margin(1e-4));
    CHECK((*normal)[2] == Approx(0.0).margin(1e-4));
    // Its middle stays where it was, 20 above the plane.
    CHECK((lying.report.boxMin.z + lying.report.boxMax.z) / 2 == Approx(20.0).margin(1e-6));
}

TEST_CASE("turn: the handles turn with the body", "[features][turn]") {
    Setup setup;
    const FeatureType& box = *setup.registry.find("box");
    ResolvedParams p = resolvedFor(box, {{"width", 20}, {"depth", 20}, {"height", 40}});
    const auto upright = box.placedHandles(p);
    CHECK(upright.size() == box.handles(p).size());

    p["rotX"].value = 90;
    const auto lying = box.placedHandles(p);
    for (const HandleSpec& handle : lying) {
        if (handle.parameter != "height") {
            continue;
        }
        // Was on the top (z = 40) pointing up; now at the end that points to -Y.
        CHECK(handle.origin.x == Approx(0.0).margin(1e-9));
        CHECK(handle.origin.y == Approx(-20.0).margin(1e-9));
        CHECK(handle.origin.z == Approx(20.0).margin(1e-9));
        CHECK(handle.direction.y == Approx(-1.0).margin(1e-9));
        CHECK(handle.direction.z == Approx(0.0).margin(1e-9));
    }
    // The rise of a sphere's handle turns too (up becomes -Y).
    const FeatureType& sphere = *setup.registry.find("sphere");
    ResolvedParams s = resolvedFor(sphere, {{"radius", 10}});
    s["rotX"].value = 90;
    for (const HandleSpec& handle : sphere.placedHandles(s)) {
        CHECK(handle.riseDirection.y == Approx(-1.0).margin(1e-9));
    }
}

TEST_CASE("turn: no turn at all costs the kernel nothing", "[features][turn]") {
    Setup setup;
    const FeatureType& box = *setup.registry.find("box");
    for (const char* name : {"rotX", "rotY", "rotZ"}) {
        const auto specs = box.parameters();
        const auto found = std::find_if(specs.begin(), specs.end(),
                                        [&](const ParamSpec& s) { return s.name == name; });
        REQUIRE(found != specs.end());
        CHECK(found->kind == QuantityKind::angle);
        CHECK(found->defaultExpression == "0");
    }
}

// --- making shapes larger and smaller in all directions alike (AP-1.6b2) ----------------------

namespace {

/// The values of a shape with every size times `factor` (what the tool "Size" writes).
std::map<std::string, double> scaledValues(const FeatureType& type,
                                           std::map<std::string, double> values, double factor) {
    const ResolvedParams all = resolvedFor(type, values);
    for (const std::string& name : type.sizeParameters()) {
        values[name] = all.at(name).value * factor;
    }
    return values;
}

} // namespace

TEST_CASE("scale: the sizes of a shape are its lengths, never its counts, place or turn",
          "[features][scale]") {
    Setup setup;
    const auto sizes = [&](const char* type) {
        return setup.registry.find(type)->sizeParameters();
    };
    using Names = std::vector<std::string>;
    CHECK(sizes("box") == Names{"width", "depth", "height", "cornerRadius"});
    CHECK(sizes("cylinder") == Names{"radius", "height"});
    CHECK(sizes("tube") == Names{"radius", "wall", "height"});
    CHECK(sizes("star") == Names{"radius", "innerRadius", "height"});
    CHECK(sizes("prism") == Names{"radius", "height"});
    CHECK(sizes("cone") == Names{"radius", "topRadius", "height"});
    CHECK(sizes("text") == Names{"size", "height"});
}

TEST_CASE("scale: every shape grows alike, volume by the factor cubed, faces and names kept",
          "[features][scale]") {
    Setup setup;
    for (const auto& [name, values] : kTurnCases) {
        const FeatureType& type = *setup.registry.find(name);
        std::map<std::string, double> placed = values;
        placed["x"] = 12;
        placed["y"] = -7;
        placed["z"] = 3;
        const Made before = make(setup, name, placed);
        for (const double factor : {2.0, 0.5, 1.37}) {
            INFO(name << " at " << factor);
            const Made after = make(setup, name, scaledValues(type, placed, factor));
            CHECK(after.report.valid);
            CHECK(after.report.volume ==
                  Approx(before.report.volume * factor * factor * factor).epsilon(1e-6));
            CHECK(after.report.area == Approx(before.report.area * factor * factor).epsilon(1e-6));
            CHECK(after.report.faces == before.report.faces);
            CHECK(after.report.edges == before.report.edges);
            CHECK(after.report.vertices == before.report.vertices);
            CHECK_FALSE(checkNamingContract(after.result.outputs[0], after.report).has_value());
            const auto& a = before.result.outputs[0].faces;
            const auto& b = after.result.outputs[0].faces;
            REQUIRE(a.size() == b.size());
            for (std::size_t i = 0; i < a.size(); ++i) {
                CHECK(a[i].role == b[i].role);
            }
            // It grew about the place of the step (the middle of its underside): the box round it
            // is the old box, from that point, times the factor.
            const fission::geo::Point3d foot{12.0, -7.0, 3.0};
            CHECK(after.report.boxMin.x - foot.x ==
                  Approx((before.report.boxMin.x - foot.x) * factor).margin(1e-6));
            CHECK(after.report.boxMax.y - foot.y ==
                  Approx((before.report.boxMax.y - foot.y) * factor).margin(1e-6));
            CHECK(after.report.boxMin.z == Approx(3.0).margin(1e-6)); // still on its plane
            CHECK(after.report.boxMax.z - foot.z ==
                  Approx((before.report.boxMax.z - foot.z) * factor).margin(1e-6));
        }
    }
}

TEST_CASE("scale: a box with 5 mm corners at 200 % has round 10 mm corners", "[features][scale]") {
    // Ablauf D, step 1, for the corner handle: a rounded box stays a rounded box, nothing turns
    // into an ellipse. The volume of a box w x d x h with round corners r is h (w d - (4 - pi) r²):
    // it only comes out like that when every corner is a quarter circle of radius r.
    Setup setup;
    const FeatureType& type = *setup.registry.find("box");
    const std::map<std::string, double> values{
        {"width", 40}, {"depth", 40}, {"height", 10}, {"cornerRadius", 5}};
    const std::map<std::string, double> twice = scaledValues(type, values, 2.0);
    CHECK(twice.at("cornerRadius") == 10.0);
    CHECK(twice.at("width") == 80.0);
    const Made made = make(setup, "box", twice);
    CHECK(made.report.valid);
    CHECK(made.report.volume == Approx(20.0 * (80.0 * 80.0 - (4.0 - kPi) * 100.0)).epsilon(1e-9));
    // The face of each corner is a quarter of a cylinder r 10, h 20.
    const fission::geo::TriangleMesh& mesh = made.mesh;
    const FeatureOutput& output = made.result.outputs[0];
    const std::uint32_t corner = faceOf(output, "cornerFrontLeft");
    double nearest = 1e9;
    double farthest = 0.0;
    // Corner front left: the middle of its quarter circle is at (-30, -30).
    for (std::size_t t = 0; t < mesh.faceIds.size(); ++t) {
        if (mesh.faceIds[t] != corner) {
            continue;
        }
        for (int k = 0; k < 3; ++k) {
            const auto& p = mesh.positions[mesh.indices[t * 3 + static_cast<std::size_t>(k)]];
            const double r = std::hypot(p.x + 30.0, p.y + 30.0);
            nearest = std::min(nearest, r);
            farthest = std::max(farthest, r);
        }
    }
    CHECK(nearest == Approx(10.0).margin(1e-4)); // the mesh is in float
    CHECK(farthest == Approx(10.0).margin(1e-4));
}

TEST_CASE("scale: a turned shape grows about the place of its step like a picture",
          "[features][scale]") {
    // The tool "Size" relies on this: with the place kept, sizes times a factor are the turned
    // body made larger about the place of the step (the middle of its underside before the turn),
    // because the point it turns about grows with it. So it can grow about any point.
    Setup setup;
    for (const auto& [name, values] : kTurnCases) {
        INFO(name);
        const FeatureType& type = *setup.registry.find(name);
        std::map<std::string, double> placed = values;
        placed["x"] = 5;
        placed["y"] = 6;
        placed["z"] = 7;
        placed["rotX"] = 30;
        placed["rotY"] = 40;
        placed["rotZ"] = 50;
        const Made before = make(setup, name, placed);
        const Made after = make(setup, name, scaledValues(type, placed, 1.5));
        const auto box = [&](const Made& made) {
            return setup.kernel->exactBounds(made.result.outputs[0].solid);
        };
        const fission::geo::Bounds a = box(before);
        const fission::geo::Bounds b = box(after);
        CHECK(b.min.x - 5.0 == Approx((a.min.x - 5.0) * 1.5).margin(1e-5));
        CHECK(b.max.y - 6.0 == Approx((a.max.y - 6.0) * 1.5).margin(1e-5));
        CHECK(b.min.z - 7.0 == Approx((a.min.z - 7.0) * 1.5).margin(1e-5));
        CHECK(b.max.z - 7.0 == Approx((a.max.z - 7.0) * 1.5).margin(1e-5));
        CHECK(after.report.valid);
    }
}
