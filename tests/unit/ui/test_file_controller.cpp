// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Save, open, new project and recent files in the interface (AP-1.10c).

#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QUrl>
#include <QVariantMap>

#include <memory>

#include "fission/geo/occt/kernel.h"
#include "fission/services/recent_files.h"
#include "fission/ui/document_model.h"
#include "fission/ui/file_controller.h"

using fission::ui::DocumentModel;
using fission::ui::FileController;

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
    fission::services::RecentFiles recent;
    FileController files;

    explicit Session(const QString& recentSettings)
        : recent(recentSettings), files(&model, &recent) {
        model.addDemoBox();
        model.waitForCalculation();
    }
};

// Collects the codes of `fileProblem` (QSignalSpy needs QtTest, which the tests do not link).
struct Problems {
    QStringList codes;
    explicit Problems(FileController& files) {
        QObject::connect(&files, &FileController::fileProblem, &files,
                         [this](const QString& code) { codes.push_back(code); });
    }
};

struct Folder {
    QTemporaryDir dir;
    QString path(const QString& name) const { return dir.filePath(name); }
    QUrl url(const QString& name) const { return QUrl::fromLocalFile(path(name)); }
};

} // namespace

TEST_CASE("save, change, open: the file brings back what was saved", "[files]") {
    application();
    Folder folder;
    Session session(folder.path("recent.ini"));
    REQUIRE(session.model.setBodyName(0, QStringLiteral("Deckel")));
    CHECK(session.files.modified());
    CHECK(session.files.filePath().isEmpty());

    REQUIRE(session.files.saveTo(folder.url("modell")));
    session.files.waitForSave();
    CHECK(session.files.filePath() == folder.path("modell.fission")); // ".fission" was added
    CHECK(session.files.fileName() == QStringLiteral("modell"));
    CHECK_FALSE(session.files.modified());
    REQUIRE(QFile::exists(folder.path("modell.fission")));

    REQUIRE(session.model.setBodyName(0, QStringLiteral("Boden")));
    CHECK(session.files.modified());

    // Opening replaces what is there: the changed name is saved first, so reopen gives "Boden".
    Session other(folder.path("recent2.ini"));
    REQUIRE(other.files.openFrom(folder.url("modell.fission")));
    CHECK_FALSE(other.files.modified());
    CHECK(other.model.bodies().size() == 1);
    CHECK(other.model.bodies().front().toMap().value(QStringLiteral("name")).toString() ==
          QStringLiteral("Deckel"));    // the file on disk still holds the first save
    CHECK_FALSE(other.model.canUndo()); // the undo list starts empty
}

TEST_CASE("a change made while the file is written keeps the project marked as changed",
          "[files]") {
    application();
    Folder folder;
    Session session(folder.path("recent.ini"));
    REQUIRE(session.files.saveTo(folder.url("a.fission")));
    // The result of the save is taken in by the event loop, which has not run yet.
    REQUIRE(session.model.setBodyName(0, QStringLiteral("Neu")));
    session.files.waitForSave();
    CHECK(session.files.modified());
    REQUIRE(session.files.save());
    session.files.waitForSave();
    CHECK_FALSE(session.files.modified());
}

TEST_CASE("the palette of the project is saved and comes back", "[files]") {
    application();
    Folder folder;
    Session session(folder.path("recent.ini"));
    REQUIRE(session.files.saveTo(folder.url("a.fission")));
    session.files.waitForSave();
    CHECK_FALSE(session.files.modified());

    REQUIRE(session.model.setPaletteColor(0, QStringLiteral("#123456")));
    CHECK(session.files.modified()); // no undo step, but something to save
    CHECK_FALSE(session.model.canUndo());
    const int own = session.model.addPaletteColor(QStringLiteral("#abcdef"));
    REQUIRE(own >= 0);
    REQUIRE(session.files.save());
    session.files.waitForSave();
    CHECK_FALSE(session.files.modified());

    Session other(folder.path("recent2.ini"));
    REQUIRE(other.files.openFrom(folder.url("a.fission")));
    const auto palette = other.model.palette();
    REQUIRE(palette.size() == session.model.palette().size());
    CHECK(palette.at(0).toMap().value(QStringLiteral("color")).toString() ==
          QStringLiteral("#123456"));
    CHECK(palette.at(own).toMap().value(QStringLiteral("custom")).toBool());
    CHECK_FALSE(other.files.modified());
}

TEST_CASE("a file that cannot be opened leaves the document as it is", "[files]") {
    application();
    Folder folder;
    Session session(folder.path("recent.ini"));
    QFile broken(folder.path("kaputt.fission"));
    REQUIRE(broken.open(QIODevice::WriteOnly));
    broken.write("das ist keine Datei");
    broken.close();

    Problems problems(session.files);
    CHECK_FALSE(session.files.openFrom(folder.url("kaputt.fission")));
    REQUIRE(problems.codes.size() == 1);
    CHECK(problems.codes.front() == QStringLiteral("notAFissionFile"));
    CHECK_FALSE(session.files.openFrom(folder.url("fehlt.fission")));
    CHECK(problems.codes.back() == QStringLiteral("cannotRead"));
    CHECK(session.model.bodies().size() == 1);
    CHECK(session.files.filePath().isEmpty());
}

TEST_CASE("leaving: a project with a file is saved, one without is not thrown away", "[files]") {
    application();
    Folder folder;
    Session session(folder.path("recent.ini"));
    REQUIRE(session.model.setBodyName(0, QStringLiteral("Wichtig")));

    Problems problems(session.files);
    CHECK_FALSE(session.files.leave()); // never saved: refused, work stays
    REQUIRE(problems.codes.size() == 1);
    CHECK(problems.codes.front() == QStringLiteral("unsavedWork"));
    CHECK_FALSE(session.files.newDocument());
    CHECK(session.model.bodies().size() == 1);

    REQUIRE(session.files.saveTo(folder.url("a.fission")));
    session.files.waitForSave();
    REQUIRE(session.model.setBodyName(0, QStringLiteral("Noch wichtiger")));
    CHECK(session.files.leave()); // saved on the way out
    CHECK_FALSE(session.files.modified());

    REQUIRE(session.files.newDocument());
    CHECK(session.model.bodies().isEmpty());
    CHECK(session.files.filePath().isEmpty());
    CHECK_FALSE(session.files.modified());

    REQUIRE(session.files.openFrom(folder.url("a.fission")));
    CHECK(session.model.bodies().front().toMap().value(QStringLiteral("name")).toString() ==
          QStringLiteral("Noch wichtiger"));
}

TEST_CASE("the recent files are newest first, without repeats, at most ten", "[files]") {
    application();
    Folder folder;
    fission::services::RecentFiles recent(folder.path("recent.ini"));
    for (int i = 0; i < 12; ++i) {
        QFile file(folder.path(QStringLiteral("f%1.fission").arg(i)));
        REQUIRE(file.open(QIODevice::WriteOnly));
        recent.add(file.fileName());
    }
    recent.add(folder.path("f5.fission")); // moves to the top
    const QStringList paths = recent.paths();
    REQUIRE(paths.size() == 10);
    CHECK(paths.front() == folder.path("f5.fission"));
    CHECK(paths.count(folder.path("f5.fission")) == 1);

    QFile::remove(folder.path("f11.fission")); // a file that is gone is not offered
    CHECK_FALSE(recent.paths().contains(folder.path("f11.fission")));
    recent.remove(folder.path("f5.fission"));
    CHECK_FALSE(recent.paths().contains(folder.path("f5.fission")));
}
