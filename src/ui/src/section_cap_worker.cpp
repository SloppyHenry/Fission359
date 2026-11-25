// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/ui/section_cap_worker.h"

#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QLoggingCategory>
#include <QMetaObject>

#include <exception>
#include <utility>

#include "fission/geo/error.h"

namespace fission::ui {

namespace {

constexpr double kDeflection = 0.05; // mm: the cut face is drawn, not exported

} // namespace

SectionCapWorker::SectionCapWorker(QObject* parent) : QObject(parent) {
    m_pool.setMaxThreadCount(1);
}

SectionCapWorker::~SectionCapWorker() {
    m_waiting.reset();
    m_pool.waitForDone();
}

void SectionCapWorker::request(std::shared_ptr<const geo::GeometryKernel> kernel,
                               std::vector<Body> bodies, geo::Point3d point, geo::Point3d normal) {
    Job job{std::move(kernel), std::move(bodies), point, normal};
    if (m_running) {
        m_waiting = std::move(job);
        return;
    }
    start(std::move(job));
}

void SectionCapWorker::reset() {
    m_waiting.reset();
    ++m_generation; // the result of a running calculation is dropped
    m_running = false;
    if (m_caps) {
        m_caps.reset();
        emit changed();
    }
}

void SectionCapWorker::start(Job job) {
    m_running = true;
    const std::uint64_t generation = ++m_generation;
    QPointer<SectionCapWorker> self(this);
    m_pool.start([self, generation, job = std::move(job)] {
        auto set = std::make_shared<render::CapSet>();
        for (const Body& body : job.bodies) {
            try {
                const geo::SectionMesh cut =
                    job.kernel->sectionMesh(body.solid, job.point, job.normal, kDeflection);
                if (cut.empty()) {
                    continue;
                }
                render::CapMesh cap;
                cap.color = body.color;
                cap.indices = cut.indices;
                cap.positions.reserve(cut.positions.size() * 3);
                for (const geo::Point3d& p : cut.positions) {
                    cap.positions.push_back(static_cast<float>(p.x));
                    cap.positions.push_back(static_cast<float>(p.y));
                    cap.positions.push_back(static_cast<float>(p.z));
                }
                set->push_back(std::move(cap));
            } catch (const std::exception& error) {
                // No cut face for this body; the others still get theirs.
                qWarning("section view: no cut face for body %s: %s", body.id.c_str(),
                         error.what());
            }
        }
        QMetaObject::invokeMethod(
            qApp,
            [self, generation,
             set = std::shared_ptr<const render::CapSet>(std::move(set))]() mutable {
                if (self) {
                    self->finished(generation, std::move(set));
                }
            },
            Qt::QueuedConnection);
    });
}

void SectionCapWorker::finished(std::uint64_t generation,
                                std::shared_ptr<const render::CapSet> caps) {
    if (generation != m_generation) {
        return; // reset meanwhile
    }
    if (m_waiting) {
        // a newer request is waiting: this result is already old, do not show it
        Job next = std::move(*m_waiting);
        m_waiting.reset();
        start(std::move(next));
        return;
    }
    m_running = false;
    m_caps = std::move(caps);
    emit changed();
}

bool SectionCapWorker::waitUntilIdle(int milliseconds) {
    const QDeadlineTimer deadline(milliseconds, Qt::PreciseTimer);
    while (m_running && !deadline.hasExpired()) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
    return !m_running;
}

} // namespace fission::ui
