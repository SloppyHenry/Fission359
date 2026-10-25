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
#include <vector>

#include "fission/core/commands.h"
#include "fission/core/cuts.h"
#include "fission/core/scaling.h"
#include "fission/features/builtin.h"
#include "fission/geo/error.h"
#include "fission/geo/seat.h"
#include "fission/interaction/align.h"
#include "fission/render/detail.h"
#include "fission/ui/numbers.h"
#include "fission/ui/printer_catalog.h"
#include "fission/ui/theme_tokens.h"

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
    case core::FeatureIssueCode::combineEmpty:
        return "combineEmpty";
    case core::FeatureIssueCode::combineNoOverlap:
        return "combineNoOverlap";
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
    for (const auto& [key, color] : filamentPalette()) {
        m_palette.push_back({key, color, false});
    }
    m_detailPool.setMaxThreadCount(1);
    m_detailTimer.setSingleShot(true);
    m_detailTimer.setInterval(120); // zooming in steps: wait until it settles
    connect(&m_detailTimer, &QTimer::timeout, this, [this] { startDetailJob(); });
    m_liveCutTimer.setSingleShot(true);
    m_liveCutTimer.setInterval(150);
    connect(&m_liveCutTimer, &QTimer::timeout, this, [this] { liveCuts(); });
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

// How a body takes part in the recipe once steps combine bodies (ADR-0011).
struct BodyPart {
    bool consumed = false;  ///< Part of another body's result: not shown as a body of its own.
    bool toolGhost = false; ///< A tool of a subtract: stays a ghost, still within reach.
    bool cut = false; ///< Its result had something cut out of it (the symbol behind its name).
};

// The word "mode" of a combine step, empty for any other step.
std::string combineMode(const core::Feature* step) {
    if (step == nullptr || step->type != "combine") {
        return {};
    }
    const auto mode = step->texts.find("mode");
    return mode != step->texts.end() ? mode->second : std::string();
}

