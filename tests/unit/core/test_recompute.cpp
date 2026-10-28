// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

#include "fission/core/recompute.h"
#include "fission/features/builtin.h"
#include "fission/geo/error.h"
#include "fission/geo/occt/kernel.h"

using Catch::Approx;
using namespace fission::core;
using Clock = std::chrono::steady_clock;

namespace {

// The real kernel, counting how often it was asked for a box.
class CountingKernel final : public fission::geo::GeometryKernel {
public:
    explicit CountingKernel(std::unique_ptr<fission::geo::GeometryKernel> inner)
        : inner_(std::move(inner)) {}

    fission::geo::NamedSolid makeBox(double x, double y, double z, fission::geo::Point3d corner,
                                     double cornerRadius) const override {
        ++boxes;
        if (failBoxes) {
            throw fission::geo::GeometryError(fission::geo::GeometryErrorCode::kernelFailure,
                                              "boom (raw kernel text)");
        }
        return inner_->makeBox(x, y, z, corner, cornerRadius);
    }
    fission::geo::NamedSolid makeCylinder(double r, double h,
                                          fission::geo::Point3d base) const override {
        ++cylinders;
        return inner_->makeCylinder(r, h, base);
    }
    fission::geo::NamedSolid makePrimitive(const fission::geo::PrimitiveSpec& spec,
                                           fission::geo::Point3d base) const override {
        return inner_->makePrimitive(spec, base);
    }
    fission::geo::SolidReport measure(const fission::geo::Solid& s) const override {
        return inner_->measure(s);
    }
    fission::geo::TriangleMesh
    tessellate(const fission::geo::Solid& s,
               const fission::geo::TessellationParams& p) const override {
        return inner_->tessellate(s, p);
    }

