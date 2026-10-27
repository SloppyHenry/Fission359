// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Export from the interface (AP-1.10e): which bodies go in, background work, results as codes.

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

using fission::ui::DocumentModel;
using fission::ui::ExportController;

namespace {

QCoreApplication& application() {
    static int argc = 1;
    static char name[] = "fission_unit_tests";
    static char* argv[] = {name, nullptr};
    static QCoreApplication app(argc, argv);
    return app;
}

struct Session {
    std::shared_ptr<const fission::geo::GeometryKernel> kernel = fission::geo::occt::makeKernel();
    DocumentModel model{kernel};
    ExportController exporter{&model, kernel};
    QString code = QStringLiteral("none");
    int bodies = -1;

    Session() {
        model.addDemoBox();
        model.waitForCalculation();
        QObject::connect(&exporter, &ExportController::finished, &exporter,
                         [this](const QString& result, const QString&, int count) {
                             code = result;
                             bodies = count;
                         });
    }

    // Waits for the answer of the background thread.
    void waitForAnswer() {
        QElapsedTimer clock;
        clock.start();
        while (code == QLatin1String("none") && clock.elapsed() < 20000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        }
    }
};

} // namespace

TEST_CASE("export writes the visible bodies and reports how many", "[export-ui]") {
    application();
    QTemporaryDir folder;
    Session session;
    REQUIRE(session.model.addShape(QStringLiteral("cylinder"), false) >= 0);
    session.model.waitForCalculation();
    CHECK(session.exporter.exportableCount({}) == 2);

    const QString file = folder.filePath(QStringLiteral("out"));
    REQUIRE(session.exporter.start(QStringLiteral("3mf"), QStringLiteral("normal"), {},
                                   QUrl::fromLocalFile(file), {}));
    CHECK(session.exporter.exporting());
    session.waitForAnswer();
    CHECK(session.code.isEmpty());
    CHECK(session.bodies == 2);
    CHECK_FALSE(session.exporter.exporting());
    CHECK(QFile::exists(file + QStringLiteral(".3mf"))); // the ending was added
}

TEST_CASE("only the chosen bodies, and never holes or hidden bodies", "[export-ui]") {
    application();
    QTemporaryDir folder;
    Session session;
    REQUIRE(session.model.addShape(QStringLiteral("cylinder"), false) >= 0);
    session.model.waitForCalculation();
    const QString first = session.model.bodyIdAt(0);
    CHECK(session.exporter.exportableCount({first}) == 1);

    session.model.setBodyHole(1, true);
    CHECK(session.exporter.exportableCount({}) == 1); // the hole is a tool, not a part
    session.model.setBodyVisible(0, false);
    CHECK(session.exporter.exportableCount({}) == 0);

    CHECK_FALSE(
        session.exporter.start(QStringLiteral("stl"), QStringLiteral("coarse"), {},
                               QUrl::fromLocalFile(folder.filePath(QStringLiteral("x.stl"))), {}));
    CHECK(session.code == QStringLiteral("nothing"));
    CHECK_FALSE(QFile::exists(folder.filePath(QStringLiteral("x.stl"))));
}

TEST_CASE("an export into a missing folder says so and leaves nothing", "[export-ui]") {
    application();
    QTemporaryDir folder;
    Session session;
    REQUIRE(session.exporter.start(QStringLiteral("stl"), QStringLiteral("normal"), {},
                                   QUrl::fromLocalFile(folder.filePath(QStringLiteral("no/x.stl"))),
                                   {}));
    session.waitForAnswer();
    CHECK(session.code == QStringLiteral("cannotWrite"));
}

TEST_CASE("cancelling stops the export without a half file", "[export-ui]") {
    application();
    QTemporaryDir folder;
    Session session;
    const QString file = folder.filePath(QStringLiteral("c.stl"));
    REQUIRE(session.exporter.start(QStringLiteral("stl"), QStringLiteral("fine"), {},
                                   QUrl::fromLocalFile(file), {}));
    session.exporter.cancel();
    session.waitForAnswer();
    // Either it was stopped in time or it was already done: never something in between.
    CHECK((session.code == QStringLiteral("cancelled") || session.code.isEmpty()));
    if (session.code == QStringLiteral("cancelled")) {
        CHECK_FALSE(QFile::exists(file));
    }
    CHECK_FALSE(session.exporter.exporting());
}
