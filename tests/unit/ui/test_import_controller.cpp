// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Import and the new export formats from the interface (AP-2.13): background work, results as
// codes, one step of the history, nothing changed when a file cannot be used.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QTemporaryDir>
#include <QUrl>
#include <QVariantMap>

#include <memory>

#include "fission/geo/occt/kernel.h"
#include "fission/ui/document_model.h"
#include "fission/ui/export_controller.h"
#include "fission/ui/import_controller.h"

using Catch::Approx;
using fission::ui::DocumentModel;
using fission::ui::ExportController;
using fission::ui::ImportController;

namespace {

QCoreApplication& application() {
    static int argc = 1;
    static char name[] = "fission_unit_tests";
    static char* argv[] = {name, nullptr};
    static QCoreApplication app(argc, argv);
    return app;
}

QString fixture(const char* name) {
    return QStringLiteral(FISSION_TEST_FIXTURES_DIR "/step/") + QString::fromUtf8(name);
}

struct Session {
    std::shared_ptr<const fission::geo::GeometryKernel> kernel = fission::geo::occt::makeKernel();
    DocumentModel model{kernel};
    ImportController importer{&model, kernel};
    ExportController exporter{&model, kernel};
    QString code = QStringLiteral("none");
    QString fileName;
    int bodies = -1;
    QVariantMap notes;
    QString exportCode = QStringLiteral("none");

    Session() {
        QObject::connect(&importer, &ImportController::finished, &importer,
                         [this](const QString& result, const QString& name, int count,
                                const QVariantMap& details) {
                             code = result;
                             fileName = name;
                             bodies = count;
                             notes = details;
                         });
        QObject::connect(
            &exporter, &ExportController::finished, &exporter,
            [this](const QString& result, const QString&, int) { exportCode = result; });
    }

    // Waits until the answer in `what` is there (it starts as "none").
    void waitFor(const QString& what) {
        QElapsedTimer clock;
        clock.start();
        while (what == QLatin1String("none") && clock.elapsed() < 30000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        }
    }
    bool import(const char* file) {
        code = QStringLiteral("none");
        const bool started = importer.importFrom(QUrl::fromLocalFile(fixture(file)));
        if (started) {
            waitFor(code);
            model.waitForCalculation();
        }
        return started;
    }
    int steps() const { return static_cast<int>(model.document().features().size()); }
};

} // namespace

TEST_CASE("import: a STEP file becomes a body with its name and colour", "[import-ui]") {
    application();
    Session session;
    REQUIRE(session.import("block-with-hole.step"));
    CHECK(session.code.isEmpty());
    CHECK(session.bodies == 1);
    CHECK(session.fileName == QStringLiteral("block-with-hole.step"));
    CHECK(session.steps() == 1);
    CHECK_FALSE(session.notes.value(QStringLiteral("assembly")).toBool());
    CHECK_FALSE(session.notes.value(QStringLiteral("unitAssumed")).toBool());
    CHECK(session.notes.value(QStringLiteral("repaired")).toInt() == 0);
    REQUIRE(session.model.document().bodies().size() == 1);
    CHECK(session.model.document().bodies().front().name == "Block with hole");
    CHECK(session.model.document().bodies().front().color.red >= 203);
    CHECK_FALSE(session.importer.importing());
}

TEST_CASE("import: an assembly comes in flat and says so", "[import-ui]") {
    application();
    Session session;
    REQUIRE(session.import("assembly-two-parts.step"));
    CHECK(session.code.isEmpty());
    CHECK(session.bodies == 2);
    CHECK(session.steps() == 2);
    CHECK(session.notes.value(QStringLiteral("assembly")).toBool());
}

TEST_CASE("import: one step of the history for the whole file", "[import-ui]") {
    application();
    Session session;
    REQUIRE(session.import("assembly-two-parts.step"));
    REQUIRE(session.model.canUndo());
    REQUIRE(session.model.undo());
    CHECK(session.steps() == 0);
    CHECK(session.model.document().assets().empty());
    CHECK_FALSE(session.model.canUndo());
    REQUIRE(session.model.redo());
    CHECK(session.steps() == 2);
    CHECK(session.model.document().assets().size() == 1);
}

