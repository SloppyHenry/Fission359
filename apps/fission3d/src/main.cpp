// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QString>
#include <QTimer>
#include <QtQml/qqml.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string_view>

#include <spdlog/spdlog.h>

#include "fission/core/version.h"
#include "fission/geo/occt/layer.h"
#include "fission/geo/occt/solids.h"
#include "fission/render/scene.h"
#include "fission/services/log.h"
#include "fission/services/startup_clock.h"
#include "fission/ui/app_info.h"
#include "fission/ui/app_options.h"
#include "fission/ui/language.h"
#include "fission/ui/numbers.h"
#include "fission/ui/theme.h"
#include "fission/ui/viewport_3d.h"

namespace {

QString toQString(std::string_view text) {
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

// "0.0.1 (OpenCASCADE 8.0.1)": handy in bug reports. Asking for the kernel version calls into
// the library, so a broken library setup shows up right at startup.
QString versionText() {
    return QStringLiteral("%1 (OpenCASCADE %2)")
        .arg(toQString(fission::core::versionString()),
             QString::fromStdString(fission::geo::occt::kernelVersion()));
}

struct CommandLine {
    QString smokeTestPath;  // --smoke-test: save a picture of the window and quit
    bool measureStartup = false;
    QString theme;          // dark | light | system (empty: dark)
    QString language;       // de | en | system (empty: system)
    bool focus = false;
    bool about = false;
    bool reduceMotion = false;
    QSize size;
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
        QStringLiteral("Save a screenshot of the first frame and exit."), QStringLiteral("file.png"));
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
    const QCommandLineOption reduceMotion(QStringLiteral("reduce-motion"),
                                          QStringLiteral("Switch animations off."));
    const QCommandLineOption size(QStringLiteral("size"),
                                  QStringLiteral("Window size, for example 1280x800."),
                                  QStringLiteral("WxH"));
    parser.addOptions({smokeTest, measureStartup, theme, language, focus, about, reduceMotion, size});
    parser.process(core); // prints help or version and exits when asked

    CommandLine result;
    result.smokeTestPath = parser.value(smokeTest);
    result.measureStartup = parser.isSet(measureStartup);
    result.theme = parser.value(theme);
    result.language = parser.value(language);
    result.focus = parser.isSet(focus);
    result.about = parser.isSet(about);
    result.reduceMotion = parser.isSet(reduceMotion);
    const QStringList parts = parser.value(size).split(QLatin1Char('x'));
    if (parts.size() == 2 && parts[0].toInt() > 0 && parts[1].toInt() > 0) {
        result.size = QSize(parts[0].toInt(), parts[1].toInt());
    }
    return result;
}

// The first body of Fission3D: a 20 mm cube on the print bed, made by the geometry kernel.
std::shared_ptr<const fission::render::RenderScene> makeDemoScene() {
    auto scene = std::make_shared<fission::render::RenderScene>();
    const fission::geo::Solid cube =
        fission::geo::occt::makeBox(20.0, 20.0, 20.0, {-10.0, -10.0, 0.0});
    fission::render::RenderBody body;
    body.mesh = fission::geo::occt::tessellate(cube);
    scene->bodies.push_back(std::move(body));
    return scene;
}

void loadFonts() {
    // Fallback if the bundled fonts cannot be loaded (docs/08-gestaltung.md §3).
    QFont::insertSubstitutions(QStringLiteral("IBM Plex Sans"),
                               fission::ui::Theme::fontFamilies().mid(1));
    QFont::insertSubstitutions(QStringLiteral("IBM Plex Mono"),
                               fission::ui::Theme::monoFamilies().mid(1));
    // IBM Plex (SIL Open Font License 1.1) ships with the program: nothing is downloaded.
    for (const char* file : {"IBMPlexSans-Regular", "IBMPlexSans-Medium", "IBMPlexSans-SemiBold",
                             "IBMPlexSans-Bold", "IBMPlexMono-Regular", "IBMPlexMono-Medium",
                             "IBMPlexMono-SemiBold"}) {
        const QString path = QStringLiteral(":/fission/fonts/%1.ttf").arg(QLatin1String(file));
        if (QFontDatabase::addApplicationFont(path) < 0) {
            spdlog::warn("font {} could not be loaded; the system font is used", file);
        }
    }
}

} // namespace

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
    theme.setMode(options.theme.isEmpty() ? QStringLiteral("dark") : options.theme);
    fission::ui::AppInfo info;
    info.setVersion(toQString(fission::core::versionString()));
    info.setQtVersion(QString::fromLatin1(qVersion()));
    info.setKernelVersion(QString::fromStdString(fission::geo::occt::kernelVersion()));
    info.setLogFile(QString::fromStdString(logFile.string()));
    fission::ui::AppOptions appOptions;
    appOptions.focusMode = options.focus;
    appOptions.reduceMotion = options.reduceMotion;
    fission::ui::Numbers numbers;

    QQmlApplicationEngine engine;
    fission::ui::Language language(&engine);
    language.setCode(options.language.isEmpty() ? QStringLiteral("system") : options.language);

    fission::ui::registerViewportTypes();
    qmlRegisterSingletonInstance("Fission3D.Ui", 1, 0, "Theme", &theme);
    qmlRegisterSingletonInstance("Fission3D.Ui", 1, 0, "AppInfo", &info);
    qmlRegisterSingletonInstance("Fission3D.Ui", 1, 0, "AppOptions", &appOptions);
    qmlRegisterSingletonInstance("Fission3D.Ui", 1, 0, "Numbers", &numbers);
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
    auto* viewport = window->findChild<fission::ui::Viewport3D*>(QStringLiteral("viewport"));
    if (viewport != nullptr) {
        try {
            viewport->setScene(makeDemoScene());
        } catch (const std::exception& error) {
            spdlog::error("the demo body could not be built: {}", error.what());
        }
    }
    if (options.about) {
        QTimer::singleShot(0, window, [window] {
            QMetaObject::invokeMethod(window, "run", Q_ARG(QVariant, QVariant(QStringLiteral("about"))));
        });
    }

    const auto reportFirstFrame = [&, mainEntered, ageAtMain, reported = std::make_shared<bool>(false)] {
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
        QObject::connect(viewport, &fission::ui::Viewport3D::firstFrameShown, &app, reportFirstFrame);
    } else {
        QTimer::singleShot(0, &app, reportFirstFrame);
    }

    if (!options.smokeTestPath.isEmpty()) {
        // Wait for the first frame and a moment more (animations, fonts), then grab the window.
        const QString path = options.smokeTestPath;
        auto grab = [window, path] {
            QTimer::singleShot(600, window, [window, path] {
                const QImage frame = window->grabWindow();
                const bool saved = !frame.isNull() && frame.save(path);
                QCoreApplication::exit(saved ? 0 : 3);
            });
        };
        if (viewport != nullptr) {
            QObject::connect(viewport, &fission::ui::Viewport3D::firstFrameShown, &app, grab);
            // No graphics device (software rendering): the 3D view never reports a frame.
            QTimer::singleShot(4000, &app, [window, path] {
                const QImage frame = window->grabWindow();
                QCoreApplication::exit(!frame.isNull() && frame.save(path) ? 0 : 3);
            });
        } else {
            grab();
        }
    }

    return QGuiApplication::exec();
}
