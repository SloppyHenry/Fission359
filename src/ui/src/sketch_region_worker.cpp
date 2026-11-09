// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/ui/sketch_region_worker.h"

#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QMetaObject>

#include <chrono>
#include <exception>
#include <utility>

#include "fission/geo/error.h"

namespace fission::ui {

SketchRegionWorker::SketchRegionWorker(QObject* parent) : QObject(parent) {
    m_pool.setMaxThreadCount(1);
}

SketchRegionWorker::~SketchRegionWorker() {
    m_waiting.reset();
    m_pool.waitForDone(); // the running job only holds copies, but must end before the pool does
}

void SketchRegionWorker::request(std::shared_ptr<const geo::GeometryKernel> kernel,
                                 const std::vector<core::SketchElement>& elements) {
    if (m_last && *m_last == elements) {
        return;
    }
    m_last = elements;
    Job job{std::move(kernel), elements};
    if (m_running) {
        m_waiting = std::move(job);
        return;
    }
    start(std::move(job));
}

void SketchRegionWorker::reset() {
    m_last.reset();
    m_waiting.reset();
    ++m_generation; // the result of a running calculation is dropped
    m_running = false;
    if (!m_regions.regions.empty() || m_failed) {
        m_regions = {};
        m_failed = false;
        emit changed();
    }
}

void SketchRegionWorker::start(Job job) {
    m_running = true;
    const std::uint64_t generation = ++m_generation;
    QPointer<SketchRegionWorker> self(this);
    m_pool.start([self, generation, job = std::move(job)] {
        const auto begin = std::chrono::steady_clock::now();
        interaction::SketchRegionSet set;
        bool failed = false;
        try {
            set = interaction::computeSketchRegions(*job.kernel, job.elements);
        } catch (const std::exception&) {
            failed = true; // a refusal of the kernel: no regions, a hint (rule 5)
        }
        const double milliseconds =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin)
                .count();
        // Back on the thread of the interface; `self` is only looked at there.
        QMetaObject::invokeMethod(
            qApp,
            [self, generation, set = std::move(set), failed, milliseconds]() mutable {
                if (self) {
                    self->finished(generation, std::move(set), failed, milliseconds);
                }
            },
            Qt::QueuedConnection);
    });
}

void SketchRegionWorker::finished(std::uint64_t generation, interaction::SketchRegionSet set,
                                  bool failed, double milliseconds) {
    if (generation != m_generation) {
        return; // reset meanwhile
    }
    m_lastMilliseconds = milliseconds;
    if (m_waiting) {
        // a newer request is waiting: this result is already old, do not show it
        Job next = std::move(*m_waiting);
        m_waiting.reset();
        start(std::move(next));
        return;
    }
    m_running = false;
    m_regions = std::move(set);
    m_failed = failed;
    emit changed();
}

bool SketchRegionWorker::waitUntilIdle(int milliseconds) {
    const QDeadlineTimer deadline(milliseconds, Qt::PreciseTimer);
    while (m_running && !deadline.hasExpired()) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
    return !m_running;
}

} // namespace fission::ui