TEST_CASE("import: files that cannot be used leave the project as it was", "[import-ui]") {
    application();
    QTemporaryDir folder;
    Session session;

    SECTION("a kind of file that is not known") {
        CHECK_FALSE(session.importer.importFrom(QUrl::fromLocalFile(folder.filePath("a.stl"))));
        CHECK(session.code == QStringLiteral("unsupported"));
    }
    SECTION("a file that is not there") {
        REQUIRE(session.importer.importFrom(QUrl::fromLocalFile(folder.filePath("gone.step"))));
        session.waitFor(session.code);
        CHECK(session.code == QStringLiteral("cannotRead"));
    }
    SECTION("a file with the right ending and the wrong content") {
        QFile broken(folder.filePath("broken.step"));
        REQUIRE(broken.open(QIODevice::WriteOnly));
        broken.write("this is not a STEP file");
        broken.close();
        REQUIRE(session.importer.importFrom(QUrl::fromLocalFile(broken.fileName())));
        session.waitFor(session.code);
        CHECK(session.code == QStringLiteral("unreadable"));
    }
    SECTION("a file without a solid body") {
        REQUIRE(session.import("sheet-only.step"));
        CHECK(session.code == QStringLiteral("empty"));
    }
    CHECK(session.steps() == 0);
    CHECK(session.model.document().assets().empty());
    CHECK_FALSE(session.model.canUndo());
    CHECK_FALSE(session.importer.importing());
}

TEST_CASE("import: an IGES file works, and the ending may be wrong", "[import-ui]") {
    application();
    Session session;
    REQUIRE(session.import("block-with-hole.iges"));
    CHECK(session.code.isEmpty());
    CHECK(session.bodies == 1);
    CHECK(session.importer.canImport(QUrl::fromLocalFile("/x/part.IGS")));
    CHECK(session.importer.canImport(QUrl::fromLocalFile("/x/part.STP")));
    CHECK_FALSE(session.importer.canImport(QUrl::fromLocalFile("/x/part.stl")));
}

TEST_CASE("import: 'it was inches' makes the whole file larger, in one step", "[import-ui]") {
    application();
    Session session;
    REQUIRE(session.import("block-with-hole.step"));
    const QString asset = session.notes.value(QStringLiteral("asset")).toString();
    REQUIRE_FALSE(asset.isEmpty());
    const int before = session.model.historyPosition();
    REQUIRE(session.importer.setInches(asset));
    session.model.waitForCalculation();
    CHECK(session.model.historyPosition() == before + 1);
    CHECK(session.model.document().features().front().params.at("unitFactor").expression ==
          "25.400000");
    CHECK_FALSE(session.importer.setInches(QStringLiteral("no-such-file.step")));
    REQUIRE(session.model.undo());
    CHECK(session.model.document().features().front().params.at("unitFactor").expression == "1");
}

TEST_CASE("export: OBJ and STEP files are written", "[export-ui]") {
    application();
    QTemporaryDir folder;
    Session session;
    session.model.addDemoBox();
    session.model.waitForCalculation();
    for (const char* format : {"obj", "step"}) {
        session.exportCode = QStringLiteral("none");
        const QString file = folder.filePath(QStringLiteral("out"));
        REQUIRE(session.exporter.start(QString::fromLatin1(format), QStringLiteral("normal"), {},
                                       QUrl::fromLocalFile(file), {}));
        session.waitFor(session.exportCode);
        CHECK(session.exportCode.isEmpty());
        CHECK(QFile::exists(file + QLatin1Char('.') + QString::fromLatin1(format)));
    }
    CHECK(QFile::exists(folder.filePath(QStringLiteral("out.mtl"))));
}

TEST_CASE("export: the outline of a flat face, and a refusal for a round one", "[export-ui]") {
    application();
    QTemporaryDir folder;
    Session session;
    session.model.addDemoBox();
    REQUIRE(session.model.addShape(QStringLiteral("cylinder"), false) >= 0);
    session.model.waitForCalculation();
    const QString box = session.model.bodyIdAt(0);
    const QString cylinder = session.model.bodyIdAt(1);

    const auto run = [&](const QString& format, const QString& body, int face,
                         const QString& name) {
        session.exportCode = QStringLiteral("none");
        REQUIRE(session.exporter.startOutline(format, body, face,
                                              QUrl::fromLocalFile(folder.filePath(name))));
        session.waitFor(session.exportCode);
    };
    // Face 1 of a box is flat; a cylinder's side is the face that is not.
    run(QStringLiteral("svg"), box, 1, QStringLiteral("face"));
    CHECK(session.exportCode.isEmpty());
    CHECK(QFile::exists(folder.filePath(QStringLiteral("face.svg"))));
    run(QStringLiteral("dxf"), box, 1, QStringLiteral("face"));
    CHECK(session.exportCode.isEmpty());
    CHECK(QFile::exists(folder.filePath(QStringLiteral("face.dxf"))));

    bool refused = false;
    for (int face = 1; face <= 3 && !refused; ++face) {
        run(QStringLiteral("svg"), cylinder, face, QStringLiteral("round%1").arg(face));
        refused = session.exportCode == QStringLiteral("notFlat");
    }
    CHECK(refused);
    CHECK_FALSE(session.exporter.startOutline(QStringLiteral("svg"), box, 0,
                                              QUrl::fromLocalFile(folder.filePath("no.svg"))));
    CHECK(session.exportCode == QStringLiteral("nothing"));
}
