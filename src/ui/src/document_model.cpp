// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/ui/document_model.h"

#include <QColor>
#include <QCoreApplication>
#include <QDebug>
#include <QMetaObject>
#include <QStringList>
#include <QVariantMap>

#include <algorithm>
#include <cmath>
#include <set>

#include "fission/core/commands.h"
#include "fission/features/builtin.h"
#include "fission/geo/error.h"
#include "fission/render/detail.h"
#include "fission/ui/numbers.h"
#include "fission/ui/printer_catalog.h"

namespace fission::ui {

namespace {
/// A finer tessellation that would need more triangles than this is not used (the view would
/// get slow); the body keeps its basic mesh.
constexpr std::size_t kMaxDetailTriangles = 3'000'000;
} // namespace

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
    case core::FeatureIssueCode::textGlyphsMissing:
        return "textGlyphsMissing";
    case core::FeatureIssueCode::fontNotFound:
        return "fontNotFound";
    case core::FeatureIssueCode::fontStyleMissing:
        return "fontStyleMissing";
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
    m_detailPool.setMaxThreadCount(1);
    m_detailTimer.setSingleShot(true);
    m_detailTimer.setInterval(120); // zooming in steps: wait until it settles
    connect(&m_detailTimer, &QTimer::timeout, this, [this] { startDetailJob(); });
    m_engine = std::make_unique<core::RecomputeEngine>(
        m_registry, *m_kernel, [this](const core::RecomputeEvent& event) { onEngineEvent(event); });
    // Reading the computer's fonts takes a moment: not on the interface thread (rule 9).
    m_detailPool.start([this, kernel = m_kernel] {
        QStringList families;
        for (const std::string& family : kernel->fontFamilies()) {
            families.push_back(toQString(family));
        }
        QMetaObject::invokeMethod(
            this,
            [this, families = std::move(families)]() mutable {
                m_fontFamilies = std::move(families);
                emit fontsChanged();
            },
            Qt::QueuedConnection);
    });
}

DocumentModel::~DocumentModel() {
    m_detailCurrent->store(~std::uint64_t{0}); // a running job stops at the next body
    m_detailPool.waitForDone();
    m_engine.reset(); // joins the worker; nothing may call back after this
}

namespace {
// How much of a hole one sees (0 - 255): enough to grab it, not enough to take it for a part.
constexpr int kHoleAlpha = 110;

const PrinterProfile& generic() {
    return printerCatalog().front();
}
} // namespace

QString DocumentModel::printerName() const {
    const PrinterProfile* printer = findPrinter(m_printerId);
    return printer != nullptr ? printer->name : QString();
}
double DocumentModel::bedX() const {
    const PrinterProfile* printer = findPrinter(m_printerId);
    return (printer != nullptr ? *printer : generic()).bedX;
}
double DocumentModel::bedY() const {
    const PrinterProfile* printer = findPrinter(m_printerId);
    return (printer != nullptr ? *printer : generic()).bedY;
}
double DocumentModel::bedZ() const {
    const PrinterProfile* printer = findPrinter(m_printerId);
    return (printer != nullptr ? *printer : generic()).bedZ;
}
double DocumentModel::nozzle() const {
    const PrinterProfile* printer = findPrinter(m_printerId);
    return (printer != nullptr ? *printer : generic()).nozzle;
}

QVariantList DocumentModel::printers() const {
    QVariantList list;
    for (const PrinterProfile& printer : printerCatalog()) {
        list.push_back(QVariantMap{{QStringLiteral("id"), printer.id},
                                   {QStringLiteral("name"), printer.name},
                                   {QStringLiteral("bedX"), printer.bedX},
                                   {QStringLiteral("bedY"), printer.bedY},
                                   {QStringLiteral("bedZ"), printer.bedZ},
                                   {QStringLiteral("nozzle"), printer.nozzle}});
    }
    return list;
}

bool DocumentModel::setPrinter(const QString& id) {
    if (findPrinter(id) == nullptr) {
        return false;
    }
    if (id != m_printerId) {
        m_printerId = id;
        emit printerChanged();
        rebuildScene(); // the print bed has another size
    }
    return true;
}

