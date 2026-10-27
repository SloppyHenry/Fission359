// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The .fission container (AP-1.10b): save, open, crash during saving, damaged files.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <unistd.h>

#include "fission/core/feature_type.h"
#include "fission/core/recompute.h"
#include "fission/features/builtin.h"
#include "fission/geo/occt/kernel.h"
#include "fission/io/fission_file.h"

using Catch::Approx;
using namespace fission::core;
using namespace fission::io;
namespace fs = std::filesystem;

namespace {

/// A folder of its own for one test, removed afterwards.
struct TempFolder {
    fs::path path;
    TempFolder() {
        static int counter = 0;
        path = fs::temp_directory_path() / ("fission-file-test-" + std::to_string(::getpid()) +
                                            "-" + std::to_string(++counter));
        fs::create_directories(path);
    }
    ~TempFolder() {
        std::error_code ignored;
        fs::remove_all(path, ignored);
    }
    TempFolder(const TempFolder&) = delete;
    TempFolder& operator=(const TempFolder&) = delete;

    std::size_t fileCount() const {
        return static_cast<std::size_t>(std::distance(fs::directory_iterator(path), {}));
    }
};

std::string slurp(const fs::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

void spit(const fs::path& path, const std::string& data) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << data;
}

struct World {
    FeatureRegistry registry;
    std::unique_ptr<fission::geo::GeometryKernel> kernel = fission::geo::occt::makeKernel();

    World() { fission::features::registerBuiltinFeatures(registry); }

    Document model() {
        Document document(9);
        document.title = "Testst\xC3\xBC"
                         "ck";
        document.addParameter({"Breite", "30", QuantityKind::length, ""});
        FeatureDraft box = defaultDraft(*registry.find("box"));
        box.params.at("width").expression = "Breite";
        document.addFeature(std::move(box));
        FeatureDraft cylinder = defaultDraft(*registry.find("cylinder"));
        document.addFeature(std::move(cylinder));
        document.setBodyColor(document.bodies().at(0).id, {200, 30, 40});
        return document;
    }

    std::map<BodyId, double> volumes(const Document& document) {
        RecomputeCache cache;
        auto result = recompute(document, registry, *kernel, cache);
        REQUIRE(result.has_value());
        std::map<BodyId, double> volumes;
        for (const auto& [id, body] : result->bodies) {
            volumes[id] = kernel->measure(body.solid).volume;
        }
        return volumes;
    }
};

SaveExtras extras() {
    return {{0x89, 'P', 'N', 'G', 1, 2, 3}, "0.1.0", ""};
}

} // namespace

TEST_CASE("saving and opening gives the same document", "[fission-file]") {
    World world;
    TempFolder folder;
    const Document original = world.model();
    const fs::path file = folder.path / "model.fission";

    REQUIRE(save(original, file, extras()).ok);
    const OpenResult opened = open(file);

    REQUIRE(opened.document);
    CHECK(opened.problems.empty());
    CHECK(opened.document->sameContentAs(original));
    CHECK(opened.info.formatVersion == currentFormatVersion);
    CHECK(opened.info.appVersion == "0.1.0");
    CHECK_FALSE(opened.info.created.empty());
    CHECK(opened.previewPng == extras().previewPng);
    CHECK(folder.fileCount() == 1); // no temporary file left
}

TEST_CASE("the geometry after opening has the same volumes", "[fission-file]") {
    World world;
    TempFolder folder;
    const Document original = world.model();
    const fs::path file = folder.path / "model.fission";
    REQUIRE(save(original, file, extras()).ok);

    const auto before = world.volumes(original);
    const auto after = world.volumes(*open(file).document);
    REQUIRE(before.size() == 2);
    REQUIRE(after.size() == before.size());
    for (const auto& [id, volume] : before) {
        CHECK(after.at(id) == Approx(volume).epsilon(1e-9));
    }
}

