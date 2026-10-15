// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/ui/document_model.h"

#include <QColor>
#include <QCoreApplication>
#include <QDebug>
#include <QMetaObject>
#include <QVariantMap>

#include <algorithm>
#include <cmath>
#include <set>

#include "fission/features/builtin.h"
#include "fission/geo/error.h"
#include "fission/ui/numbers.h"

namespace fission::ui {

namespace {

QString toQString(const std::string& text) {
    return QString::fromStdString(text);
}

QString stateName(const core::FeatureState* state) {
    if (state == nullptr) {
        return QStringLiteral("pending");
    }
    switch (state->status) {
    case core::FeatureStatus::ok:
        return QStringLiteral("ok");
    case core::FeatureStatus::warning:
        return QStringLiteral("warning");
    case core::FeatureStatus::error:
        return QStringLiteral("fails");
    case core::FeatureStatus::suppressed:
        return QStringLiteral("off");
    }
    return QStringLiteral("pending");
}

const char* issueCodeName(core::FeatureIssueCode code) {
    switch (code) {
    case core::FeatureIssueCode::unknownType:
        return "unknownType";
    case core::FeatureIssueCode::parameterInvalid:
        return "parameterInvalid";
    case core::FeatureIssueCode::parameterOutOfRange:
        return "parameterOutOfRange";
    case core::FeatureIssueCode::inputMissing:
        return "inputMissing";
    case core::FeatureIssueCode::inputFailed:
        return "inputFailed";
    case core::FeatureIssueCode::namingContractViolated:
        return "namingContractViolated";
    case core::FeatureIssueCode::kernelRejected:
        return "kernelRejected";
    case core::FeatureIssueCode::kernelResultInvalid:
        return "kernelResultInvalid";
    case core::FeatureIssueCode::kernelFailed:
        return "kernelFailed";
    case core::FeatureIssueCode::cancelled:
        return "cancelled";
    }
    return "kernelFailed";
}

const char* expressionCodeName(core::ExpressionErrorCode code) {
    switch (code) {
    case core::ExpressionErrorCode::unknownName:
        return "unknownName";
    case core::ExpressionErrorCode::divisionByZero:
        return "divisionByZero";
    case core::ExpressionErrorCode::brokenDependency:
        return "brokenDependency";
    case core::ExpressionErrorCode::cycle:
        return "cycle";
    case core::ExpressionErrorCode::unknownUnit:
        return "unknownUnit";
    case core::ExpressionErrorCode::wrongKind:
        return "wrongKind";
    default:
        break;
    }
    return "invalid";
}

// "5" and "-2,5" are plain numbers; anything else is a formula the person should see again.
bool isPlainNumber(const std::string& text) {
    std::size_t i = 0;
    while (i < text.size() && text[i] == ' ') {
        ++i;
    }
    if (i < text.size() && (text[i] == '-' || text[i] == '+')) {
        ++i;
    }
    bool digit = false;
    bool separator = false;
    for (; i < text.size(); ++i) {
        const char c = text[i];
        if (c >= '0' && c <= '9') {
            digit = true;
        } else if ((c == '.' || c == ',') && !separator) {
            separator = true;
        } else if (c != ' ' || !digit) {
            return false;
        }
    }
    return digit;
}

} // namespace

DocumentModel::DocumentModel(std::shared_ptr<const geo::GeometryKernel> kernel, QObject* parent)
    : QObject(parent), m_kernel(std::move(kernel)) {
    features::registerBuiltinFeatures(m_registry);
    m_engine = std::make_unique<core::RecomputeEngine>(
        m_registry, *m_kernel, [this](const core::RecomputeEvent& event) { onEngineEvent(event); });
}

DocumentModel::~DocumentModel() {
    m_engine.reset(); // joins the worker; nothing may call back after this
}

void DocumentModel::addDemoBox() {
    const core::FeatureType* box = m_registry.find("box");
    if (box == nullptr) {
        return;
    }
    // The box is where the document starts, not something the person did: not an undo step.
    core::Document start;
    start.addFeature(core::defaultDraft(*box));
    m_history.reset(std::move(start));
    rebuildLists();
    requestRecompute();
}

void DocumentModel::waitForCalculation() {
    m_engine->waitIdle();
    QCoreApplication::sendPostedEvents(this, 0);
}

std::uint64_t DocumentModel::meshKey(const core::BodyResult& body) noexcept {
    return body.token * 256U + body.outputIndex;
}

void DocumentModel::requestRecompute() {
    setCalculating(true);
    m_engine->request(m_history.document());
}

void DocumentModel::setCalculating(bool calculating) {
    if (m_calculating != calculating) {
        m_calculating = calculating;
        emit calculatingChanged();
    }
}

// Worker thread.
void DocumentModel::onEngineEvent(const core::RecomputeEvent& event) {
    if (event.type != core::RecomputeEventType::finished || !event.result) {
        return;
    }
    // Tessellate here, not on the interface thread (rule 9). Bodies that did not change keep
    // their mesh from the last time.
    {
        const std::lock_guard lock(m_meshMutex);
        for (const auto& [id, body] : event.result->bodies) {
            const std::uint64_t key = meshKey(body);
            if (m_meshes.contains(key)) {
                continue;
            }
            try {
                m_meshes.emplace(key, m_kernel->tessellate(body.solid, {}));
            } catch (const geo::GeometryError& error) {
                qWarning() << "a body could not be tessellated:" << error.what();
            }
        }
    }
    QMetaObject::invokeMethod(
        this, [this, result = event.result] { applyResult(result); }, Qt::QueuedConnection);
}

// Interface thread.
void DocumentModel::applyResult(std::shared_ptr<const core::RecomputeResult> result) {
    if (m_result && result->revision < m_result->revision) {
        return;
    }
    m_result = std::move(result);
    {
        std::set<std::uint64_t> alive;
        for (const auto& [id, body] : m_result->bodies) {
            alive.insert(meshKey(body));
        }
        const std::lock_guard lock(m_meshMutex);
        for (auto it = m_meshes.begin(); it != m_meshes.end();) {
            it = alive.contains(it->first) ? std::next(it) : m_meshes.erase(it);
        }
    }
    if (m_numbers != nullptr) {
        core::NamedMeasures names;
        for (const core::MeasureResult& measure : m_result->measures.results) {
            if (measure.value && measure.value->dimension == core::Dimension{1, 0}) {
                names[measure.name] = measure.value->value;
            }
        }
        m_numbers->setNamedMeasures(std::move(names));
    }
    rebuildLists();
    rebuildScene();
    setCalculating(false);
}

void DocumentModel::rebuildScene() {
    auto scene = std::make_shared<render::RenderScene>();
    if (m_result) {
        const std::lock_guard lock(m_meshMutex);
        for (const core::Body& body : m_history.document().bodies()) {
            const auto made = m_result->bodies.find(body.id);
            if (!body.visible || made == m_result->bodies.end()) {
                continue;
            }
            const auto mesh = m_meshes.find(meshKey(made->second));
            if (mesh == m_meshes.end()) {
                continue;
            }
            render::RenderBody drawn;
            drawn.mesh = mesh->second;
            drawn.color = QColor(body.color.red, body.color.green, body.color.blue);
            scene->bodies.push_back(std::move(drawn));
        }
    }
    m_scene = scene;
    emit sceneReady(m_scene);
}

void DocumentModel::rebuildLists() {
    // Recipe
    QVariantList recipe;
    const auto& features = m_history.document().features();
    for (std::size_t i = 0; i < features.size(); ++i) {
        const core::Feature& feature = features[i];
        const core::FeatureState* state = m_result ? m_result->state(feature.id) : nullptr;
        QVariantMap entry;
        entry[QStringLiteral("index")] = static_cast<int>(i);
        entry[QStringLiteral("type")] = toQString(feature.type);
        entry[QStringLiteral("state")] =
            feature.suppressed ? QStringLiteral("off") : stateName(state);
        QVariantList values;
        if (state != nullptr && state->description) {
            for (const core::Quantity& value : state->description->values) {
                values.push_back(value.value);
            }
        }
        entry[QStringLiteral("values")] = values;
        if (state != nullptr && state->issue) {
            QVariantMap problem;
            problem[QStringLiteral("code")] =
                QString::fromLatin1(issueCodeName(state->issue->code));
            problem[QStringLiteral("field")] = toQString(state->issue->subject);
            if (state->issue->expression) {
                problem[QStringLiteral("detail")] =
                    QString::fromLatin1(expressionCodeName(state->issue->expression->code));
                problem[QStringLiteral("subject")] = toQString(state->issue->expression->subject);
            }
            entry[QStringLiteral("problem")] = problem;
        }
        recipe.push_back(entry);
    }
    m_recipe = recipe;

    // Bodies
    QVariantList bodies;
    std::map<std::string, int> counted;
    int index = 0;
    for (const core::Body& body : m_history.document().bodies()) {
        const core::Feature* maker = m_history.document().findFeature(body.createdBy);
        const std::string type = maker != nullptr ? maker->type : std::string();
        QVariantMap entry;
        entry[QStringLiteral("index")] = index++;
        entry[QStringLiteral("type")] = toQString(type);
        entry[QStringLiteral("number")] = ++counted[type];
        entry[QStringLiteral("name")] = toQString(body.name);
        entry[QStringLiteral("color")] =
            QColor(body.color.red, body.color.green, body.color.blue).name();
        entry[QStringLiteral("visible")] = body.visible;
        bodies.push_back(entry);
    }
    m_bodies = bodies;

    // The chosen step is followed by its id: undo may remove or add steps before it.
    const int stepNow = m_currentFeature.empty() ? -1 : stepIndexOf(m_currentFeature);
    if (stepNow < 0) {
        m_currentFeature = {};
    }
    if (stepNow != m_currentStep) {
        m_currentStep = stepNow;
        emit currentStepChanged();
    }
    m_stepValues = buildStepValues();

    emit recipeChanged();
    emit bodiesChanged();
    emit stepValuesChanged();
}

QVariantList DocumentModel::buildStepValues() const {
    QVariantList values;
    const auto& features = m_history.document().features();
    if (m_currentStep < 0 || m_currentStep >= static_cast<int>(features.size())) {
        return values;
    }
    const core::Feature& feature = features[static_cast<std::size_t>(m_currentStep)];
    const core::FeatureType* type = m_registry.find(feature.type);
    if (type == nullptr) {
        return values;
    }
    const core::FeatureState* state = m_result ? m_result->state(feature.id) : nullptr;
    for (const core::ParamSpec& spec : type->parameters()) {
        const auto given = feature.params.find(spec.name);
        const std::string& text =
            given != feature.params.end() ? given->second.expression : spec.defaultExpression;
        QVariantMap entry;
        entry[QStringLiteral("name")] = toQString(spec.name);
        entry[QStringLiteral("group")] = (spec.name == "x" || spec.name == "y" || spec.name == "z")
                                             ? QStringLiteral("position")
                                             : QStringLiteral("size");
        entry[QStringLiteral("expression")] = isPlainNumber(text) ? QString() : toQString(text);
        double number = 0.0;
        bool error = true;
        if (state != nullptr) {
            const auto found = state->params.find(spec.name);
            if (found != state->params.end()) {
                number = found->second.value;
                error = false;
            }
        }
        if (error && isPlainNumber(text)) {
            // Not calculated yet (or the step failed before this value): show what was typed.
            number = core::evaluateExpression(text, spec.kind).value_or(0.0);
        }
        entry[QStringLiteral("value")] = number;
        entry[QStringLiteral("error")] = error && !isPlainNumber(text);
        entry[QStringLiteral("kind")] = spec.kind == core::QuantityKind::angle
                                            ? QStringLiteral("angle")
                                            : QStringLiteral("length");
        values.push_back(entry);
    }
    return values;
}

int DocumentModel::stepIndexOf(const core::FeatureId& id) const {
    const auto index = m_history.document().indexOf(id);
    return index ? static_cast<int>(*index) : -1;
}

void DocumentModel::selectStep(int index) {
    const auto& features = m_history.document().features();
    const int count = static_cast<int>(features.size());
    const int next = (index >= 0 && index < count) ? index : -1;
    if (next == m_currentStep) {
        return;
    }
    m_currentStep = next;
    m_currentFeature = next >= 0 ? features[static_cast<std::size_t>(next)].id : core::FeatureId{};
    m_stepValues = buildStepValues();
    emit currentStepChanged();
    emit stepValuesChanged();
}

void DocumentModel::documentChanged(bool geometryChanged) {
    rebuildLists();
    emit historyChanged();
    rebuildScene(); // at once: what is shown or hidden follows the document without waiting
    if (geometryChanged) {
        requestRecompute();
    }
}

bool DocumentModel::setStepValue(int index, const QString& name, const QString& text) {
    const auto& features = m_history.document().features();
    if (index < 0 || index >= static_cast<int>(features.size())) {
        return false;
    }
    const core::FeatureId id = features[static_cast<std::size_t>(index)].id;
    const std::uint64_t before = m_history.document().revision();
    if (!m_history.execute(core::makeSetFeatureValue(id, name.toStdString(), text.toStdString()))) {
        return false;
    }
    if (m_history.document().revision() != before) { // typing the same value again does nothing
        documentChanged();
    }
    return true;
}

void DocumentModel::setBodyVisible(int index, bool visible) {
    const auto& bodies = m_history.document().bodies();
    if (index < 0 || index >= static_cast<int>(bodies.size())) {
        return;
    }
    const std::uint64_t before = m_history.document().revision();
    if (m_history.execute(
            core::makeSetBodyVisible(bodies[static_cast<std::size_t>(index)].id, visible)) &&
        m_history.document().revision() != before) {
        documentChanged(false); // geometry did not change: no recompute needed
    }
}

bool DocumentModel::undo() {
    if (!m_history.undo()) {
        return false;
    }
    documentChanged();
    return true;
}

bool DocumentModel::redo() {
    if (!m_history.redo()) {
        return false;
    }
    documentChanged();
    return true;
}

bool DocumentModel::goToHistory(int position) {
    if (position < 0 || !m_history.goTo(static_cast<std::size_t>(position))) {
        return false;
    }
    documentChanged();
    return true;
}

void DocumentModel::beginChange(const QString& field) {
    m_history.beginTransaction({"setValue", {{"field", field.toStdString()}}});
}

void DocumentModel::commitChange() {
    m_history.commitTransaction();
    emit historyChanged();
}

void DocumentModel::cancelChange() {
    m_history.cancelTransaction();
    documentChanged();
}

namespace {

QVariantMap describeStep(const core::CommandDescription& step) {
    QVariantMap map;
    map[QStringLiteral("key")] = toQString(step.key);
    for (const auto& [name, value] : step.values) {
        map[toQString(name)] = toQString(value);
    }
    return map;
}

} // namespace

QVariantMap DocumentModel::nextUndo() const {
    const auto step = m_history.nextUndo();
    return step ? describeStep(*step) : QVariantMap{};
}

QVariantMap DocumentModel::nextRedo() const {
    const auto step = m_history.nextRedo();
    return step ? describeStep(*step) : QVariantMap{};
}

QVariantList DocumentModel::history() const {
    QVariantList list;
    const auto steps = m_history.steps();
    for (std::size_t i = 0; i < steps.size(); ++i) {
        QVariantMap entry = describeStep(steps[i]);
        entry[QStringLiteral("done")] = i < m_history.position();
        list.push_back(entry);
    }
    return list;
}

} // namespace fission::ui