void DocumentModel::setDetailLevel(int level) {
    level = std::clamp(level, 0, render::kMaxDetailLevel);
    if (level == m_detailLevel) {
        return;
    }
    m_detailLevel = level;
    m_detailTimer.start();
}

void DocumentModel::startDetailJob() {
    // A newer wish replaces a running job: the job looks at the generation between bodies.
    const std::uint64_t generation = ++m_detailGeneration;
    m_detailCurrent->store(generation);
    const int level = m_detailLevel;
    if (level == 0 || !m_result) {
        // Back to the basic meshes: nothing to calculate, they are still in the cache.
        applyDetail(generation, 0, {});
        return;
    }
    struct Work {
        std::uint64_t key;
        geo::Solid solid;
    };
    std::vector<Work> work;
    for (const core::Body& body : m_history.document().bodies()) {
        const auto made = m_result->bodies.find(body.id);
        if (body.visible && made != m_result->bodies.end()) {
            work.push_back({meshKey(made->second), made->second.solid});
        }
    }
    const auto kernel = m_kernel;
    const auto current = m_detailCurrent;
    m_detailPool.start([this, kernel, current, generation, level, work = std::move(work)] {
        std::map<std::uint64_t, std::shared_ptr<const geo::TriangleMesh>> meshes;
        const geo::TessellationParams params = render::tessellationForDetail(level);
        for (const Work& item : work) {
            if (current->load() != generation) {
                return; // somebody asked for something else meanwhile
            }
            try {
                auto mesh = std::make_shared<const geo::TriangleMesh>(
                    kernel->tessellate(item.solid, params));
                // A body that would need too many triangles keeps its basic mesh.
                if (mesh->triangleCount() <= kMaxDetailTriangles) {
                    meshes.emplace(item.key, std::move(mesh));
                }
            } catch (const geo::GeometryError& error) {
                qWarning() << "a body could not be tessellated finer:" << error.what();
            }
        }
        if (current->load() != generation) {
            return;
        }
        QMetaObject::invokeMethod(
            this,
            [this, generation, level, meshes = std::move(meshes)]() mutable {
                applyDetail(generation, level, std::move(meshes));
            },
            Qt::QueuedConnection);
    });
}

void DocumentModel::applyDetail(
    std::uint64_t generation, int level,
    std::map<std::uint64_t, std::shared_ptr<const geo::TriangleMesh>> meshes) {
    if (generation != m_detailGeneration) {
        return; // outdated
    }
    m_detailMeshes = std::move(meshes);
    m_shownDetailLevel = m_detailMeshes.empty() ? 0 : level;
    rebuildScene();
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
                m_meshes.emplace(key, std::make_shared<const geo::TriangleMesh>(
                                          m_kernel->tessellate(body.solid, {})));
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
        // Finer meshes of bodies that changed are gone; new bodies get theirs again below.
        for (auto it = m_detailMeshes.begin(); it != m_detailMeshes.end();) {
            it = alive.contains(it->first) ? std::next(it) : m_detailMeshes.erase(it);
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
    {
        auto names = std::make_shared<ElementNameMap>();
        const std::lock_guard lock(m_meshMutex);
        for (const auto& [id, body] : m_result->bodies) {
            const auto mesh = m_meshes.find(meshKey(body));
            if (mesh != m_meshes.end()) {
                names->emplace(id, core::ElementNames::build(body.faces, *mesh->second));
            }
        }
        m_elementNames = std::move(names);
    }
    rebuildLists();
    rebuildScene();
    emit elementsChanged();
    setCalculating(false);
    if (m_detailLevel > 0) {
        m_detailTimer.start(); // new or changed bodies get their fine mesh in the background
    }
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
            const std::uint64_t key = meshKey(made->second);
            const auto mesh = m_meshes.find(key);
            if (mesh == m_meshes.end()) {
                continue;
            }
            render::RenderBody drawn;
            drawn.id = body.id.value;
            // The finer mesh of the current zoom if it is ready, otherwise the basic one.
            const auto fine = m_detailMeshes.find(key);
            if (fine != m_detailMeshes.end()) {
                drawn.mesh = fine->second;
                drawn.meshVersion = key * 16 + static_cast<std::uint64_t>(m_shownDetailLevel);
            } else {
                drawn.mesh = mesh->second;
                drawn.meshVersion = key * 16;
            }
            drawn.color = QColor(body.color.red, body.color.green, body.color.blue);
            if (body.hole) {
                drawn.color.setAlpha(kHoleAlpha); // a ghost: it is a tool, not a part
            }
            scene->bodies.push_back(std::move(drawn));
        }
    }
    scene->bed.sizeX = static_cast<float>(bedX());
    scene->bed.sizeY = static_cast<float>(bedY());
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
        QVariantMap words; // typed words of the step (text, font) for its sentence
        for (const auto& [name, text] : feature.texts) {
            words[toQString(name)] = toQString(text);
        }
        entry[QStringLiteral("texts")] = words;
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
        entry[QStringLiteral("hole")] = body.hole;
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
    m_stepTexts = buildStepTexts();
    m_stepHandles = buildStepHandles();

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
        entry[QStringLiteral("kind")] =
            spec.kind == core::QuantityKind::angle   ? QStringLiteral("angle")
            : spec.kind == core::QuantityKind::plain ? QStringLiteral("plain") // a count: no unit
                                                     : QStringLiteral("length");
        values.push_back(entry);
    }
    return values;
}

