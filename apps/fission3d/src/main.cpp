// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QBuffer>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QFileOpenEvent>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QImage>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMouseEvent>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSaveFile>
#include <QStandardPaths>
#include <QString>
#include <QTemporaryDir>
#include <QTimer>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <unistd.h>
#include <vector>

#include "fission/core/version.h"
#include "fission/geo/occt/kernel.h"
#include "fission/geo/occt/layer.h"
#include "fission/render/benchmark_scene.h"
#include "fission/render/scene.h"
#include "fission/services/autosave.h"
#include "fission/services/log.h"
#include "fission/services/recent_files.h"
#include "fission/services/recovery_store.h"
#include "fission/services/settings.h"
#include "fission/services/startup_clock.h"
#include "fission/ui/align_controller.h"
#include "fission/ui/app_info.h"
#include "fission/ui/app_options.h"
#include "fission/ui/command_search.h"
#include "fission/ui/distance_controller.h"
#include "fission/ui/document_model.h"
#include "fission/ui/export_controller.h"
#include "fission/ui/file_controller.h"
#include "fission/ui/gizmo_controller.h"
#include "fission/ui/handle_controller.h"
#include "fission/ui/language.h"
#include "fission/ui/numbers.h"
#include "fission/ui/placement_controller.h"
#include "fission/ui/preferences.h"
#include "fission/ui/rotate_controller.h"
#include "fission/ui/scale_controller.h"
#include "fission/ui/selection_controller.h"
#include "fission/ui/shape_previews.h"
#include "fission/ui/snap_session.h"
#include "fission/ui/theme.h"
#include "fission/ui/viewport_3d.h"

#include <QtQml/qqml.h>
#include <spdlog/spdlog.h>

