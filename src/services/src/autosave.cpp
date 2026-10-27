// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/services/autosave.h"

namespace fission::services {

Autosave::Autosave(int intervalMilliseconds, int commandsPerSave, QObject* parent)
    : QObject(parent), commandsPerSave_(commandsPerSave) {
    timer_.setInterval(intervalMilliseconds);
    connect(&timer_, &QTimer::timeout, this, [this] {
        // Nothing done since the last save: nothing to be due.
        if (commands_ > 0) {
            emit due();
        }
    });
}

void Autosave::commandDone() {
    if (++commands_ >= commandsPerSave_) {
        emit due();
    }
}

void Autosave::acknowledge() {
    commands_ = 0;
    if (timer_.isActive()) {
        timer_.start(); // the next minute counts from now
    }
}

void Autosave::stop() {
    timer_.stop();
}

void Autosave::start() {
    timer_.start();
}

} // namespace fission::services
