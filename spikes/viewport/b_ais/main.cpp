// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Spike AP-0.4, prototype B: OpenCASCADE AIS viewer inside Qt Quick (OpenGL).
//   viewport_b [--scene part|bench0.5|bench2|bench5] [--samples N]

#include "../common/run_options.h"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QSurfaceFormat>

int main(int argc, char* argv[]) {
    // QQuickFramebufferObject needs the OpenGL backend of Qt Quick. This is the price of
    // embedding OpenCASCADE's OpenGL viewer: no Metal on macOS.
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    QSurfaceFormat format;
    format.setVersion(4, 1);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    QSurfaceFormat::setDefaultFormat(format);

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("viewport_b");

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOptions({
        {"scene", "part | bench0.5 | bench2 | bench5", "name", "part"},
        {"samples", "MSAA samples", "n", "4"},
    });
    parser.process(app);
    fission::spike::runOptions().scene = parser.value("scene");
    fission::spike::runOptions().samples = parser.value("samples").toInt();

    QQmlApplicationEngine engine;
    engine.loadFromModule("ViewportB", "Main");
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }
    return app.exec();
}