TEST_CASE("saving over a file keeps the creation time that was handed in", "[fission-file]") {
    World world;
    TempFolder folder;
    const fs::path file = folder.path / "model.fission";
    SaveExtras first = extras();
    first.created = "2020-01-02T03:04:05Z";
    REQUIRE(save(world.model(), file, first).ok);
    CHECK(open(file).info.created == "2020-01-02T03:04:05Z");
}

TEST_CASE("a crash while saving leaves the old file untouched", "[fission-file]") {
    World world;
    TempFolder folder;
    const Document old = world.model();
    const fs::path file = folder.path / "model.fission";
    REQUIRE(save(old, file, extras()).ok);
    const std::string before = slurp(file);

    Document changed = old;
    changed.title = "Neu";
    changed.addParameter({"Mehr", "1", QuantityKind::plain, ""});

    for (const std::size_t cut :
         {std::size_t{0}, std::size_t{1}, before.size() / 2, before.size() - 1}) {
        SaveOptions crash;
        crash.failAfterBytes = cut;
        const SaveResult result = save(changed, file, extras(), crash);
        INFO("cut after " << cut << " bytes");
        CHECK_FALSE(result.ok);
        CHECK(slurp(file) == before);   // byte for byte the old file
        CHECK(folder.fileCount() == 1); // and no temporary file
        const OpenResult opened = open(file);
        REQUIRE(opened.document);
        CHECK(opened.document->sameContentAs(old));
    }
}

TEST_CASE("a crash on a first save leaves nothing behind", "[fission-file]") {
    World world;
    TempFolder folder;
    SaveOptions crash;
    crash.failAfterBytes = 10;
    CHECK_FALSE(save(world.model(), folder.path / "new.fission", extras(), crash).ok);
    CHECK(folder.fileCount() == 0);
}

TEST_CASE("saving into a folder that is not there fails cleanly", "[fission-file]") {
    World world;
    TempFolder folder;
    const SaveResult result = save(world.model(), folder.path / "nope" / "a.fission", extras());
    CHECK_FALSE(result.ok);
    REQUIRE(result.problem);
    CHECK(result.problem->code == FileProblemCode::cannotWrite);
}

TEST_CASE("files that are not .fission files are refused", "[fission-file]") {
    TempFolder folder;
    const fs::path file = folder.path / "x.fission";

    CHECK(open(folder.path / "missing.fission").problems.at(0).code == FileProblemCode::cannotRead);
    for (const std::string& content : {std::string(), std::string("hello"),
                                       std::string("PK\x03\x04 broken"), std::string(1000, 'x')}) {
        spit(file, content);
        const OpenResult opened = open(file);
        CHECK_FALSE(opened.document);
        CHECK(opened.problems.at(0).code == FileProblemCode::notAFissionFile);
    }
}

TEST_CASE("cut-off files are refused, never crash", "[fission-file]") {
    World world;
    TempFolder folder;
    const fs::path file = folder.path / "model.fission";
    REQUIRE(save(world.model(), file, extras()).ok);
    const std::string whole = slurp(file);
    for (std::size_t length = 0; length < whole.size(); length += 13) {
        spit(file, whole.substr(0, length));
        CHECK_FALSE(open(file).document);
    }
}

TEST_CASE("a wrong checksum is found, with an offer to open anyway", "[fission-file]") {
    World world;
    TempFolder folder;
    const fs::path file = folder.path / "model.fission";
    const Document original = world.model();
    SaveOptions damaged;
    damaged.wrongChecksum = true;
    REQUIRE(save(original, file, extras(), damaged).ok);

    const OpenResult refused = open(file);
    CHECK_FALSE(refused.document);
    REQUIRE(refused.problems.size() == 1);
    CHECK(refused.problems[0].code == FileProblemCode::damaged);
    CHECK(refused.problems[0].readableAnyway);

    const OpenResult forced = open(file, {true});
    REQUIRE(forced.document);
    CHECK(forced.document->sameContentAs(original));
    CHECK(forced.problems.at(0).code == FileProblemCode::damaged);
}