namespace {

QString toQString(std::string_view text) {
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

// "0.1.0 (OpenCASCADE 8.0.1)": handy in bug reports. Asking for the kernel version calls into
// the library, so a broken library setup shows up right at startup.
QString versionText() {
    return QStringLiteral("%1 (OpenCASCADE %2)")
        .arg(toQString(fission::core::versionString()),
             QString::fromStdString(fission::geo::occt::kernelVersion()));
}

struct CommandLine {
    QString smokeTestPath; // --smoke-test: save a picture of the window and quit
    bool measureStartup = false;
    QString theme;    // dark | light | system (empty: dark)
    QString language; // de | en | system (empty: system)
    bool focus = false;
    bool about = false;
    int openMenu = -1;
    bool reduceMotion = false;
    bool showHistory = false;
    bool showShapes = false;
    QString camera;
    QString rightTab;
    bool softShadows = true;
    int benchmarkTriangles = 0;
    QString simulate;
    QSize size;
    QString file; // a .fission file to open (double click, "fission3d model.fission")
};

// Reads the command line with a plain QCoreApplication: it needs no display, so --help and
// --version also work in a terminal without a window system.
CommandLine parseCommandLine(int& argc, char** argv) {
    const QCoreApplication core(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("Fission3D"));
    QCoreApplication::setApplicationVersion(versionText());

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    // Developer options: used by scripts/test and for screenshots.
    const QCommandLineOption smokeTest(
        QStringLiteral("smoke-test"),
        QStringLiteral("Save a screenshot of the first frame and exit."),
        QStringLiteral("file.png"));
    const QCommandLineOption measureStartup(
        QStringLiteral("measure-startup"),
        QStringLiteral("Print the time from process start to the first drawn frame and exit."));
    const QCommandLineOption theme(QStringLiteral("theme"),
                                   QStringLiteral("Design: dark (default), light or system."),
                                   QStringLiteral("name"));
    const QCommandLineOption language(QStringLiteral("language"),
                                      QStringLiteral("Language: de, en or system (default)."),
                                      QStringLiteral("code"));
    const QCommandLineOption focus(QStringLiteral("focus"),
                                   QStringLiteral("Start in focus mode (side bars hidden)."));
    const QCommandLineOption about(QStringLiteral("show-about"),
                                   QStringLiteral("Open the About window at start."));
    const QCommandLineOption openMenu(
        QStringLiteral("open-menu"),
        QStringLiteral("Open the menu with this number (0 = first) at start."),
        QStringLiteral("n"));
    const QCommandLineOption showHistory(
        QStringLiteral("show-history"),
        QStringLiteral("Make a few changes, undo one and open the list of recent actions."));
    const QCommandLineOption showShapes(QStringLiteral("show-shapes"),
                                        QStringLiteral("Open the shape gallery at start."));
    const QCommandLineOption camera(
        QStringLiteral("camera"),
        QStringLiteral("Start with the camera at yaw,pitch,distance (degrees, degrees, mm)."),
        QStringLiteral("yaw,pitch,distance"));
    const QCommandLineOption benchmark(
        QStringLiteral("benchmark"),
        QStringLiteral("Measure the frame time with a scene of this many triangles while the "
                       "camera turns, print the result and exit."),
        QStringLiteral("triangles"));
    const QCommandLineOption rightTab(
        QStringLiteral("right-tab"),
        QStringLiteral("Start with this tab on the right: object or printer."),
        QStringLiteral("name"));
    const QCommandLineOption noSoftShadows(QStringLiteral("no-soft-shadows"),
                                           QStringLiteral("Start with the soft shadows off."));
    const QCommandLineOption reduceMotion(QStringLiteral("reduce-motion"),
                                          QStringLiteral("Switch animations off."));
    const QCommandLineOption simulate(
        QStringLiteral("simulate"),
        QStringLiteral(
            "Play pointer actions after the first frame, before the screenshot: "
            "hover:x,y  click:x,y  shiftclick:x,y  doubleclick:x,y  frame:x1,y1,x2,y2 "
            "(window pixels of the 3D view), drop:shape,x,y  drophole:shape,x,y "
            "(drag a shape of the gallery there and let go; a third value other than 0 holds Alt: "
            "upright instead of lying on a wall), put:shape  puthole:shape "
            "(click its tile), type:50,tab,20,enter (type sizes), set:style,bold (a word "
            "of the chosen step), grab:width,40,0,8 (press a handle, pull it by view pixels "
            "in small moves, let go), point:cornerRadius (put the pointer on that handle and leave "
            "it there), recipe:box,cylinder,combine (fail unless the recipe has exactly these "
            "steps), "
            "expect:width,40 (fail unless the chosen step has at least "
            "that value, no drag is left and its handles are still shown), near:x,12 (fail "
            "unless the chosen step has exactly that value), tool:size (choose a tool of the "
            "palette), flyout:combine (open that group of the palette), filtermenu (open the "
            "choice menu of the 3D view), snap:off (snapping off as "
            "with Shift held; snap:on), grid:0.1 (the step of "
            "snapping in mm, 0 = automatic), "
            "distance:left,5 (type 5 into the distance measure on that side of the chosen "
            "body: left, right, front or back), selectall (choose every body), step:2 (choose the "
            "body of that step, from 0), align:x,min "
            "(click the point that lines the chosen bodies up along x, y or z: min, centre or "
            "max), menu:dropToBed (choose that entry of the menus), "
            "gizmo:x,40,0,8 (press "
            "a part of the gizmo: x, y, z, xy, xz, yz or free, "
            "pull it by view pixels, let go; with a fifth value hold the button stays down), "
            "ring:z,60,8 (press the ring z, x or y of the tool Rotate where it is easiest to take "
            "and go 60 degrees round it in 8 small moves, let go; a fourth value hold keeps the "
            "button down), "
            "corner:1,40,-20,8 (press the corner 0 to 3 of the tool Size - front left, front "
            "right, back right, back left - pull it by view pixels, let go; a fifth value hold "
            "keeps the button down), "
            "drag:x,y,40,0,8 (press at a point of the 3D view, "
            "pull, let go), focus:width (the cursor into that field), key:Right (press a key), "
            "separated by semicolons."),
        QStringLiteral("actions"));
    const QCommandLineOption size(QStringLiteral("size"),
                                  QStringLiteral("Window size, for example 1280x800."),
                                  QStringLiteral("WxH"));
    parser.addPositionalArgument(QStringLiteral("file"), QStringLiteral("A .fission file to open."),
                                 QStringLiteral("[file.fission]"));
    parser.addOptions({smokeTest, measureStartup, theme, language, focus, about, openMenu,
                       showHistory, showShapes, camera, rightTab, benchmark, noSoftShadows,
                       reduceMotion, simulate, size});
    parser.process(core); // prints help or version and exits when asked

    CommandLine result;
    if (!parser.positionalArguments().isEmpty()) {
        result.file = parser.positionalArguments().constFirst();
    }
    result.smokeTestPath = parser.value(smokeTest);
    result.measureStartup = parser.isSet(measureStartup);
    result.theme = parser.value(theme);
    result.language = parser.value(language);
    result.focus = parser.isSet(focus);
    result.about = parser.isSet(about);
    result.reduceMotion = parser.isSet(reduceMotion);
    result.showHistory = parser.isSet(showHistory);
    result.showShapes = parser.isSet(showShapes);
    result.camera = parser.value(camera);
    result.rightTab = parser.value(rightTab);
    result.benchmarkTriangles = parser.value(benchmark).toInt();
    result.simulate = parser.value(simulate);
    result.softShadows = !parser.isSet(noSoftShadows);
    result.openMenu = parser.isSet(openMenu) ? parser.value(openMenu).toInt() : -1;
    const QStringList parts = parser.value(size).split(QLatin1Char('x'));
    if (parts.size() == 2 && parts[0].toInt() > 0 && parts[1].toInt() > 0) {
        result.size = QSize(parts[0].toInt(), parts[1].toInt());
    }
    return result;
}

// Developer option --simulate: plays pointer actions through the real input path of the 3D view
// (graphics card, pick pass, selection), one every 350 ms. Calls `done` when the last one had
// time to show its result.
// Set by --simulate "expect:..." when what was expected did not happen.
int g_simulationFailure = 0;

// A mouse event through the window, at a point of the 3D view: as a hand would do it.
void sendMouse(QQuickWindow* window, fission::ui::Viewport3D* viewport, QEvent::Type type,
               const QPointF& inView, Qt::MouseButtons buttons) {
    const QPointF scene = viewport->mapToScene(inView);
    // A move names no button (it is only held); press and release name the left one.
    const Qt::MouseButton changed = type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton;
    QMouseEvent event(type, scene, window->mapToGlobal(scene.toPoint()), changed, buttons,
                      Qt::NoModifier);
    QCoreApplication::sendEvent(window, &event);
}

// Pulls that have been started and are not let go yet. A check waits for them, so that it never
// sees the middle of a pull (a loaded machine may fire the timer of a check before the last move).
int g_pullsRunning = 0;

// Press at `start()` (asked when the press happens), pull by `pull` view pixels in `steps`
// moves, let go. Returns the delay after the last event.
// Runs `check` once the model has worked out the last change, so that a check sees the values of
// the document and not those of the change before (what `stepValues` shows until the result is
// there). A window that was busy for a moment fires the timers of an action and of the check
// after it at once; without this the check came too early now and then. Gives up waiting after
// `patience` milliseconds and checks anyway.
void whenCalculated(QObject* context, fission::ui::DocumentModel* model,
                    std::function<void()> check, int patience = 5000) {
    // Also not while a shape is still being put down: on a slow machine (CI with the memory
    // checker) the check came before the shape was let go, and a hole cuts only then.
    const bool done = g_pullsRunning == 0 && !model->placing() && !model->calculating() &&
                      model->result() && model->result()->revision == model->document().revision();
    if (done || patience <= 0) {
        check();
        return;
    }
    QTimer::singleShot(50, context, [context, model, check = std::move(check), patience]() mutable {
        whenCalculated(context, model, std::move(check), patience - 50);
    });
}

void checkAfter(int delay, QObject* context, fission::ui::DocumentModel* model,
                std::function<void()> check) {
    QTimer::singleShot(delay, context, [context, model, check = std::move(check)]() mutable {
        whenCalculated(context, model, std::move(check));
    });
}

// One pull of the pointer. Every timer holds the pull, so it lives until its last move.
struct Pull : std::enable_shared_from_this<Pull> {
    QQuickWindow* window = nullptr;
    fission::ui::Viewport3D* viewport = nullptr;
    std::function<QPointF()> start;
    QPointF pull;
    int steps = 1;
    bool hold = false;
    QPointF from;

    // The press waits for what it presses on: `start()` finds nothing (a null point) while the
    // shape that carries the handle is not calculated yet, which a loaded machine makes likely.
    // The moves follow the press, whenever it happens, so the pull is never begun on nothing.
    void press(int patience) {
        from = start();
        if (from.isNull() && patience > 0) {
            QTimer::singleShot(50, viewport, [self = shared_from_this(), patience] {
                self->press(patience - 50);
            });
            return;
        }
        sendMouse(window, viewport, QEvent::MouseButtonPress, from, Qt::LeftButton);
        next(1);
    }

    void next(int step) {
        QTimer::singleShot(90, viewport, [self = shared_from_this(), step] {
            if (step > self->steps) {
                if (!self->hold) {
                    sendMouse(self->window, self->viewport, QEvent::MouseButtonRelease,
                              self->from + self->pull, Qt::NoButton);
                }
                --g_pullsRunning;
                return;
            }
            sendMouse(self->window, self->viewport, QEvent::MouseMove,
                      self->from + self->pull * step / self->steps, Qt::LeftButton);
            self->next(step + 1);
        });
    }
};

int pressAndPull(QQuickWindow* window, fission::ui::Viewport3D* viewport, int delay,
                 std::function<QPointF()> start, QPointF pull, int steps, bool hold = false) {
    auto action = std::make_shared<Pull>();
    action->window = window;
    action->viewport = viewport;
    action->start = std::move(start);
    action->pull = pull;
    action->steps = steps;
    action->hold = hold;
    delay += 350;
    QTimer::singleShot(delay, viewport, [action] {
        ++g_pullsRunning;
        action->press(5000);
    });
    // The planned end of the pull; a check after it waits for the real one (`g_pullsRunning`).
    delay += 90 * steps;
    return hold ? delay : delay + 90;
}

void playActions(QQuickWindow* window, fission::ui::Viewport3D* viewport,
                 fission::ui::PlacementController* placement, fission::ui::DocumentModel* model,
                 fission::ui::HandleController* handles, fission::ui::GizmoController* gizmo,
                 fission::ui::RotateController* rotate, fission::ui::ScaleController* scale,
                 fission::ui::DistanceController* distances, fission::ui::AlignController* align,
                 fission::ui::SelectionController* selection, fission::ui::FileController* files,
                 fission::ui::ExportController* exporter, fission::ui::Preferences* preferences,
                 const QString& actions, std::function<void()> done) {
    int delay = 0;
    for (const QString& action : actions.split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
        const int colon = action.indexOf(QLatin1Char(':'));
        const QString kind = action.left(colon).trimmed();
        QList<double> numbers;
        QString shape; // drop:sphere,x,y and put:sphere start with the name of a shape
        if (kind == QLatin1String("expect")) {
            // expect:width,40 - the chosen step has at least this value, and no drag is left
            // hanging. Otherwise the program ends with an error (for the tests).
            const QStringList spec = action.mid(colon + 1).split(QLatin1Char(','));
            delay += 350;
            checkAfter(delay, viewport, model, [model, handles, spec] {
                double value = -1.0;
                for (const QVariant& entry : model->stepValues()) {
                    if (entry.toMap()["name"].toString() == spec.value(0)) {
                        value = entry.toMap()["value"].toDouble();
                    }
                }
                if (value < spec.value(1).toDouble() || handles->editing() ||
                    handles->handles().isEmpty()) {
                    qWarning().noquote()
                        << QStringLiteral(
                               "expected %1 >= %2, nothing being dragged and the handles "
                               "still there; found %3 (editing: %4, handles: %5)")
                               .arg(spec.value(0), spec.value(1))
                               .arg(value)
                               .arg(handles->editing())
                               .arg(handles->handles().size());
                    g_simulationFailure = 4;
                }
            });
            continue;
        }
        if (kind == QLatin1String("recipe")) {
            // recipe:box,cylinder,combine - the steps of the recipe are exactly these types, in
            // this order (a step "group" must never appear: holes cut without it). Otherwise the
            // program ends with an error (for the tests).
            QStringList wanted = action.mid(colon + 1).split(QLatin1Char(','));
            if (wanted == QStringList{QString()}) {
                wanted.clear(); // "recipe:" - no steps at all
            }
            delay += 350;
            checkAfter(delay, viewport, model, [model, wanted] {
                QStringList found;
                for (const QVariant& step : model->recipe()) {
                    found << step.toMap()["type"].toString();
                }
                if (found != wanted) {
                    qWarning().noquote()
                        << QStringLiteral("expected the recipe %1; found %2")
                               .arg(wanted.join(QLatin1Char(',')), found.join(QLatin1Char(',')));
                    g_simulationFailure = 5;
                }
            });
            continue;
        }
        if (kind == QLatin1String("point")) {
            // point:cornerRadius - the pointer goes onto the handle of that value and stays there
            // (a real mouse move through the window): what it shows then can be photographed.
            const QString parameter = action.mid(colon + 1);
            delay += 350;
            QTimer::singleShot(delay, viewport, [window, viewport, handles, parameter] {
                for (const QVariant& entry : handles->handles()) {
                    if (entry.toMap()["parameter"].toString() != parameter) {
                        continue;
                    }
                    const QPointF scene = viewport->mapToScene(
                        QPointF(entry.toMap()["x"].toDouble(), entry.toMap()["y"].toDouble()));
                    QMouseEvent event(QEvent::MouseMove, scene,
                                      window->mapToGlobal(scene.toPoint()), Qt::NoButton,
                                      Qt::NoButton, Qt::NoModifier);
                    QCoreApplication::sendEvent(window, &event);
                }
            });
            continue;
        }
        if (kind == QLatin1String("grab")) {
            // grab:width,40,0,8 - press on the handle of "width", pull it 40 x 0 view pixels in
            // 8 small moves, let go: real mouse events through the window, as a hand would do.
            const QStringList spec = action.mid(colon + 1).split(QLatin1Char(','));
            const int steps = spec.size() >= 4 ? std::max(1, spec[3].toInt()) : 8;
            const QString parameter = spec.value(0);
            delay = pressAndPull(
                window, viewport, delay,
                [handles, parameter] {
                    for (const QVariant& entry : handles->handles()) {
                        if (entry.toMap()["parameter"].toString() == parameter) {
                            return QPointF(entry.toMap()["x"].toDouble(),
                                           entry.toMap()["y"].toDouble());
                        }
                    }
                    return QPointF();
                },
                QPointF(spec.value(1).toDouble(), spec.value(2).toDouble()), steps);
            continue;
        }
        if (kind == QLatin1String("gizmo")) {
            // gizmo:x,40,0,8 - press on a part of the gizmo, pull, let go (as for grab).
            const QStringList spec = action.mid(colon + 1).split(QLatin1Char(','));
            const int steps = spec.size() >= 4 ? std::max(1, spec[3].toInt()) : 8;
            const QString part = spec.value(0);
            delay = pressAndPull(
                window, viewport, delay,
                [gizmo, part] {
                    for (const QVariant& entry : gizmo->parts()) {
                        if (entry.toMap()["part"].toString() == part) {
                            return QPointF(entry.toMap()["x"].toDouble(),
                                           entry.toMap()["y"].toDouble());
                        }
                    }
                    return QPointF();
                },
                QPointF(spec.value(1).toDouble(), spec.value(2).toDouble()), steps,
                spec.value(4) == QLatin1String("hold"));
            continue;
        }
        if (kind == QLatin1String("ring")) {
            // ring:z,60,8 - press the ring where it is easiest to take, go 60 degrees round it
            // in 8 small moves, let go (real mouse events, as for grab).
            const QStringList spec = action.mid(colon + 1).split(QLatin1Char(','));
            const QString part = spec.value(0).trimmed();
            const double degrees = spec.value(1).toDouble();
            const int steps = spec.size() >= 3 ? std::max(1, spec[2].toInt()) : 8;
            const bool hold = spec.value(3).trimmed() == QLatin1String("hold");
            auto from = std::make_shared<double>(0.0);
            delay += 350;
            QTimer::singleShot(delay, viewport, [window, viewport, rotate, part, from] {
                *from = rotate->grabAngle(part);
                sendMouse(window, viewport, QEvent::MouseButtonPress,
                          rotate->ringPoint(part, *from), Qt::LeftButton);
            });
            for (int step = 1; step <= steps; ++step) {
                delay += 90;
                QTimer::singleShot(
                    delay, viewport, [window, viewport, rotate, part, from, degrees, step, steps] {
                        sendMouse(window, viewport, QEvent::MouseMove,
                                  rotate->ringPoint(part, *from + degrees * step / steps),
                                  Qt::LeftButton);
                    });
            }
            if (!hold) {
                delay += 90;
                QTimer::singleShot(
                    delay, viewport, [window, viewport, rotate, part, from, degrees] {
                        sendMouse(window, viewport, QEvent::MouseButtonRelease,
                                  rotate->ringPoint(part, *from + degrees), Qt::NoButton);
                    });
            }
            continue;
        }
        if (kind == QLatin1String("corner")) {
            // corner:1,40,-20,8 - press a corner square of the tool Size, pull, let go.
            const QStringList spec = action.mid(colon + 1).split(QLatin1Char(','));
            const int steps = spec.size() >= 4 ? std::max(1, spec[3].toInt()) : 8;
            const int corner = spec.value(0).toInt();
            delay = pressAndPull(
                window, viewport, delay,
                [scale, corner] {
                    const QVariantMap entry = scale->corners().value(corner).toMap();
                    return QPointF(entry["x"].toDouble(), entry["y"].toDouble());
                },
                QPointF(spec.value(1).toDouble(), spec.value(2).toDouble()), steps,
                spec.value(4).trimmed() == QLatin1String("hold"));
            continue;
        }
        if (kind == QLatin1String("drag")) {
            // drag:500,300,40,0,8 - press at (500, 300) in the 3D view, pull, let go.
            const QStringList spec = action.mid(colon + 1).split(QLatin1Char(','));
            const int steps = spec.size() >= 5 ? std::max(1, spec[4].toInt()) : 8;
            const QPointF at(spec.value(0).toDouble(), spec.value(1).toDouble());
            delay = pressAndPull(
                window, viewport, delay, [at] { return at; },
                QPointF(spec.value(2).toDouble(), spec.value(3).toDouble()), steps);
            continue;
        }
        if (action.trimmed() == QLatin1String("filtermenu")) { // opens "Choose: ..."
            delay += 350;
            QTimer::singleShot(delay, viewport,
                               [window] { window->setProperty("filterMenuOpen", true); });
            continue;
        }
        if (kind == QLatin1String("flyout")) { // flyout:combine - opens that group of the palette
            const QString group = action.mid(colon + 1).trimmed();
            delay += 350;
            QTimer::singleShot(delay, viewport,
                               [window, group] { window->setProperty("flyout", group); });
            continue;
        }
        if (kind == QLatin1String("grid")) { // grid:0.1 - the step of snapping (0: automatic)
            const double step = action.mid(colon + 1).trimmed().toDouble();
            delay += 350;
            QTimer::singleShot(delay, viewport,
                               [viewport, step] { viewport->setProperty("fixedSnapStep", step); });
            continue;
        }
        if (kind == QLatin1String("snap")) { // snap:off / snap:on - Shift held, or not
            const bool off = action.mid(colon + 1).trimmed() == QLatin1String("off");
            delay += 350;
            QTimer::singleShot(delay, viewport, [gizmo, off] {
                gizmo->setSnapSuppressed(off);
                fission::ui::setSnappingSuppressedForTests(
                    off); // size handles, corners, putting down
            });
            continue;
        }
        if (kind == QLatin1String("tool")) { // tool:size - a tool of the palette
            const QString tool = action.mid(colon + 1).trimmed();
            delay += 350;
            QTimer::singleShot(delay, viewport,
                               [window, tool] { window->setProperty("tool", tool); });
            continue;
        }
        if (kind == QLatin1String("distance")) {
            // distance:left,5 - type 5 into the distance measure on that side (left | right |
            // front | back) of the chosen body, as a click on the number and Enter would.
            const QStringList spec = action.mid(colon + 1).split(QLatin1Char(','));
            delay += 350;
            QTimer::singleShot(delay, viewport, [distances, spec] {
                const int index = distances->indexOfSide(spec.value(0).trimmed());
                if (index < 0 || !distances->setDistance(index, spec.value(1).toDouble())) {
                    qWarning().noquote() << QStringLiteral("no distance %1 to set to %2")
                                                .arg(spec.value(0), spec.value(1));
                    g_simulationFailure = 4;
                }
            });
            continue;
        }
        if (action.trimmed() == QLatin1String("selectall")) { // all bodies chosen
            delay += 350;
            QTimer::singleShot(delay, viewport, [selection] { selection->chooseAllBodies(); });
            continue;
        }
        if (kind == QLatin1String("step")) { // step:1 - choose that step of the recipe (from 0)
            const int step = action.mid(colon + 1).trimmed().toInt();
            delay += 350;
            QTimer::singleShot(delay, viewport, [selection, model, step] {
                selection->chooseBody(model->bodyIndexOf(model->bodyOfStep(step)), false);
            });
            continue;
        }
        if (kind == QLatin1String("align")) {
            // align:x,min - click the point of the tool Move that lines the chosen bodies up
            // along x (or y, z): min | centre | max.
            const QStringList spec = action.mid(colon + 1).split(QLatin1Char(','));
            delay += 350;
            QTimer::singleShot(delay, viewport, [align, spec] {
                const int index = align->indexOf(spec.value(0).trimmed(), spec.value(1).trimmed());
                if (index < 0 || !align->align(index)) {
                    qWarning().noquote()
                        << QStringLiteral("no point to align %1 on %2 (or nothing moves)")
                               .arg(spec.value(0), spec.value(1));
                    g_simulationFailure = 4;
                }
            });
            continue;
        }
        if (kind == QLatin1String("menu")) {
            // menu:dropToBed - the same as choosing that entry of the menus (duplicate, copy,
            // paste, dropToBed, seatOnFace ...).
            const QString id = action.mid(colon + 1).trimmed();
            delay += 350;
            QTimer::singleShot(delay, viewport, [window, id] {
                QMetaObject::invokeMethod(window, "run", Q_ARG(QVariant, id));
            });
            continue;
        }
        if (kind ==
            QLatin1String("marker")) { // marker:2 - the time marker behind 2 steps (-1: end)
            const int count = action.mid(colon + 1).trimmed().toInt();
            delay += 350;
            QTimer::singleShot(delay, viewport, [model, count] { model->moveMarker(count); });
            continue;
        }
        if (kind == QLatin1String("rowdouble") || kind == QLatin1String("rowediting")) {
            // rowdouble:1 - a double click on the row of step 1 of the recipe (real mouse events);
            // rowediting:1 - that row is being renamed now (otherwise the program ends with an
            // error). "body:1" names a row of the body list instead.
            const QString which = action.mid(colon + 1).trimmed();
            const QString name = which.startsWith(QLatin1Char('b'))
                                     ? QStringLiteral("bodyRow_") + which.mid(1)
                                     : QStringLiteral("recipeRow_") + which;
            const auto findRow = [window, name] {
                std::function<QQuickItem*(QQuickItem*)> find = [&](QQuickItem* item) {
                    if (item->objectName() == name) {
                        return item;
                    }
                    for (QQuickItem* child : item->childItems()) {
                        if (QQuickItem* found = find(child)) {
                            return found;
                        }
                    }
                    return static_cast<QQuickItem*>(nullptr);
                };
                return find(window->contentItem());
            };
            if (kind == QLatin1String("rowediting")) {
                delay += 350;
                QTimer::singleShot(delay, viewport, [findRow, name] {
                    QQuickItem* row = findRow();
                    if (row == nullptr || !row->property("editing").toBool()) {
                        qWarning().noquote()
                            << QStringLiteral("expected %1 to be renamed now").arg(name);
                        g_simulationFailure = 5;
                    }
                });
                continue;
            }
            for (const QEvent::Type type : {QEvent::MouseButtonPress, QEvent::MouseButtonRelease,
                                            QEvent::MouseButtonPress, QEvent::MouseButtonRelease}) {
                delay += type == QEvent::MouseButtonPress ? 120 : 40;
                QTimer::singleShot(delay, viewport, [window, findRow, type] {
                    QQuickItem* row = findRow();
                    if (row == nullptr) {
                        return;
                    }
                    const QPointF scene =
                        row->mapToScene(QPointF(row->width() / 3, row->height() / 2));
                    const bool press = type == QEvent::MouseButtonPress;
                    QMouseEvent event(type, scene, window->mapToGlobal(scene.toPoint()),
                                      Qt::LeftButton, press ? Qt::LeftButton : Qt::NoButton,
                                      Qt::NoModifier);
                    QCoreApplication::sendEvent(window, &event);
                });
            }
            continue;
        }
        if (kind == QLatin1String("clickitem") || kind == QLatin1String("rightclickitem")) {
            // clickitem:menu_rename - a click on the item of the interface with that object name
            // (rightclickitem: the right button): real mouse events through the window.
            const QString name = action.mid(colon + 1).trimmed();
            const Qt::MouseButton button =
                kind == QLatin1String("clickitem") ? Qt::LeftButton : Qt::RightButton;
            for (const QEvent::Type type : {QEvent::MouseButtonPress, QEvent::MouseButtonRelease}) {
                delay += type == QEvent::MouseButtonPress ? 350 : 60;
                QTimer::singleShot(delay, viewport, [window, name, type, button] {
                    std::function<QQuickItem*(QQuickItem*)> find = [&](QQuickItem* item) {
                        if (item->objectName() == name && item->isVisible()) {
                            return item;
                        }
                        for (QQuickItem* child : item->childItems()) {
                            if (QQuickItem* found = find(child)) {
                                return found;
                            }
                        }
                        return static_cast<QQuickItem*>(nullptr);
                    };
                    QQuickItem* item = find(window->contentItem());
                    if (item == nullptr) {
                        qWarning().noquote() << QStringLiteral("no item %1 to click").arg(name);
                        g_simulationFailure = 5;
                        return;
                    }
                    const QPointF scene =
                        item->mapToScene(QPointF(item->width() / 3, item->height() / 2));
                    const bool press = type == QEvent::MouseButtonPress;
                    QMouseEvent event(type, scene, window->mapToGlobal(scene.toPoint()), button,
                                      press ? button : Qt::NoButton, Qt::NoModifier);
                    QCoreApplication::sendEvent(window, &event);
                });
            }
            continue;
        }
        if (kind == QLatin1String("markerdrag")) {
            // markerdrag:-150,8 - press on the time marker at the end of the recipe, pull it 150
            // view pixels up in 8 moves, let go: real mouse events, as a hand would do.
            const QStringList spec = action.mid(colon + 1).split(QLatin1Char(','));
            const int steps = spec.size() >= 2 ? std::max(1, spec[1].toInt()) : 8;
            delay = pressAndPull(
                window, viewport, delay,
                [window, viewport] {
                    // Visual children, not QObject children: what is on screen under the window.
                    std::function<QQuickItem*(QQuickItem*)> find = [&](QQuickItem* item) {
                        if (item->objectName() == QLatin1String("markerEnd")) {
                            return item;
                        }
                        for (QQuickItem* child : item->childItems()) {
                            if (QQuickItem* found = find(child)) {
                                return found;
                            }
                        }
                        return static_cast<QQuickItem*>(nullptr);
                    };
                    QQuickItem* line = find(window->contentItem());
                    if (line == nullptr) {
                        return QPointF();
                    }
                    return viewport->mapFromScene(
                        line->mapToScene(QPointF(line->width() / 2, line->height() / 2)));
                },
                QPointF(0.0, spec.value(0).toDouble()), steps);
            continue;
        }
        if (kind == QLatin1String("markerset")) { // markerset - the time marker is pulled back
            delay += 350;
            checkAfter(delay, viewport, model, [model] {
                if (model->marker() < 0) {
                    qWarning().noquote()
                        << QStringLiteral("expected the time marker to be pulled back");
                    g_simulationFailure = 5;
                }
            });
            continue;
        }
        if (kind == QLatin1String("expectpref")) {
            // expectpref:mouseStyle,blender - the setting has this value (settings are kept
            // between starts: the next start must find them).
            const QStringList spec = action.mid(colon + 1).split(QLatin1Char(','));
            delay += 350;
            QTimer::singleShot(delay, viewport, [preferences, spec] {
                const QString found = preferences->property(spec.value(0).toUtf8()).toString();
                if (found != spec.value(1)) {
                    qWarning().noquote() << QStringLiteral("expected the setting %1 = %2; found %3")
                                                .arg(spec.value(0), spec.value(1), found);
                    g_simulationFailure = 7;
                }
            });
            continue;
        }
        if (kind == QLatin1String("example")) { // example:keychain - opened from the start page
            const QString name = action.mid(colon + 1).trimmed();
            delay += 600;
            QTimer::singleShot(delay, viewport, [files, name] {
                if (!files->openExample(name)) {
                    g_simulationFailure = 6;
                }
            });
            continue;
        }
        if (kind == QLatin1String("expectmessage")) {
            // expectmessage:parameterOutOfRange,width,bigger than zero - what the message
            // catalogue says for that problem (a part of it).
            const QStringList spec = action.mid(colon + 1).split(QLatin1Char(','));
            delay += 400;
            QTimer::singleShot(delay, viewport, [window, spec] {
                const QVariantMap problem{{QStringLiteral("code"), spec.value(0)},
                                          {QStringLiteral("field"), spec.value(1)}};
                QVariant text;
                QMetaObject::invokeMethod(window, "problemText", Q_RETURN_ARG(QVariant, text),
                                          Q_ARG(QVariant, problem), Q_ARG(QVariant, QVariantMap{}));
                if (!text.toString().contains(spec.value(2))) {
                    qWarning().noquote()
                        << QStringLiteral("the message for %1 says \"%2\", expected %3")
                               .arg(spec.value(0), text.toString(), spec.value(2));
                    g_simulationFailure = 10;
                }
            });
            continue;
        }
        if (kind == QLatin1String("expecttab")) {
            // expecttab:0,2 - the tab shown is number 0 (counted from 0) of 2 open tabs.
            const QStringList spec = action.mid(colon + 1).split(QLatin1Char(','));
            delay += 600;
            QTimer::singleShot(delay, viewport, [files, spec] {
                if (files->currentTab() != spec.value(0).toInt() ||
                    files->tabs().size() != spec.value(1).toInt()) {
                    qWarning().noquote()
                        << QStringLiteral("expected tab %1 of %2; found tab %3 of %4")
                               .arg(spec.value(0), spec.value(1))
                               .arg(files->currentTab())
                               .arg(files->tabs().size());
                    g_simulationFailure = 10;
                }
            });
            continue;
        }
        if (kind == QLatin1String("expectbubble")) {
            // expectbubble:Breitee - the value bubble is still open and explains the typed text
            // (the line under it says this, a part of it).
            const QString part = action.mid(colon + 1).trimmed();
            delay += 400;
            QTimer::singleShot(delay, viewport, [window, part] {
                const QObject* layer = window->findChild<QObject*>(QStringLiteral("handleLayer"));
                const QString shown =
                    layer != nullptr ? layer->property("bubbleProblem").toString() : QString();
                if (!shown.contains(part)) {
                    qWarning().noquote()
                        << QStringLiteral("the bubble says \"%1\", expected %2").arg(shown, part);
                    g_simulationFailure = 10;
                }
            });
            continue;
        }
        if (kind == QLatin1String("expectundo")) {
            // expectundo:addFeature - the next step to undo is of this kind (the list "Recent
            // actions"): a refused input must not have added one.
            const QString key = action.mid(colon + 1).trimmed();
            delay += 400;
            QTimer::singleShot(delay, viewport, [model, key] {
                const QString found = model->nextUndo()[QStringLiteral("key")].toString();
                if (found != key) {
                    qWarning().noquote()
                        << QStringLiteral("the next undo is \"%1\", expected %2").arg(found, key);
                    g_simulationFailure = 10;
                }
            });
            continue;
        }
        if (kind == QLatin1String("expecthint")) {
            // expecthint:Start with a shape - the hint line says this (a part of it).
            const QString part = action.mid(colon + 1).trimmed();
            delay += 400;
            QTimer::singleShot(delay, viewport, [window, part] {
                const QString hint = window->property("currentHint").toString();
                if (!hint.contains(part)) {
                    qWarning().noquote()
                        << QStringLiteral("the hint line says \"%1\", expected %2").arg(hint, part);
                    g_simulationFailure = 9;
                }
            });
            continue;
        }
        if (kind == QLatin1String("search") || kind == QLatin1String("runcommand")) {
            // search:runde ecken,soon.fillet - the best match for the text is this command
            // (looked up through the real catalogue of the program).
            // runcommand:tool.rotate - does that command, as a click in the list would.
            const QStringList spec = action.mid(colon + 1).split(QLatin1Char(','));
            delay += 400;
            QTimer::singleShot(delay, viewport, [window, kind, spec] {
                QVariant found;
                const bool search = kind == QLatin1String("search");
                QMetaObject::invokeMethod(window, search ? "topCommand" : "runCommand",
                                          Q_RETURN_ARG(QVariant, found),
                                          Q_ARG(QVariant, QVariant(spec.value(0))));
                const bool ok = search ? found.toString() == spec.value(1) : true;
                if (!ok) {
                    qWarning().noquote()
                        << QStringLiteral("the search %1 found %2, expected %3")
                               .arg(spec.value(0), found.toString(), spec.value(1));
                    g_simulationFailure = 8;
                }
            });
            continue;
        }
        if (kind == QLatin1String("setpref") || kind == QLatin1String("expecttr")) {
            // setpref:language,de - a setting changed as the settings page does it (no restart).
            // expecttr:SettingsPage,Settings,Einstellungen - this text of the interface reads so
            // now (the language was switched while the program ran).
            const QStringList spec = action.mid(colon + 1).split(QLatin1Char(','));
            delay += 500;
            QTimer::singleShot(delay, viewport, [=] {
                bool ok = true;
                if (kind == QLatin1String("setpref")) {
                    ok = preferences->setProperty(spec.value(0).toUtf8(), spec.value(1));
                } else {
                    const QString found = QCoreApplication::translate(
                        spec.value(0).toUtf8().constData(), spec.value(1).toUtf8().constData());
                    ok = found == spec.value(2);
                    if (!ok) {
                        qWarning().noquote() << QStringLiteral("expected %1 to read %2; found %3")
                                                    .arg(spec.value(1), spec.value(2), found);
                    }
                }
                if (!ok) {
                    g_simulationFailure = 7;
                }
            });
            continue;
        }
        // --- files (AP-1.10): the whole path through the real program, for the tests ---------
        if (kind == QLatin1String("save") || kind == QLatin1String("open") ||
            kind == QLatin1String("autosave") || kind == QLatin1String("crash") ||
            kind == QLatin1String("rescue") || kind == QLatin1String("expectmodified") ||
            kind == QLatin1String("export")) {
            const QString argument = action.mid(colon + 1);
            delay += 600; // what came before (a shape put down) is calculated and shown
            QTimer::singleShot(delay, viewport, [=] {
                bool ok = true;
                if (kind == QLatin1String("save")) { // save:/path/file.fission
                    ok = files->saveTo(QUrl::fromLocalFile(argument));
                    files->waitForSave();
                    ok = ok && !files->modified();
                } else if (kind == QLatin1String("open")) { // open:/path/file.fission
                    ok = files->openPath(argument);
                } else if (kind == QLatin1String("autosave")) {
                    ok = files->autosaveNow(); // what the timer does, at once
                } else if (kind == QLatin1String("crash")) {
                    ::kill(::getpid(), SIGKILL);              // no clean end: the same as kill -9
                } else if (kind == QLatin1String("rescue")) { // rescue:restore
                    QString id;
                    const QMetaObject::Connection offered =
                        QObject::connect(files, &fission::ui::FileController::rescuedWork, files,
                                         [&id](const QString& found) { id = found; });
                    files->checkRecovery();
                    QObject::disconnect(offered);
                    ok = !id.isEmpty() && files->restoreRescued(id);
                } else if (kind == QLatin1String("expectmodified")) { // expectmodified:1
                    ok = files->modified() == (argument.toInt() != 0);
                } else if (kind == QLatin1String("export")) { // export:/path/file.3mf (or .stl)
                    model->waitForCalculation();
                    QString code = QStringLiteral("none");
                    QEventLoop wait;
                    QObject::connect(exporter, &fission::ui::ExportController::finished, &wait,
                                     [&](const QString& result) {
                                         code = result;
                                         wait.quit();
                                     });
                    QTimer::singleShot(30000, &wait, &QEventLoop::quit);
                    const bool stl = argument.endsWith(QLatin1String(".stl"));
                    exporter->start(stl ? QStringLiteral("stl") : QStringLiteral("3mf"),
                                    QStringLiteral("normal"), {}, QUrl::fromLocalFile(argument),
                                    {});
                    wait.exec();
                    ok = code.isEmpty();
                }
                if (!ok) {
                    qWarning().noquote()
                        << QStringLiteral("the action %1 did not work").arg(action);
                    g_simulationFailure = 6;
                }
            });
            continue;
        }
        if (kind == QLatin1String("key")) { // key:Right - a key pressed and let go
            const QKeySequence sequence = QKeySequence::fromString(action.mid(colon + 1).trimmed());
            delay += 350;
            if (!sequence.isEmpty()) {
                const QKeyCombination key = sequence[0];
                QTimer::singleShot(delay, viewport, [window, viewport, key] {
                    // Shortcuts only answer in the window that is active; right after the start
                    // it may not be yet, so it is asked to be, and given a moment. A window that
                    // is active is left alone: asking again would move the focus out of a field.
                    const bool wasActive = window->isActive();
                    if (!wasActive) {
                        window->requestActivate();
                    }
                    QTimer::singleShot(wasActive ? 0 : 150, viewport, [window, key] {
                        QKeyEvent press(QEvent::KeyPress, key.key(), key.keyboardModifiers());
                        QCoreApplication::sendEvent(window, &press);
                        QKeyEvent release(QEvent::KeyRelease, key.key(), key.keyboardModifiers());
                        QCoreApplication::sendEvent(window, &release);
                    });
                });
            }
            continue;
        }
        if (kind == QLatin1String("focus")) { // focus:width - the cursor goes into that field
            const QString field = action.mid(colon + 1).trimmed();
            delay += 350;
            QTimer::singleShot(delay, viewport, [window, field] {
                // Fields made by a Repeater are children of the picture, not of the object tree.
                const QString name = QStringLiteral("field_") + field;
                const auto find = [&name](auto&& self, QQuickItem* parent) -> QQuickItem* {
                    for (QQuickItem* child : parent->childItems()) {
                        if (child->objectName() == name) {
                            return child;
                        }
                        if (QQuickItem* found = self(self, child)) {
                            return found;
                        }
                    }
                    return nullptr;
                };
                QQuickItem* item = find(find, window->contentItem());
                if (item == nullptr) {
                    qWarning().noquote() << QStringLiteral("no field %1 to focus").arg(field);
                    g_simulationFailure = 4;
                    return;
                }
                QMetaObject::invokeMethod(item, "takeFocus");
            });
            continue;
        }
        if (kind == QLatin1String("near")) {
            // near:x,12 - the chosen step has exactly this value (to a thousandth of a mm).
            const QStringList spec = action.mid(colon + 1).split(QLatin1Char(','));
            delay += 350;
            checkAfter(delay, viewport, model, [model, gizmo, rotate, scale, spec] {
                double value = std::numeric_limits<double>::quiet_NaN();
                for (const QVariant& entry : model->stepValues()) {
                    if (entry.toMap()["name"].toString() == spec.value(0)) {
                        value = entry.toMap()["value"].toDouble();
                    }
                }
                if (!(std::abs(value - spec.value(1).toDouble()) < 1e-3) || gizmo->editing() ||
                    rotate->editing() || scale->editing()) {
                    qWarning().noquote()
                        << QStringLiteral("expected %1 = %2 and nothing being moved; found %3 "
                                          "(moving: %4)")
                               .arg(spec.value(0), spec.value(1))
                               .arg(value)
                               .arg(gizmo->editing());
                    g_simulationFailure = 4;
                }
            });
            continue;
        }
        if (kind == QLatin1String("least")) {
            // least:rotX,15 - the chosen step has at least this much of the value (by size, so a
            // turn the other way counts too), and nothing is being moved or turned.
            const QStringList spec = action.mid(colon + 1).split(QLatin1Char(','));
            delay += 350;
            checkAfter(delay, viewport, model, [model, gizmo, rotate, scale, spec] {
                double value = 0.0;
                for (const QVariant& entry : model->stepValues()) {
                    if (entry.toMap()["name"].toString() == spec.value(0)) {
                        value = entry.toMap()["value"].toDouble();
                    }
                }
                if (std::abs(value) < spec.value(1).toDouble() || gizmo->editing() ||
                    rotate->editing() || scale->editing()) {
                    qWarning().noquote()
                        << QStringLiteral("expected |%1| >= %2 and nothing being moved; found %3")
                               .arg(spec.value(0), spec.value(1))
                               .arg(value);
                    g_simulationFailure = 4;
                }
            });
            continue;
        }
        if (kind == QLatin1String("value")) {
            // value:height,1.5 - a number or formula for a value of the chosen step, as if it had
            // been typed into the field of the panel.
            const QStringList pair = action.mid(colon + 1).split(QLatin1Char(','));
            delay += 350;
            QTimer::singleShot(delay, viewport, [model, pair] {
                if (pair.size() != 2 ||
                    !model->setStepValue(model->currentStep(), pair[0], pair[1])) {
                    qWarning().noquote()
                        << QStringLiteral("value %1 not accepted").arg(pair.join(QLatin1Char('=')));
                    g_simulationFailure = 4;
                }
            });
            continue;
        }
        if (kind == QLatin1String("color")) {
            // color:#e53935 - the body of the chosen step gets this colour.
            const QString color = action.mid(colon + 1).trimmed();
            delay += 350;
            QTimer::singleShot(delay, viewport, [model, color] {
                const int body = model->bodyIndexOf(model->bodyOfStep(model->currentStep()));
                if (!model->setBodiesColor({body}, color)) {
                    qWarning().noquote() << QStringLiteral("colour %1 not accepted").arg(color);
                    g_simulationFailure = 4;
                }
            });
            continue;
        }
        if (kind == QLatin1String("set")) { // set:style,boldItalic - words of the chosen step
            const QStringList pair = action.mid(colon + 1).split(QLatin1Char(','));
            delay += 350;
            if (pair.size() == 2 && model != nullptr) {
                QTimer::singleShot(delay, viewport, [model, pair] {
                    model->setStepText(model->currentStep(), pair[0], pair[1]);
                });
            }
            continue;
        }
        QStringList parts = action.mid(colon + 1).split(QLatin1Char(','));
        if (kind == QLatin1String("type")) { // type:50,tab,20,enter - what the keys would do
            delay += 350;
            QTimer::singleShot(delay, viewport, [window, gizmo, rotate, scale, parts] {
                // While a body is moved, the digits are for the distance, while it is turned for
                // the angle; else for the sizes.
                const QString layerName = gizmo->editing()    ? QStringLiteral("gizmoLayer")
                                          : rotate->editing() ? QStringLiteral("rotateLayer")
                                          : scale->editing()  ? QStringLiteral("scaleLayer")
                                                              : QStringLiteral("handleLayer");
                if (QObject* layer = window->findChild<QObject*>(layerName)) {
                    QMetaObject::invokeMethod(
                        layer, "simulate", Q_ARG(QVariant, QVariant(QVariant::fromValue(parts))));
                }
            });
            continue;
        }
        if (kind.startsWith(QLatin1String("drop")) || kind.startsWith(QLatin1String("put"))) {
            shape = parts.takeFirst().trimmed();
        }
        for (const QString& number : parts) {
            numbers.push_back(number.toDouble());
        }
        delay += 350;
        QTimer::singleShot(delay, viewport, [viewport, placement, kind, shape, numbers] {
            using fission::ui::Viewport3D;
            // The same calls the gallery makes when a tile is dragged or clicked.
            if (kind == QLatin1String("drop") || kind == QLatin1String("drophole")) {
                // A third number other than 0 holds the Alt key: the shape stays upright.
                const bool upright = numbers.size() >= 3 && numbers[2] != 0.0;
                if (numbers.size() >= 2 &&
                    placement->begin(shape, kind == QLatin1String("drophole"), numbers[0],
                                     numbers[1], upright)) {
                    // The size of the shape is known a moment after it appeared; from then on it
                    // catches on other bodies, so the pointer moves once more before it lets go
                    // (as a hand would).
                    QTimer::singleShot(250, viewport, [placement, numbers, upright] {
                        placement->moveTo(numbers[0], numbers[1], upright);
                        placement->finish();
                    });
                }
                return;
            }
            if (kind == QLatin1String("put") || kind == QLatin1String("puthole")) {
                placement->add(shape, kind == QLatin1String("puthole"));
                return;
            }
            if (numbers.size() >= 2) {
                const QPointF at(numbers[0], numbers[1]);
                if (kind == QLatin1String("hover")) {
                    viewport->hoverAt(at.x(), at.y());
                } else if (kind == QLatin1String("click")) {
                    viewport->requestPick(Viewport3D::PickIntent::click, at, false);
                } else if (kind == QLatin1String("shiftclick")) {
                    viewport->requestPick(Viewport3D::PickIntent::click, at, true);
                } else if (kind == QLatin1String("doubleclick")) {
                    viewport->requestPick(Viewport3D::PickIntent::doubleClick, at, false);
                } else if (kind == QLatin1String("frame") && numbers.size() >= 4) {
                    viewport->requestRegionPick(
                        QRectF(at, QPointF(numbers[2], numbers[3])).normalized(), false);
                }
            }
        });
    }
    QTimer::singleShot(delay + 500, viewport, std::move(done));
}

// Part of --benchmark: how long the 3D view needs to say what is under the pointer (AP-1.4,
// "hover feedback under 16 ms"). The pointer moves over the scene; every answer starts the next
// question after 8 ms, about as often as a mouse reports. The time runs from the question until
// the answer is in the interface thread.
void measurePicks(fission::ui::Viewport3D* viewport) {
    constexpr int kSamples = 150;
    struct State {
        std::vector<double> milliseconds;
        std::vector<double> work; ///< the graphics card's part of it
        std::chrono::steady_clock::time_point asked;
        int hits = 0;
    };
    auto state = std::make_shared<State>();
    auto ask = [viewport, state] {
        const int n = static_cast<int>(state->milliseconds.size());
        state->asked = std::chrono::steady_clock::now();
        const double cx = 0.5 * viewport->width();
        const double cy = 0.5 * viewport->height();
        viewport->hoverAt(cx - 200.0 + (n * 37) % 400, cy - 150.0 + (n * 53) % 300);
    };
    QObject::connect(viewport, &fission::ui::Viewport3D::pickTimed, viewport,
                     [state](double ms) { state->work.push_back(ms); });
    QObject::connect(
        viewport, &fission::ui::Viewport3D::picked, viewport,
        [viewport, state, ask](int, bool, const QString& body, int, int) {
            state->milliseconds.push_back(std::chrono::duration<double, std::milli>(
                                              std::chrono::steady_clock::now() - state->asked)
                                              .count());
            state->hits += body.isEmpty() ? 0 : 1;
            if (static_cast<int>(state->milliseconds.size()) < kSamples) {
                QTimer::singleShot(8, viewport, ask);
                return;
            }
            std::vector<double> sorted = state->milliseconds;
            std::sort(sorted.begin(), sorted.end());
            double sum = 0.0;
            for (const double v : sorted) {
                sum += v;
            }
            double workSum = 0.0;
            double workMax = 0.0;
            for (const double v : state->work) {
                workSum += v;
                workMax = std::max(workMax, v);
            }
            const double workAvg = state->work.empty() ? 0.0 : workSum / state->work.size();
            std::printf("pick samples=%d hits=%d answer_ms_avg=%.2f answer_ms_p95=%.2f "
                        "answer_ms_max=%.2f card_ms_avg=%.2f card_ms_max=%.2f\n",
                        kSamples, state->hits, sum / kSamples,
                        sorted[static_cast<std::size_t>(0.95 * (kSamples - 1))], sorted.back(),
                        workAvg, workMax);
            std::fflush(stdout);
            QCoreApplication::quit();
        });
    ask();
}

// Developer option --benchmark: a scene of the wanted size, the camera turns by itself, and the
// time per frame is printed (AP-1.3: the viewport must hold 60 frames per second at 2 million
// triangles). The graphics card's own time is the number that counts: the screen refresh
// limits how fast frames are shown, not how fast they could be made.
void runBenchmark(QQuickWindow* window, fission::ui::Viewport3D* viewport,
                  const QMetaObject::Connection& sceneConnection, std::size_t triangles) {
    constexpr int kWarmUpFrames = 60;
    constexpr int kMeasuredFrames = 300; // frame swaps; every one asks for the next frame
    struct State {
        int frames = 0;
        bool orbiting = true;
        std::chrono::steady_clock::time_point started;
    };
    auto state = std::make_shared<State>();
    QObject::connect(
        viewport, &fission::ui::Viewport3D::firstFrameShown, viewport,
        [window, viewport, sceneConnection, triangles, state] {
            QObject::disconnect(sceneConnection); // the document's scene is not wanted now
            viewport->setProperty("animated", false);
            viewport->setScene(fission::render::makeBenchmarkScene(triangles));
            viewport->frameAll();
            viewport->orbitBy(0.0, 0.0);
            QObject::connect(window, &QQuickWindow::frameSwapped, viewport, [viewport, state] {
                ++state->frames;
                if (state->frames == kWarmUpFrames) {
                    viewport->startFrameStatistics();
                    state->started = std::chrono::steady_clock::now();
                } else if (state->frames == kWarmUpFrames + kMeasuredFrames) {
                    const double wall = std::chrono::duration<double, std::milli>(
                                            std::chrono::steady_clock::now() - state->started)
                                            .count();
                    const auto stats = viewport->finishFrameStatistics();
                    std::printf("benchmark triangles=%d soft_shadows=%s backend=%s frames=%d "
                                "gpu_ms_avg=%.2f gpu_ms_p95=%.2f cpu_ms_avg=%.2f "
                                "frame_interval_ms=%.2f\n",
                                viewport->triangleCount(),
                                viewport->ambientOcclusion() ? "on" : "off",
                                viewport->backendName().toUtf8().constData(), stats.frames,
                                stats.gpuAverageMs, stats.gpuP95Ms, stats.cpuAverageMs,
                                stats.frames > 0 ? wall / stats.frames : 0.0);
                    std::fflush(stdout);
                    state->orbiting = false;
                    measurePicks(viewport);
                    return;
                }
                if (state->orbiting) {
                    viewport->orbitBy(1.5, 0.0); // asks for the next frame
                }
            });
        });
}

void loadFonts() {
    // Fallback if the bundled fonts cannot be loaded (docs/08-gestaltung.md §3).
    QFont::insertSubstitutions(QStringLiteral("IBM Plex Sans"),
                               fission::ui::Theme::fontFamilies().mid(1));
    QFont::insertSubstitutions(QStringLiteral("IBM Plex Mono"),
                               fission::ui::Theme::monoFamilies().mid(1));
    // IBM Plex (SIL Open Font License 1.1) ships with the program: nothing is downloaded.
    for (const char* file :
         {"IBMPlexSans-Regular", "IBMPlexSans-Medium", "IBMPlexSans-SemiBold", "IBMPlexSans-Bold",
          "IBMPlexMono-Regular", "IBMPlexMono-Medium", "IBMPlexMono-SemiBold"}) {
        const QString path = QStringLiteral(":/fission/fonts/%1.ttf").arg(QLatin1String(file));
        if (QFontDatabase::addApplicationFont(path) < 0) {
            spdlog::warn("font {} could not be loaded; the system font is used", file);
        }
    }
}

// The geometry kernel reads fonts from files, not from the program's resources. The Plex
// fonts that lettering offers are copied (once, and again when they change) to the cache folder.
std::vector<std::string> bundledFontFiles() {
    std::vector<std::string> paths;
    const QString folder =
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/fonts");
    if (!QDir().mkpath(folder)) {
        spdlog::warn("the font folder {} could not be made; lettering uses system fonts only",
                     folder.toStdString());
        return paths;
    }
    for (const char* file : {"IBMPlexSans-Regular", "IBMPlexSans-Bold", "IBMPlexSans-Italic",
                             "IBMPlexSans-BoldItalic", "IBMPlexMono-Regular", "IBMPlexMono-Bold",
                             "IBMPlexMono-Italic", "IBMPlexMono-BoldItalic"}) {
        QFile source(QStringLiteral(":/fission/fonts/%1.ttf").arg(QLatin1String(file)));
        const QString target = QStringLiteral("%1/%2.ttf").arg(folder, QLatin1String(file));
        if (!source.open(QIODevice::ReadOnly)) {
            continue;
        }
        const QByteArray data = source.readAll();
        if (QFileInfo(target).size() != data.size()) {
            QSaveFile out(target); // all or nothing: never a half-written font
            if (!out.open(QIODevice::WriteOnly) || out.write(data) != data.size() ||
                !out.commit()) {
                spdlog::warn("font {} could not be copied for lettering", file);
                continue;
            }
        }
        paths.push_back(target.toStdString());
    }
    return paths;
}

} // namespace

// macOS hands a double-clicked document to the running program (or the one that is starting) as
// an event, not as an argument. The path is kept until the window exists.
class FileOpenFilter final : public QObject {
public:
    std::function<void(const QString&)> open;
    QString pending;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() == QEvent::FileOpen) {
            const QString path = static_cast<QFileOpenEvent*>(event)->file();
            if (open) {
                open(path);
            } else {
                pending = path;
            }
            return true;
        }
        return QObject::eventFilter(watched, event);
    }
};