    mutable std::atomic<int> boxes{0};
    mutable std::atomic<int> cylinders{0};
    std::atomic<bool> failBoxes{false};

private:
    std::unique_ptr<fission::geo::GeometryKernel> inner_;
};

// A step that works on a body: a box of its own height with the footprint of nothing in
// particular. What matters is that it has an input, so changes travel through it.
class StackType final : public FeatureType {
public:
    std::string_view id() const override { return "stack"; }
    int version() const override { return 1; }
    std::vector<ParamSpec> parameters() const override {
        return {{"height", QuantityKind::length, "5"}};
    }
    std::optional<FeatureIssue> validate(const ResolvedParams& params) const override {
        if (!(params.at("height").value > 0.0)) {
            FeatureIssue issue;
            issue.code = FeatureIssueCode::parameterOutOfRange;
            issue.subject = "height";
            return issue;
        }
        return std::nullopt;
    }
    FeatureResult execute(const ExecuteInput& input, const ExecuteContext& context) const override {
        auto made = context.kernel.makeBox(10, 10, input.params.at("height").value, {}, 0.0);
        FeatureResult result;
        result.outputs.push_back({input.feature.outputs.at(0), made.solid, made.faces});
        return result;
    }
    Description describe(const ResolvedParams& params) const override {
        return {"stack", {params.at("height")}};
    }
    std::vector<HandleSpec> handles(const ResolvedParams&) const override { return {}; }
};

// Takes as long as it is told (in milliseconds), but looks at the stop request every millisecond.
class SlowType final : public FeatureType {
public:
    std::string_view id() const override { return "slow"; }
    int version() const override { return 1; }
    std::vector<ParamSpec> parameters() const override {
        return {{"ms", QuantityKind::plain, "10"}};
    }
    std::optional<FeatureIssue> validate(const ResolvedParams&) const override { return {}; }
    FeatureResult execute(const ExecuteInput& input, const ExecuteContext& context) const override {
        const auto until = Clock::now() + std::chrono::milliseconds(
                                              static_cast<long>(input.params.at("ms").value));
        while (Clock::now() < until) {
            if (context.cancel.stop_requested()) {
                return {};
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        auto made = context.kernel.makeBox(5, 5, 5, {}, 0.0);
        FeatureResult result;
        result.outputs.push_back({input.feature.outputs.at(0), made.solid, made.faces});
        return result;
    }
    Description describe(const ResolvedParams&) const override { return {"slow", {}}; }
    std::vector<HandleSpec> handles(const ResolvedParams&) const override { return {}; }
};

// Breaks the naming contract on purpose: no names at all.
class NamelessType final : public FeatureType {
public:
    std::string_view id() const override { return "nameless"; }
    int version() const override { return 1; }
    std::vector<ParamSpec> parameters() const override { return {}; }
    std::optional<FeatureIssue> validate(const ResolvedParams&) const override { return {}; }
    FeatureResult execute(const ExecuteInput& input, const ExecuteContext& context) const override {
        auto made = context.kernel.makeBox(5, 5, 5, {}, 0.0);
        FeatureResult result;
        result.outputs.push_back({input.feature.outputs.at(0), made.solid, {}});
        return result;
    }
    Description describe(const ResolvedParams&) const override { return {"nameless", {}}; }
    std::vector<HandleSpec> handles(const ResolvedParams&) const override { return {}; }
};

struct World {
    FeatureRegistry registry;
    CountingKernel kernel{fission::geo::occt::makeKernel()};
    RecomputeCache cache;
    Document document{11};

    World() {
        fission::features::registerBuiltinFeatures(registry);
        registry.add(std::make_unique<StackType>());
        registry.add(std::make_unique<SlowType>());
        registry.add(std::make_unique<NamelessType>());
    }

    FeatureId add(const char* type, std::map<std::string, std::string> values = {},
                  std::vector<BodyId> inputs = {}) {
        FeatureDraft draft = defaultDraft(*registry.find(type));
        for (auto& [name, text] : values) {
            draft.params[name].expression = text;
        }
        draft.inputs = std::move(inputs);
        return *document.addFeature(std::move(draft));
    }

    BodyId bodyOf(const FeatureId& id) const { return document.findFeature(id)->outputs.at(0); }

    RecomputeResult run() {
        auto result = recompute(document, registry, kernel, cache);
        REQUIRE(result.has_value());
        return std::move(*result);
    }
};

} // namespace

TEST_CASE("a recipe becomes bodies, with values worked out from named measures", "[recompute]") {
    World world;
    world.document.addParameter({"Breite", "25", QuantityKind::length, ""});
    const auto box =
        world.add("box", {{"width", "Breite * 2"}, {"depth", "1 cm"}, {"height", "5"}});
    const auto cylinder = world.add("cylinder", {{"radius", "4"}});

    const RecomputeResult result = world.run();
    REQUIRE(result.features.size() == 2);
    CHECK(result.features[0].status == FeatureStatus::ok);
    CHECK(result.executed == 2);
    CHECK(result.remembered == 0);

    const FeatureState* state = result.state(box);
    REQUIRE(state != nullptr);
    CHECK(state->params.at("width").value == Approx(50.0));
    CHECK(state->params.at("depth").value == Approx(10.0));
    CHECK(state->description->key == "box");
    CHECK(state->computed);

    const BodyResult& body = result.bodies.at(world.bodyOf(box));
    CHECK(body.producer == box);
    CHECK(world.kernel.measure(body.solid).volume == Approx(50.0 * 10.0 * 5.0));
    CHECK(body.faces.size() == 6);
    // Names carry the feature number and type, so a reference survives inserting steps before.
    const auto top = std::find_if(body.faces.begin(), body.faces.end(),
                                  [](const BodyFace& f) { return f.name == "F1/box/top"; });
    CHECK(top != body.faces.end());
    CHECK(result.bodies.contains(world.bodyOf(cylinder)));
}

TEST_CASE("acceptance: a change recomputes only what it affects", "[recompute][acceptance]") {
    World world;
    world.document.addParameter({"Breite", "10", QuantityKind::length, ""});
    const auto a = world.add("box", {{"width", "Breite"}});                  // depends on Breite
    const auto b = world.add("stack", {{"height", "5"}}, {world.bodyOf(a)}); // works on a
    const auto c = world.add("stack", {{"height", "6"}}, {world.bodyOf(b)}); // works on b
    const auto d = world.add("box", {{"width", "30"}});                      // independent
    const auto e = world.add("cylinder");                                    // independent

    RecomputeResult result = world.run();
    CHECK(result.executed == 5);
    CHECK(world.kernel.boxes == 4);

    SECTION("nothing changed: nothing is calculated") {
        result = world.run();
        CHECK(result.executed == 0);
        CHECK(result.remembered == 5);
        CHECK(world.kernel.boxes == 4);
    }

    SECTION("a named measure that one step uses: that step and what works on it") {
        world.document.setParameterExpression("Breite", "12");
        result = world.run();
        CHECK(result.executed == 3); // a, b, c
        CHECK(result.remembered == 2);
        CHECK(result.state(a)->computed);
        CHECK(result.state(b)->computed);
        CHECK(result.state(c)->computed);
        CHECK_FALSE(result.state(d)->computed);
        CHECK_FALSE(result.state(e)->computed);
    }

    SECTION("a value in the middle: it and what works on it, not what came before") {
        world.document.setFeatureValue(b, "height", "7");
        result = world.run();
        CHECK(result.executed == 2); // b, c
        CHECK_FALSE(result.state(a)->computed);
        CHECK(result.state(b)->computed);
        CHECK(result.state(c)->computed);
    }

    SECTION("the last step alone") {
        world.document.setFeatureValue(c, "height", "9");
        result = world.run();
        CHECK(result.executed == 1);
        CHECK(world.kernel.boxes == 5);
    }

    SECTION("an independent step") {
        world.document.setFeatureValue(e, "radius", "3");
        result = world.run();
        CHECK(result.executed == 1);
        CHECK(world.kernel.cylinders == 2);
        CHECK(world.kernel.boxes == 4);
    }

    SECTION("a change and its undo: the old result comes from memory") {
        world.document.setFeatureValue(a, "width", "99");
        world.run();
        const int boxesAfterChange = world.kernel.boxes;
        world.document.setFeatureValue(a, "width", "Breite");
        result = world.run();
        CHECK(result.executed == 0);
        CHECK(world.kernel.boxes == boxesAfterChange);
        CHECK(result.remembered == 5);
    }

    SECTION("two steps that are equal share their result") {
        world.add("box", {{"width", "30"}}); // same as d
        result = world.run();
        CHECK(result.executed == 0);
    }
}

TEST_CASE("mistakes stay local", "[recompute]") {
    World world;
    const auto good = world.add("box", {{"width", "10"}});
    const auto typo = world.add("box", {{"width", "Breitee"}});
    const auto after = world.add("cylinder");

    const RecomputeResult result = world.run();
    CHECK(result.state(good)->status == FeatureStatus::ok);
    CHECK(result.state(after)->status == FeatureStatus::ok);

    const FeatureState* failed = result.state(typo);
    REQUIRE(failed != nullptr);
    CHECK(failed->status == FeatureStatus::error);
    REQUIRE(failed->issue.has_value());
    CHECK(failed->issue->code == FeatureIssueCode::parameterInvalid);
    CHECK(failed->issue->subject == "width");
    REQUIRE(failed->issue->expression.has_value());
    CHECK(failed->issue->expression->code == ExpressionErrorCode::unknownName);
    CHECK(failed->issue->expression->subject == "Breitee");
    CHECK_FALSE(result.bodies.contains(world.bodyOf(typo)));
}

TEST_CASE("a value that is understood but unusable is out of range", "[recompute]") {
    World world;
    const auto box = world.add("box", {{"height", "0"}});
    const RecomputeResult result = world.run();
    const FeatureState* state = result.state(box);
    CHECK(state->status == FeatureStatus::error);
    CHECK(state->issue->code == FeatureIssueCode::parameterOutOfRange);
    CHECK(state->issue->subject == "height");
    CHECK(world.kernel.boxes == 0); // stopped before the kernel
}

TEST_CASE("a measure with an error is named as the cause", "[recompute]") {
    World world;
    world.document.addParameter({"a", "b", QuantityKind::length, ""});
    world.document.addParameter({"b", "a", QuantityKind::length, ""});
    const auto box = world.add("box", {{"width", "a + 1"}});
    const RecomputeResult result = world.run();
    const FeatureState* state = result.state(box);
    CHECK(state->status == FeatureStatus::error);
    REQUIRE(state->issue->expression.has_value());
    CHECK(state->issue->expression->code == ExpressionErrorCode::brokenDependency);
    CHECK(state->issue->expression->subject == "a");
    CHECK(result.measures.find("a")->error->code == ExpressionErrorCode::cycle);
}

TEST_CASE("a failing step keeps its last valid result; what depends on it gets a warning",
          "[recompute]") {
    World world;
    const auto base = world.add("box", {{"width", "10"}});
    const auto top = world.add("stack", {{"height", "4"}}, {world.bodyOf(base)});
    const RecomputeResult first = world.run();
    const auto goodVolume = world.kernel.measure(first.bodies.at(world.bodyOf(base)).solid).volume;

    world.document.setFeatureValue(base, "width", "-5");
    const RecomputeResult broken = world.run();

    CHECK(broken.state(base)->status == FeatureStatus::error);
    CHECK(broken.state(base)->stale);
    const BodyResult& shown = broken.bodies.at(world.bodyOf(base));
    CHECK(shown.stale);
    CHECK(world.kernel.measure(shown.solid).volume == Approx(goodVolume));

    CHECK(broken.state(top)->status == FeatureStatus::warning);
    CHECK(broken.state(top)->issue->code == FeatureIssueCode::inputFailed);
    CHECK(broken.bodies.contains(world.bodyOf(top)));

    // Fixed: back to normal, without a warning.
    world.document.setFeatureValue(base, "width", "10");
    const RecomputeResult fixed = world.run();
    CHECK(fixed.state(base)->status == FeatureStatus::ok);
    CHECK(fixed.state(top)->status == FeatureStatus::ok);
    CHECK_FALSE(fixed.bodies.at(world.bodyOf(base)).stale);
}

TEST_CASE("steps that cannot run say why", "[recompute]") {
    World world;
    const auto first = world.add("box");
    const auto user = world.add("stack", {}, {world.bodyOf(first)});
    FeatureDraft unknown;
    unknown.type = "hologram";
    const auto mystery = *world.document.addFeature(unknown);

    SECTION("unknown type") {
        const auto result = world.run();
        CHECK(result.state(mystery)->issue->code == FeatureIssueCode::unknownType);
        CHECK(result.state(mystery)->issue->subject == "hologram");
    }
    SECTION("switched off: its bodies are gone, its users say so") {
        world.document.setFeatureSuppressed(first, true);
        const auto result = world.run();
        CHECK(result.state(first)->status == FeatureStatus::suppressed);
        CHECK(result.state(user)->status == FeatureStatus::error);
        CHECK(result.state(user)->issue->code == FeatureIssueCode::inputFailed);
    }
    SECTION("removed: the user finds its input missing") {
        world.document.removeFeature(first);
        const auto result = world.run();
        CHECK(result.state(user)->issue->code == FeatureIssueCode::inputMissing);
    }
}

TEST_CASE("the kernel failing is a clean error, its words stay in the log", "[recompute]") {
    World world;
    const auto box = world.add("box");
    world.kernel.failBoxes = true;
    const auto result = world.run();
    const FeatureState* state = result.state(box);
    CHECK(state->status == FeatureStatus::error);
    CHECK(state->issue->code == FeatureIssueCode::kernelFailed);
    CHECK(state->issue->detail == "boom (raw kernel text)");
    CHECK(state->issue->subject.empty());

    // A failure is not remembered: the next try asks the kernel again.
    world.kernel.failBoxes = false;
    CHECK(world.run().state(box)->status == FeatureStatus::ok);
}

TEST_CASE("a step that does not name its faces is refused", "[recompute]") {
    World world;
    const auto step = world.add("nameless");
    const auto result = world.run();
    CHECK(result.state(step)->status == FeatureStatus::error);
    CHECK(result.state(step)->issue->code == FeatureIssueCode::namingContractViolated);
    CHECK(world.cache.size() == 0); // not remembered either
}

TEST_CASE("memory stays within its capacity", "[recompute]") {
    World world;
    world.cache = RecomputeCache(4);
    const auto box = world.add("box");
    for (int i = 1; i <= 20; ++i) {
        world.document.setFeatureValue(box, "width", std::to_string(i));
        world.run();
    }
    CHECK(world.cache.size() <= 4);
}

TEST_CASE("a cancelled calculation gives nothing", "[recompute]") {
    World world;
    world.add("box");
    CancelSource stop;
    stop.request_stop();
    CHECK_FALSE(recompute(world.document, world.registry, world.kernel, world.cache, stop.token())
                    .has_value());
}

// ---------------------------------------------------------------------------------------------
// The worker

namespace {

struct Recorder {
    std::mutex mutex;
    std::vector<RecomputeEvent> events;
    std::vector<Clock::time_point> times;

