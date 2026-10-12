// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: LGPL-2.1-or-later

// Minimal stand-in for FreeCAD's Base::Console and Base::TimeElapsed, which PlaneGCS uses
// for diagnostics. Warnings and errors go to stderr; log and message output is dropped.
#pragma once

#include <chrono>
#include <cstdarg>
#include <cstdio>

namespace Base {

class ConsoleSingleton {
public:
    void log(const char* /*format*/, ...) {}
    void message(const char* /*format*/, ...) {}

    void warning(const char* format, ...) {
        std::va_list args;
        va_start(args, format);
        std::vfprintf(stderr, format, args);
        va_end(args);
    }

    void error(const char* format, ...) {
        std::va_list args;
        va_start(args, format);
        std::vfprintf(stderr, format, args);
        va_end(args);
    }
};

inline ConsoleSingleton& Console() {
    static ConsoleSingleton instance;
    return instance;
}

class TimeElapsed {
public:
    TimeElapsed() : start_(std::chrono::steady_clock::now()) {}

    static double diffTimeF(const TimeElapsed& from, const TimeElapsed& to) {
        return std::chrono::duration<double>(to.start_ - from.start_).count();
    }

private:
    std::chrono::steady_clock::time_point start_;
};

}  // namespace Base
