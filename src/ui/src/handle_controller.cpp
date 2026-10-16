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
    // Whole millimetres are the everyday step: a corner radius of 0 to 10 mm must not come in
    // jumps of 5. Finer when zoomed in, coarser only when the handle is hardly longer than a
    // pixel per millimetre.
    if (pixelsPerMillimetre >= 20.0) {
        return 0.1;
    }
    if (pixelsPerMillimetre >= 6.0) {
        return 0.5;
    }
    if (pixelsPerMillimetre >= 0.8) {
        return 1.0;
    }
    return pixelsPerMillimetre >= 0.3 ? 5.0 : 10.0;
}

void HandleController::refresh() {
    QVariantList shown;
    if (m_model != nullptr && m_viewport != nullptr) {
        const QVariantList world = m_model->stepHandles();
        for (int i = 0; i < world.size(); ++i) {
            const QVariantMap handle = world[i].toMap();
            QVector3D at(static_cast<float>(handle["x"].toDouble()),
                         static_cast<float>(handle["y"].toDouble()),
                         static_cast<float>(handle["z"].toDouble()));
            const QVector3D direction(static_cast<float>(handle["dx"].toDouble()),
                                      static_cast<float>(handle["dy"].toDouble()),
                                      static_cast<float>(handle["dz"].toDouble()));
            double value = handle["value"].toDouble();
            if (m_dragging && i == m_dragIndex) {
                // The shape is calculated a moment later: the dot goes where the pointer took it
                // at once, so that it does not trail behind.
                value = m_dragValue;
                at = m_dragOrigin +
                     m_dragAxis * static_cast<float>((m_dragValue - m_dragStartValue) * m_dragSign *
                                                     m_dragTravel);
            }
            const QPointF here = m_viewport->project(at.x(), at.y(), at.z());
            // One millimetre further along the handle's direction: which way the dot moves.
            const QVector3D next = at + direction;
            const QPointF step = m_viewport->project(next.x(), next.y(), next.z()) - here;
            const double length = std::hypot(step.x(), step.y());
            QVariantMap entry;
            entry["index"] = i;
            entry["parameter"] = handle["parameter"];
            entry["kind"] = handle["kind"];
            entry["x"] = here.x();
            entry["y"] = here.y();
            entry["dirX"] = length > 1e-9 ? step.x() / length : 0.0;
            entry["dirY"] = length > 1e-9 ? step.y() / length : 0.0;
            entry["pixelsPerMillimetre"] = length;
            entry["value"] = value;
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
    m_dragOrigin = QVector3D(static_cast<float>(world["x"].toDouble()),
                             static_cast<float>(world["y"].toDouble()),
                             static_cast<float>(world["z"].toDouble()));
    m_dragAxis = QVector3D(static_cast<float>(world["dx"].toDouble()),
                           static_cast<float>(world["dy"].toDouble()),
                           static_cast<float>(world["dz"].toDouble()))
                     .normalized();
    m_dragStartValue = world["value"].toDouble();
    m_dragValue = m_dragStartValue;
    m_dragSign = world["sign"].toDouble();
    m_dragTravel = std::max(world["travel"].toDouble(), 0.05);
    m_dragParameter = world["parameter"].toString();
    m_dragIndex = index;

    // The plane through the axis that faces the viewer most: the part of the line of sight that
    // does not run along the axis.
    m_dragUsable = false;
    m_dragPlane = {};
    m_dragGrabbed = 0.0;
    if (const auto ray = m_viewport->rayAt(x, y)) {
        const QVector3D across =
            ray->direction - QVector3D::dotProduct(ray->direction, m_dragAxis) * m_dragAxis;
        if (across.length() >= kLeastSight) {
            m_dragPlane = across.normalized();
            const float denominator = QVector3D::dotProduct(ray->direction, m_dragPlane);
            if (std::abs(denominator) > 1e-6F) {
                const float reach =
                    QVector3D::dotProduct(m_dragOrigin - ray->origin, m_dragPlane) / denominator;
                const QVector3D met = ray->origin + ray->direction * reach;
                m_dragGrabbed = QVector3D::dotProduct(met - m_dragOrigin, m_dragAxis);
                m_dragUsable = reach > 0.0F;
            }
        }
    }
    const QPointF here = m_viewport->project(m_dragOrigin.x(), m_dragOrigin.y(), m_dragOrigin.z());
    const QVector3D next = m_dragOrigin + m_dragAxis;
    const QPointF step = m_viewport->project(next.x(), next.y(), next.z()) - here;
    m_dragPixelsPerMillimetre = std::hypot(step.x(), step.y());

    m_dragging = true;
    m_model->beginChange(m_dragParameter);
    setEditing(true);
    return true;
}

double HandleController::dragTo(double x, double y) {
    if (!m_dragging || m_model == nullptr || !m_dragUsable) {
        return m_dragValue;
    }
    const auto ray = m_viewport->rayAt(x, y);
    if (!ray) {
        return m_dragValue;
    }
    const float denominator = QVector3D::dotProduct(ray->direction, m_dragPlane);
    if (std::abs(denominator) < 1e-6F) {
        return m_dragValue;
    }
    const float reach =
        QVector3D::dotProduct(m_dragOrigin - ray->origin, m_dragPlane) / denominator;
    if (reach <= 0.0F) {
        return m_dragValue; // behind the eye
    }
    const QVector3D met = ray->origin + ray->direction * reach;
    // How far along the axis the pointer has gone since it grabbed the handle. The face under the
    // pointer follows it one to one; a shape that grows to both sides changes its value by more.
    const double moved = QVector3D::dotProduct(met - m_dragOrigin, m_dragAxis) - m_dragGrabbed;
    const double step = snapStep(m_dragPixelsPerMillimetre);
    double wanted =
        std::round((m_dragStartValue + m_dragSign * moved / m_dragTravel) / step) * step;
    wanted = std::min(wanted, kLargestMillimetres);
    // Where the shape stops being possible (a corner radius wider than half the box, no wall left),
    // the handle stops, too.
    if (wanted != m_dragValue && m_model->acceptsValue(m_dragParameter, wanted) &&
        m_model->setStepValue(m_model->currentStep(), m_dragParameter,
                              QString::number(wanted, 'g', 12))) {
        m_dragValue = wanted;
        refresh(); // the dot goes with the pointer at once
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
    refresh();
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
