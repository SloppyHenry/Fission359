// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Save, open, new project and recent files in the interface (AP-1.10c).

#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QTemporaryDir>
#include <QThread>
#include <QUrl>
#include <QVariantMap>

#include <memory>

#include "fission/geo/occt/kernel.h"
#include "fission/services/autosave.h"
#include "fission/services/recent_files.h"
#include "fission/services/recovery_store.h"
#include "fission/ui/clipboard_payload.h"
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

    REQUIRE(session.files.saveTo(folder.url("a.fission")));
    session.files.waitForSave();
    REQUIRE(session.model.setBodyName(0, QStringLiteral("Noch wichtiger")));
    CHECK(session.files.leave()); // saved on the way out
    CHECK_FALSE(session.files.modified());

    // A new project is a tab of its own; the first one stays as it is.
    REQUIRE(session.files.newDocument());
    CHECK(session.files.tabs().size() == 2);
    CHECK(session.model.bodies().isEmpty());
    CHECK(session.files.filePath().isEmpty());
    CHECK_FALSE(session.files.modified());

    // The file is open in the first tab already: opening it shows that tab.
    REQUIRE(session.files.openFrom(folder.url("a.fission")));
    CHECK(session.files.tabs().size() == 2);
    CHECK(session.files.currentTab() == 0);
    CHECK(session.model.bodies().front().toMap().value(QStringLiteral("name")).toString() ==
          QStringLiteral("Noch wichtiger"));
}

TEST_CASE("two documents do not touch each other", "[files]") {
    application();
    Folder folder;
    Session session(folder.path("recent.ini"));
    REQUIRE(session.model.setBodyName(0, QStringLiteral("Eins")));
    REQUIRE(session.files.saveTo(folder.url("one.fission")));
    session.files.waitForSave();

    REQUIRE(session.files.newDocument());
    REQUIRE(session.model.addShape(QStringLiteral("cylinder"), false) >= 0);
    session.model.waitForCalculation();
    REQUIRE(session.model.setBodyName(0, QStringLiteral("Zwei")));
    CHECK(session.files.modified());
    CHECK(session.model.canUndo());

    // Back to the first: its own steps, nothing of the second, and no undo of the second.
    REQUIRE(session.files.switchTab(0));
    session.model.waitForCalculation();
    CHECK(session.model.bodies().size() == 1);
    CHECK(session.model.bodies().front().toMap().value(QStringLiteral("name")).toString() ==
          QStringLiteral("Eins"));
    CHECK(session.files.fileName() == QStringLiteral("one"));
    CHECK_FALSE(session.files.modified());
    CHECK(session.model.historyPosition() == 1); // its own undo list: the one rename

    // The second kept its steps and its undo list.
    REQUIRE(session.files.switchTab(1));
    session.model.waitForCalculation();
    CHECK(session.model.bodies().front().toMap().value(QStringLiteral("name")).toString() ==
          QStringLiteral("Zwei"));
    CHECK(session.files.modified());
    CHECK(session.model.historyPosition() == 2); // the shape and the rename, nothing of the first
    CHECK(session.model.undo());
    CHECK(session.model.bodies().front().toMap().value(QStringLiteral("type")).toString() ==
          QStringLiteral("cylinder"));

    // Undo in the second never reached the first.
    REQUIRE(session.files.switchTab(0));
    session.model.waitForCalculation();
    CHECK(session.model.bodies().front().toMap().value(QStringLiteral("name")).toString() ==
          QStringLiteral("Eins"));
}

