// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/services/log.h"

#include <QByteArray>
#include <QString>
#include <QtLogging>

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace fission::services {

namespace {

spdlog::level::level_enum levelOf(QtMsgType type) {
    switch (type) {
    case QtDebugMsg:
        return spdlog::level::debug;
    case QtInfoMsg:
        return spdlog::level::info;
    case QtWarningMsg:
        return spdlog::level::warn;
    case QtCriticalMsg:
        return spdlog::level::err;
    case QtFatalMsg:
        return spdlog::level::critical;
    }
    return spdlog::level::info;
}

void qtMessageHandler(QtMsgType type, const QMessageLogContext& /*context*/,
                      const QString& message) {
    const QByteArray utf8 = message.toUtf8();
    if (auto logger = spdlog::default_logger()) {
        logger->log(levelOf(type), "[qt] {}", utf8.constData());
        if (type == QtFatalMsg) {
            logger->flush();
        }
    }
    if (type == QtFatalMsg) {
        std::abort();
    }
}

} // namespace

std::filesystem::path logDirectory() {
    if (const char* override = std::getenv("FISSION_LOG_DIR");
        override != nullptr && *override != '\0') {
        return override;
    }
    const char* home = std::getenv("HOME");
    const std::filesystem::path homePath = home != nullptr ? home : ".";
#if defined(__APPLE__)
    return homePath / "Library" / "Logs" / "Fission359";
#else
    if (const char* state = std::getenv("XDG_STATE_HOME"); state != nullptr && *state != '\0') {
        return std::filesystem::path(state) / "fission359";
    }
    return homePath / ".local" / "state" / "fission359";
#endif
}

std::filesystem::path startLogging(std::string_view program) {
    static std::filesystem::path logFile;
    if (!logFile.empty()) {
        return logFile;
    }
    std::vector<spdlog::sink_ptr> sinks;
    sinks.push_back(std::make_shared<spdlog::sinks::stderr_color_sink_mt>());
    try {
        const std::filesystem::path directory = logDirectory();
        std::filesystem::create_directories(directory);
        logFile = directory / "fission359.log";
        sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            logFile.string(), std::size_t{1} << 20, 3));
    } catch (const std::exception& error) {
        // No log file (read-only home, ...): the program still runs, the terminal gets the log.
        logFile = "(no log file)";
        fprintf(stderr, "Fission359: cannot write a log file: %s\n", error.what());
    }
    auto logger =
        std::make_shared<spdlog::logger>(std::string(program), sinks.begin(), sinks.end());
    logger->set_pattern("%Y-%m-%d %H:%M:%S.%e %^%l%$ %v");
    logger->set_level(spdlog::level::info);
    logger->flush_on(spdlog::level::warn);
    spdlog::set_default_logger(logger);
    qInstallMessageHandler(qtMessageHandler);
    return logFile;
}

} // namespace fission::services