// The small picture for the file (preview.png): the window, 320 px wide.
QByteArray previewPng(QQuickWindow* window) {
    QImage frame = window->grabWindow();
    if (frame.isNull()) {
        return {};
    }
    frame = frame.scaledToWidth(320, Qt::SmoothTransformation);
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    return frame.save(&buffer, "PNG") ? bytes : QByteArray();
}

int main(int argc, char* argv[]) {
    // Process start (by the operating system) to the first drawn frame is the start time.
    const auto mainEntered = std::chrono::steady_clock::now();
    const double ageAtMain = fission::services::processAgeMilliseconds().value_or(0.0);

    const CommandLine options = parseCommandLine(argc, argv);
    const auto logFile = fission::services::startLogging("fission3d");

    // The look is ours (docs/08-gestaltung.md), not that of the platform's controls.
    qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("Fission3D"));
    QGuiApplication::setApplicationVersion(versionText());
    // Lets Wayland and the desktop match the window to the installed .desktop file and icon.
    QGuiApplication::setDesktopFileName(QStringLiteral(FISSION_APP_ID));

    spdlog::info("Fission3D {} starting (Qt {}, platform {})", versionText().toStdString(),
                 qVersion(), QGuiApplication::platformName().toStdString());
    loadFonts();

    fission::ui::Theme theme;
    fission::ui::AppInfo info;
    info.setVersion(toQString(fission::core::versionString()));
    info.setQtVersion(QString::fromLatin1(qVersion()));
    info.setKernelVersion(QString::fromStdString(fission::geo::occt::kernelVersion()));
    info.setLogFile(QString::fromStdString(logFile.string()));
    fission::ui::AppOptions appOptions;
    appOptions.focusMode = options.focus;
    appOptions.reduceMotion = options.reduceMotion;
    appOptions.openMenu = options.openMenu;
    appOptions.showHistory = options.showHistory;
    appOptions.showShapes = options.showShapes;
    appOptions.camera = options.camera;
    appOptions.rightTab = options.rightTab;
    appOptions.softShadows = options.softShadows;
    appOptions.parked = qEnvironmentVariableIsSet("FISSION_PARK_WINDOW");
    // Test runs (a picture of the window, a parked window) keep their rescue files in a folder
    // of their own: they must not leave work behind that the next real start would offer.
    // FISSION_DATA_DIR names that folder (tests that start the program twice share it).
    std::unique_ptr<QTemporaryDir> testRescueFolder;
    QString dataFolder = qEnvironmentVariable("FISSION_DATA_DIR");
    if (dataFolder.isEmpty() && (!options.smokeTestPath.isEmpty() || appOptions.parked)) {
        testRescueFolder = std::make_unique<QTemporaryDir>();
        dataFolder = testRescueFolder->path();
    }
    fission::services::Settings settings(
        dataFolder.isEmpty() ? QString() : dataFolder + QStringLiteral("/settings.ini"));
    fission::ui::Numbers numbers;

    // The document: the one box of the start, calculated before the first frame.
    fission::geo::occt::KernelOptions kernelOptions;
    kernelOptions.fontFiles = bundledFontFiles();
    std::shared_ptr<const fission::geo::GeometryKernel> kernel =
        fission::geo::occt::makeKernel(kernelOptions);
    fission::ui::DocumentModel documentModel(kernel);
    documentModel.setNumbers(&numbers);
    documentModel.addDemoBox();
    documentModel.waitForCalculation();

    QQmlApplicationEngine engine;
    fission::services::RecentFiles recentFiles(
        dataFolder.isEmpty() ? QString() : dataFolder + QStringLiteral("/recent.ini"));
    fission::services::RecoveryStore recovery(
        dataFolder.isEmpty() ? QString() : dataFolder + QStringLiteral("/recovery"));
    recovery.beginSession();
    fission::services::Autosave autosave; // every 60 seconds and after every 20th command
    fission::ui::FileController files(&documentModel, &recentFiles, &recovery, &autosave);
    // Whatever way the program ends, nothing the person made is lost.
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &files,
                     [&files] { files.finishSession(); });
    qmlRegisterSingletonInstance("Fission3D.Ui", 1, 0, "Files", &files);
    fission::ui::ExportController exporter(&documentModel, kernel);
    qmlRegisterSingletonInstance("Fission3D.Ui", 1, 0, "Exporter", &exporter);
    FileOpenFilter fileOpenFilter;
    app.installEventFilter(&fileOpenFilter);
    fission::ui::Language language(&engine);
    // What the person set up is applied; the command line (for tests and pictures) wins over it
    // without being written down.
    fission::ui::Preferences preferences(&settings, &theme, &language);
    // The assistant is for people; test runs (pictures, parked windows) skip it so that the
    // pictures show the plain window, unless a test asks for it with FISSION_FIRST_RUN=1.
    const bool testRun = !options.smokeTestPath.isEmpty() || appOptions.parked;
    // The start page opens with the program unless a file was asked for.
    appOptions.startPage =
        options.file.isEmpty() && (!testRun || qEnvironmentVariableIsSet("FISSION_START_PAGE"));
    appOptions.firstRun =
        !preferences.firstRunDone() && (!testRun || qEnvironmentVariableIsSet("FISSION_FIRST_RUN"));
    preferences.applyToInterface(!options.language.isEmpty(), !options.theme.isEmpty());
    if (!options.language.isEmpty()) {
        language.setCode(options.language);
    }
    if (!options.theme.isEmpty()) {
        theme.setMode(options.theme);
    }
    qmlRegisterSingletonInstance("Fission3D.Ui", 1, 0, "Preferences", &preferences);
    fission::ui::CommandSearch commandSearch;
    qmlRegisterSingletonInstance("Fission3D.Ui", 1, 0, "CommandSearch", &commandSearch);
    documentModel.setPrinter(preferences.printerId());
    files.setNewDocumentUnit(preferences.units() == QLatin1String("in") ? 25.4 : 1.0);
    QObject::connect(&preferences, &fission::ui::Preferences::unitsChanged, &files, [&] {
        files.setNewDocumentUnit(preferences.units() == QLatin1String("in") ? 25.4 : 1.0);
    });
    numbers.setMaxDecimals(preferences.decimals());
    QObject::connect(&preferences, &fission::ui::Preferences::decimalsChanged, &numbers,
                     [&] { numbers.setMaxDecimals(preferences.decimals()); });
    autosave.setEnabled(preferences.autosave());
    autosave.setIntervalMinutes(preferences.autosaveMinutes());
    QObject::connect(&preferences, &fission::ui::Preferences::autosaveChanged, &autosave, [&] {
        autosave.setEnabled(preferences.autosave());
        autosave.setIntervalMinutes(preferences.autosaveMinutes());
    });

    fission::ui::registerViewportTypes();
    qmlRegisterSingletonInstance("Fission3D.Ui", 1, 0, "Theme", &theme);
    qmlRegisterSingletonInstance("Fission3D.Ui", 1, 0, "AppInfo", &info);
    qmlRegisterSingletonInstance("Fission3D.Ui", 1, 0, "AppOptions", &appOptions);
    qmlRegisterSingletonInstance("Fission3D.Ui", 1, 0, "Numbers", &numbers);
    qmlRegisterSingletonInstance("Fission3D.Ui", 1, 0, "DocumentModel", &documentModel);
    fission::ui::SelectionController selection(&documentModel);
    qmlRegisterSingletonInstance("Fission3D.Ui", 1, 0, "Selection", &selection);
    fission::ui::PlacementController placement(&documentModel);
    qmlRegisterSingletonInstance("Fission3D.Ui", 1, 0, "Placement", &placement);
    fission::ui::HandleController handles(&documentModel);
    handles.setStyle(preferences.handleStyle());
    QObject::connect(&preferences, &fission::ui::Preferences::handleStyleChanged, &handles,
                     [&] { handles.setStyle(preferences.handleStyle()); });
    qmlRegisterSingletonInstance("Fission3D.Ui", 1, 0, "Handles", &handles);
    fission::ui::GizmoController gizmo(&documentModel);
    qmlRegisterSingletonInstance("Fission3D.Ui", 1, 0, "Gizmo", &gizmo);
    fission::ui::RotateController rotate(&documentModel);
    qmlRegisterSingletonInstance("Fission3D.Ui", 1, 0, "Rotate", &rotate);
    fission::ui::ScaleController scale(&documentModel);
    qmlRegisterSingletonInstance("Fission3D.Ui", 1, 0, "Scale", &scale);
    fission::ui::DistanceController distances(&documentModel);
    qmlRegisterSingletonInstance("Fission3D.Ui", 1, 0, "Distances", &distances);
    fission::ui::AlignController align(&documentModel, &selection);
    qmlRegisterSingletonInstance("Fission3D.Ui", 1, 0, "Align", &align);
    // The pictures on the tiles of the shape gallery come from the geometry kernel.
    engine.addImageProvider(QStringLiteral("shapes"), new fission::ui::ShapePreviews(kernel));
    qmlRegisterSingletonInstance("Fission3D.Ui", 1, 0, "Language", &language);

    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(2); }, Qt::QueuedConnection);
    engine.loadFromModule("Fission3D", "Main");

    auto* window = engine.rootObjects().isEmpty()
                       ? nullptr
                       : qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
    if (window == nullptr) {
        spdlog::error("the main window could not be created");
        return 2;
    }
    if (options.size.isValid()) {
        window->resize(options.size);
    }
    files.setPreviewGrabber([window] { return previewPng(window); });
    // Work saved for the person after a crash is offered by a card, once the window is there.
    QTimer::singleShot(0, &files, [&files] { files.checkRecovery(); });
    // A file to open: from the command line, from a double click that started the program, or
    // from one that arrives later.
    const auto openFile = [&files](const QString& path) { files.openPath(path); };
    fileOpenFilter.open = openFile;
    if (!options.file.isEmpty()) {
        QTimer::singleShot(0, &app, [openFile, path = options.file] { openFile(path); });
    } else if (!fileOpenFilter.pending.isEmpty()) {
        QTimer::singleShot(0, &app, [openFile, path = fileOpenFilter.pending] { openFile(path); });
    }
    auto* viewport = window->findChild<fission::ui::Viewport3D*>(QStringLiteral("viewport"));
    if (viewport != nullptr) {
        selection.attach(viewport);
        placement.attach(viewport);
        handles.attach(viewport);
        gizmo.attach(viewport);
        rotate.attach(viewport);
        scale.attach(viewport);
        distances.attach(viewport);
        align.attach(viewport);
        distances.attachGizmo(&gizmo);
        viewport->setScene(documentModel.scene());
        const QMetaObject::Connection sceneConnection =
            QObject::connect(&documentModel, &fission::ui::DocumentModel::sceneReady, viewport,
                             [viewport](std::shared_ptr<const fission::render::RenderScene> scene) {
                                 viewport->setScene(std::move(scene));
                             });
        if (options.benchmarkTriangles > 0) {
            runBenchmark(window, viewport, sceneConnection,
                         static_cast<std::size_t>(options.benchmarkTriangles));
        }
        // Zooming in asks for finer tessellation; the model makes it in the background.
        QObject::connect(viewport, &fission::ui::Viewport3D::detailLevelWanted, &documentModel,
                         &fission::ui::DocumentModel::setDetailLevel);
    }
    if (options.about) {
        QTimer::singleShot(0, window, [window] {
            QMetaObject::invokeMethod(window, "run",
                                      Q_ARG(QVariant, QVariant(QStringLiteral("about"))));
        });
    }

    const auto reportFirstFrame = [&, mainEntered, ageAtMain,
                                   reported = std::make_shared<bool>(false)] {
        if (*reported) {
            return;
        }
        *reported = true;
        const double sinceMain = std::chrono::duration<double, std::milli>(
                                     std::chrono::steady_clock::now() - mainEntered)
                                     .count();
        const int startup = static_cast<int>(std::lround(ageAtMain + sinceMain));
        info.setStartupMilliseconds(startup);
        info.setGraphicsBackend(viewport != nullptr ? viewport->backendName() : QString());
        spdlog::info("start time: {} ms from process start to the first drawn frame (graphics: {})",
                     startup, info.property("graphicsBackend").toString().toStdString());
        if (options.measureStartup) {
            std::printf("startup_ms=%d\n", startup);
            std::fflush(stdout);
            QCoreApplication::quit();
        }
    };
    if (viewport != nullptr) {
        QObject::connect(viewport, &fission::ui::Viewport3D::firstFrameShown, &app,
                         reportFirstFrame);
    } else {
        QTimer::singleShot(0, &app, reportFirstFrame);
    }

    fission::ui::PlacementController* const placementController = &placement;
    fission::ui::HandleController* const handleController = &handles;
    fission::ui::GizmoController* const gizmoController = &gizmo;
    fission::ui::RotateController* const rotateController = &rotate;
    fission::ui::ScaleController* const scaleController = &scale;
    fission::ui::DistanceController* const distanceController = &distances;
    fission::ui::AlignController* const alignController = &align;
    fission::ui::SelectionController* const selectionController = &selection;
    fission::ui::FileController* const filesController = &files;
    fission::ui::Preferences* const preferencesPointer = &preferences;
    fission::ui::ExportController* const exporterPointer = &exporter;
    if (!options.smokeTestPath.isEmpty()) {
        // Wait for the first frame and a moment more (animations, fonts), then grab the window.
        const QString path = options.smokeTestPath;
        const QString simulate = options.simulate;
        // True once the pictures are being taken: the fallback below must not end a long
        // simulation after 4 seconds (and report success for what never ran).
        const auto started = std::make_shared<bool>(false);
        auto grab = [window, viewport, placementController, &documentModel, handleController,
                     gizmoController, rotateController, scaleController, distanceController,
                     alignController, selectionController, filesController, exporterPointer,
                     preferencesPointer, path, simulate, started] {
            if (*started) {
                return; // the first frame can be reported more than once
            }
            *started = true;
            auto shoot = [window, path] {
                QTimer::singleShot(600, window, [window, path] {
                    const QImage frame = window->grabWindow();
                    const bool saved = !frame.isNull() && frame.save(path);
                    QCoreApplication::exit(saved ? g_simulationFailure : 3);
                });
            };
            if (viewport != nullptr && !simulate.isEmpty()) {
                playActions(window, viewport, placementController, &documentModel, handleController,
                            gizmoController, rotateController, scaleController, distanceController,
                            alignController, selectionController, filesController, exporterPointer,
                            preferencesPointer, simulate, shoot);
            } else {
                shoot();
            }
        };
        if (viewport != nullptr) {
            QObject::connect(viewport, &fission::ui::Viewport3D::firstFrameShown, &app, grab);
            // No graphics device (software rendering): the 3D view never reports a frame.
            QTimer::singleShot(4000, &app, [window, path, started] {
                if (*started) {
                    return;
                }
                const QImage frame = window->grabWindow();
                QCoreApplication::exit(!frame.isNull() && frame.save(path) ? 0 : 3);
            });
        } else {
            grab();
        }
    }

    return QGuiApplication::exec();
}