TEST_CASE("closing a tab saves it, and the last tab leaves an empty project", "[files]") {
    application();
    Folder folder;
    Session session(folder.path("recent.ini"));
    REQUIRE(session.files.saveTo(folder.url("one.fission")));
    session.files.waitForSave();
    REQUIRE(session.files.newDocument());
    REQUIRE(session.model.addShape(QStringLiteral("sphere"), false) >= 0);
    REQUIRE(session.files.saveTo(folder.url("two.fission")));
    session.files.waitForSave();
    REQUIRE(session.model.setBodyName(0, QStringLiteral("Geändert")));

    REQUIRE(session.files.closeTab(1)); // saved on the way out
    CHECK(session.files.tabs().size() == 1);
    CHECK(session.files.fileName() == QStringLiteral("one"));

    Session other(folder.path("recent2.ini"));
    REQUIRE(other.files.openFrom(folder.url("two.fission")));
    CHECK(other.model.bodies().front().toMap().value(QStringLiteral("name")).toString() ==
          QStringLiteral("Geändert"));

    // The last tab: an empty project takes its place.
    REQUIRE(session.files.closeTab(0));
    CHECK(session.files.tabs().size() == 1);
    CHECK(session.model.bodies().isEmpty());
    CHECK(session.files.filePath().isEmpty());
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

TEST_CASE("copied bodies survive the clipboard format", "[files]") {
    using namespace fission::ui;
    ClipboardPart part;
    part.draft.type = "box";
    part.draft.typeVersion = 1;
    part.draft.label = "Fu\xC3\x9F";
    part.draft.params["width"] = {"2,5 * 4", fission::core::QuantityKind::length};
    part.draft.texts["note"] = "hallo";
    part.name = "Deckel";
    part.color = {1, 2, 3};
    part.hole = true;
    ClipboardPart second = part;
    second.name.clear();
    second.hole = false;

    const auto back = clipboardFromJson(clipboardToJson({part, second}));
    REQUIRE(back.size() == 2);
    CHECK(back[0].draft.type == "box");
    CHECK(back[0].draft.label == part.draft.label);
    CHECK(back[0].draft.params == part.draft.params);
    CHECK(back[0].draft.texts == part.draft.texts);
    CHECK(back[0].name == "Deckel");
    CHECK(back[0].color == part.color);
    CHECK(back[0].hole);
    CHECK(back[1].name.empty());
    CHECK_FALSE(back[1].hole);

    CHECK(clipboardFromJson("").empty());
    CHECK(clipboardFromJson("not json").empty());
    CHECK(clipboardFromJson("{}").empty());
}

// --- autosave and rescue (AP-1.10d) ---------------------------------------------------------

namespace {

// A session with rescue folder and an autosave that is due after `commands` steps.
struct RescueSession {
    std::shared_ptr<const fission::geo::GeometryKernel> kernel = fission::geo::occt::makeKernel();
    DocumentModel model{kernel};
    fission::services::RecentFiles recent;
    fission::services::RecoveryStore recovery;
    fission::services::Autosave autosave;
    FileController files;

    RescueSession(const QString& recentSettings, const QString& rescueFolder, int commands = 3)
        : recent(recentSettings), recovery(rescueFolder), autosave(3600000, commands),
          files(&model, &recent, &recovery, &autosave) {
        recovery.beginSession();
        model.addDemoBox();
        model.waitForCalculation();
    }

    void rename(int times) {
        for (int i = 0; i < times; ++i) {
            REQUIRE(model.setBodyName(0, QStringLiteral("Name %1").arg(++counter)));
        }
    }
    int counter = 0;
};

// What a crash leaves: the marker of the dead program stays, naming a process that is gone.
void pretendItCrashed(const fission::services::RecoveryStore& store) {
    QFile marker(store.folder() + QLatin1Char('/') + store.sessionId() +
                 QStringLiteral(".running"));
    REQUIRE(marker.open(QIODevice::WriteOnly | QIODevice::Truncate));
    marker.write("2147483000");
}

} // namespace

TEST_CASE("the autosave is due after the 20th command and after a minute", "[files]") {
    application();
    int due = 0;
    fission::services::Autosave counting(3600000, 20);
    QObject::connect(&counting, &fission::services::Autosave::due, &counting, [&] { ++due; });
    for (int i = 0; i < 19; ++i) {
        counting.commandDone();
    }
    CHECK(due == 0);
    counting.commandDone();
    CHECK(due == 1);
    counting.acknowledge();
    counting.commandDone();
    CHECK(due == 1);

    // The timer: nothing is due when nothing was done, something is when it was.
    fission::services::Autosave timed(20, 1000);
    int timedDue = 0;
    QObject::connect(&timed, &fission::services::Autosave::due, &timed, [&] { ++timedDue; });
    timed.start();
    QElapsedTimer clock;
    clock.start();
    while (clock.elapsed() < 120) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
    CHECK(timedDue == 0);
    timed.commandDone();
    clock.restart();
    while (timedDue == 0 && clock.elapsed() < 2000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
    CHECK(timedDue >= 1);
}

TEST_CASE("work of a session that crashed is offered and comes back", "[files]") {
    application();
    Folder folder;
    const QString rescueFolder = folder.path("recovery");
    QString id;
    {
        RescueSession session(folder.path("r1.ini"), rescueFolder);
        session.rename(3); // the third step makes the autosave due
        session.files.waitForSave();
        REQUIRE(QFile::exists(session.recovery.filePath()));
        pretendItCrashed(session.recovery);
        id = session.recovery.sessionId();
        // no finishSession: the program was killed
    }
    RescueSession next(folder.path("r2.ini"), rescueFolder);
    QString offered;
    QObject::connect(&next.files, &FileController::rescuedWork, &next.files,
                     [&](const QString& found) { offered = found; });
    next.files.checkRecovery();
    CHECK(offered == id);

    REQUIRE(next.files.restoreRescued(id));
    CHECK(next.model.bodies().front().toMap().value(QStringLiteral("name")).toString() ==
          QStringLiteral("Name 3"));
    CHECK(next.files.modified()); // it is not what lies on disk
    CHECK(next.files.filePath().isEmpty());
    offered.clear();
    next.files.checkRecovery();
    CHECK(offered.isEmpty());                       // the old rescue copy is let go of
    CHECK(QFile::exists(next.recovery.filePath())); // and the work lies in this session's file
}

TEST_CASE("a rescued project remembers its file", "[files]") {
    application();
    Folder folder;
    const QString rescueFolder = folder.path("recovery");
    QString id;
    {
        RescueSession session(folder.path("r1.ini"), rescueFolder);
        REQUIRE(session.files.saveTo(folder.url("mine.fission")));
        session.files.waitForSave();
        session.rename(3);
        session.files.waitForSave();
        pretendItCrashed(session.recovery);
        id = session.recovery.sessionId();
    }
    RescueSession next(folder.path("r2.ini"), rescueFolder);
    REQUIRE(next.files.restoreRescued(id));
    CHECK(next.files.filePath() == folder.path("mine.fission"));
    CHECK(next.files.modified());
}

TEST_CASE("a clean end leaves no rescue files, a project without a file stays rescuable",
          "[files]") {
    application();
    Folder folder;
    const QString rescueFolder = folder.path("recovery");
    {
        RescueSession session(folder.path("r1.ini"), rescueFolder);
        REQUIRE(session.files.saveTo(folder.url("a.fission")));
        session.files.waitForSave();
        session.rename(3);
        session.files.waitForSave();
        REQUIRE(QFile::exists(session.recovery.filePath()));
        session.files.finishSession(); // saves into a.fission, rescue copy goes away
        CHECK_FALSE(QFile::exists(session.recovery.filePath()));
        CHECK(session.recovery.rescued().empty());
    }
    CHECK(QDir(rescueFolder).entryList(QDir::Files).isEmpty());

    {
        RescueSession session(folder.path("r1.ini"), rescueFolder);
        session.rename(1); // modified, never saved
        session.files.finishSession();
    }
    RescueSession next(folder.path("r2.ini"), rescueFolder);
    const auto rescued = next.recovery.rescued();
    REQUIRE(rescued.size() == 1);
    CHECK(rescued.front().originalPath.isEmpty());

    next.files.discardRescued(rescued.front().id);
    CHECK(next.recovery.rescued().empty());
}

TEST_CASE("saving removes the rescue copy; undoing is no new work", "[files]") {
    application();
    Folder folder;
    RescueSession session(folder.path("r1.ini"), folder.path("recovery"));
    session.rename(3);
    session.files.waitForSave();
    REQUIRE(QFile::exists(session.recovery.filePath()));
    REQUIRE(session.files.saveTo(folder.url("a.fission")));
    session.files.waitForSave();
    CHECK_FALSE(QFile::exists(session.recovery.filePath()));
}

TEST_CASE("the rescue folder holds at most ten files, the oldest go first", "[files]") {
    application();
    Folder folder;
    const QString rescueFolder = folder.path("recovery");
    QDir().mkpath(rescueFolder);
    for (int i = 0; i < 12; ++i) {
        fission::services::RecoveryStore old(rescueFolder);
        QFile file(old.filePath());
        REQUIRE(file.open(QIODevice::WriteOnly));
        file.write("x");
        file.close();
        old.writeInfo(QString());
        // Sessions that are over: no marker. Older ones first.
        QFile info(rescueFolder + QLatin1Char('/') + old.sessionId() + QStringLiteral(".json"));
        QThread::msleep(5);
    }
    fission::services::RecoveryStore reader(rescueFolder);
    CHECK(reader.rescued().size() <= 10);
    CHECK(reader.rescued().size() >= 9);
}

TEST_CASE("a tab never saved is rescue work when it is closed", "[files]") {
    application();
    Folder folder;
    const QString rescueFolder = folder.path("recovery");
    {
        RescueSession session(folder.path("r1.ini"), rescueFolder);
        session.rename(1);
        REQUIRE(session.files.closeTab(0)); // never saved: set aside, not lost
        CHECK(session.recovery.rescued().size() == 1);
    }
    RescueSession next(folder.path("r2.ini"), rescueFolder);
    CHECK(next.recovery.rescued().size() == 1);
}

TEST_CASE("every tab has its own rescue file after a crash", "[files]") {
    application();
    Folder folder;
    const QString rescueFolder = folder.path("recovery");
    {
        RescueSession session(folder.path("r1.ini"), rescueFolder);
        session.rename(3); // first tab: autosave is due
        REQUIRE(session.files.newDocument());
        REQUIRE(session.model.addShape(QStringLiteral("cone"), false) >= 0);
        session.model.waitForCalculation();
        REQUIRE(session.files.switchTab(0)); // the second is put aside and written first
        session.files.waitForSave();
        pretendItCrashed(session.recovery);
    }
    RescueSession next(folder.path("r2.ini"), rescueFolder);
    CHECK(next.recovery.rescued().size() == 2);
}
