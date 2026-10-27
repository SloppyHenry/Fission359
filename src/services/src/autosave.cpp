// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/services/autosave.h"

#include <algorithm>

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
    if (!enabled_) {
        return;
    }
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
    if (enabled_) {
        timer_.start();
    }
}

void Autosave::setEnabled(bool enabled) {
    enabled_ = enabled;
    if (enabled) {
        timer_.start();
    } else {
        timer_.stop();
    }
}

void Autosave::setIntervalMinutes(int minutes) {
    timer_.setInterval(std::clamp(minutes, 1, 60) * 60000);
}

} // namespace fission::services
