// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The step "mirror" (AP-2.8c): the mirror image of a body at a plane, as a second body (copy),
// joined to the original (merge) or alone (only).

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <set>

#include "../naming/refs_support.h"

using Catch::Approx;
using namespace fission::test;
namespace core = fission::core;
namespace geo = fission::geo;

namespace {

core::FeatureId addMirror(RefWorld& world, const core::FeatureId& source, const char* mode,
                          std::map<std::string, std::string> values) {
    core::FeatureDraft draft = core::defaultDraft(*world.registry.find("mirror"));
    draft.inputs = {world.bodyOf(source)};
    draft.texts["mode"] = mode;
    draft.newBodies = std::string(mode) == "copy" ? 2 : 1;
    for (const auto& [name, expression] : values) {
        draft.params.at(name).expression = expression;
    }
    return *world.document.addFeature(std::move(draft));
}

const core::BodyResult& bodyOf(const core::RecomputeResult& result, const core::Feature& step,
                               std::size_t number) {
    return result.bodies.at(step.outputs.at(number));
}

std::vector<std::string> namesOf(const core::BodyResult& body) {
    std::vector<std::string> names;
    for (const core::BodyFace& face : body.faces) {
        names.push_back(face.name);
    }
    return names;
}

bool allDifferent(const std::vector<std::string>& names) {
    return std::set<std::string>(names.begin(), names.end()).size() == names.size();
}

// An L: a 30 x 10 x 10 bar with a 10 x 10 x 10 block on top at its left end.
core::FeatureId lShape(RefWorld& world) {
    const core::FeatureId bar =
        world.add("box", {{"width", "30"}, {"depth", "10"}, {"height", "10"}, {"x", "0"}});
    const core::FeatureId block = world.add(
        "box", {{"width", "10"}, {"depth", "10"}, {"height", "10"}, {"x", "-10"}, {"z", "10"}});
    return world.add("combine", {}, {world.bodyOf(bar), world.bodyOf(block)}, {{"mode", "merge"}});
}

} // namespace

TEST_CASE("mirror: copy keeps the body and adds its mirror image, touching", "[mirror][feature]") {
    RefWorld world;
    const core::FeatureId box = world.plate("20", "20", "20");
    const geo::Bounds before = [&] {
        const core::RecomputeResult first = world.run();
        return world.kernel->bounds(first.bodies.at(world.bodyOf(box)).solid);
    }();
    const core::FeatureId step =
        addMirror(world, box, "copy", {{"planeX", std::to_string(before.max.x)}});
    const core::RecomputeResult result = world.run();
    REQUIRE(result.state(step)->status == core::FeatureStatus::ok);
    const core::Feature& feature = *world.document.findFeature(step);
    REQUIRE(feature.outputs.size() == 2);
    const geo::SolidReport original = world.kernel->measure(bodyOf(result, feature, 0).solid);
    const geo::SolidReport image = world.kernel->measure(bodyOf(result, feature, 1).solid);
    CHECK(original.valid);
    CHECK(image.valid);
    CHECK(original.volume == Approx(8000.0).epsilon(1e-9));
    CHECK(image.volume == Approx(8000.0).epsilon(1e-9));
    CHECK(image.faces == 6);
    CHECK(image.boxMin.x == Approx(before.max.x).margin(1e-7)); // they just touch
    CHECK(image.boxMax.x == Approx(before.max.x + 20.0).margin(1e-7));
    // The original keeps its names; the image has its own, all different.
    CHECK(allDifferent(namesOf(bodyOf(result, feature, 0))));
    CHECK(allDifferent(namesOf(bodyOf(result, feature, 1))));
    for (const std::string& name : namesOf(bodyOf(result, feature, 1))) {
        CHECK(name.find("[mirror]") != std::string::npos);
    }
}

TEST_CASE("mirror: merge makes one body of twice the volume, only keeps the image",
          "[mirror][feature]") {
    RefWorld world;
    const core::FeatureId box = world.plate("20", "20", "20");
    const geo::Bounds box0 = [&] {
        const core::RecomputeResult first = world.run();
        return world.kernel->bounds(first.bodies.at(world.bodyOf(box)).solid);
    }();
    const std::string at = std::to_string(box0.max.x);
    const core::FeatureId joined = addMirror(world, box, "merge", {{"planeX", at}});
    core::RecomputeResult result = world.run();
    REQUIRE(result.state(joined)->status == core::FeatureStatus::ok);
    {
        const geo::SolidReport report =
            world.kernel->measure(bodyOf(result, *world.document.findFeature(joined), 0).solid);
        CHECK(report.valid);
        CHECK(report.solids == 1);
        CHECK(report.volume == Approx(16000.0).epsilon(1e-9));
        CHECK(report.faces == 6); // the two boxes grow into one
    }
    RefWorld other;
    const core::FeatureId box2 = other.plate("20", "20", "20");
    const core::FeatureId only = addMirror(other, box2, "only", {{"planeX", at}});
    result = other.run();
    REQUIRE(result.state(only)->status == core::FeatureStatus::ok);
    const geo::SolidReport report =
        other.kernel->measure(bodyOf(result, *other.document.findFeature(only), 0).solid);
    CHECK(report.volume == Approx(8000.0).epsilon(1e-9));
    CHECK(report.boxMin.x == Approx(box0.max.x).margin(1e-7));
}

