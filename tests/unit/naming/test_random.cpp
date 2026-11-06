// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The random probe of persistent references with the product code (AP-2.10d): chains of random
// changes to a model with references, judged after every change against the geometry. Fixed
// seeds and an own generator (std::uniform_*_distribution gives other numbers on macOS and
// Linux), so a failure can be reproduced anywhere.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "judge.h"

using namespace fission::core;
using namespace fission::test;
namespace geo = fission::geo;

namespace {

// xorshift64*: the same numbers on every platform.
class Random {
public:
    explicit Random(std::uint64_t seed) : state_(seed * 0x9E3779B97F4A7C15ull + 1ull) {}
    std::uint64_t next() {
        state_ ^= state_ >> 12;
        state_ ^= state_ << 25;
        state_ ^= state_ >> 27;
        return state_ * 0x2545F4914F6CDD1Dull;
    }
    int between(int low, int high) { // inclusive
        return low + static_cast<int>(next() % static_cast<std::uint64_t>(high - low + 1));
    }

private:
    std::uint64_t state_;
};

struct Side {
    const char* role;
    geo::Point3d normal;
    int axis; // 0 = x, 1 = y, 2 = z
};
const std::vector<Side> kSides{{"top", {0, 0, 1}, 2},    {"bottom", {0, 0, -1}, 2},
                               {"left", {-1, 0, 0}, 0},  {"right", {1, 0, 0}, 0},
                               {"front", {0, -1, 0}, 1}, {"back", {0, 1, 0}, 1}};

struct Tracked {
    ElementKind kind;
    const Side* a;
    const Side* b; // edges only
    FeatureId mark;
};

std::string nameOf(const Tracked& t) {
    std::string first = std::string("F1/box/") + t.a->role;
    if (t.kind == ElementKind::face) {
        return first;
    }
    std::string second = std::string("F1/box/") + t.b->role;
    if (second < first) {
        std::swap(first, second);
    }
    return "edge[" + first + "|" + second + "]";
}

struct Model {
    int width = 40;
    int depth = 30;
    int height = 10;
    int x = 0;
    int turn = 0;
    bool unrelatedOff = false;
};

void setPlate(RefWorld& world, const FeatureId& plate, const Model& m) {
    world.document.setFeatureValue(plate, "width", std::to_string(m.width));
    world.document.setFeatureValue(plate, "depth", std::to_string(m.depth));
    world.document.setFeatureValue(plate, "height", std::to_string(m.height));
    world.document.setFeatureValue(plate, "x", std::to_string(m.x));
    world.document.setFeatureValue(plate, "rotZ", std::to_string(m.turn));
}

Truth truthOf(const BodyLooks& looks, const Tracked& t, const Model& m) {
    const geo::Point3d a = turnedAboutZ(t.a->normal, m.turn);
    if (t.kind == ElementKind::face) {
        return facesLooking(looks, a);
    }
    return edgesBetween(looks, a, turnedAboutZ(t.b->normal, m.turn));
}

std::size_t firstMark(const Document& document, const std::vector<Tracked>& tracked) {
    std::size_t first = document.features().size();
    for (const Tracked& t : tracked) {
        for (std::size_t i = 0; i < document.features().size(); ++i) {
            if (document.features()[i].id == t.mark) {
                first = std::min(first, i);
            }
        }
    }
    return first;
}

} // namespace

TEST_CASE("naming-product: random changes, judged against the geometry",
          "[naming-product][random]") {
    Tally tally;
    constexpr int kChains = 10;
    constexpr int kChanges = 12;

    for (int seed = 1; seed <= kChains; ++seed) {
        Random random(static_cast<std::uint64_t>(seed));
        RefWorld world;
        Model model;
        FeatureId plate = world.plate("40", "30", "10");
        const FeatureId unrelated = world.add("cylinder", {{"radius", "4"}, {"height", "6"}});

        // Two faces and two edges of the box, picked as the person would.
        const RecomputeResult first = world.run();
        std::vector<Tracked> tracked;
        for (int i = 0; i < 2; ++i) {
            const Side* side = &kSides[static_cast<std::size_t>(random.between(0, 5))];
            tracked.push_back({ElementKind::face, side, nullptr, {}});
        }
        for (int i = 0; i < 2; ++i) {
            const Side* a = &kSides[static_cast<std::size_t>(random.between(0, 5))];
            const Side* b = a;
            while (b->axis == a->axis) {
                b = &kSides[static_cast<std::size_t>(random.between(0, 5))];
            }
            tracked.push_back({ElementKind::edge, a, b, {}});
        }
        for (Tracked& t : tracked) {
            t.mark = world.mark_(plate, world.pick(first, plate, t.kind, nameOf(t)));
        }

        for (int change = 0; change < kChanges; ++change) {
            std::string what;
            switch (random.between(0, 5)) {
            case 0:
                model.width = random.between(15, 80);
                model.depth = random.between(15, 80);
                model.height = random.between(4, 40);
                setPlate(world, plate, model);
                what = "resize";
                break;
            case 1: {
                static const int turns[] = {0, 90, 180, 270, 45, 17};
                model.turn = turns[random.between(0, 5)];
                setPlate(world, plate, model);
                what = "turn to " + std::to_string(model.turn);
                break;
            }
            case 2:
                model.x = random.between(-30, 30);
                setPlate(world, plate, model);
                what = "move";
                break;
            case 3: {
                FeatureDraft draft = defaultDraft(*world.registry.find("cylinder"));
                world.document.addFeature(std::move(draft), 0);
                what = "insert a step before";
                break;
            }
            case 4:
                model.unrelatedOff = !model.unrelatedOff;
                world.document.setFeatureSuppressed(unrelated, model.unrelatedOff);
                what = "switch an unrelated step";
                break;
            default: {
                FeatureDraft draft = defaultDraft(*world.registry.find("box"));
                const FeatureId fresh = *world.document.addFeature(
                    std::move(draft), firstMark(world.document, tracked));
                setPlate(world, fresh, model);
                for (const Tracked& t : tracked) {
                    world.document.setFeatureInputs(t.mark, {world.bodyOf(fresh)});
                }
                world.document.removeFeature(plate);
                plate = fresh;
                what = "replace the box";
                break;
            }
            }

            const RecomputeResult result = world.run();
            applyHealedRefs(world.document, result); // as the program does after every run
            const BodyLooks looks = world.looksOf(result, plate);
            for (Tracked& t : tracked) {
                const Truth truth = truthOf(looks, t, model);
                const Verdict verdict = judge(world, result, t.mark, truth);
                tally.add(verdict);
                INFO("seed " << seed << ", change " << change << " (" << what << "), "
                             << nameOf(t));
                CHECK(verdict != Verdict::silentlyWrong);
                if (verdict == Verdict::helpMinus) {
                    // The person picks the right element again, so one case counts once.
                    const std::string name = [&] {
                        const auto& list = t.kind == ElementKind::face ? looks.faces : looks.edges;
                        for (const NamedLook& entry : list) {
                            if (entry.id == truth.front()) {
                                return entry.name;
                            }
                        }
                        return std::string();
                    }();
                    const auto again =
                        makeRef(looks, t.kind, truth.front(),
                                placementOf(world.document, world.registry, result.features, name));
                    REQUIRE(again.has_value());
                    REQUIRE(world.document.setFeatureRefs(t.mark, "target", {*again}));
                }
            }
        }
    }

    report("random", tally);
    CHECK(tally.total() == kChains * kChanges * 4);
    CHECK(tally.wrong == 0);
    CHECK(tally.automatic() >= 0.90);
}