QVariantList DocumentModel::buildStepTexts() const {
    QVariantList texts;
    const auto& features = m_history.document().features();
    if (m_currentStep < 0 || m_currentStep >= static_cast<int>(features.size())) {
        return texts;
    }
    const core::Feature& feature = features[static_cast<std::size_t>(m_currentStep)];
    const core::FeatureType* type = m_registry.find(feature.type);
    if (type == nullptr) {
        return texts;
    }
    for (const core::TextFieldSpec& spec : type->texts()) {
        const auto given = feature.texts.find(spec.name);
        QVariantMap entry;
        entry[QStringLiteral("name")] = toQString(spec.name);
        entry[QStringLiteral("value")] =
            toQString(given != feature.texts.end() ? given->second : spec.defaultValue);
        entry[QStringLiteral("kind")] = spec.name == "font"    ? QStringLiteral("font")
                                        : spec.name == "style" ? QStringLiteral("style")
                                                               : QStringLiteral("text");
        texts.push_back(entry);
    }
    return texts;
}

QVariantList DocumentModel::buildStepHandles() const {
    QVariantList handles;
    const auto& features = m_history.document().features();
    if (m_currentStep < 0 || m_currentStep >= static_cast<int>(features.size()) || !m_result) {
        return handles;
    }
    const core::Feature& feature = features[static_cast<std::size_t>(m_currentStep)];
    const core::FeatureType* type = m_registry.find(feature.type);
    const core::FeatureState* state = m_result->state(feature.id);
    if (type == nullptr || state == nullptr || feature.suppressed ||
        state->status == core::FeatureStatus::error ||
        state->status == core::FeatureStatus::suppressed || !state->description) {
        return handles;
    }
    for (const core::HandleSpec& spec : type->handles(state->params)) {
        const auto value = state->params.find(spec.parameter);
        if (value == state->params.end()) {
            continue;
        }
        QVariantMap entry;
        entry[QStringLiteral("parameter")] = toQString(spec.parameter);
        entry[QStringLiteral("kind")] = spec.kind == core::HandleKind::radius
                                            ? QStringLiteral("radius")
                                            : QStringLiteral("distance");
        entry[QStringLiteral("x")] = spec.origin.x;
        entry[QStringLiteral("y")] = spec.origin.y;
        entry[QStringLiteral("z")] = spec.origin.z;
        entry[QStringLiteral("dx")] = spec.direction.x;
        entry[QStringLiteral("dy")] = spec.direction.y;
        entry[QStringLiteral("dz")] = spec.direction.z;
        entry[QStringLiteral("sign")] = spec.sign;
        entry[QStringLiteral("travel")] = spec.travel;
        entry[QStringLiteral("value")] = value->second.value;
        handles.push_back(entry);
    }
    return handles;
}