    RecomputeObserver observer() {
        return [this](const RecomputeEvent& event) {
            const std::lock_guard lock(mutex);
            events.push_back(event);
            times.push_back(Clock::now());
        };
    }
    std::size_t count(RecomputeEventType type) {
        const std::lock_guard lock(mutex);
        return static_cast<std::size_t>(std::count_if(
            events.begin(), events.end(), [&](const RecomputeEvent& e) { return e.type == type; }));
    }
};

} // namespace

TEST_CASE("the worker calculates in the background and reports", "[engine]") {
    World world;
    const auto box = world.add("box");
    world.add("cylinder");
    Recorder recorder;
    RecomputeEngine engine(world.registry, world.kernel, recorder.observer());

    CHECK(engine.latest() == nullptr);
    engine.request(world.document);
    engine.waitIdle();

    const auto latest = engine.latest();
    REQUIRE(latest != nullptr);
    CHECK(latest->bodies.size() == 2);
    CHECK(latest->revision == world.document.revision());
    CHECK(recorder.count(RecomputeEventType::started) == 1);
    CHECK(recorder.count(RecomputeEventType::finished) == 1);
    CHECK(recorder.count(RecomputeEventType::featureStatusChanged) == 2); // both are new

    // Same document again: results are remembered, no status changed, so no status events.
    engine.request(world.document);
    engine.waitIdle();
    CHECK(engine.latest()->executed == 0);
    CHECK(recorder.count(RecomputeEventType::featureStatusChanged) == 2);

    // A step that starts to fail: one status event.
    world.document.setFeatureValue(box, "width", "0");
    engine.request(world.document);
    engine.waitIdle();
    CHECK(recorder.count(RecomputeEventType::featureStatusChanged) == 3);
    CHECK(engine.latest()->state(box)->status == FeatureStatus::error);
}

TEST_CASE("acceptance: a new change stops a running calculation within 50 ms",
          "[engine][acceptance]") {
    World world;
    const auto slow = world.add("slow", {{"ms", "5000"}});
    Recorder recorder;
    RecomputeEngine engine(world.registry, world.kernel, recorder.observer());

    engine.request(world.document);
    // Wait until the long calculation is really running.
    while (recorder.count(RecomputeEventType::started) == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    world.document.setFeatureValue(slow, "ms", "5");
    const auto asked = Clock::now();
    engine.request(world.document);

    // The old calculation reports "cancelled" ...
    while (recorder.count(RecomputeEventType::cancelled) == 0) {
        REQUIRE(Clock::now() - asked < std::chrono::seconds(2));
        std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
    std::chrono::steady_clock::time_point cancelledAt;
    {
        const std::lock_guard lock(recorder.mutex);
        for (std::size_t i = 0; i < recorder.events.size(); ++i) {
            if (recorder.events[i].type == RecomputeEventType::cancelled) {
                cancelledAt = recorder.times[i];
                break;
            }
        }
    }
    const auto millis =
        std::chrono::duration_cast<std::chrono::milliseconds>(cancelledAt - asked).count();
    INFO("stopped after " << millis << " ms");
    CHECK(millis <= 50);

    // ... and the new one finishes with the new value.
    engine.waitIdle();
    const auto latest = engine.latest();
    REQUIRE(latest != nullptr);
    CHECK(latest->revision == world.document.revision());
    CHECK(latest->state(slow)->status == FeatureStatus::ok);
}

TEST_CASE("only the newest of several quick requests is calculated", "[engine]") {
    World world;
    const auto box = world.add("box");
    Recorder recorder;
    RecomputeEngine engine(world.registry, world.kernel, recorder.observer());

    for (int i = 1; i <= 30; ++i) {
        world.document.setFeatureValue(box, "width", std::to_string(10 + i));
        engine.request(world.document);
    }
    engine.waitIdle();
    const auto latest = engine.latest();
    REQUIRE(latest != nullptr);
    CHECK(latest->revision == world.document.revision());
    CHECK(latest->state(box)->params.at("width").value == Approx(40.0));
}

TEST_CASE("several documents calculate side by side", "[engine]") {
    World left;
    World right;
    const auto a = left.add("box", {{"width", "11"}});
    const auto b = right.add("box", {{"width", "22"}});
    RecomputeEngine first(left.registry, left.kernel);
    RecomputeEngine second(right.registry, right.kernel);
    first.request(left.document);
    second.request(right.document);
    first.waitIdle();
    second.waitIdle();
    CHECK(first.latest()->state(a)->params.at("width").value == Approx(11.0));
    CHECK(second.latest()->state(b)->params.at("width").value == Approx(22.0));
}

TEST_CASE("closing an engine during a long calculation does not hang", "[engine]") {
    World world;
    world.add("slow", {{"ms", "10000"}});
    const auto begun = Clock::now();
    {
        Recorder recorder;
        RecomputeEngine engine(world.registry, world.kernel, recorder.observer());
        engine.request(world.document);
        while (recorder.count(RecomputeEventType::started) == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    CHECK(Clock::now() - begun < std::chrono::seconds(3));
}

TEST_CASE("the time marker: steps behind it are not calculated, new steps go in at it",
          "[recompute][marker]") {
    World world;
    const auto first = world.add("box", {{"width", "10"}});
    const auto second = world.add("box", {{"width", "20"}});
    const auto third = world.add("box", {{"width", "30"}});
    const auto fourth = world.add("cylinder", {{"radius", "4"}});
    REQUIRE_FALSE(world.document.rollback().has_value());
    const Document everything = world.document;

    REQUIRE(world.document.setRollback(2));
    CHECK_FALSE(world.document.setRollback(2)); // nothing changes
    const RecomputeResult before = world.run();
    CHECK(before.features[0].status == FeatureStatus::ok);
    CHECK(before.features[1].status == FeatureStatus::ok);
    CHECK(before.features[2].status == FeatureStatus::suppressed);
    CHECK(before.features[3].status == FeatureStatus::suppressed);
    CHECK(before.bodies.contains(world.bodyOf(first)));
    CHECK(before.bodies.contains(world.bodyOf(second)));
    CHECK_FALSE(before.bodies.contains(world.bodyOf(third)));
    CHECK_FALSE(before.bodies.contains(world.bodyOf(fourth)));

    // A new step goes in at the marker and the marker stays behind it.
    FeatureDraft draft = defaultDraft(*world.registry.find("sphere"));
    const auto sphere = *world.document.addFeature(std::move(draft));
    CHECK(*world.document.indexOf(sphere) == 2);
    CHECK(world.document.rollback() == 3);
    const RecomputeResult after = world.run();
    CHECK(after.bodies.contains(world.bodyOf(sphere)));
    CHECK_FALSE(after.bodies.contains(world.bodyOf(third)));

    // Taking out a step before the marker keeps it in front of the same steps.
    REQUIRE(world.document.removeFeature(first));
    CHECK(world.document.rollback() == 2);
    // A marker at the end is no marker.
    REQUIRE(world.document.setRollback(99));
    CHECK_FALSE(world.document.rollback().has_value());
    CHECK_FALSE(world.document.sameContentAs(everything)); // steps differ now
    CHECK(everything.sameContentAs(everything));
}
