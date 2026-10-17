// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/ui/handle_controller.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>

namespace fission::ui {

HandleController::HandleController(DocumentModel* model, QObject* parent)
    : QObject(parent), m_model(model) {
    if (m_model == nullptr) {
        return;
    }
    connect(m_model, &DocumentModel::stepValuesChanged, this, &HandleController::refresh);
    connect(m_model, &DocumentModel::sceneReady, this, [this] { refresh(); });
    connect(m_model, &DocumentModel::currentStepChanged, this, [this] {
        if (m_armed && m_model->currentStep() != m_armedStep && !m_editing) {
            setArmed(false); // something else was chosen: digits are for the views again
        }
        refresh();
    });
    connect(m_model, &DocumentModel::shapePlaced, this, [this](int step, const QString&) {
        m_armedStep = step;
        setArmed(true);
    });
}

void HandleController::attach(Viewport3D* viewport) {
    m_viewport = viewport;
    if (m_viewport != nullptr) {
        connect(m_viewport, &Viewport3D::cameraChanged, this, &HandleController::refresh);
    }
    refresh();
}

void HandleController::setArmed(bool armed) {
    if (armed != m_armed) {
        m_armed = armed;
        emit armedChanged();
    }
}

void HandleController::setEditing(bool editing) {
    if (editing != m_editing) {
        m_editing = editing;
        emit editingChanged();
    }
}

double HandleController::snapStep(double pixelsPerMillimetre) {
    if (pixelsPerMillimetre >= 30.0) {
        return 0.1;
    }
    if (pixelsPerMillimetre >= 3.0) {
        return 1.0;
    }
    return pixelsPerMillimetre >= 0.6 ? 5.0 : 10.0;
}

void HandleController::refresh() {
    QVariantList shown;
    if (m_model != nullptr && m_viewport != nullptr) {
        const QVariantList world = m_model->stepHandles();
        for (int i = 0; i < world.size(); ++i) {
            const QVariantMap handle = world[i].toMap();
            const double x = handle["x"].toDouble();
            const double y = handle["y"].toDouble();
            const double z = handle["z"].toDouble();
            const QPointF at = m_viewport->project(x, y, z);
            // One millimetre further along the handle's direction: where the pointer has to go.
            const QPointF along =
                m_viewport->project(x + handle["dx"].toDouble(), y + handle["dy"].toDouble(),
                                    z + handle["dz"].toDouble());
            const QPointF step = along - at;
            const double length = std::hypot(step.x(), step.y());
            QVariantMap entry;
            entry["index"] = i;
            entry["parameter"] = handle["parameter"];
            entry["kind"] = handle["kind"];
            entry["x"] = at.x();
            entry["y"] = at.y();
            entry["dirX"] = length > 1e-9 ? step.x() / length : 0.0;
            entry["dirY"] = length > 1e-9 ? step.y() / length : 0.0;
            entry["pixelsPerMillimetre"] = length;
            entry["value"] = handle["value"];
            shown.push_back(entry);
        }
    }
    if (shown != m_handles) {
        m_handles = shown;
        emit handlesChanged();
    }
}

bool HandleController::beginDrag(int index, double x, double y) {
    if (m_model == nullptr || m_viewport == nullptr || m_editing || index < 0 ||
        index >= m_model->stepHandles().size()) {
        return false;
    }
    const QVariantMap world = m_model->stepHandles()[index].toMap();
    const QPointF at =
        m_viewport->project(world["x"].toDouble(), world["y"].toDouble(), world["z"].toDouble());
    const QPointF along = m_viewport->project(world["x"].toDouble() + world["dx"].toDouble(),
                                              world["y"].toDouble() + world["dy"].toDouble(),
                                              world["z"].toDouble() + world["dz"].toDouble());
    m_dragPerMillimetre = along - at;
    m_dragIndex = index;
    m_dragStart = QPointF(x, y);
    m_dragStartValue = world["value"].toDouble();
    m_dragValue = m_dragStartValue;
    m_dragSign = world["sign"].toDouble();
    m_dragParameter = world["parameter"].toString();
    m_dragging = true;
    m_model->beginChange(m_dragParameter);
    setEditing(true);
    return true;
}

double HandleController::dragTo(double x, double y) {
    if (!m_dragging || m_model == nullptr) {
        return m_dragValue;
    }
    const double length2 = m_dragPerMillimetre.x() * m_dragPerMillimetre.x() +
                           m_dragPerMillimetre.y() * m_dragPerMillimetre.y();
    // Looking along the handle: the pointer cannot tell how far to go. The value stays.
    if (length2 < 0.25) {
        return m_dragValue;
    }
    const QPointF moved = QPointF(x, y) - m_dragStart;
    const double millimetres =
        (moved.x() * m_dragPerMillimetre.x() + moved.y() * m_dragPerMillimetre.y()) / length2;
    const double step = snapStep(std::sqrt(length2));
    const double wanted = std::round((m_dragStartValue + m_dragSign * millimetres) / step) * step;
    // Where the shape stops being possible (a corner radius wider than half the box, no wall left),
    // the handle stops, too.
    if (wanted != m_dragValue && m_model->acceptsValue(m_dragParameter, wanted)) {
        if (m_model->setStepValue(m_model->currentStep(), m_dragParameter,
                                  QString::number(wanted, 'g', 12))) {
            m_dragValue = wanted;
        }
    }
    return m_dragValue;
}

bool HandleController::endDrag() {
    if (!m_dragging) {
        return false;
    }
    m_dragging = false;
    m_model->commitChange();
    setEditing(false);
    return true;
}

void HandleController::cancelDrag() {
    if (!m_dragging) {
        return;
    }
    m_dragging = false;
    m_model->cancelChange();
    setEditing(false);
}

bool HandleController::typeInsteadOfDrag() {
    if (!m_dragging) {
        return false;
    }
    m_dragging = false; // still editing: the same transaction goes on
    return true;
}

bool HandleController::beginEdit(int index) {
    if (m_model == nullptr || m_editing || index < 0 || index >= m_model->stepHandles().size()) {
        return false;
    }
    m_model->beginChange(m_model->stepHandles()[index].toMap()["parameter"].toString());
    setEditing(true);
    return true;
}

bool HandleController::setValue(int index, const QString& text) {
    if (!m_editing || m_model == nullptr || index < 0 || index >= m_model->stepHandles().size()) {
        return false;
    }
    return m_model->setStepValue(m_model->currentStep(),
                                 m_model->stepHandles()[index].toMap()["parameter"].toString(),
                                 text);
}

bool HandleController::commitEdit() {
    if (!m_editing || m_dragging) {
        return false;
    }
    m_model->commitChange();
    setEditing(false);
    setArmed(false);
    return true;
}

void HandleController::cancelEdit() {
    if (!m_editing || m_dragging) {
        return;
    }
    m_model->cancelChange();
    setEditing(false);
}

} // namespace fission::ui