bool DocumentModel::acceptsValue(const QString& parameter, double value) const {
    const auto& features = m_history.document().features();
    if (m_currentStep < 0 || m_currentStep >= static_cast<int>(features.size()) || !m_result) {
        return false;
    }
    const core::Feature& feature = features[static_cast<std::size_t>(m_currentStep)];
    const core::FeatureType* type = m_registry.find(feature.type);
    const core::FeatureState* state = m_result->state(feature.id);
    if (type == nullptr || state == nullptr) {
        return false;
    }
    core::ResolvedParams params = state->params;
    const auto found = params.find(parameter.toStdString());
    if (found == params.end()) {
        return false;
    }
    found->second.value = value;
    return !type->validate(params).has_value();
}

QString DocumentModel::placingBodyId() const {
    if (!m_placing) {
        return {};
    }
    const core::Feature* feature = m_history.document().findFeature(m_placingFeature);
    return feature != nullptr && !feature->outputs.empty()
               ? toQString(feature->outputs.front().value)
               : QString();
}

QString DocumentModel::bodyIdAt(int index) const {
    const auto& bodies = m_history.document().bodies();
    return index >= 0 && index < static_cast<int>(bodies.size())
               ? toQString(bodies[static_cast<std::size_t>(index)].id.value)
               : QString();
}

