// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QString>
#include <QTimer>

#include <memory>
#include <string_view>

#include "fission/core/version.h"
#include "fission/geo/occt/layer.h"
#include "fission/geo/occt/solids.h"
#include "fission/render/scene.h"
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

// Reads the command line with a plain QCoreApplication: it needs no display, so --help and
// --version also work in a terminal without a window system. Returns the screenshot path of
// --smoke-test (empty if not given).
QString parseCommandLine(int& argc, char** argv) {
    const QCoreApplication core(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("Fission3D"));
    QCoreApplication::setApplicationVersion(versionText());

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    // Developer option: render one frame, save it as PNG and quit. Used by scripts/test
    // to prove that the window opens, also on machines without a screen.
    const QCommandLineOption smokeTest(
        QStringLiteral("smoke-test"),
        QStringLiteral("Save a screenshot of the first frame and exit."),
        QStringLiteral("file.png"));
    parser.addOption(smokeTest);
    parser.process(core); // prints help or version and exits when asked
    return parser.value(smokeTest);
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

} // namespace

int main(int argc, char* argv[]) {
    const QString smokeTestPath = parseCommandLine(argc, argv);

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("Fission3D"));
    QGuiApplication::setApplicationVersion(versionText());
    // Lets Wayland and the desktop match the window to the installed .desktop file and icon.
    QGuiApplication::setDesktopFileName(QStringLiteral(FISSION_APP_ID));

    fission::ui::registerViewportTypes();
    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(2); }, Qt::QueuedConnection);
    engine.loadFromModule("Fission3D", "Main");
    if (!engine.rootObjects().isEmpty()) {
        if (auto* viewport =
                engine.rootObjects().constFirst()->findChild<fission::ui::Viewport3D*>(
                    QStringLiteral("viewport"))) {
            viewport->setScene(makeDemoScene());
        }
    }

    if (!smokeTestPath.isEmpty()) {
        // Give the scene graph a moment to render the first frame, then grab it.
        QTimer::singleShot(1500, &app, [&engine, smokeTestPath] {
            auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().value(0));
            const QImage frame = window != nullptr ? window->grabWindow() : QImage();
            const bool saved = !frame.isNull() && frame.save(smokeTestPath);
            QCoreApplication::exit(saved ? 0 : 3);
        });
    }

    return QGuiApplication::exec();
}