TEST_CASE("a flipped byte anywhere never opens a wrong document", "[fission-file]") {
    World world;
    TempFolder folder;
    const fs::path file = folder.path / "model.fission";
    const Document original = world.model();
    REQUIRE(save(original, file, extras()).ok);
    const std::string whole = slurp(file);
    for (std::size_t at = 0; at < whole.size(); at += 7) {
        std::string copy = whole;
        copy[at] ^= 0x10;
        spit(file, copy);
        const OpenResult opened = open(file);
        // Either refused, or (a byte the ZIP does not use) exactly the original.
        if (opened.document) {
            CHECK(opened.document->sameContentAs(original));
        }
    }
}

// --- old files (tests/fixtures/files/v1) ---------------------------------------------------
// Written once by the hidden test below and NEVER changed again: every later format must still
// open them. If a change breaks these tests, write a migration, do not touch the files.

namespace {

fs::path fixtureFolder(const char* version) {
    return fs::path(FISSION_TEST_FIXTURES_DIR) / "files" / version;
}

/// One document per feature type: the type alone, with what it needs to work on.
Document fixtureDocument(World& world, const std::string& type) {
    Document document(1000);
    document.title = "Fixture " + type;
    document.addParameter({"Breite", "25,5 mm", QuantityKind::length, "ein Ma\xC3\x9F"});
    std::vector<BodyId> bodies;
    const auto add = [&](const std::string& id, std::vector<BodyId> inputs) {
        FeatureDraft draft = defaultDraft(*world.registry.find(id));
        draft.inputs = std::move(inputs);
        const auto made = document.addFeature(std::move(draft));
        REQUIRE(made);
        return document.findFeature(*made)->outputs;
    };
    if (type == "combine" || type == "split" || type == "place") {
        bodies = add("box", {});
        const auto second = add("cylinder", {});
        bodies.insert(bodies.end(), second.begin(), second.end());
        add(type, type == "combine" ? bodies : std::vector<BodyId>{bodies.front()});
    } else {
        add(type, {});
    }
    document.setBodyColor(document.bodies().front().id, {12, 200, 99});
    return document;
}

} // namespace

TEST_CASE("write the version 1 fixture files", "[.generate-fixtures]") {
    World world;
    fs::create_directories(fixtureFolder("v1"));
    for (const std::string& type : world.registry.ids()) {
        SaveExtras fixed{{}, "0.1.0", "2026-10-26T12:00:00Z"};
        REQUIRE(save(fixtureDocument(world, type), fixtureFolder("v1") / (type + ".fission"), fixed)
                    .ok);
    }
}

TEST_CASE("every version 1 fixture file still opens and calculates", "[fission-file]") {
    World world;
    std::size_t count = 0;
    for (const auto& entry : fs::directory_iterator(fixtureFolder("v1"))) {
        if (entry.path().extension() != ".fission") {
            continue;
        }
        ++count;
        INFO(entry.path().filename().string());
        const OpenResult opened = open(entry.path());
        REQUIRE(opened.document);
        CHECK(opened.problems.empty());
        CHECK(opened.documentProblems.empty());
        CHECK(opened.info.formatVersion == 1);

        const std::string type = entry.path().stem().string();
        CHECK(opened.document->title == "Fixture " + type);
        CHECK(opened.document->features().back().type == type);
        CHECK(opened.document->findParameter("Breite")->expression == "25,5 mm");
        CHECK(opened.document->bodies().front().color == Color{12, 200, 99});

        RecomputeCache cache;
        const auto result = recompute(*opened.document, world.registry, *world.kernel, cache);
        REQUIRE(result.has_value());
        if (type != "text") { // needs a font that the test machine may not have
            CHECK_FALSE(result->bodies.empty());
        }
    }
    CHECK(count == world.registry.ids().size());
}
