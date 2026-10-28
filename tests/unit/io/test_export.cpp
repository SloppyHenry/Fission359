// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Export to STL and 3MF (AP-1.10e): what is written describes the body that was modelled.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <numbers>
#include <string>
#include <unistd.h>
#include <vector>

#include "fission/core/feature_type.h"
#include "fission/core/recompute.h"
#include "fission/features/builtin.h"
#include "fission/geo/occt/kernel.h"
#include "fission/io/export.h"

#include <lib3mf_implicit.hpp>

using Catch::Approx;
using namespace fission::core;
using namespace fission::io;
namespace fs = std::filesystem;

namespace {

struct Folder {
    fs::path path;
    Folder() {
        static int counter = 0;
        path = fs::temp_directory_path() / ("fission-export-test-" + std::to_string(::getpid()) +
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

struct World {
    Document document{5};
    FeatureRegistry registry;
    std::unique_ptr<fission::geo::GeometryKernel> kernel = fission::geo::occt::makeKernel();
    RecomputeCache cache;

    World() { fission::features::registerBuiltinFeatures(registry); }

    FeatureId add(const char* type, std::map<std::string, std::string> values = {},
                  std::vector<BodyId> inputs = {}, std::map<std::string, std::string> words = {}) {
        FeatureDraft draft = defaultDraft(*registry.find(type));
        for (const auto& [name, expression] : values) {
            draft.params.at(name).expression = expression;
        }
        for (const auto& [name, text] : words) {
            draft.texts[name] = text;
        }
        draft.inputs = std::move(inputs);
        return *document.addFeature(std::move(draft));
    }
    BodyId bodyOf(const FeatureId& id) const { return document.findFeature(id)->outputs.at(0); }

    RecomputeResult run() {
        auto result = recompute(document, registry, *kernel, cache);
        REQUIRE(result.has_value());
        return std::move(*result);
    }

    MeshForExport meshOf(const RecomputeResult& result, const BodyId& id, const std::string& name,
                         Color color, ExportQuality quality = ExportQuality::normal) const {
        return {name, color,
                kernel->tessellate(result.bodies.at(id).solid, exportTessellation(quality))};
    }
};

/// Volume of a closed triangle mesh (divergence theorem), mm³.
struct Raw {
    std::vector<std::array<double, 3>> points;
    std::vector<std::array<std::uint32_t, 3>> triangles;

    double volume() const {
        double sum = 0.0;
        for (const auto& t : triangles) {
            const auto& a = points[t[0]];
            const auto& b = points[t[1]];
            const auto& c = points[t[2]];
            sum += a[0] * (b[1] * c[2] - b[2] * c[1]) - a[1] * (b[0] * c[2] - b[2] * c[0]) +
                   a[2] * (b[0] * c[1] - b[1] * c[0]);
        }
        return sum / 6.0;
    }
};

/// Reads our own binary STL: every triangle gets three corners of its own.
Raw readStl(const fs::path& path, std::size_t& declared) {
    std::ifstream stream(path, std::ios::binary);
    const std::string data((std::istreambuf_iterator<char>(stream)),
                           std::istreambuf_iterator<char>());
    REQUIRE(data.size() >= 84);
    std::uint32_t count = 0;
    std::memcpy(&count, data.data() + 80, 4);
    declared = count;
    REQUIRE(data.size() == 84 + static_cast<std::size_t>(count) * 50);
    Raw raw;
    for (std::uint32_t t = 0; t < count; ++t) {
        const char* at = data.data() + 84 + static_cast<std::size_t>(t) * 50 + 12; // skip normal
        std::array<std::uint32_t, 3> corners{};
        for (int k = 0; k < 3; ++k) {
            float xyz[3];
            std::memcpy(xyz, at + k * 12, 12);
            corners[static_cast<std::size_t>(k)] = static_cast<std::uint32_t>(raw.points.size());
            raw.points.push_back({xyz[0], xyz[1], xyz[2]});
        }
        raw.triangles.push_back(corners);
    }
    return raw;
}

} // namespace

TEST_CASE("the finer the quality, the more triangles", "[export]") {
    World world;
    const FeatureId pin = world.add("cylinder", {{"radius", "10"}, {"height", "20"}});
    const RecomputeResult result = world.run();
    const auto count = [&](ExportQuality quality) {
        return world.meshOf(result, world.bodyOf(pin), "x", {}, quality).mesh.triangleCount();
    };
    CHECK(count(ExportQuality::coarse) < count(ExportQuality::normal));
    CHECK(count(ExportQuality::normal) < count(ExportQuality::fine));
    CHECK(exportTessellation(ExportQuality::normal).chordTolerance == Approx(0.01));
}

TEST_CASE("STL: binary, right size, and the mesh has the volume of the body", "[export]") {
    World world;
    Folder folder;
    const FeatureId pin = world.add("cylinder", {{"radius", "10"}, {"height", "20"}});
    const FeatureId box =
        world.add("box", {{"width", "30"}, {"depth", "20"}, {"height", "10"}, {"x", "50"}});
    const RecomputeResult result = world.run();
    const MeshForExport round = world.meshOf(result, world.bodyOf(pin), "Rund", {});
    const MeshForExport square = world.meshOf(result, world.bodyOf(box), "Eckig", {});

    const fs::path file = folder.path / "two.stl";
    REQUIRE(exportStl({round, square}, file));
    std::size_t declared = 0;
    const Raw raw = readStl(file, declared);
    CHECK(declared == round.mesh.triangleCount() + square.mesh.triangleCount());

    const double exact = world.kernel->measure(result.bodies.at(world.bodyOf(pin)).solid).volume +
                         world.kernel->measure(result.bodies.at(world.bodyOf(box)).solid).volume;
    CHECK(raw.volume() == Approx(exact).epsilon(1e-3));
    CHECK(std::count_if(fs::directory_iterator(folder.path), {}, [](auto&&) { return true; }) == 1);
}

TEST_CASE("3MF: one object per body with name and colour, and the same volume", "[export]") {
    World world;
    Folder folder;
    const FeatureId pin = world.add("cylinder", {{"radius", "10"}, {"height", "20"}});
    const FeatureId box =
        world.add("box", {{"width", "30"}, {"depth", "20"}, {"height", "10"}, {"x", "50"}});
    const RecomputeResult result = world.run();
    const MeshForExport round = world.meshOf(result, world.bodyOf(pin), "Rund", {200, 10, 30});
    const MeshForExport square = world.meshOf(result, world.bodyOf(box), "Eckig", {5, 120, 250});

    const fs::path file = folder.path / "two.3mf";
    REQUIRE(export3mf({round, square}, file));

    const auto wrapper = Lib3MF::CWrapper::loadLibrary();
    auto model = wrapper->CreateModel();
    model->QueryReader("3mf")->ReadFromFile(file.string());
    CHECK(model->GetBuildItems()->Count() == 2);
    auto objects = model->GetMeshObjects();
    REQUIRE(objects->Count() == 2);

    struct Found {
        std::string name;
        double volume = 0.0;
        Lib3MF::sColor color{};
    };
    std::vector<Found> found;
    while (objects->MoveNext()) {
        auto object = objects->GetCurrentMeshObject();
        std::vector<Lib3MF::sPosition> vertices;
        std::vector<Lib3MF::sTriangle> triangles;
        object->GetVertices(vertices);
        object->GetTriangleIndices(triangles);
        Raw raw;
        for (const auto& v : vertices) {
            raw.points.push_back({v.m_Coordinates[0], v.m_Coordinates[1], v.m_Coordinates[2]});
        }
        for (const auto& t : triangles) {
            raw.triangles.push_back({t.m_Indices[0], t.m_Indices[1], t.m_Indices[2]});
        }
        Found one;
        one.name = object->GetName();
        one.volume = raw.volume();
        Lib3MF_uint32 group = 0;
        Lib3MF_uint32 property = 0;
        object->GetObjectLevelProperty(group, property);
        one.color = model->GetBaseMaterialGroupByID(group)->GetDisplayColor(property);
        found.push_back(one);
        // Merged corners: far fewer points than three per triangle.
        CHECK(vertices.size() < triangles.size() * 3 / 2);
    }
    std::sort(found.begin(), found.end(),
              [](const Found& a, const Found& b) { return a.name < b.name; });
    REQUIRE(found.size() == 2);
    CHECK(found[0].name == "Eckig");
    CHECK(found[1].name == "Rund");
    CHECK(found[0].volume == Approx(30.0 * 20.0 * 10.0).epsilon(1e-3));
    CHECK(found[1].volume ==
          Approx(world.kernel->measure(result.bodies.at(world.bodyOf(pin)).solid).volume)
              .epsilon(1e-3));
    CHECK(found[1].color.m_Red == 200);
    CHECK(found[1].color.m_Green == 10);
    CHECK(found[1].color.m_Blue == 30);
    CHECK(found[0].color.m_Blue == 250);
}

TEST_CASE("round stays round: the edge of a hole is within 0.01 mm of the circle", "[export]") {
    // Flow D, step 6: a pin cut out of a plate. Every corner of the mesh on the edge lies on the
    // circle, and between two corners the chord is no further than 0.01 mm from it.
    World world;
    Folder folder;
    const FeatureId plate =
        world.add("box", {{"width", "40"}, {"depth", "40"}, {"height", "10"}, {"x", "0"}});
    const FeatureId pin = world.add("cylinder", {{"radius", "3"}, {"height", "30"}, {"z", "-10"}});
    const FeatureId cut =
        world.add("combine", {}, {world.bodyOf(plate), world.bodyOf(pin)}, {{"mode", "subtract"}});
    const RecomputeResult result = world.run();
    const MeshForExport body = world.meshOf(result, world.bodyOf(cut), "Platte", {});

    const fs::path file = folder.path / "plate.stl";
    REQUIRE(exportStl({body}, file));
    std::size_t declared = 0;
    const Raw raw = readStl(file, declared);

    std::vector<double> angles;
    for (const auto& p : raw.points) {
        const double radius = std::hypot(p[0], p[1]);
        if (std::abs(radius - 3.0) < 1e-3 && std::abs(p[2] - 10.0) < 1e-3) { // the top edge
            angles.push_back(std::atan2(p[1], p[0]));
        }
    }
    std::sort(angles.begin(), angles.end());
    angles.erase(std::unique(angles.begin(), angles.end(),
                             [](double a, double b) { return std::abs(a - b) < 1e-6; }),
                 angles.end());
    REQUIRE(angles.size() >= 8);
    double widest = 2.0 * std::numbers::pi - (angles.back() - angles.front());
    for (std::size_t i = 1; i < angles.size(); ++i) {
        widest = std::max(widest, angles[i] - angles[i - 1]);
    }
    const double deviation = 3.0 * (1.0 - std::cos(widest / 2.0));
    CHECK(deviation <= 0.01);
}

TEST_CASE("a file that cannot be written leaves nothing behind", "[export]") {
    World world;
    Folder folder;
    const FeatureId box = world.add("box");
    const RecomputeResult result = world.run();
    const MeshForExport body = world.meshOf(result, world.bodyOf(box), "Box", {});

    CHECK_FALSE(exportStl({body}, folder.path / "missing" / "a.stl"));
    CHECK_FALSE(export3mf({body}, folder.path / "missing" / "a.3mf"));
    CHECK(std::distance(fs::directory_iterator(folder.path), {}) == 0);

    // Writing over an existing file replaces it completely.
    REQUIRE(exportStl({body}, folder.path / "a.stl"));
    REQUIRE(exportStl({body}, folder.path / "a.stl"));
    CHECK(std::distance(fs::directory_iterator(folder.path), {}) == 1);
}
