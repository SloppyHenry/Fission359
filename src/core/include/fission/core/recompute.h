// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "fission/core/cancel.h"
#include "fission/core/document.h"
#include "fission/core/feature_type.h"
#include "fission/core/measures.h"

namespace fission::core {

enum class FeatureStatus {
    ok,
    warning,   ///< Calculated, but from the last valid result of an earlier step (see `issue`).
    error,     ///< Could not be calculated; `issue` says why.
    suppressed ///< Switched off by the person.
};

/// What the recompute knows about one step. Not saved in the file: it is derived.
struct FeatureState {
    FeatureId id;
    FeatureStatus status = FeatureStatus::ok;
    std::optional<FeatureIssue> issue;
    std::optional<Description> description; ///< Set when the values could be worked out.
    ResolvedParams params;                  ///< The values as worked out (mm, degrees).
    bool computed = false;                  ///< The kernel was asked in this run (not remembered).
    bool stale = false;                     ///< Its bodies show the last valid result.
};

struct BodyFace {
    std::uint32_t faceId = 0; ///< Number of the face in the body (as in the tessellation).
    std::string name;         ///< "F1/box/top"
};

/// The geometry of one body after the last step that made or changed it.
struct BodyResult {
    BodyId id;
    FeatureId producer;
    geo::Solid solid;
    std::vector<BodyFace> faces;
    bool stale = false;
    std::uint64_t token = 0;     ///< Identity of the result in the memory (for cache keys) ...
    std::size_t outputIndex = 0; ///< ... together with the number of the output.
};

struct RecomputeResult {
    std::uint64_t revision = 0; ///< Revision of the document this was calculated from.
    MeasureResolution measures;
    std::vector<FeatureState> features;  ///< In recipe order.
    std::map<BodyId, BodyResult> bodies; ///< Only bodies that have geometry.
    std::size_t executed = 0;            ///< Steps the kernel was asked for.
    std::size_t remembered = 0;          ///< Steps taken from the memory.

    const FeatureState* state(const FeatureId& id) const;
};

/// Remembers results between recomputes, so an unchanged step costs nothing and an undo usually
/// needs no kernel call. The key of a result is everything that went into it: type, version,
/// every value, and which results of earlier steps it was made from. Equal key, equal result.
///
/// Also remembers the last valid result of every step, for steps that start to fail.
/// Not thread safe: one memory belongs to one recompute at a time.
class RecomputeCache {
public:
    explicit RecomputeCache(std::size_t capacity = 512) : capacity_(capacity) {}

    std::size_t size() const noexcept { return entries_.size(); }
    void clear();

private:
    friend std::optional<RecomputeResult> recompute(const Document&, const FeatureRegistry&,
                                                    const geo::GeometryKernel&, RecomputeCache&,
                                                    CancelToken);

    struct Output {
        geo::Solid solid;
        std::vector<geo::NamedFace> roles;
    };
    struct Entry {
        std::uint64_t token = 0;
        std::vector<Output> outputs;
        std::uint64_t lastUsed = 0;
    };

    const Entry* find(const std::string& key);
    const Entry& store(const std::string& key, std::vector<Output> outputs);
    void trim();

    std::size_t capacity_;
    std::map<std::string, Entry> entries_;
    std::map<FeatureId, std::shared_ptr<const Entry>> lastGood_;
    std::uint64_t nextToken_ = 1;
    std::uint64_t clock_ = 0;
};

/// Calculates every step of `document` in recipe order. Returns nothing when `cancel` was asked
/// to stop (checked between steps and handed to the steps themselves).
///
/// Mistakes stay local (docs/03-architektur.md §3.3): a step that fails gets an error, steps that
/// do not depend on it are unaffected, and steps that do continue from its last valid result,
/// marked as warning.
std::optional<RecomputeResult> recompute(const Document& document, const FeatureRegistry& registry,
                                         const geo::GeometryKernel& kernel, RecomputeCache& cache,
                                         CancelToken cancel = {});

// --- the worker --------------------------------------------------------------------------

enum class RecomputeEventType {
    started,              ///< A calculation for `revision` began.
    featureStatusChanged, ///< `state` differs from the previous result (or is new).
    finished,             ///< `result` is complete.
    cancelled             ///< Replaced by a newer request before it was done.
};

struct RecomputeEvent {
    RecomputeEventType type = RecomputeEventType::started;
    std::uint64_t revision = 0;
    std::optional<FeatureState> state;
    std::shared_ptr<const RecomputeResult> result;
};

/// Called from the worker thread. The user interface hands the event over to its own thread.
using RecomputeObserver = std::function<void(const RecomputeEvent&)>;

/// Recomputes one document in a worker thread. A new request replaces a running calculation:
/// the old one is asked to stop at once and its result is thrown away.
///
/// The registry and the kernel must outlive the engine. The engine works on a copy of the
/// document, so the caller can keep editing.
class RecomputeEngine {
public:
    RecomputeEngine(const FeatureRegistry& registry, const geo::GeometryKernel& kernel,
                    RecomputeObserver observer = {});
    ~RecomputeEngine();
    RecomputeEngine(const RecomputeEngine&) = delete;
    RecomputeEngine& operator=(const RecomputeEngine&) = delete;

    /// Calculates `snapshot` (newest request wins).
    void request(Document snapshot);

    /// The last complete result, or nothing before the first one.
    std::shared_ptr<const RecomputeResult> latest() const;

    /// Blocks until nothing is running or waiting (for tests and for closing a document).
    void waitIdle();

private:
    void run();

    const FeatureRegistry& registry_;
    const geo::GeometryKernel& kernel_;
    RecomputeObserver observer_;

    mutable std::mutex mutex_;
    std::condition_variable wake_;
    std::condition_variable idle_;
    std::optional<Document> pending_;
    CancelSource running_;
    bool quit_ = false;
    bool busy_ = false;
    std::shared_ptr<const RecomputeResult> latest_;
    RecomputeCache cache_;
    std::thread worker_;
};

} // namespace fission::core