// `result`: the last calculation. A step that has no result (it failed and never worked) takes
// nothing in: what it was to combine stays in view, and the step says what is wrong.
BodyPart partOf(const core::Document& document, const core::RecomputeResult* result,
                const core::Body& body) {
    BodyPart part;
    const auto consumer = document.consumerOf(body.id);
    const core::Feature* step = consumer ? document.findFeature(*consumer) : nullptr;
    const bool works = step != nullptr && !step->outputs.empty() && result != nullptr &&
                       result->bodies.contains(step->outputs.front());
    if (step != nullptr && works) {
        const bool isTool = !step->inputs.empty() && step->inputs.front() != body.id;
        if (isTool && combineMode(step) == "subtract") {
            part.toolGhost = true;
        } else {
            part.consumed = true;
        }
    }
    part.cut = combineMode(document.findFeature(body.createdBy)) == "subtract";
    return part;
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
        for (const Work& item : work) {
            if (current->load() != generation) {
                return; // somebody asked for something else meanwhile
            }
            try {
                // Finer with the zoom, coarser for a big body (render::kRelativeChord).
                const geo::TessellationParams params =
                    render::tessellationForDetail(level, kernel->bounds(item.solid).diagonal());
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
    // their mesh from the last time. The lock is held only to look and to store, never while the
    // kernel works: the interface thread reads the meshes and must not wait for a long mesh.
    for (const auto& [id, body] : event.result->bodies) {
        const std::uint64_t key = meshKey(body);
        {
            const std::lock_guard lock(m_meshMutex);
            if (m_meshes.contains(key)) {
                continue;
            }
        }
        try {
            // The basic mesh; a big body gets a coarser one (render::kRelativeChord).
            const geo::TessellationParams params =
                render::tessellationForDetail(0, m_kernel->bounds(body.solid).diagonal());
            auto mesh =
                std::make_shared<const geo::TriangleMesh>(m_kernel->tessellate(body.solid, params));
            const std::lock_guard lock(m_meshMutex);
            m_meshes.emplace(key, std::move(mesh));
        } catch (const geo::GeometryError& error) {
            qWarning() << "a body could not be tessellated:" << error.what();
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
        // The worker makes the mesh of a newer result before this one is applied here (changes
        // come faster than they are shown, e.g. while a handle is dragged). Its mesh must not be
        // swept away as "not in this result": that left the body out of the view and let go of
        // the selection. So the newest meshes that are not part of this result stay; only older
        // ones go (the keys grow with time).
        constexpr std::size_t kKeptNewer = 24;
        std::vector<std::uint64_t> others;
        for (const auto& [key, mesh] : m_meshes) {
            if (!alive.contains(key)) {
                others.push_back(key);
            }
        }
        for (std::size_t i = 0; i + kKeptNewer < others.size(); ++i) { // `others` is ascending
            m_meshes.erase(others[i]);
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
            const BodyPart part = partOf(m_history.document(), m_result.get(), body);
            if (!body.visible || made == m_result->bodies.end() || part.consumed) {
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
            if (body.hole || part.toolGhost) {
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
        entry[QStringLiteral("label")] = toQString(feature.label);
        entry[QStringLiteral("suppressed")] = feature.suppressed;
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
    // A result of combining keeps the name of the body it started as ("Box 1"), also its type
    // and number: a small symbol behind the name says it was cut (ADR-0011).
    struct Origin {
        std::string type;
        int number = 0;
        int part = 0; ///< 1, 2 ...: which part of a split it is (0 = not a part)
    };
    std::map<std::string, Origin> origin; // body id -> what it started as
    int index = 0;
    for (const core::Body& body : m_history.document().bodies()) {
        const core::Feature* maker = m_history.document().findFeature(body.createdBy);
        std::string type = maker != nullptr ? maker->type : std::string();
        int number = 0;
        int partNumber = 0;
        std::string name = body.name;
        const bool continues =
            maker != nullptr && (type == "combine" || type == "place" || type == "split") &&
            !maker->inputs.empty() && origin.contains(maker->inputs.front().value);
        if (continues) {
            const Origin& started = origin.at(maker->inputs.front().value);
            type = started.type;
            number = started.number;
            partNumber = started.part;
            if (maker->type == "split") {
                const auto at = std::find(maker->outputs.begin(), maker->outputs.end(), body.id);
                partNumber = static_cast<int>(at - maker->outputs.begin()) + 1;
            }
            const core::Body* first = m_history.document().findBody(maker->inputs.front());
            if (name.empty() && first != nullptr) {
                name = first->name;
            }
        } else {
            number = ++counted[type];
        }
        origin[body.id.value] = {type, number, partNumber};
        QVariantMap entry;
        entry[QStringLiteral("index")] = index++;
        entry[QStringLiteral("type")] = toQString(type);
        entry[QStringLiteral("number")] = number;
        entry[QStringLiteral("part")] = partNumber; // "Box 1, part 2" after a split
        entry[QStringLiteral("name")] = toQString(name);
        entry[QStringLiteral("color")] =
            QColor(body.color.red, body.color.green, body.color.blue).name();
        entry[QStringLiteral("visible")] = body.visible;
        entry[QStringLiteral("hole")] = body.hole;
        const BodyPart part = partOf(m_history.document(), m_result.get(), body);
        entry[QStringLiteral("consumed")] = part.consumed; // part of a result: not listed
        entry[QStringLiteral("cut")] = part.cut;
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
        const bool turn = spec.name == "rotX" || spec.name == "rotY" || spec.name == "rotZ";
        entry[QStringLiteral("group")] =
            (spec.name == "x" || spec.name == "y" || spec.name == "z") ? QStringLiteral("position")
            : turn                                                     ? QStringLiteral("rotation")
            : spec.name == "through"                                   ? QStringLiteral("through")
            : spec.name == "axis"                                      ? QStringLiteral("axis")
            : spec.name == "position"                                  ? QStringLiteral("cut")
            : spec.name == "anchorU" || spec.name == "anchorV"         ? QStringLiteral("hidden")
                                                                       : QStringLiteral("size");
        // The coloured letter in front of a place or a turn: x, y, z (empty for sizes).
        entry[QStringLiteral("axis")] = turn ? toQString(spec.name.substr(3, 1)).toLower()
                                        : (spec.name == "x" || spec.name == "y" || spec.name == "z")
                                            ? toQString(spec.name)
                                            : QString();
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
        entry[QStringLiteral("advanced")] = spec.advanced;
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
        // `mode` and `auto` are the program's business (combine steps), `keepTools` a switch.
        entry[QStringLiteral("kind")] =
            spec.name == "font"                                ? QStringLiteral("font")
            : spec.name == "style"                             ? QStringLiteral("style")
            : spec.name == "keepTools" || spec.name == "fuzzy" ? QStringLiteral("flag")
            : spec.name == "mode" || spec.name == "auto"       ? QStringLiteral("hidden")
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
    for (const core::HandleSpec& spec : type->placedHandles(state->params)) {
        const auto value = state->params.find(spec.parameter);
        if (value == state->params.end()) {
            continue;
        }
        QVariantMap entry;
        entry[QStringLiteral("parameter")] = toQString(spec.parameter);
        entry[QStringLiteral("kind")] = spec.kind == core::HandleKind::radius
                                            ? QStringLiteral("radius")
                                            : QStringLiteral("distance");
        entry[QStringLiteral("role")] =
            spec.role == core::HandleRole::rounding ? QStringLiteral("rounding")
            : spec.role == core::HandleRole::detail ? QStringLiteral("detail")
                                                    : QStringLiteral("size");
        entry[QStringLiteral("x")] = spec.origin.x;
        entry[QStringLiteral("y")] = spec.origin.y;
        entry[QStringLiteral("z")] = spec.origin.z;
        entry[QStringLiteral("dx")] = spec.direction.x;
        entry[QStringLiteral("dy")] = spec.direction.y;
        entry[QStringLiteral("dz")] = spec.direction.z;
        entry[QStringLiteral("sign")] = spec.sign;
        entry[QStringLiteral("travel")] = spec.travel;
        entry[QStringLiteral("rise")] = spec.rise;
        entry[QStringLiteral("riseDx")] = spec.riseDirection.x;
        entry[QStringLiteral("riseDy")] = spec.riseDirection.y;
        entry[QStringLiteral("riseDz")] = spec.riseDirection.z;
        entry[QStringLiteral("extent")] = spec.extent;
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
    auto command = core::makeSetFeatureValue(id, name.toStdString(), text.toStdString());
    // With holes in the document the change may move one into or out of a body: its cuts follow
    // in the same step.
    if (hasCuttingParts() && !m_history.inTransaction()) {
        if (!executeWithCuts(std::move(command), {"setValue", {{"field", name.toStdString()}}})) {
            return false;
        }
    } else {
        // Typed alone: the cuts follow in its step. During a drag they follow every so often.
        if (!m_history.execute(std::move(command))) {
            return false;
        }
        scheduleLiveCuts();
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

bool DocumentModel::setBodyName(int index, const QString& name) {
    const auto& bodies = m_history.document().bodies();
    const std::string trimmed = name.trimmed().toStdString();
    if (index < 0 || index >= static_cast<int>(bodies.size()) || trimmed.empty()) {
        return false;
    }
    const std::uint64_t before = m_history.document().revision();
    if (!m_history.execute(
            core::makeSetBodyName(bodies[static_cast<std::size_t>(index)].id, trimmed)) ||
        m_history.document().revision() == before) {
        return false;
    }
    documentChanged(false); // a name is no geometry
    return true;
}

QVariantList DocumentModel::palette() const {
    QVariantList list;
    const auto defaults = filamentPalette();
    for (const PaletteEntry& entry : m_palette) {
        // The word follows the colour: an adjusted swatch is no longer "Grey" when it is green.
        QString word = colourNameKey(entry.color);
        for (const auto& [key, color] : defaults) {
            if (!entry.custom && key == entry.key && color == entry.color) {
                word = key;
            }
        }
        list.push_back(QVariantMap{{QStringLiteral("key"), entry.key},
                                   {QStringLiteral("nameKey"), word},
                                   {QStringLiteral("color"), entry.color.name()},
                                   {QStringLiteral("custom"), entry.custom}});
    }
    return list;
}

bool DocumentModel::setPaletteColor(int index, const QString& color) {
    const QColor chosen(color);
    if (!chosen.isValid() || index < 0 || index >= static_cast<int>(m_palette.size()) ||
        m_palette[static_cast<std::size_t>(index)].color == chosen) {
        return false;
    }
    m_palette[static_cast<std::size_t>(index)].color = chosen;
    emit paletteChanged();
    return true;
}

int DocumentModel::addPaletteColor(const QString& color) {
    const QColor chosen(color);
    if (!chosen.isValid()) {
        return -1;
    }
    m_palette.push_back({QStringLiteral("custom"), chosen, true});
    emit paletteChanged();
    return static_cast<int>(m_palette.size()) - 1;
}

bool DocumentModel::removePaletteColor(int index) {
    if (index < 0 || index >= static_cast<int>(m_palette.size()) ||
        !m_palette[static_cast<std::size_t>(index)].custom) {
        return false;
    }
    m_palette.erase(m_palette.begin() + index);
    emit paletteChanged();
    return true;
}

bool DocumentModel::resetPaletteColor(int index) {
    if (index < 0 || index >= static_cast<int>(m_palette.size()) ||
        m_palette[static_cast<std::size_t>(index)].custom) {
        return false;
    }
    PaletteEntry& entry = m_palette[static_cast<std::size_t>(index)];
    for (const auto& [key, color] : filamentPalette()) {
        if (key == entry.key && entry.color != color) {
            entry.color = color;
            emit paletteChanged();
            return true;
        }
    }
    return false;
}

bool DocumentModel::setBodiesColor(const QVariantList& indexes, const QString& color) {
    const QColor chosen(color);
    const auto& bodies = m_history.document().bodies();
    if (!chosen.isValid() || indexes.isEmpty()) {
        return false;
    }
    const core::Color target{static_cast<std::uint8_t>(chosen.red()),
                             static_cast<std::uint8_t>(chosen.green()),
                             static_cast<std::uint8_t>(chosen.blue())};
    std::vector<core::BodyId> changing;
    for (const QVariant& entry : indexes) {
        const int index = entry.toInt();
        if (index >= 0 && index < static_cast<int>(bodies.size()) &&
            !(bodies[static_cast<std::size_t>(index)].color == target)) {
            changing.push_back(bodies[static_cast<std::size_t>(index)].id);
        }
    }
    if (changing.empty()) {
        return false;
    }
    const core::Body* first = m_history.document().findBody(changing.front());
    core::CommandDescription label{"colorBody", {{"name", first->name}}};
    if (changing.size() > 1) {
        label.values["count"] = std::to_string(changing.size());
    }
    m_history.beginTransaction(std::move(label));
    for (const core::BodyId& id : changing) {
        m_history.execute(core::makeSetBodyColor(id, target));
    }
    m_history.commitTransaction();
    documentChanged(false); // a colour is no geometry
    emit historyChanged();
    return true;
}

namespace {

// A cut the program keeps for holes: it goes with its hole, it is no step somebody built on.
bool isAutoCutStep(const core::Feature& step) {
    const auto word = [&](const char* name) {
        const auto found = step.texts.find(name);
        return found != step.texts.end() ? found->second : std::string();
    };
    return step.type == "combine" && word("mode") == "subtract" && word("auto") == "yes";
}

} // namespace

// The steps outside `steps` that work on what `steps` make, as positions.
static QVariantList dependentsOutside(const core::Document& document,
                                      const std::vector<core::FeatureId>& steps) {
    std::set<std::size_t> found;
    for (const core::FeatureId& id : steps) {
        for (const core::FeatureId& dependent : document.dependentsOf(id)) {
            if (std::find(steps.begin(), steps.end(), dependent) != steps.end()) {
                continue;
            }
            const core::Feature* step = document.findFeature(dependent);
            if (step != nullptr && !isAutoCutStep(*step)) {
                found.insert(*document.indexOf(dependent));
            }
        }
    }
    QVariantList list;
    for (const std::size_t position : found) {
        list.push_back(static_cast<int>(position));
    }
    return list;
}

static QVariantList positionsOf(const core::Document& document,
                                const std::vector<core::FeatureId>& steps) {
    std::set<std::size_t> found;
    for (const core::FeatureId& id : steps) {
        found.insert(*document.indexOf(id));
    }
    QVariantList list;
    for (const std::size_t position : found) {
        list.push_back(static_cast<int>(position));
    }
    return list;
}

QVariantMap DocumentModel::deletionOfStep(int index) const {
    const auto& features = m_history.document().features();
    QVariantMap plan;
    plan[QStringLiteral("steps")] = QVariantList{};
    plan[QStringLiteral("dependents")] = QVariantList{};
    if (index < 0 || index >= static_cast<int>(features.size())) {
        return plan;
    }
    const std::vector<core::FeatureId> steps{features[static_cast<std::size_t>(index)].id};
    plan[QStringLiteral("steps")] = positionsOf(m_history.document(), steps);
    plan[QStringLiteral("dependents")] = dependentsOutside(m_history.document(), steps);
    return plan;
}

QVariantMap DocumentModel::deletionOfBodies(const QStringList& bodyIds) const {
    const core::Document& document = m_history.document();
    std::vector<core::FeatureId> steps;
    const auto add = [&](const core::FeatureId& id) {
        if (std::find(steps.begin(), steps.end(), id) == steps.end()) {
            steps.push_back(id);
        }
    };
    for (const QString& id : bodyIds) {
        if (const core::Body* body = findBody(id)) {
            add(body->createdBy);
        }
    }
    // The shapes that are only there for a combine or split that goes: every step whose bodies
    // are used by nothing but the steps that go.
    for (bool grew = true; grew;) {
        grew = false;
        const std::size_t before = steps.size();
        const std::vector<core::FeatureId> now = steps;
        for (const core::FeatureId& id : now) {
            const core::Feature* step = document.findFeature(id);
            if (step == nullptr || (step->type != "combine" && step->type != "split")) {
                continue;
            }
            for (const core::BodyId& input : step->inputs) {
                const core::Body* made = document.findBody(input);
                if (made == nullptr) {
                    continue;
                }
                const auto users = document.dependentsOf(made->createdBy);
                const bool onlyForThem =
                    std::all_of(users.begin(), users.end(), [&](const core::FeatureId& user) {
                        const core::Feature* other = document.findFeature(user);
                        return std::find(steps.begin(), steps.end(), user) != steps.end() ||
                               (other != nullptr && isAutoCutStep(*other));
                    });
                if (onlyForThem) {
                    add(made->createdBy);
                }
            }
        }
        grew = steps.size() != before;
    }
    QVariantMap plan;
    plan[QStringLiteral("steps")] = positionsOf(document, steps);
    plan[QStringLiteral("dependents")] = dependentsOutside(document, steps);
    return plan;
}

bool DocumentModel::deleteSteps(const QVariantList& indexes) {
    const auto& features = m_history.document().features();
    std::vector<core::FeatureId> ids;
    for (const QVariant& entry : indexes) {
        const int index = entry.toInt();
        if (index >= 0 && index < static_cast<int>(features.size())) {
            ids.push_back(features[static_cast<std::size_t>(index)].id);
        }
    }
    if (ids.empty()) {
        return false;
    }
    const core::Feature& first = *m_history.document().findFeature(ids.front());
    core::CommandDescription label{"removeFeature", {{"type", first.type}}};
    if (ids.size() > 1) {
        label.values["count"] = std::to_string(ids.size());
    }
    m_history.beginTransaction(std::move(label));
    bool any = false;
    for (const core::FeatureId& id : ids) {
        any = m_history.execute(core::makeRemoveFeature(id)) || any;
    }
    if (!any) {
        m_history.cancelTransaction();
        return false;
    }
    finishTransaction(); // the cuts of holes follow, in the same step
    documentChanged();
    emit historyChanged();
    return true;
}

bool DocumentModel::setStepSuppressed(int index, bool suppressed) {
    const auto& features = m_history.document().features();
    if (index < 0 || index >= static_cast<int>(features.size())) {
        return false;
    }
    const core::Feature& step = features[static_cast<std::size_t>(index)];
    if (step.suppressed == suppressed) {
        return false;
    }
    core::CommandDescription label{suppressed ? "suppress" : "unsuppress", {{"type", step.type}}};
    if (!executeWithCuts(core::makeSetFeatureSuppressed(step.id, suppressed), std::move(label))) {
        return false;
    }
    documentChanged();
    return true;
}

bool DocumentModel::setStepLabel(int index, const QString& label) {
    const auto& features = m_history.document().features();
    if (index < 0 || index >= static_cast<int>(features.size())) {
        return false;
    }
    const core::Feature& step = features[static_cast<std::size_t>(index)];
    const std::string text = label.trimmed().toStdString();
    const std::uint64_t before = m_history.document().revision();
    if (!m_history.execute(core::makeSetFeatureLabel(step.id, text)) ||
        m_history.document().revision() == before) {
        return false;
    }
    documentChanged(false); // a name is no geometry
    return true;
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

void DocumentModel::scheduleLiveCuts() {
    if (m_history.inTransaction() && hasCuttingParts() && !m_liveCutTimer.isActive()) {
        m_liveCutTimer.start();
    }
}

void DocumentModel::liveCuts() {
    // Inside the open step of the drag, so that all of it stays one step to undo; never alone.
    if (m_history.inTransaction() && hasCuttingParts() &&
        core::reconcileCuts(m_history, m_registry, *m_kernel, m_cutCache)) {
        documentChanged();
    }
}

void DocumentModel::finishTransaction() {
    m_liveCutTimer.stop();
    const bool cut = core::reconcileCuts(m_history, m_registry, *m_kernel, m_cutCache);
    m_history.commitTransaction();
    if (cut) {
        documentChanged();
    }
}

bool DocumentModel::hasCuttingParts() const {
    const core::Document& document = m_history.document();
    return std::any_of(document.bodies().begin(), document.bodies().end(),
                       [](const core::Body& body) { return body.hole; }) ||
           std::any_of(document.features().begin(), document.features().end(),
                       [](const core::Feature& step) {
                           return step.type == "combine" && step.texts.count("auto") != 0 &&
                                  step.texts.at("auto") == "yes";
                       });
}

bool DocumentModel::executeWithCuts(std::unique_ptr<core::Command> command,
                                    core::CommandDescription label) {
    m_history.beginTransaction(std::move(label));
    const bool done = m_history.execute(std::move(command));
    if (!done) {
        m_history.cancelTransaction();
        return false;
    }
    finishTransaction();
    return true;
}

void DocumentModel::commitChange() {
    finishTransaction();
    emit historyChanged();
}

void DocumentModel::cancelChange() {
    m_liveCutTimer.stop();
    m_history.cancelTransaction();
    documentChanged();
}

std::optional<std::array<double, 3>> DocumentModel::currentPosition() const {
    const auto& steps = m_history.document().features();
    if (m_currentStep >= 0 && m_currentStep < static_cast<int>(steps.size()) &&
        steps[static_cast<std::size_t>(m_currentStep)].type == "combine") {
        return positionOfFeature(steps[static_cast<std::size_t>(m_currentStep)]);
    }
    std::array<double, 3> position{0.0, 0.0, 0.0};
    int found = 0;
    for (const QVariant& entry : m_stepValues) {
        const QVariantMap value = entry.toMap();
        const QString name = value[QStringLiteral("name")].toString();
        const int axis = name == QLatin1String("x")   ? 0
                         : name == QLatin1String("y") ? 1
                         : name == QLatin1String("z") ? 2
                                                      : -1;
        if (axis >= 0) {
            position[static_cast<std::size_t>(axis)] = value[QStringLiteral("value")].toDouble();
            ++found;
        }
    }
    if (found != 3) {
        return std::nullopt;
    }
    return position;
}

bool DocumentModel::beginMove() {
    const auto& features = m_history.document().features();
    if (!currentPosition() || m_currentStep < 0 ||
        m_currentStep >= static_cast<int>(features.size())) {
        return false;
    }
    const core::Feature& feature = features[static_cast<std::size_t>(m_currentStep)];
    core::CommandDescription label{
        "moveBody", {{"type", feature.type}, {"number", std::to_string(feature.number)}}};
    if (!feature.outputs.empty()) {
        if (const core::Body* body = m_history.document().findBody(feature.outputs.front())) {
            label.values["name"] = body->name;
        }
    }
    m_history.beginTransaction(std::move(label));
    return true;
}

bool DocumentModel::moveCurrent(const std::array<std::optional<double>, 3>& position) {
    const auto& features = m_history.document().features();
    if (!currentPosition() || m_currentStep < 0 ||
        m_currentStep >= static_cast<int>(features.size())) {
        return false;
    }
    const core::Feature& current = features[static_cast<std::size_t>(m_currentStep)];
    const core::FeatureId id = current.id;
    const std::uint64_t before = m_history.document().revision();
    static constexpr const char* kNames[3] = {"x", "y", "z"};
    bool done = true;
    if (current.type == "combine") {
        // A combined body: every part moves by the same distance, so the first part ends where
        // the person put it (the others keep their distance to it).
        std::vector<const core::Feature*> forms;
        collectForms(current, forms);
        const auto anchor = forms.empty() ? std::nullopt : liveValues(*forms.front());
        if (!anchor) {
            return false;
        }
        for (const core::Feature* form : forms) {
            const auto from = liveValues(*form);
            if (!from) {
                done = false;
                continue;
            }
            for (std::size_t axis = 0; axis < 3; ++axis) {
                if (position[axis]) {
                    const double target = (*from)[axis] + (*position[axis] - (*anchor)[axis]);
                    done = m_history.execute(core::makeSetFeatureValue(
                               form->id, kNames[axis],
                               QString::number(target, 'g', 12).toStdString())) &&
                           done;
                }
            }
        }
        if (m_history.document().revision() != before) {
            documentChanged();
            scheduleLiveCuts();
        }
        return done;
    }
    for (std::size_t axis = 0; axis < 3; ++axis) {
        if (position[axis]) {
            // 12 digits: snapped numbers read back as typed ("12.5", not "12.5000000001").
            done =
                m_history.execute(core::makeSetFeatureValue(
                    id, kNames[axis], QString::number(*position[axis], 'g', 12).toStdString())) &&
                done;
        }
    }
    if (m_history.document().revision() != before) {
        documentChanged();
        scheduleLiveCuts();
    }
    return done;
}

bool DocumentModel::groupCanTurn(const core::Feature& step) const {
    std::vector<const core::Feature*> forms;
    collectForms(step, forms);
    return !forms.empty() && std::all_of(forms.begin(), forms.end(), [](const core::Feature* form) {
        return form->params.contains("rotX") && form->params.contains("rotY") &&
               form->params.contains("rotZ");
    });
}

std::optional<geo::Angles> DocumentModel::currentTurn() const {
    const auto& steps = m_history.document().features();
    if (m_currentStep >= 0 && m_currentStep < static_cast<int>(steps.size()) &&
        steps[static_cast<std::size_t>(m_currentStep)].type == "combine") {
        // A combined body has no turn of its own: it is turned from "not turned" by the amount
        // dragged, all its parts about the middle of the whole.
        return groupCanTurn(steps[static_cast<std::size_t>(m_currentStep)])
                   ? std::optional<geo::Angles>(geo::Angles{})
                   : std::nullopt;
    }
    geo::Angles angles;
    int found = 0;
    for (const QVariant& entry : m_stepValues) {
        const QVariantMap value = entry.toMap();
        const QString name = value[QStringLiteral("name")].toString();
        double* target = name == QLatin1String("rotX")   ? &angles.x
                         : name == QLatin1String("rotY") ? &angles.y
                         : name == QLatin1String("rotZ") ? &angles.z
                                                         : nullptr;
        if (target != nullptr) {
            *target = value[QStringLiteral("value")].toDouble();
            ++found;
        }
    }
    if (found != 3) {
        return std::nullopt;
    }
    return angles;
}

std::optional<geo::Point3d> DocumentModel::currentTurnCentre() const {
    const auto& features = m_history.document().features();
    if (m_currentStep >= 0 && m_currentStep < static_cast<int>(features.size()) &&
        features[static_cast<std::size_t>(m_currentStep)].type == "combine") {
        const auto box = currentBodyBounds();
        if (!box || !currentTurn()) {
            return std::nullopt;
        }
        return geo::Point3d{(box->min.x + box->max.x) / 2.0, (box->min.y + box->max.y) / 2.0,
                            (box->min.z + box->max.z) / 2.0};
    }
    if (!currentTurn() || m_currentStep < 0 || m_currentStep >= static_cast<int>(features.size()) ||
        !m_result) {
        return std::nullopt;
    }
    const core::Feature& feature = features[static_cast<std::size_t>(m_currentStep)];
    const core::FeatureType* type = m_registry.find(feature.type);
    const core::FeatureState* state = m_result->state(feature.id);
    if (type == nullptr || state == nullptr || !state->description) {
        return std::nullopt;
    }
    return type->bodyCentre(state->params);
}

bool DocumentModel::beginTurn() {
    const auto& features = m_history.document().features();
    if (!currentTurn() || m_currentStep < 0 || m_currentStep >= static_cast<int>(features.size())) {
        return false;
    }
    const core::Feature& feature = features[static_cast<std::size_t>(m_currentStep)];
    m_turnGroup.clear();
    if (feature.type == "combine") {
        const auto centre = currentTurnCentre();
        std::vector<const core::Feature*> forms;
        collectForms(feature, forms);
        if (!centre || !m_result) {
            return false;
        }
        for (const core::Feature* form : forms) {
            const core::FeatureType* type = m_registry.find(form->type);
            const core::FeatureState* state = m_result->state(form->id);
            if (type == nullptr || state == nullptr || !state->description) {
                m_turnGroup.clear();
                return false;
            }
            TurnStart start;
            start.id = form->id;
            static constexpr const char* kPlace[3] = {"x", "y", "z"};
            static constexpr const char* kTurn[3] = {"rotX", "rotY", "rotZ"};
            for (std::size_t i = 0; i < 3; ++i) {
                start.place[i] = form->params.at(kPlace[i]);
                start.turn[i] = form->params.at(kTurn[i]);
                start.placeValue[i] = state->params.at(kPlace[i]).value;
            }
            start.turnValue = core::FeatureType::turnOf(state->params);
            // The middle of the part before its turn, minus its foot.
            core::ResolvedParams atOrigin = state->params;
            for (const char* name : kPlace) {
                atOrigin.at(name).value = 0.0;
            }
            start.centreOffset = type->bodyCentre(atOrigin);
            m_turnGroup.push_back(std::move(start));
        }
        m_turnGroupCentre = *centre;
    }
    core::CommandDescription label{
        "rotateBody", {{"type", feature.type}, {"number", std::to_string(feature.number)}}};
    if (!feature.outputs.empty()) {
        if (const core::Body* body = m_history.document().findBody(feature.outputs.front())) {
            label.values["name"] = body->name;
        }
    }
    m_history.beginTransaction(std::move(label));
    return true;
}

bool DocumentModel::turnCurrent(const geo::Angles& angles) {
    if (!m_turnGroup.empty() && m_history.inTransaction()) {
        // Every part turns about the same point by the same amount: its own turn changes, and
        // its place so that its middle goes where the turn takes it.
        const std::uint64_t before = m_history.document().revision();
        const bool back = angles.isZero(); // back to the start: as typed
        static constexpr const char* kPlace[3] = {"x", "y", "z"};
        static constexpr const char* kTurn[3] = {"rotX", "rotY", "rotZ"};
        bool done = true;
        for (const TurnStart& part : m_turnGroup) {
            const geo::Angles turned = geo::anglesOf(
                geo::rotationOf(angles) * geo::rotationOf(part.turnValue), part.turnValue);
            const geo::Point3d middle{part.placeValue[0] + part.centreOffset.x,
                                      part.placeValue[1] + part.centreOffset.y,
                                      part.placeValue[2] + part.centreOffset.z};
            const geo::Point3d moved = geo::rotatedAbout(middle, m_turnGroupCentre, angles);
            const double foot[3] = {part.placeValue[0] + moved.x - middle.x,
                                    part.placeValue[1] + moved.y - middle.y,
                                    part.placeValue[2] + moved.z - middle.z};
            const double turnNow[3] = {turned.x, turned.y, turned.z};
            const double turnWas[3] = {part.turnValue.x, part.turnValue.y, part.turnValue.z};
            for (std::size_t i = 0; i < 3; ++i) {
                const bool same = back || std::abs(turnNow[i] - turnWas[i]) < 1e-9;
                done = m_history.execute(core::makeSetFeatureValue(
                           part.id, kTurn[i],
                           same ? part.turn[i].expression : core::numberText(turnNow[i]))) &&
                       done;
                const bool stays = back || std::abs(foot[i] - part.placeValue[i]) < 1e-9;
                done = m_history.execute(core::makeSetFeatureValue(
                           part.id, kPlace[i],
                           stays ? part.place[i].expression : core::numberText(foot[i]))) &&
                       done;
            }
        }
        if (m_history.document().revision() != before) {
            documentChanged();
            scheduleLiveCuts();
        }
        return done;
    }
    const auto& features = m_history.document().features();
    const auto present = currentTurn();
    if (!present || m_currentStep < 0 || m_currentStep >= static_cast<int>(features.size())) {
        return false;
    }
    const core::FeatureId id = features[static_cast<std::size_t>(m_currentStep)].id;
    const std::uint64_t before = m_history.document().revision();
    const struct {
        const char* name;
        double wanted;
        double now;
    } values[3] = {{"rotX", angles.x, present->x},
                   {"rotY", angles.y, present->y},
                   {"rotZ", angles.z, present->z}};
    bool done = true;
    for (const auto& value : values) {
        if (std::abs(value.wanted - value.now) > 1e-9) {
            // 12 digits: a snapped 45 reads back as "45", not "45.0000000001".
            done = m_history.execute(core::makeSetFeatureValue(
                       id, value.name, QString::number(value.wanted, 'g', 12).toStdString())) &&
                   done;
        }
    }
    if (m_history.document().revision() != before) {
        documentChanged();
        scheduleLiveCuts();
    }
    return done;
}

std::optional<geo::Bounds> DocumentModel::boundsOfFeature(const core::Feature& feature) const {
    if (feature.outputs.empty() || !m_result) {
        return std::nullopt;
    }
    const auto found = m_result->bodies.find(feature.outputs.front());
    if (found == m_result->bodies.end() || found->second.stale) {
        return std::nullopt; // the last result of a step that fails now
    }
    try {
        return m_kernel->exactBounds(found->second.solid);
    } catch (const geo::GeometryError&) {
        return std::nullopt;
    }
}

std::optional<geo::Bounds> DocumentModel::currentBodyBounds() const {
    const auto& features = m_history.document().features();
    if (m_currentStep < 0 || m_currentStep >= static_cast<int>(features.size())) {
        return std::nullopt;
    }
    return boundsOfFeature(features[static_cast<std::size_t>(m_currentStep)]);
}

std::optional<geo::Bounds> DocumentModel::placingBodyBounds() const {
    if (!m_placing) {
        return std::nullopt;
    }
    const core::Feature* feature = m_history.document().findFeature(m_placingFeature);
    return feature != nullptr ? boundsOfFeature(*feature) : std::nullopt;
}

std::optional<DocumentModel::ScaleStart>
DocumentModel::scaleStartOf(const core::Feature& feature) const {
    const core::FeatureType* type = m_registry.find(feature.type);
    const core::FeatureState* state = m_result ? m_result->state(feature.id) : nullptr;
    if (type == nullptr || state == nullptr || !state->description) {
        return std::nullopt;
    }
    ScaleStart start;
    start.id = feature.id;
    for (const std::string& name : type->sizeParameters()) {
        const auto value = feature.params.find(name);
        if (value != feature.params.end() && state->params.contains(name)) {
            start.sizes.emplace_back(name, value->second);
        }
    }
    static constexpr const char* kNames[3] = {"x", "y", "z"};
    for (std::size_t axis = 0; axis < 3; ++axis) {
        const auto value = feature.params.find(kNames[axis]);
        const auto worked = state->params.find(kNames[axis]);
        if (value == feature.params.end() || worked == state->params.end()) {
            return std::nullopt;
        }
        start.place[axis] = value->second;
        start.placeValue[axis] = worked->second.value;
    }
    if (start.sizes.empty()) {
        return std::nullopt;
    }
    start.params = state->params;
    return start;
}

bool DocumentModel::beginScale() {
    const auto& features = m_history.document().features();
    if (!currentPosition() || m_currentStep < 0 ||
        m_currentStep >= static_cast<int>(features.size()) || !m_result) {
        return false;
    }
    const core::Feature& feature = features[static_cast<std::size_t>(m_currentStep)];
    // A combined body is made larger as every part it is made of, holes included.
    std::vector<const core::Feature*> forms;
    if (feature.type == "combine") {
        collectForms(feature, forms);
    } else {
        forms.push_back(&feature);
    }
    std::vector<ScaleStart> starts;
    for (const core::Feature* form : forms) {
        auto start = scaleStartOf(*form);
        if (!start) {
            return false; // a part that has no sizes (a part of a split) cannot be made larger
        }
        starts.push_back(std::move(*start));
    }
    if (starts.empty()) {
        return false;
    }
    m_scale = starts.front();
    m_scaleGroup = std::move(starts);
    core::CommandDescription label{
        "scaleBody",
        {{"type", feature.type}, {"number", std::to_string(feature.number)}, {"value", "100"}}};
    if (!feature.outputs.empty()) {
        if (const core::Body* body = m_history.document().findBody(feature.outputs.front())) {
            label.values["name"] = body->name;
        }
    }
    m_history.beginTransaction(std::move(label));
    return true;
}

bool DocumentModel::acceptsScaleOf(const ScaleStart& start, double factor) const {
    const core::Feature* feature = m_history.document().findFeature(start.id);
    const core::FeatureType* type = feature != nullptr ? m_registry.find(feature->type) : nullptr;
    if (type == nullptr) {
        return false;
    }
    core::ResolvedParams params = start.params;
    for (const auto& [name, value] : start.sizes) {
        core::Quantity& quantity = params.at(name);
        const double scaled = quantity.value * factor;
        if ((quantity.value > 0.0 && scaled < kSmallestSize) || scaled > kLargestSize) {
            return false;
        }
        quantity.value = scaled;
    }
    return !type->validate(params).has_value();
}

bool DocumentModel::acceptsScale(double factor) const {
    if (!m_scale || !std::isfinite(factor) || !(factor > 0.0)) {
        return false;
    }
    return std::all_of(m_scaleGroup.begin(), m_scaleGroup.end(),
                       [&](const ScaleStart& start) { return acceptsScaleOf(start, factor); });
}

bool DocumentModel::applyScale(const ScaleStart& start, double factor, const geo::Point3d& anchor) {
    const core::FeatureId id = start.id;
    const bool unchanged = std::abs(factor - 1.0) < 1e-12; // back to the start: as typed
    bool done = true;
    for (const auto& [name, value] : start.sizes) {
        const std::optional<std::string> text =
            unchanged ? value.expression
                      : core::scaledExpression(value.expression, value.kind, factor);
        done = text && m_history.execute(core::makeSetFeatureValue(id, name, *text)) && done;
    }
    static constexpr const char* kNames[3] = {"x", "y", "z"};
    const double from[3] = {anchor.x, anchor.y, anchor.z};
    for (std::size_t axis = 0; axis < 3; ++axis) {
        const double place = start.placeValue[axis];
        // Grown about its own place on this axis (the bed under a body): the value stays as it
        // was typed, a formula included.
        const double fixed = std::abs(from[axis] - place) < 1e-6 ? place : from[axis];
        const double wanted = fixed + factor * (place - fixed);
        const std::string text = unchanged || std::abs(wanted - place) < 1e-9
                                     ? start.place[axis].expression
                                     : core::numberText(wanted);
        done = m_history.execute(core::makeSetFeatureValue(id, kNames[axis], text)) && done;
    }
    return done;
}

bool DocumentModel::scaleCurrent(double factor, const geo::Point3d& anchor) {
    if (!m_scale || !m_history.inTransaction() || !std::isfinite(factor) || !(factor > 0.0)) {
        return false;
    }
    const std::uint64_t before = m_history.document().revision();
    bool done = true;
    for (const ScaleStart& start : m_scaleGroup) {
        if (m_history.document().findFeature(start.id) == nullptr) {
            return false;
        }
        done = applyScale(start, factor, anchor) && done;
    }
    m_history.setTransactionValue("value", core::numberText(factor * 100.0));
    if (m_history.document().revision() != before) {
        documentChanged();
        scheduleLiveCuts();
    }
    return done;
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
    const core::Body& body = bodies[static_cast<std::size_t>(index)];
    core::CommandDescription label{hole ? "holeBody" : "solidBody", {{"name", body.name}}};
    if (const core::Feature* maker = m_history.document().findFeature(body.createdBy)) {
        label.values["type"] = maker->type;
        label.values["number"] = std::to_string(maker->number);
    }
    // A hole cuts what it touches, a body that is none any more stops cutting: the cuts change
    // with the mark, in the same step.
    if (executeWithCuts(core::makeSetBodyHole(body.id, hole), std::move(label)) &&
        m_history.document().revision() != before) {
        documentChanged();
    }
}

// --- putting shapes down ---------------------------------------------------------------------

bool DocumentModel::setValueQuietly(const core::FeatureId& id, const char* name, double value) {
    // 'g' with enough digits: the number reads back exactly, and "." is understood everywhere.
    return m_history.execute(
        core::makeSetFeatureValue(id, name, QString::number(value, 'g', 15).toStdString()));
}

namespace {

// The middle of the box round a freshly made shape minus its foot, worked out from the values a
// new step starts with (all of them plain numbers). Where the shape turns about.
geo::Point3d centreOffsetOfDefaults(const core::FeatureType& type) {
    core::ResolvedParams params;
    for (const core::ParamSpec& spec : type.parameters()) {
        const double value =
            core::evaluateExpression(spec.defaultExpression, spec.kind).value_or(0.0);
        params[spec.name] = core::Quantity{value, core::dimensionOf(spec.kind)};
    }
    for (const char* place : {"x", "y", "z"}) {
        const auto found = params.find(place);
        if (found != params.end()) {
            found->second.value = 0.0; // measured from the foot
        }
    }
    return type.bodyCentre(params);
}

} // namespace

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
    m_placingOnFace = false;
    m_placingCentre = centreOffsetOfDefaults(*featureType);
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
    const bool turns = feature->params.contains("rotX") && feature->params.contains("rotY") &&
                       feature->params.contains("rotZ");
    const std::uint64_t before = m_history.document().revision();
    setValueQuietly(m_placingFeature, "x", x);
    setValueQuietly(m_placingFeature, "y", y);
    setValueQuietly(m_placingFeature, "z", z);
    if (m_placingOnFace) {
        // Back from a wall onto a flat place: the shape stands up again.
        if (turns) {
            for (const char* angle : {"rotX", "rotY", "rotZ"}) {
                setValueQuietly(m_placingFeature, angle, 0.0);
            }
        }
        m_placingOnFace = false;
    }
    m_placingZ = z;
    m_placingSurface = surfaceBody;
    if (m_history.document().revision() != before) {
        documentChanged();
    }
}

void DocumentModel::placeOnFace(const geo::Point3d& point, const geo::Point3d& normal) {
    if (!m_placing) {
        return;
    }
    const core::Feature* feature = m_history.document().findFeature(m_placingFeature);
    if (feature == nullptr || !feature->params.contains("rotX") ||
        !feature->params.contains("rotY") || !feature->params.contains("rotZ")) {
        return;
    }
    const std::uint64_t before = m_history.document().revision();
    writeSeat(m_placingFeature, geo::seatOnFace(point, normal, m_placingCentre));
    m_placingOnFace = true;
    // The work plane is a height; a wall does not move it.
    m_placingZ = m_workplaneZ;
    m_placingSurface = m_workplaneBody;
    if (m_history.document().revision() != before) {
        documentChanged();
    }
}

bool DocumentModel::finishPlacing() {
    if (!m_placing) {
        return false;
    }
    finishTransaction();
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
    m_liveCutTimer.stop();
    m_history.cancelTransaction();
    m_placing = false;
    documentChanged();
    emit placingChanged();
}

// --- changing bodies that are already there (AP-1.6e) ---

namespace {

// The text of a value moved by `by` mm: a plain number is a plain number again, a formula stays a
// formula that is moved ("(Width / 2) + 10").
std::string movedExpression(const std::string& expression, double by) {
    if (isPlainNumber(expression)) {
        const double now =
            core::evaluateExpression(expression, core::QuantityKind::length).value_or(0.0);
        return QString::number(now + by, 'g', 12).toStdString();
    }
    return "(" + expression + ") + (" + QString::number(by, 'g', 12).toStdString() + ")";
}

} // namespace

const core::Body* DocumentModel::findBody(const QString& bodyId) const {
    for (const core::Body& body : m_history.document().bodies()) {
        if (toQString(body.id.value) == bodyId) {
            return &body;
        }
    }
    return nullptr;
}

std::optional<geo::Bounds> DocumentModel::bodyBounds(const QString& bodyId) const {
    const core::Body* body = findBody(bodyId);
    const core::Feature* feature =
        body != nullptr ? m_history.document().findFeature(body->createdBy) : nullptr;
    return feature != nullptr ? boundsOfFeature(*feature) : std::nullopt;
}

QVariantMap DocumentModel::boundsOfBodies(const QStringList& bodyIds) const {
    std::optional<geo::Bounds> total;
    for (const QString& id : bodyIds) {
        const auto box = bodyBounds(id);
        if (!box) {
            continue;
        }
        if (!total) {
            total = box;
            continue;
        }
        total->min = {std::min(total->min.x, box->min.x), std::min(total->min.y, box->min.y),
                      std::min(total->min.z, box->min.z)};
        total->max = {std::max(total->max.x, box->max.x), std::max(total->max.y, box->max.y),
                      std::max(total->max.z, box->max.z)};
    }
    QVariantMap map;
    map[QStringLiteral("valid")] = total.has_value();
    if (total) {
        map[QStringLiteral("min")] = QVariantList{total->min.x, total->min.y, total->min.z};
        map[QStringLiteral("max")] = QVariantList{total->max.x, total->max.y, total->max.z};
    }
    return map;
}

// x, y and z as they are written in the step now (the last calculation may still be behind a
// drag); a value that is a formula is taken from the last calculation.
std::optional<std::array<double, 3>> DocumentModel::liveValues(const core::Feature& step) const {
    const core::FeatureState* state = m_result ? m_result->state(step.id) : nullptr;
    std::array<double, 3> values{};
    static constexpr const char* kNames[3] = {"x", "y", "z"};
    for (std::size_t axis = 0; axis < 3; ++axis) {
        const auto given = step.params.find(kNames[axis]);
        if (given == step.params.end()) {
            return std::nullopt;
        }
        if (const auto number =
                core::evaluateExpression(given->second.expression, given->second.kind)) {
            values[axis] = *number;
        } else if (state != nullptr && state->params.contains(kNames[axis])) {
            values[axis] = state->params.at(kNames[axis]).value;
        } else {
            return std::nullopt;
        }
    }
    return values;
}

void DocumentModel::collectForms(const core::Feature& step,
                                 std::vector<const core::Feature*>& out) const {
    if (step.params.contains("x") && step.params.contains("y") && step.params.contains("z")) {
        if (std::find(out.begin(), out.end(), &step) == out.end()) {
            out.push_back(&step);
        }
        return;
    }
    if (step.type != "combine") {
        return;
    }
    for (const core::BodyId& input : step.inputs) {
        const core::Body* body = m_history.document().findBody(input);
        const core::Feature* maker =
            body != nullptr ? m_history.document().findFeature(body->createdBy) : nullptr;
        if (maker != nullptr && maker != &step) {
            collectForms(*maker, out);
        }
    }
}

std::optional<std::array<double, 3>>
DocumentModel::positionOfFeature(const core::Feature& feature) const {
    if (feature.type == "combine") {
        std::vector<const core::Feature*> forms;
        collectForms(feature, forms);
        return forms.empty() ? std::nullopt : positionOfFeature(*forms.front());
    }
    const core::FeatureState* state = m_result ? m_result->state(feature.id) : nullptr;
    std::array<double, 3> position{};
    static constexpr const char* kNames[3] = {"x", "y", "z"};
    for (std::size_t axis = 0; axis < 3; ++axis) {
        const auto given = feature.params.find(kNames[axis]);
        if (given == feature.params.end()) {
            return std::nullopt; // this step has no place
        }
        if (state != nullptr) {
            const auto found = state->params.find(kNames[axis]);
            if (found != state->params.end()) {
                position[axis] = found->second.value;
                continue;
            }
        }
        const auto value = core::evaluateExpression(given->second.expression, given->second.kind);
        if (!value) {
            return std::nullopt;
        }
        position[axis] = *value;
    }
    return position;
}

bool DocumentModel::moveBodiesBy(
    const std::vector<std::pair<QString, std::array<double, 3>>>& moves, const char* key) {
    struct Mover {
        core::FeatureId id;
        std::array<double, 3> from;
        std::array<double, 3> delta;
        std::string type;
        std::string name;
    };
    std::vector<Mover> movers;
    std::set<std::string> done;
    for (const auto& [bodyId, delta] : moves) {
        const core::Body* body = findBody(bodyId);
        const core::Feature* feature =
            body != nullptr ? m_history.document().findFeature(body->createdBy) : nullptr;
        // A step that makes several bodies would move them all: left alone.
        if (feature == nullptr || feature->outputs.size() != 1 ||
            (delta[0] == 0.0 && delta[1] == 0.0 && delta[2] == 0.0)) {
            continue;
        }
        // A combined body moves as the parts it is made of, its holes with it.
        std::vector<const core::Feature*> forms;
        collectForms(*feature, forms);
        for (const core::Feature* form : forms) {
            const auto from = positionOfFeature(*form);
            if (from && done.insert(form->id.value).second) {
                movers.push_back({form->id, *from, delta, feature->type, body->name});
            }
        }
    }
    if (movers.empty()) {
        return false;
    }
    core::CommandDescription label{
        key, {{"count", std::to_string(movers.size())}, {"type", movers.front().type}}};
    if (movers.size() == 1 && !movers.front().name.empty()) {
        label.values["name"] = movers.front().name;
    }
    const std::uint64_t before = m_history.document().revision();
    m_history.beginTransaction(std::move(label));
    static constexpr const char* kNames[3] = {"x", "y", "z"};
    bool ok = true;
    for (const Mover& mover : movers) {
        for (std::size_t axis = 0; axis < 3; ++axis) {
            if (mover.delta[axis] != 0.0) {
                ok = m_history.execute(core::makeSetFeatureValue(
                         mover.id, kNames[axis],
                         QString::number(mover.from[axis] + mover.delta[axis], 'g', 12)
                             .toStdString())) &&
                     ok;
            }
        }
    }
    finishTransaction();
    if (m_history.document().revision() != before) {
        documentChanged();
    }
    emit historyChanged();
    return ok;
}

bool DocumentModel::dropToBed(const QStringList& bodyIds) {
    std::vector<std::pair<QString, std::array<double, 3>>> moves;
    for (const QString& id : bodyIds) {
        const auto bounds = bodyBounds(id);
        if (bounds && std::abs(bounds->min.z) >= 1e-6) {
            moves.push_back({id, {0.0, 0.0, -bounds->min.z}});
        }
    }
    return moveBodiesBy(moves, "dropBody");
}

bool DocumentModel::alignBodies(const QStringList& bodyIds, int axis, const QString& to) {
    interaction::AlignTo where = interaction::AlignTo::centre;
    if (to == QLatin1String("min")) {
        where = interaction::AlignTo::min;
    } else if (to == QLatin1String("max")) {
        where = interaction::AlignTo::max;
    } else if (to != QLatin1String("centre")) {
        return false;
    }
    std::vector<QString> ids;
    std::vector<interaction::Box3> boxes;
    for (const QString& id : bodyIds) {
        const auto bounds = bodyBounds(id);
        const core::Body* body = findBody(id);
        const core::Feature* feature =
            body != nullptr ? m_history.document().findFeature(body->createdBy) : nullptr;
        // Only bodies that can be moved and are calculated take part.
        if (bounds && feature != nullptr && positionOfFeature(*feature)) {
            ids.push_back(id);
            boxes.push_back({{bounds->min.x, bounds->min.y, bounds->min.z},
                             {bounds->max.x, bounds->max.y, bounds->max.z}});
        }
    }
    const auto offsets = interaction::alignOffsets(boxes, axis, where);
    std::vector<std::pair<QString, std::array<double, 3>>> moves;
    for (std::size_t i = 0; i < ids.size(); ++i) {
        moves.push_back({ids[i], offsets[i]});
    }
    return moveBodiesBy(moves, "alignBodies");
}

void DocumentModel::writeSeat(const core::FeatureId& id, const geo::Seat& seat) {
    setValueQuietly(id, "x", seat.foot.x);
    setValueQuietly(id, "y", seat.foot.y);
    setValueQuietly(id, "z", seat.foot.z);
    setValueQuietly(id, "rotX", seat.turn.x);
    setValueQuietly(id, "rotY", seat.turn.y);
    setValueQuietly(id, "rotZ", seat.turn.z);
}

bool DocumentModel::seatBodyOnFace(const QString& bodyId, const geo::Point3d& point,
                                   const geo::Point3d& normal) {
    const core::Body* body = findBody(bodyId);
    const core::Feature* feature =
        body != nullptr ? m_history.document().findFeature(body->createdBy) : nullptr;
    const core::FeatureType* type = feature != nullptr ? m_registry.find(feature->type) : nullptr;
    const core::FeatureState* state =
        feature != nullptr && m_result ? m_result->state(feature->id) : nullptr;
    if (type == nullptr || state == nullptr || feature->outputs.size() != 1 ||
        !positionOfFeature(*feature) || !feature->params.contains("rotX") ||
        !feature->params.contains("rotY") || !feature->params.contains("rotZ")) {
        return false;
    }
    // The shape turns about the middle of its box: measured from its foot, before the turn.
    core::ResolvedParams params = state->params;
    for (const char* place : {"x", "y", "z"}) {
        const auto found = params.find(place);
        if (found != params.end()) {
            found->second.value = 0.0;
        }
    }
    const geo::Point3d centre = type->bodyCentre(params);
    geo::Angles hint;
    for (const auto& [name, angle] :
         {std::pair{"rotX", &hint.x}, std::pair{"rotY", &hint.y}, std::pair{"rotZ", &hint.z}}) {
        const auto found = state->params.find(name);
        if (found != state->params.end()) {
            *angle = found->second.value;
        }
    }
    const geo::Seat seat = geo::seatOnFace(point, normal, centre, hint);
    core::CommandDescription label{"seatBody", {{"type", feature->type}}};
    if (!body->name.empty()) {
        label.values["name"] = body->name;
    }
    const std::uint64_t before = m_history.document().revision();
    const core::FeatureId id = feature->id;
    m_history.beginTransaction(std::move(label));
    writeSeat(id, seat);
    finishTransaction();
    if (m_history.document().revision() != before) {
        documentChanged();
    }
    emit historyChanged();
    return true;
}

bool DocumentModel::copyBodies(const QStringList& bodyIds) {
    std::vector<ClipboardEntry> entries;
    std::set<std::string> taken;
    for (const QString& id : bodyIds) {
        const core::Body* body = findBody(id);
        const core::Feature* feature =
            body != nullptr ? m_history.document().findFeature(body->createdBy) : nullptr;
        if (feature == nullptr) {
            continue;
        }
        // Steps that work on other bodies need those: not before combining exists (AP-1.7).
        if (!feature->inputs.empty() || feature->outputs.size() != 1) {
            return false;
        }
        if (!taken.insert(feature->id.value).second) {
            continue;
        }
        ClipboardEntry entry;
        entry.draft.type = feature->type;
        entry.draft.typeVersion = feature->typeVersion;
        entry.draft.label = feature->label;
        entry.draft.params = feature->params;
        entry.draft.texts = feature->texts;
        entry.draft.newBodies = 1;
        entry.name = body->name;
        entry.color = body->color;
        entry.hole = body->hole;
        entries.push_back(std::move(entry));
    }
    if (entries.empty()) {
        return false;
    }
    m_clipboard = std::move(entries);
    m_pasted = 0;
    emit clipboardChanged();
    return true;
}

QStringList DocumentModel::pasteBodies() {
    QStringList created;
    if (m_clipboard.empty()) {
        return created;
    }
    ++m_pasted;
    const double by = kPasteOffset * m_pasted;
    const std::uint64_t before = m_history.document().revision();
    m_history.beginTransaction({"pasteBodies",
                                {{"count", std::to_string(m_clipboard.size())},
                                 {"type", m_clipboard.front().draft.type}}});
    for (const ClipboardEntry& entry : m_clipboard) {
        core::FeatureDraft draft = entry.draft;
        for (const char* place : {"x", "y"}) {
            const auto found = draft.params.find(place);
            if (found != draft.params.end()) {
                found->second.expression = movedExpression(found->second.expression, by);
            }
        }
        const auto id = m_history.addFeature(std::move(draft));
        const core::Feature* feature = id ? m_history.document().findFeature(*id) : nullptr;
        if (feature == nullptr || feature->outputs.empty()) {
            continue;
        }
        const core::BodyId body = feature->outputs.front();
        if (!entry.name.empty()) {
            m_history.execute(core::makeSetBodyName(body, entry.name));
        }
        m_history.execute(core::makeSetBodyColor(body, entry.color));
        if (entry.hole) {
            m_history.execute(core::makeSetBodyHole(body, true));
        }
        created.push_back(toQString(body.value));
    }
    finishTransaction();
    if (m_history.document().revision() != before) {
        documentChanged();
    }
    emit historyChanged();
    return created;
}

QVariantMap DocumentModel::cutLabel(int bodyIndex) const {
    QVariantMap label;
    label[QStringLiteral("valid")] = false;
    const auto& bodies = m_history.document().bodies();
    if (bodyIndex < 0 || bodyIndex >= static_cast<int>(bodies.size()) || !m_result) {
        return label;
    }
    const core::Body& hole = bodies[static_cast<std::size_t>(bodyIndex)];
    const auto bounds = bodyBounds(toQString(hole.id.value));
    const auto targets =
        core::cutTargetsOf(m_history.document(), *m_result, m_registry, *m_kernel, hole.id);
    if (!hole.hole || !hole.visible || !bounds || targets.empty()) {
        return label;
    }
    QVariantList list;
    for (const core::CutTarget& target : targets) {
        QVariantMap entry;
        entry[QStringLiteral("index")] = bodyIndexOf(toQString(target.root.value));
        entry[QStringLiteral("cut")] = target.cutNow;
        list.push_back(entry);
    }
    label[QStringLiteral("valid")] = true;
    label[QStringLiteral("x")] = (bounds->min.x + bounds->max.x) / 2.0;
    label[QStringLiteral("y")] = (bounds->min.y + bounds->max.y) / 2.0;
    label[QStringLiteral("z")] = bounds->max.z;
    label[QStringLiteral("targets")] = list;
    return label;
}

bool DocumentModel::setCutTarget(int holeIndex, int rootIndex, bool cut) {
    const auto& bodies = m_history.document().bodies();
    const auto count = static_cast<int>(bodies.size());
    if (holeIndex < 0 || holeIndex >= count || rootIndex < 0 || rootIndex >= count ||
        !bodies[static_cast<std::size_t>(holeIndex)].hole) {
        return false;
    }
    const core::Body& hole = bodies[static_cast<std::size_t>(holeIndex)];
    const core::BodyId root = bodies[static_cast<std::size_t>(rootIndex)].id;
    std::vector<core::BodyId> exceptions = hole.cutExceptions;
    const auto found = std::find(exceptions.begin(), exceptions.end(), root);
    if (cut && found != exceptions.end()) {
        exceptions.erase(found);
    } else if (!cut && found == exceptions.end()) {
        exceptions.push_back(root);
    }
    core::CommandDescription label{"cutTargets", {{"name", hole.name}}};
    if (const core::Feature* maker = m_history.document().findFeature(hole.createdBy)) {
        label.values["type"] = maker->type;
        label.values["number"] = std::to_string(maker->number);
    }
    const std::uint64_t before = m_history.document().revision();
    if (!executeWithCuts(core::makeSetBodyCutExceptions(hole.id, std::move(exceptions)),
                         std::move(label))) {
        return false;
    }
    if (m_history.document().revision() != before) {
        documentChanged();
    }
    return true;
}

QVariantMap DocumentModel::splitPlane() const {
    QVariantMap plane;
    plane[QStringLiteral("valid")] = false;
    const auto& features = m_history.document().features();
    if (m_currentStep < 0 || m_currentStep >= static_cast<int>(features.size()) || !m_result) {
        return plane;
    }
    const core::Feature& step = features[static_cast<std::size_t>(m_currentStep)];
    const core::FeatureState* state = m_result->state(step.id);
    const auto input =
        step.inputs.empty() ? m_result->bodies.end() : m_result->bodies.find(step.inputs.front());
    if (step.type != "split" || state == nullptr || !state->description ||
        input == m_result->bodies.end() || state->params.empty()) {
        return plane;
    }
    geo::Bounds box;
    try {
        box = m_kernel->bounds(input->second.solid);
    } catch (const geo::GeometryError&) {
        return plane;
    }
    const int axis = static_cast<int>(std::lround(state->params.at("axis").value));
    const double at = state->params.at("position").value;
    if (axis < 0 || axis > 2) {
        return plane;
    }
    const double low[3] = {box.min.x, box.min.y, box.min.z};
    const double high[3] = {box.max.x, box.max.y, box.max.z};
    const double margin = 0.12 * box.diagonal();
    const int u = axis == 0 ? 1 : 0;
    const int v = axis == 2 ? 1 : 2;
    QVariantList corners;
    for (const auto& [cu, cv] :
         {std::pair{-1, -1}, std::pair{1, -1}, std::pair{1, 1}, std::pair{-1, 1}}) {
        double point[3] = {0.0, 0.0, 0.0};
        point[axis] = at;
        point[u] = cu < 0 ? low[u] - margin : high[u] + margin;
        point[v] = cv < 0 ? low[v] - margin : high[v] + margin;
        QVariantMap corner;
        corner[QStringLiteral("x")] = point[0];
        corner[QStringLiteral("y")] = point[1];
        corner[QStringLiteral("z")] = point[2];
        corners.push_back(corner);
    }
    plane[QStringLiteral("valid")] = true;
    plane[QStringLiteral("corners")] = corners;
    return plane;
}

bool DocumentModel::setSplitAxis(int stepIndex, int axis) {
    const auto& features = m_history.document().features();
    if (stepIndex < 0 || stepIndex >= static_cast<int>(features.size()) || axis < 0 || axis > 2 ||
        !m_result) {
        return false;
    }
    const core::Feature& step = features[static_cast<std::size_t>(stepIndex)];
    const auto input =
        step.inputs.empty() ? m_result->bodies.end() : m_result->bodies.find(step.inputs.front());
    if (step.type != "split" || input == m_result->bodies.end()) {
        return false;
    }
    geo::Bounds box;
    try {
        box = m_kernel->exactBounds(input->second.solid);
    } catch (const geo::GeometryError&) {
        return false;
    }
    const double size[3] = {box.max.x - box.min.x, box.max.y - box.min.y, box.max.z - box.min.z};
    const double low[3] = {box.min.x, box.min.y, box.min.z};
    const int u = axis == 0 ? 1 : 0;
    const int v = axis == 2 ? 1 : 2;
    const auto text = [](double value) { return QString::number(value, 'g', 12).toStdString(); };
    const core::FeatureId id = step.id;
    // The plane goes to the middle of the body on the new axis (where it surely cuts).
    m_history.beginTransaction({"setValue", {{"field", "axis"}}});
    bool done = m_history.execute(core::makeSetFeatureValue(id, "axis", std::to_string(axis)));
    done = m_history.execute(
               core::makeSetFeatureValue(id, "position", text(low[axis] + size[axis] / 2.0))) &&
           done;
    done =
        m_history.execute(core::makeSetFeatureValue(id, "anchorU", text(low[u] + size[u] / 2.0))) &&
        done;
    done =
        m_history.execute(core::makeSetFeatureValue(id, "anchorV", text(low[v] + size[v] / 2.0))) &&
        done;
    finishTransaction();
    documentChanged();
    return done;
}

int DocumentModel::splitBody(const QString& bodyId) {
    const core::Body* body = findBody(bodyId);
    const auto box = bodyBounds(bodyId);
    if (body == nullptr || !box || m_history.document().consumerOf(body->id)) {
        return -1;
    }
    // Across the longest side, in the middle: halving is what most people want first.
    const double size[3] = {box->max.x - box->min.x, box->max.y - box->min.y,
                            box->max.z - box->min.z};
    const double low[3] = {box->min.x, box->min.y, box->min.z};
    int axis = 0;
    for (int i = 1; i < 3; ++i) {
        axis = size[i] > size[axis] + 1e-9 ? i : axis;
    }
    const int u = axis == 0 ? 1 : 0;
    const int v = axis == 2 ? 1 : 2;
    const auto number = [](double value) {
        return core::Value{QString::number(value, 'g', 12).toStdString(),
                           core::QuantityKind::length};
    };

    core::FeatureDraft draft = core::defaultDraft(*m_registry.find("split"));
    draft.inputs = {body->id};
    draft.params["axis"] = {std::to_string(axis), core::QuantityKind::plain};
    draft.params["position"] = number(low[axis] + size[axis] / 2.0);
    draft.params["anchorU"] = number(low[u] + size[u] / 2.0);
    draft.params["anchorV"] = number(low[v] + size[v] / 2.0);

    core::CommandDescription label{"splitBody", {{"name", body->name}}};
    if (const core::Feature* maker = m_history.document().findFeature(body->createdBy)) {
        label.values["type"] = maker->type;
        label.values["number"] = std::to_string(maker->number);
    }
    m_history.beginTransaction(std::move(label));
    const auto split = m_history.addFeature(std::move(draft));
    const core::Feature* step = split ? m_history.document().findFeature(*split) : nullptr;
    if (step == nullptr || step->outputs.size() != 2) {
        m_history.cancelTransaction();
        return -1;
    }
    // Each part gets a step of its own that puts it where it is wanted: a part cannot be moved
    // through the split, which makes two bodies (ADR-0010 point 3).
    const std::vector<core::BodyId> parts = step->outputs;
    for (const core::BodyId& part : parts) {
        core::FeatureDraft place = core::defaultDraft(*m_registry.find("place"));
        place.inputs = {part};
        if (!m_history.addFeature(std::move(place))) {
            m_history.cancelTransaction();
            return -1;
        }
    }
    finishTransaction();
    m_currentFeature = *split;
    documentChanged();
    return stepIndexOf(*split);
}

QString DocumentModel::combineBodies(const QStringList& bodyIds, const QString& mode) {
    if (bodyIds.size() < 2 ||
        (mode != QLatin1String("merge") && mode != QLatin1String("subtract") &&
         mode != QLatin1String("intersect"))) {
        return {};
    }
    std::vector<core::BodyId> inputs;
    for (const QString& id : bodyIds) {
        const core::Body* body = findBody(id);
        if (body == nullptr || std::find(inputs.begin(), inputs.end(), body->id) != inputs.end() ||
            m_history.document().consumerOf(body->id)) {
            return {};
        }
        inputs.push_back(body->id);
    }
    const core::Body target = *findBody(bodyIds.front());

    core::FeatureDraft draft = core::defaultDraft(*m_registry.find("combine"));
    draft.texts["mode"] = mode.toStdString();
    draft.inputs = std::move(inputs);
    draft.newBodies = 1;

    m_history.beginTransaction(
        {"combineBodies",
         {{"mode", mode.toStdString()}, {"count", std::to_string(bodyIds.size())}}});
    const auto id = m_history.addFeature(std::move(draft));
    const core::Feature* feature = id ? m_history.document().findFeature(*id) : nullptr;
    if (feature == nullptr || feature->outputs.empty()) {
        m_history.cancelTransaction();
        return {};
    }
    const core::BodyId result = feature->outputs.front();
    if (!target.name.empty()) {
        m_history.execute(core::makeSetBodyName(result, target.name));
    }
    m_history.execute(core::makeSetBodyColor(result, target.color));
    m_history.commitTransaction();
    documentChanged();
    emit historyChanged();
    return toQString(result.value);
}

QStringList DocumentModel::duplicateBodies(const QStringList& bodyIds) {
    return copyBodies(bodyIds) ? pasteBodies() : QStringList();
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
