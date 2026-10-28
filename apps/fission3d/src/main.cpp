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

#include "simulate.h"
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
                    QCoreApplication::exit(saved ? simulationFailure() : 3);
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
