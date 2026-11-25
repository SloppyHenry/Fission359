// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QPointer>
#include <QThreadPool>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "fission/geo/kernel.h"
#include "fission/render/scene.h"

namespace fission::ui {

/// Works out the cut faces of the section view away from the interface thread (AP-2.11c, rule 9).
/// `request` hands over the bodies and the plane; one calculation runs at a time and only the
/// newest request counts: older results are dropped. A kernel that fails on one body gives no
/// cut face for that body (a warning in the log), never an error for the rest. Like
/// `SketchRegionWorker`, nothing of the job touches a Qt object.
class SectionCapWorker : public QObject {
    Q_OBJECT

public:
    struct Body {
        std::string id;
        geo::Solid solid;
        QColor color;
    };

    explicit SectionCapWorker(QObject* parent = nullptr);
    ~SectionCapWorker() override;

    void request(std::shared_ptr<const geo::GeometryKernel> kernel, std::vector<Body> bodies,
                 geo::Point3d point, geo::Point3d normal);
    /// Forgets everything; a running calculation's result is dropped.
    void reset();

    /// The newest finished result; null until it is there (and after `reset`).
    const std::shared_ptr<const render::CapSet>& caps() const noexcept { return m_caps; }
    bool busy() const noexcept { return m_running; }
    /// Blocks (processing events) until nothing is running or waiting, up to `milliseconds`.
    bool waitUntilIdle(int milliseconds);

signals:
    /// A new result is there.
    void changed();

private:
    struct Job {
        std::shared_ptr<const geo::GeometryKernel> kernel;
        std::vector<Body> bodies;
        geo::Point3d point;
        geo::Point3d normal;
    };
    void start(Job job);
    void finished(std::uint64_t generation, std::shared_ptr<const render::CapSet> caps);

    QThreadPool m_pool;
    std::shared_ptr<const render::CapSet> m_caps;
    bool m_running = false;
    std::uint64_t m_generation = 0;
    std::optional<Job> m_waiting;
};

} // namespace fission::ui
