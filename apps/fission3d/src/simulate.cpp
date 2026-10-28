// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Developer option --simulate (docs/arbeitspakete/anleitungen/handbuch.md §9): plays actions
// through the real input path of the program, for the window tests. Moved out of main.cpp
// without a change in behaviour (pflege.md P12).

#include "simulate.h"

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

} // namespace

int simulationFailure() {
    return g_simulationFailure;
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
