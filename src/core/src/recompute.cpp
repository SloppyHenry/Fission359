// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/core/recompute.h"

#include <algorithm>
#include <bit>
#include <exception>

#include "fission/geo/error.h"

namespace fission::core {

namespace {

FeatureIssue issueOf(FeatureIssueCode code, std::string subject = {}, std::string detail = {}) {
    FeatureIssue issue;
    issue.code = code;
    issue.subject = std::move(subject);
    issue.detail = std::move(detail);
    return issue;
}

// Works out every value of a step: formulas, named measures, units.
std::optional<FeatureIssue> resolveValues(const Feature& feature, const FeatureType& type,
                                          const MeasureResolution& measures,
                                          const MeasureLookup& lookup, double defaultUnit,
                                          ResolvedParams& out) {
    for (const ParamSpec& spec : type.parameters()) {
        const auto given = feature.params.find(spec.name);
        const std::string& text =
            given != feature.params.end() ? given->second.expression : spec.defaultExpression;

        const Outcome<Expression> parsed = Expression::parse(text);
        if (!parsed) {
            FeatureIssue issue = issueOf(FeatureIssueCode::parameterInvalid, spec.name);
            issue.expression = parsed.error();
            return issue;
        }
        // A measure that has an error itself: say so, instead of "unknown name".
        for (const NameReference& reference : parsed.value().references()) {
            const MeasureResult* measure = measures.find(reference.name);
            if (measure != nullptr && measure->error) {
                FeatureIssue issue = issueOf(FeatureIssueCode::parameterInvalid, spec.name);
                issue.expression = ExpressionError{
                    ExpressionErrorCode::brokenDependency, reference.where, measure->name, {}};
                return issue;
            }
        }
        EvaluationContext context;
        context.kind = spec.kind;
        context.defaultUnitMillimetres = defaultUnit;
        context.lookup = lookup;
        const Outcome<Quantity> value = parsed.value().evaluate(context);
        if (!value) {
            FeatureIssue issue = issueOf(FeatureIssueCode::parameterInvalid, spec.name);
            issue.expression = value.error();
            return issue;
        }
        out[spec.name] = value.value();
    }
    return std::nullopt;
}

// Everything that goes into a result, written out. Equal text, equal result.
std::string cacheKey(const Feature& feature, const FeatureType& type, const ResolvedParams& params,
                     const std::vector<const BodyResult*>& inputs) {
    std::string key(type.id());
    key += '\x1f';
    key += std::to_string(type.version());
    key += '\x1f';
    key += std::to_string(feature.outputs.size());
    for (const ParamSpec& spec : type.parameters()) {
        const Quantity& value = params.at(spec.name);
        key += '\x1f';
        key += spec.name;
        key += '=';
        key += std::to_string(std::bit_cast<std::uint64_t>(value.value));
        key += ':';
        key += std::to_string(value.dimension.length);
        key += ',';
        key += std::to_string(value.dimension.angle);
    }
    for (const TextFieldSpec& spec : type.texts()) {
        const auto given = feature.texts.find(spec.name);
        const std::string& text = given != feature.texts.end() ? given->second : spec.defaultValue;
        key += '\x1f';
        key += spec.name;
        key += '=';
        key += std::to_string(text.size()); // length first: the words may contain any character
        key += ':';
        key += text;
    }
    for (const BodyResult* input : inputs) {
        key += "\x1f#";
        key += std::to_string(input->token);
        key += '.';
        key += std::to_string(input->outputIndex);
    }
    return key;
}

FeatureIssue issueFromKernel(const geo::GeometryError& error) {
    switch (error.code()) {
    case geo::GeometryErrorCode::invalidInput:
        return issueOf(FeatureIssueCode::kernelRejected, {}, error.what());
    case geo::GeometryErrorCode::invalidResult:
        return issueOf(FeatureIssueCode::kernelResultInvalid, {}, error.what());
    case geo::GeometryErrorCode::textEmpty:
        return issueOf(FeatureIssueCode::parameterOutOfRange, "text", error.what());
    case geo::GeometryErrorCode::glyphsMissing:
        return issueOf(FeatureIssueCode::textGlyphsMissing, error.subject(), error.what());
    case geo::GeometryErrorCode::fontNotFound:
        return issueOf(FeatureIssueCode::fontNotFound, error.subject(), error.what());
    case geo::GeometryErrorCode::fontStyleMissing:
        return issueOf(FeatureIssueCode::fontStyleMissing, error.subject(), error.what());
    case geo::GeometryErrorCode::emptyResult:
        return issueOf(FeatureIssueCode::combineEmpty, {}, error.what());
    case geo::GeometryErrorCode::kernelFailure:
        break;
    }
    return issueOf(FeatureIssueCode::kernelFailed, {}, error.what());
}

bool sameState(const FeatureState& a, const FeatureState& b) {
    const auto code = [](const FeatureState& s) {
        return s.issue ? static_cast<int>(s.issue->code) : -1;
    };
    return a.id == b.id && a.status == b.status && code(a) == code(b) &&
           (a.issue ? a.issue->subject : std::string()) ==
               (b.issue ? b.issue->subject : std::string()) &&
           a.stale == b.stale;
}

} // namespace

const FeatureState* RecomputeResult::state(const FeatureId& id) const {
    const auto found = std::find_if(features.begin(), features.end(),
                                    [&](const FeatureState& s) { return s.id == id; });
    return found == features.end() ? nullptr : &*found;
}

void RecomputeCache::clear() {
    entries_.clear();
    lastGood_.clear();
}

const RecomputeCache::Entry* RecomputeCache::find(const std::string& key) {
    const auto found = entries_.find(key);
    if (found == entries_.end()) {
        return nullptr;
    }
    found->second.lastUsed = clock_;
    return &found->second;
}

const RecomputeCache::Entry& RecomputeCache::store(const std::string& key,
                                                   std::vector<Output> outputs) {
    Entry entry;
    entry.token = nextToken_++;
    entry.outputs = std::move(outputs);
    entry.lastUsed = clock_;
    const auto [position, inserted] = entries_.insert_or_assign(key, std::move(entry));
    trim();
    return position->second;
}

// Drops the entries that were not used for the longest time (never the ones of this run).
void RecomputeCache::trim() {
    while (entries_.size() > capacity_) {
        auto oldest = entries_.end();
        for (auto it = entries_.begin(); it != entries_.end(); ++it) {
            if (it->second.lastUsed != clock_ &&
                (oldest == entries_.end() || it->second.lastUsed < oldest->second.lastUsed)) {
                oldest = it;
            }
        }
        if (oldest == entries_.end()) {
            return;
        }
        entries_.erase(oldest);
    }
}

std::optional<RecomputeResult> recompute(const Document& document, const FeatureRegistry& registry,
                                         const geo::GeometryKernel& kernel, RecomputeCache& cache,
                                         CancelToken cancel) {
    RecomputeResult result;
    result.revision = document.revision();
    result.measures =
        resolveMeasures(document.measureDefinitions(), document.settings().defaultUnitMillimetres);
    const MeasureLookup lookup = result.measures.lookup();
    ++cache.clock_;

    using Entry = RecomputeCache::Entry;
    using Output = RecomputeCache::Output;

    // Puts the bodies of a calculated or remembered entry into the result.
    const auto publish = [&](const Feature& feature, const Entry& entry, bool stale) {
        const FeatureType* made = registry.find(feature.type);
        const bool passesOnNames = made != nullptr && made->passesOnNames();
        for (std::size_t i = 0; i < entry.outputs.size() && i < feature.outputs.size(); ++i) {
            BodyResult body;
            body.id = feature.outputs[i];
            body.producer = feature.id;
            body.solid = entry.outputs[i].solid;
            body.stale = stale;
            body.token = entry.token;
            body.outputIndex = i;
            for (const geo::NamedFace& role : entry.outputs[i].roles) {
                body.faces.push_back(
                    {role.faceId, passesOnNames ? role.role : elementName(feature, role.role)});
            }
            result.bodies[body.id] = std::move(body);
        }
    };

    for (const Feature& feature : document.features()) {
        if (cancel.stop_requested()) {
            return std::nullopt;
        }

        FeatureState state;
        state.id = feature.id;

        // A step that fails: error on it, its last valid result (if it had one) stays visible.
        const auto fail = [&](FeatureIssue issue) {
            state.status = FeatureStatus::error;
            state.issue = std::move(issue);
            const auto good = cache.lastGood_.find(feature.id);
            if (good != cache.lastGood_.end()) {
                publish(feature, *good->second, /*stale=*/true);
                state.stale = true;
            }
            result.features.push_back(std::move(state));
        };

        if (feature.suppressed) {
            state.status = FeatureStatus::suppressed;
            result.features.push_back(std::move(state));
            continue;
        }
        const FeatureType* type = registry.find(feature.type);
        if (type == nullptr) {
            fail(issueOf(FeatureIssueCode::unknownType, feature.type));
            continue;
        }

        if (auto issue = resolveValues(feature, *type, result.measures, lookup,
                                       document.settings().defaultUnitMillimetres, state.params)) {
            fail(std::move(*issue));
            continue;
        }
        state.description = type->describeStep(feature, state.params);

        // Bodies it works on.
        std::vector<const BodyResult*> inputBodies;
        std::vector<FeatureOutput> inputs;
        bool inputIssue = false;
        bool staleInput = false;
        for (const BodyId& id : feature.inputs) {
            const auto found = result.bodies.find(id);
            if (found == result.bodies.end()) {
                const bool known = document.findBody(id) != nullptr;
                fail(issueOf(known ? FeatureIssueCode::inputFailed
                                   : FeatureIssueCode::inputMissing));
                inputIssue = true;
                break;
            }
            staleInput = staleInput || found->second.stale;
            inputBodies.push_back(&found->second);
            FeatureOutput input;
            input.body = id;
            input.solid = found->second.solid;
            for (const BodyFace& face : found->second.faces) {
                input.faces.push_back({face.faceId, face.name});
            }
            inputs.push_back(std::move(input));
        }
        if (inputIssue) {
            continue;
        }

        if (auto issue = type->validate(state.params)) {
            fail(std::move(*issue));
            continue;
        }

        // Remembered, or ask the kernel.
        const std::string key = cacheKey(feature, *type, state.params, inputBodies);
        const Entry* entry = cache.find(key);
        if (entry != nullptr) {
            ++result.remembered;
        } else {
            std::vector<Output> outputs;
            try {
                const ExecuteContext context{kernel, cancel};
                FeatureResult made = type->execute({feature, state.params, inputs}, context);
                if (cancel.stop_requested()) {
                    return std::nullopt; // a step that gave up because it was asked to
                }
                if (made.outputs.size() != feature.outputs.size()) {
                    fail(issueOf(FeatureIssueCode::kernelFailed, {},
                                 "the step made " + std::to_string(made.outputs.size()) +
                                     " bodies, expected " +
                                     std::to_string(feature.outputs.size())));
                    continue;
                }
                bool violated = false;
                for (FeatureOutput& output : made.outputs) {
                    const geo::SolidReport report = kernel.measure(output.solid);
                    if (auto issue = checkNamingContract(output, report)) {
                        fail(std::move(*issue));
                        violated = true;
                        break;
                    }
                    outputs.push_back({std::move(output.solid), std::move(output.faces)});
                }
                if (violated) {
                    continue;
                }
            } catch (const geo::GeometryError& error) {
                if (cancel.stop_requested()) {
                    return std::nullopt;
                }
                fail(issueFromKernel(error));
                continue;
            } catch (const std::exception& error) {
                if (cancel.stop_requested()) {
                    return std::nullopt;
                }
                fail(issueOf(FeatureIssueCode::kernelFailed, {}, error.what()));
                continue;
            }
            entry = &cache.store(key, std::move(outputs));
            ++result.executed;
            state.computed = true;
        }

        publish(feature, *entry, /*stale=*/false);
        if (staleInput) {
            // Calculated, but from the last valid result of an earlier step: shown, with a warning.
            state.status = FeatureStatus::warning;
            state.issue = issueOf(FeatureIssueCode::inputFailed);
        } else {
            cache.lastGood_[feature.id] = std::make_shared<const Entry>(*entry);
        }
        result.features.push_back(std::move(state));
    }

    if (cancel.stop_requested()) {
        return std::nullopt;
    }
    return result;
}

// --- worker ------------------------------------------------------------------------------

RecomputeEngine::RecomputeEngine(const FeatureRegistry& registry, const geo::GeometryKernel& kernel,
                                 RecomputeObserver observer)
    : registry_(registry), kernel_(kernel), observer_(std::move(observer)),
      worker_([this] { run(); }) {}

RecomputeEngine::~RecomputeEngine() {
    {
        const std::lock_guard lock(mutex_);
        quit_ = true;
        running_.request_stop(); // a long calculation gives up
    }
    wake_.notify_all();
    worker_.join();
}

void RecomputeEngine::request(Document snapshot) {
    {
        const std::lock_guard lock(mutex_);
        pending_ = std::move(snapshot);
        running_.request_stop(); // whatever runs now is out of date
    }
    wake_.notify_one();
}

std::shared_ptr<const RecomputeResult> RecomputeEngine::latest() const {
    const std::lock_guard lock(mutex_);
    return latest_;
}

void RecomputeEngine::waitIdle() {
    std::unique_lock lock(mutex_);
    idle_.wait(lock, [this] { return !busy_ && !pending_.has_value(); });
}

void RecomputeEngine::run() {
    const auto emit = [this](const RecomputeEvent& event) {
        if (!observer_) {
            return;
        }
        try {
            observer_(event);
        } catch (...) { // a faulty listener must not stop the worker
        }
    };

    for (;;) {
        Document snapshot;
        CancelSource mine;
        {
            std::unique_lock lock(mutex_);
            busy_ = false;
            idle_.notify_all();
            wake_.wait(lock, [this] { return pending_.has_value() || quit_; });
            if (quit_) {
                return;
            }
            snapshot = std::move(*pending_);
            pending_.reset();
            running_ = CancelSource();
            mine = running_;
            busy_ = true;
        }

        RecomputeEvent started;
        started.type = RecomputeEventType::started;
        started.revision = snapshot.revision();
        emit(started);

        std::optional<RecomputeResult> result =
            recompute(snapshot, registry_, kernel_, cache_, mine.token());
        if (!result || mine.stop_requested()) {
            RecomputeEvent cancelled;
            cancelled.type = RecomputeEventType::cancelled;
            cancelled.revision = snapshot.revision();
            emit(cancelled);
            continue;
        }

        const auto finished = std::make_shared<const RecomputeResult>(std::move(*result));
        const std::shared_ptr<const RecomputeResult> before = latest();
        for (const FeatureState& state : finished->features) {
            const FeatureState* old = before ? before->state(state.id) : nullptr;
            if (old == nullptr || !sameState(*old, state)) {
                RecomputeEvent changed;
                changed.type = RecomputeEventType::featureStatusChanged;
                changed.revision = finished->revision;
                changed.state = state;
                emit(changed);
            }
        }
        {
            const std::lock_guard lock(mutex_);
            latest_ = finished;
        }
        RecomputeEvent done;
        done.type = RecomputeEventType::finished;
        done.revision = finished->revision;
        done.result = finished;
        emit(done);
    }
}

} // namespace fission::core