TEST_CASE("mirror: an L-shaped body lands mirrored in every axis", "[mirror][feature]") {
    for (const int axis : {0, 1, 2}) {
        RefWorld world;
        const core::FeatureId shape = lShape(world);
        const core::RecomputeResult first = world.run();
        const geo::Bounds box = world.kernel->bounds(first.bodies.at(world.bodyOf(shape)).solid);
        const double high[3] = {box.max.x, box.max.y, box.max.z};
        const double low[3] = {box.min.x, box.min.y, box.min.z};
        const double plane = high[axis] + 3.0;
        const char* names[3] = {"planeX", "planeY", "planeZ"};
        const char* normals[3] = {"normalX", "normalY", "normalZ"};
        const core::FeatureId step = addMirror(world, shape, "only",
                                               {{names[axis], std::to_string(plane)},
                                                {"normalX", axis == 0 ? "1" : "0"},
                                                {"normalY", axis == 1 ? "1" : "0"},
                                                {"normalZ", axis == 2 ? "1" : "0"}});
        (void)normals;
        const core::RecomputeResult result = world.run();
        REQUIRE(result.state(step)->status == core::FeatureStatus::ok);
        const core::BodyResult& image = bodyOf(result, *world.document.findFeature(step), 0);
        const geo::SolidReport report = world.kernel->measure(image.solid);
        CHECK(report.valid);
        CHECK(report.volume ==
              Approx(world.kernel->measure(first.bodies.at(world.bodyOf(shape)).solid).volume)
                  .epsilon(1e-9));
        const double imageLow[3] = {report.boxMin.x, report.boxMin.y, report.boxMin.z};
        const double imageHigh[3] = {report.boxMax.x, report.boxMax.y, report.boxMax.z};
        CHECK(imageLow[axis] == Approx(2 * plane - high[axis]).margin(1e-6));
        CHECK(imageHigh[axis] == Approx(2 * plane - low[axis]).margin(1e-6));
        for (int other = 0; other < 3; ++other) {
            if (other != axis) {
                CHECK(imageLow[other] == Approx(low[other]).margin(1e-6));
                CHECK(imageHigh[other] == Approx(high[other]).margin(1e-6));
            }
        }
    }
}

TEST_CASE("mirror: a plane without a direction is a problem that helps, a wrong mode too",
          "[mirror][feature]") {
    RefWorld world;
    const core::FeatureId box = world.plate("20", "20", "20");
    const core::FeatureId flat =
        addMirror(world, box, "copy", {{"normalX", "0"}, {"normalY", "0"}, {"normalZ", "0"}});
    const core::RecomputeResult result = world.run();
    REQUIRE(result.state(flat) != nullptr);
    CHECK(result.state(flat)->status == core::FeatureStatus::error);
    CHECK(result.state(flat)->issue->code == core::FeatureIssueCode::parameterOutOfRange);
    RefWorld other;
    const core::FeatureId box2 = other.plate("20", "20", "20");
    core::FeatureDraft draft = core::defaultDraft(*other.registry.find("mirror"));
    draft.inputs = {other.bodyOf(box2)};
    draft.texts["mode"] = "wobble";
    const core::FeatureId odd = *other.document.addFeature(std::move(draft));
    CHECK(other.run().state(odd)->status != core::FeatureStatus::ok);
}

TEST_CASE("mirror: changing the original changes the mirror image, names stay",
          "[mirror][feature]") {
    RefWorld world;
    const core::FeatureId box = world.plate("20", "20", "20");
    const core::FeatureId step = addMirror(world, box, "copy", {{"planeX", "30"}});
    core::RecomputeResult result = world.run();
    REQUIRE(result.state(step)->status == core::FeatureStatus::ok);
    const core::Feature& feature = *world.document.findFeature(step);
    std::vector<std::string> before = namesOf(bodyOf(result, feature, 1));
    std::sort(before.begin(), before.end());
    REQUIRE(world.document.setFeatureValue(box, "height", "10"));
    result = world.run();
    REQUIRE(result.state(step)->status == core::FeatureStatus::ok);
    CHECK(world.kernel->measure(bodyOf(result, feature, 1).solid).volume ==
          Approx(4000.0).epsilon(1e-9));
    std::vector<std::string> after = namesOf(bodyOf(result, feature, 1));
    std::sort(after.begin(), after.end());
    CHECK(after == before);
}
