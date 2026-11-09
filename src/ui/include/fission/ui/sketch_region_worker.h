// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QPointer>
#include <QThreadPool>

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "fission/core/sketch.h"
#include "fission/geo/kernel.h"
#include "fission/interaction/sketch_regions.h"

namespace fission::ui {

/// Works out the regions of a sketch away from the interface thread (AP-2.1g, decision 54, rule 9).
/// `request` hands over the elements; one calculation runs at a time, and while it runs only the
/// newest request is kept (the older ones are dropped, the running one is not interrupted but its
/// result is only used when nothing newer is waiting). Until the new result is there, `regions`
/// are the old ones. A kernel that refuses gives no regions and `failed`. Nothing of the worker
/// touches a Qt object: the result comes back as a queued call.
class SketchRegionWorker : public QObject {
    Q_OBJECT

public:
    explicit SketchRegionWorker(QObject* parent = nullptr);
    ~SketchRegionWorker() override;

    /// Calculates for these elements unless it is the same as the last request. `kernel` is kept
    /// by the running calculation.
    void request(std::shared_ptr<const geo::GeometryKernel> kernel,
                 const std::vector<core::SketchElement>& elements);
    /// Forgets everything (the sketch mode was left); a running calculation's result is dropped.
    void reset();

    const interaction::SketchRegionSet& regions() const noexcept { return m_regions; }
    /// The kernel refused the last sketch (no regions are shown, a hint is due).
    bool failed() const noexcept { return m_failed; }
    /// A calculation is running or waiting.
    bool busy() const noexcept { return m_running; }
    /// How long the last finished calculation took, in milliseconds (for tests and the log).
    double lastMilliseconds() const noexcept { return m_lastMilliseconds; }
    /// Blocks (processing events) until nothing is running or waiting, up to `milliseconds`.
    /// For tests and `--simulate`. True when it is idle.
    bool waitUntilIdle(int milliseconds);

signals:
    /// New regions (or none, or a failure) are there.
    void changed();

private:
    struct Job {
        std::shared_ptr<const geo::GeometryKernel> kernel;
        std::vector<core::SketchElement> elements;
    };
    void start(Job job);
    void finished(std::uint64_t generation, interaction::SketchRegionSet set, bool failed,
                  double milliseconds);

    QThreadPool m_pool;
    interaction::SketchRegionSet m_regions;
    bool m_failed = false;
    bool m_running = false;
    double m_lastMilliseconds = 0.0;
    std::uint64_t m_generation = 0;                         ///< of the calculation that is running
    std::optional<Job> m_waiting;                           ///< the newest request while one runs
    std::optional<std::vector<core::SketchElement>> m_last; ///< what the last request asked for
};

} // namespace fission::ui