int DocumentModel::bodyIndexOf(const QString& id) const {
    const auto& bodies = m_history.document().bodies();
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        if (toQString(bodies[i].id.value) == id) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int DocumentModel::stepOfBody(const QString& id) const {
    for (const core::Body& body : m_history.document().bodies()) {
        if (toQString(body.id.value) == id) {
            return stepIndexOf(body.createdBy);
        }
    }
    return -1;
}

QString DocumentModel::bodyOfStep(int step) const {
    const auto& features = m_history.document().features();
    if (step < 0 || step >= static_cast<int>(features.size()) ||
        features[static_cast<std::size_t>(step)].outputs.empty()) {
        return {};
    }
    return toQString(features[static_cast<std::size_t>(step)].outputs.front().value);
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
    m_stepTexts = buildStepTexts();
    m_stepHandles = buildStepHandles();
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

QStringList DocumentModel::fontStyles(const QString& family) const {
    QStringList names;
    for (const geo::FontStyle style : m_kernel->fontStyles(family.toStdString())) {
        names.push_back(QString::fromLatin1(geo::fontStyleName(style)));
    }
    return names;
}

bool DocumentModel::setStepText(int index, const QString& name, const QString& text) {
    const auto& features = m_history.document().features();
    if (index < 0 || index >= static_cast<int>(features.size())) {
        return false;
    }
    const core::FeatureId id = features[static_cast<std::size_t>(index)].id;
    const std::uint64_t before = m_history.document().revision();
    if (!m_history.execute(core::makeSetFeatureText(id, name.toStdString(), text.toStdString()))) {
        return false;
    }
    if (m_history.document().revision() != before) {
        documentChanged();
    }
    return true;
}

void DocumentModel::setBodyHole(int index, bool hole) {
    const auto& bodies = m_history.document().bodies();
    if (index < 0 || index >= static_cast<int>(bodies.size())) {
        return;
    }
    const std::uint64_t before = m_history.document().revision();
    if (m_history.execute(
            core::makeSetBodyHole(bodies[static_cast<std::size_t>(index)].id, hole)) &&
        m_history.document().revision() != before) {
        documentChanged(false); // the shape is the same, only how it is drawn changes
    }
}

// --- putting shapes down ---------------------------------------------------------------------

bool DocumentModel::setValueQuietly(const core::FeatureId& id, const char* name, double value) {
    // 'g' with enough digits: the number reads back exactly, and "." is understood everywhere.
    return m_history.execute(
        core::makeSetFeatureValue(id, name, QString::number(value, 'g', 15).toStdString()));
}

int DocumentModel::startPlacing(const QString& type, bool asHole, double x, double y, double z) {
    const core::FeatureType* featureType = m_registry.find(type.toStdString());
    if (featureType == nullptr) {
        return -1;
    }
    if (m_placing) {
        cancelPlacing();
    }
    // Everything until finishPlacing is one step: "Sphere added".
    m_history.beginTransaction({"addFeature", {{"type", type.toStdString()}}});
    const auto id = m_history.addFeature(core::defaultDraft(*featureType));
    if (!id) {
        m_history.cancelTransaction();
        return -1;
    }
    setValueQuietly(*id, "x", x);
    setValueQuietly(*id, "y", y);
    setValueQuietly(*id, "z", z);
    if (asHole) {
        const core::Feature* feature = m_history.document().findFeature(*id);
        if (feature != nullptr && !feature->outputs.empty()) {
            m_history.execute(core::makeSetBodyHole(feature->outputs.front(), true));
        }
    }
    m_placing = true;
    m_placingFeature = *id;
    m_placingZ = z;
    m_placingSurface.clear();
    const int step = stepIndexOf(*id);
    m_currentFeature = *id;
    documentChanged();
    emit placingChanged();
    return step;
}

void DocumentModel::placeAt(double x, double y, double z, const QString& surfaceBody) {
    if (!m_placing) {
        return;
    }
    const core::Feature* feature = m_history.document().findFeature(m_placingFeature);
    if (feature == nullptr) {
        return;
    }
    const std::uint64_t before = m_history.document().revision();
    setValueQuietly(m_placingFeature, "x", x);
    setValueQuietly(m_placingFeature, "y", y);
    setValueQuietly(m_placingFeature, "z", z);
    m_placingZ = z;
    m_placingSurface = surfaceBody;
    if (m_history.document().revision() != before) {
        documentChanged();
    }
}

bool DocumentModel::finishPlacing() {
    if (!m_placing) {
        return false;
    }
    m_history.commitTransaction();
    m_placing = false;
    const int placedStep = stepIndexOf(m_placingFeature);
    const core::Feature* placed = m_history.document().findFeature(m_placingFeature);
    const QString placedType = placed != nullptr ? toQString(placed->type) : QString();
    m_workplaneZ = m_placingZ;
    m_workplaneBody = m_placingSurface;
    emit historyChanged();
    emit placingChanged();
    emit workplaneChanged();
    emit shapePlaced(placedStep, placedType);
    return true;
}

void DocumentModel::cancelPlacing() {
    if (!m_placing) {
        return;
    }
    m_history.cancelTransaction();
    m_placing = false;
    documentChanged();
    emit placingChanged();
}

// The first place near the middle of the bed where a shape of about 30 mm does not stand on
// another body: the middle, then a ring of places around it.
std::pair<double, double> DocumentModel::freeSpot() const {
    constexpr double kStep = 30.0;
    struct Box {
        double minX, minY, maxX, maxY;
    };
    std::vector<Box> taken;
    if (m_scene) {
        for (const render::RenderBody& body : m_scene->bodies) {
            const geo::TriangleMesh& mesh = body.geometry();
            if (mesh.empty()) {
                continue;
            }
            Box box{1e30, 1e30, -1e30, -1e30};
            for (const geo::Point3f& p : mesh.positions) {
                box.minX = std::min(box.minX, static_cast<double>(p.x));
                box.minY = std::min(box.minY, static_cast<double>(p.y));
                box.maxX = std::max(box.maxX, static_cast<double>(p.x));
                box.maxY = std::max(box.maxY, static_cast<double>(p.y));
            }
            taken.push_back(box);
        }
    }
    const auto free = [&](double x, double y) {
        for (const Box& box : taken) {
            if (x + kStep / 2 > box.minX && x - kStep / 2 < box.maxX && y + kStep / 2 > box.minY &&
                y - kStep / 2 < box.maxY) {
                return false;
            }
        }
        return true;
    };
    for (int ring = 0; ring < 8; ++ring) {
        for (int dy = -ring; dy <= ring; ++dy) {
            for (int dx = -ring; dx <= ring; ++dx) {
                if (std::max(std::abs(dx), std::abs(dy)) != ring) {
                    continue;
                }
                const double x = dx * kStep;
                const double y = dy * kStep;
                if (std::abs(x) < bedX() / 2 - kStep / 2 && std::abs(y) < bedY() / 2 - kStep / 2 &&
                    free(x, y)) {
                    return {x, y};
                }
            }
        }
    }
    return {0.0, 0.0};
}

int DocumentModel::addShape(const QString& type, bool asHole) {
    const auto [x, y] = freeSpot();
    const int step = startPlacing(type, asHole, x, y, 0.0);
    if (step >= 0) {
        finishPlacing();
    }
    return step;
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
