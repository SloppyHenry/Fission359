// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Spike AP-0.4, prototype A: Qt RHI viewport.
//   viewport_a [--scene part|bench0.5|bench2|bench5] [--benchmark SECONDS] [--samples N]
//              [--no-vsync] [--backend metal|vulkan|opengl|software] [--screenshot FILE.png]
//              [--result-file FILE]

#include "../common/run_options.h"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QSurfaceFormat>

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("viewport_a");

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOptions({
        {"scene", "part | bench0.5 | bench2 | bench5", "name", "part"},
        {"benchmark", "Orbit automatically for N seconds, print the result and quit", "seconds", "0"},
        {"samples", "MSAA samples (1, 2, 4, 8)", "n", "4"},
        {"no-vsync", "Do not wait for the display refresh"},
        {"backend", "metal | vulkan | opengl | software", "api", ""},
        {"screenshot", "Write a PNG after a few frames and quit", "file", ""},
        {"result-file", "Append the benchmark result (JSON line) to this file", "file", ""},
    });
    parser.process(app);

    using fission::spike::runOptions;
    runOptions().scene = parser.value("scene");
    runOptions().benchmarkSeconds = parser.value("benchmark").toDouble();
    runOptions().samples = parser.value("samples").toInt();
    runOptions().vsync = !parser.isSet("no-vsync");
    runOptions().backend = parser.value("backend");
    runOptions().screenshot = parser.value("screenshot");
    runOptions().resultFile = parser.value("result-file");

    QSurfaceFormat format;
    format.setSwapInterval(runOptions().vsync ? 1 : 0);
    QSurfaceFormat::setDefaultFormat(format);

    const QString backend = runOptions().backend.toLower();
    if (backend == "metal") {
        QQuickWindow::setGraphicsApi(QSGRendererInterface::Metal);
    } else if (backend == "vulkan") {
        QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);
    } else if (backend == "opengl") {
        QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    } else if (backend == "software") {
        QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
    }

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("appBenchmark", runOptions().benchmarkSeconds > 0.0);
    engine.loadFromModule("ViewportA", "Main");
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }
    return app.exec();
}
