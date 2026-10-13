// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Spike AP-0.4: command line options shared by both prototypes.

#pragma once

#include <QString>

namespace fission::spike {

struct RunOptions {
    QString scene = QStringLiteral("part"); // part | bench0.5 | bench2 | bench5
    double benchmarkSeconds = 0.0;          // > 0: auto-orbit, measure, print, quit
    int samples = 4;                        // MSAA
    bool vsync = true;
    QString screenshot;                     // write a PNG after a few frames and quit
    QString backend;                        // metal | vulkan | opengl | software (empty: default)
    QString resultFile;                     // append the benchmark result (JSON line) here
};

inline RunOptions& runOptions() {
    static RunOptions options;
    return options;
}

} // namespace fission::spike
