// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/ui/selection_controller.h"

#include <algorithm>

namespace fission::ui {

namespace {

core::SelectableKind kindOf(int value) {
    switch (value) {
    case static_cast<int>(core::SelectableKind::face):
        return core::SelectableKind::face;
    case static_cast<int>(core::SelectableKind::edge):
        return core::SelectableKind::edge;
    case static_cast<int>(core::SelectableKind::vertex):
        return core::SelectableKind::vertex;
    default:
        return core::SelectableKind::body;
    }
}

} // namespace

SelectionController::SelectionController(DocumentModel* model, QObject* parent)
    : QObject(parent), m_model(model) {
    connect(model, &DocumentModel::elementsChanged, this, &SelectionController::onElementsChanged);
    connect(model, &DocumentModel::bodiesChanged, this, &SelectionController::onBodiesChanged);
    connect(model, &DocumentModel::currentStepChanged, this, &SelectionController::onStepChanged);
}

void SelectionController::attach(Viewport3D* viewport) {
    m_viewport = viewport;
    if (viewport == nullptr) {
        return;
    }
    connect(viewport, &Viewport3D::picked, this, &SelectionController::onPicked);
    connect(viewport, &Viewport3D::hoverChanged, this, &SelectionController::onHover);
    connect(viewport, &Viewport3D::rubberBandFinished, this, &SelectionController::onRubberBand);
    connect(viewport, &Viewport3D::regionPicked, this, &SelectionController::onRegionPicked);
    viewport->setSelectionFilter(m_selection.filter());
    showInView();
}

QString SelectionController::kindName(core::SelectableKind kind) {
    switch (kind) {
    case core::SelectableKind::body:
        return QStringLiteral("body");
    case core::SelectableKind::face:
        return QStringLiteral("face");
    case core::SelectableKind::edge:
        return QStringLiteral("edge");
    case core::SelectableKind::vertex:
        return QStringLiteral("vertex");
    }
    return {};
}

QVariantList SelectionController::items() const {
    QVariantList list;
    for (const core::SelectionItem& item : m_selection.items()) {
        const QString body = QString::fromStdString(item.body.value);
        QVariantMap entry;
        entry[QStringLiteral("body")] = body;
        entry[QStringLiteral("bodyIndex")] = m_model ? m_model->bodyIndexOf(body) : -1;
        entry[QStringLiteral("kind")] = kindName(item.kind);
        entry[QStringLiteral("number")] = static_cast<int>(item.number);
        entry[QStringLiteral("name")] = QString::fromStdString(item.name);
        list.push_back(entry);
    }
    return list;
}

QString SelectionController::filter() const {
    const core::SelectionFilter f = m_selection.filter();
    for (const core::SelectableKind kind :
         {core::SelectableKind::body, core::SelectableKind::face, core::SelectableKind::edge,
          core::SelectableKind::vertex}) {
        if (f == core::SelectionFilter::only(kind)) {
            return kindName(kind);
        }
    }
    return QStringLiteral("all");
}

void SelectionController::setFilter(const QString& name) {
    core::SelectionFilter next = core::SelectionFilter::all();
    for (const core::SelectableKind kind :
         {core::SelectableKind::body, core::SelectableKind::face, core::SelectableKind::edge,
          core::SelectableKind::vertex}) {
        if (name == kindName(kind)) {
            next = core::SelectionFilter::only(kind);
        }
    }
    if (next == m_selection.filter()) {
        return;
    }
    const bool changedChoice = m_selection.setFilter(next);
    if (m_viewport) {
        m_viewport->setSelectionFilter(next);
    }
    emit filterChanged();
    if (changedChoice) {
        choiceChanged();
    }
}

bool SelectionController::singleFaceChosen() const {
    return m_selection.size() == 1 &&
           m_selection.items().front().kind == core::SelectableKind::face;
}

std::optional<core::SelectionItem> SelectionController::itemFor(const QString& body,
                                                                core::SelectableKind kind,
                                                                std::uint32_t number) const {
    if (body.isEmpty() || !m_model) {
        return std::nullopt;
    }
    core::SelectionItem item;
    item.body = core::BodyId{body.toStdString()};
    item.kind = kind;
    if (kind == core::SelectableKind::body) {
        return item;
    }
    item.number = number;
    const auto names = m_model->elementNames();
    const auto found = names->find(item.body);
    if (found == names->end()) {
        return std::nullopt; // the body is not there any more
    }
    item.name = found->second.name(kind, number);
    return item;
}

void SelectionController::onPicked(int intent, bool extend, const QString& body, int kind,
                                   int number) {
    const auto what = static_cast<Viewport3D::PickIntent>(intent);
    if (what == Viewport3D::PickIntent::hover) {
        return;
    }
    const std::optional<core::SelectionItem> hit =
        itemFor(body, kindOf(kind), static_cast<std::uint32_t>(number));
    const bool different = what == Viewport3D::PickIntent::click
                               ? m_selection.click(hit, extend)
                               : m_selection.doubleClick(hit, extend);
    if (different) {
        choiceChanged();
    }
}

void SelectionController::onHover(const QString& body, int kind, int number) {
    QVariantMap next;
    if (!body.isEmpty()) {
        next[QStringLiteral("kind")] = kindName(kindOf(kind));
        next[QStringLiteral("bodyIndex")] = m_model ? m_model->bodyIndexOf(body) : -1;
        next[QStringLiteral("number")] = number;
    }
    if (next != m_hover) {
        m_hover = std::move(next);
        emit hoverChanged();
    }
}

void SelectionController::onRubberBand(const QRectF& area, bool extend) {
    if (m_viewport) {
        m_viewport->requestRegionPick(area, extend);
    }
}

void SelectionController::onRegionPicked(const QVariantList& elements, bool extend) {
    std::vector<core::SelectionItem> found;
    for (const QVariant& element : elements) {
        const QVariantMap map = element.toMap();
        const auto item = itemFor(map.value(QStringLiteral("body")).toString(),
                                  kindOf(map.value(QStringLiteral("kind")).toInt()),
                                  map.value(QStringLiteral("number")).toUInt());
        if (item) {
            found.push_back(*item);
        }
    }
    if (m_selection.rubberBand(found, extend)) {
        choiceChanged();
    }
}

void SelectionController::clear() {
    if (m_selection.clear()) {
        choiceChanged();
    }
}

void SelectionController::chooseBody(int bodyIndex, bool extend) {
    if (!m_model) {
        return;
    }
    const auto item = itemFor(m_model->bodyIdAt(bodyIndex), core::SelectableKind::body, 0);
    if (m_selection.click(item, extend)) {
        choiceChanged();
    }
}

void SelectionController::chooseAllBodies() {
    if (!m_model) {
        return;
    }
    std::vector<core::SelectionItem> all;
    for (const core::Body& body : m_model->document().bodies()) {
        if (body.visible) {
            core::SelectionItem item;
            item.body = body.id;
            all.push_back(std::move(item));
        }
    }
    if (m_selection.set(std::move(all))) {
        choiceChanged();
    }
}

void SelectionController::chooseBodies(const QStringList& bodyIds) {
    if (!m_model) {
        return;
    }
    std::vector<core::SelectionItem> chosen;
    for (const QString& id : bodyIds) {
        if (const auto item = itemFor(id, core::SelectableKind::body, 0)) {
            chosen.push_back(*item);
        }
    }
    if (m_selection.set(std::move(chosen))) {
        choiceChanged();
    }
}

bool SelectionController::lookAtChosenFace() {
    if (!m_viewport || !singleFaceChosen()) {
        return false;
    }
    const core::SelectionItem& item = m_selection.items().front();
    return m_viewport->lookAtFace(QString::fromStdString(item.body.value),
                                  static_cast<int>(item.number));
}

void SelectionController::onElementsChanged() {
    // The document was recalculated: what is chosen is looked up again by its name.
    const std::uint64_t before = m_selection.revision();
    m_selection.reconcile(*m_model->elementNames());
    if (m_selection.revision() != before) {
        choiceChanged();
    } else {
        showInView(); // the numbers are the same, but the view has new meshes
    }
}

void SelectionController::onBodiesChanged() {
    // A body that is hidden cannot stay chosen.
    std::vector<core::SelectionItem> kept;
    for (const core::SelectionItem& item : m_selection.items()) {
        const auto& bodies = m_model->document().bodies();
        const auto body = std::find_if(bodies.begin(), bodies.end(),
                                       [&](const core::Body& b) { return b.id == item.body; });
        if (body != bodies.end() && body->visible) {
            kept.push_back(item);
        }
    }
    if (kept.size() != m_selection.size()) {
        m_selection.set(std::move(kept));
        choiceChanged();
    }
}

void SelectionController::onStepChanged() {
    if (m_followingStep) {
        return;
    }
    // A step was chosen in the recipe: its body is what is chosen.
    const QString body = m_model->bodyOfStep(m_model->currentStep());
    const bool different = body.isEmpty()
                               ? m_selection.clear()
                               : m_selection.set({*itemFor(body, core::SelectableKind::body, 0)});
    if (different) {
        showInView();
        emit changed();
    }
}

void SelectionController::choiceChanged() {
    showInView();
    // One body (or a part of one) chosen: the panel shows the values of the step that made it.
    QString only;
    bool several = false;
    for (const core::SelectionItem& item : m_selection.items()) {
        const QString body = QString::fromStdString(item.body.value);
        if (only.isEmpty()) {
            only = body;
        } else if (only != body) {
            several = true;
        }
    }
    const int step = !several && !only.isEmpty() ? m_model->stepOfBody(only) : -1;
    m_followingStep = true;
    m_model->selectStep(step);
    m_followingStep = false;
    emit changed();
}

void SelectionController::showInView() {
    if (!m_viewport) {
        return;
    }
    std::vector<render::Highlight> highlights;
    for (const core::SelectionItem& item : m_selection.items()) {
        highlights.push_back({item.body.value, item.kind, item.number, false});
    }
    m_viewport->setSelectionHighlights(std::move(highlights));
}

} // namespace fission::ui
