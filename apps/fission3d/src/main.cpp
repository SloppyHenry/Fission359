// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QString>
#include <QTimer>

#include "fission/core/version.h"

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("Fission3D"));
    QGuiApplication::setApplicationVersion(
        QString::fromUtf8(fission::core::versionString().data(),
                          static_cast<qsizetype>(fission::core::versionString().size())));

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
    parser.process(app);

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(2); }, Qt::QueuedConnection);
    engine.loadFromModule("Fission3D", "Main");

    if (parser.isSet(smokeTest)) {
        const QString outputPath = parser.value(smokeTest);
        // Give the scene graph a moment to render the first frame, then grab it.
        QTimer::singleShot(500, &app, [&engine, outputPath] {
            auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().value(0));
            const QImage frame = window != nullptr ? window->grabWindow() : QImage();
            const bool saved = !frame.isNull() && frame.save(outputPath);
            QCoreApplication::exit(saved ? 0 : 3);
        });
    }

    return QGuiApplication::exec();
}
