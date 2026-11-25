// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/ui/section_controller.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>

#include "fission/geo/error.h"
#include "fission/render/scene.h"
#include "fission/ui/document_model.h"
#include "fission/ui/viewport_3d.h"

namespace fission::ui {

SectionController::SectionController(DocumentModel* model,
                                     std::shared_ptr<const geo::GeometryKernel> kernel,
                                     QObject* parent)
    : QObject(parent), m_model(model), m_kernel(std::move(kernel)) {
    connect(&m_caps, &SectionCapWorker::changed, this, [this] {
        if (m_enabled && m_viewport != nullptr) {
            m_viewport->setSectionCaps(m_caps.caps());
        }
    });
    if (m_model != nullptr) {
        // A new or opened document starts without a cut.
        connect(m_model, &DocumentModel::documentReplaced, this, [this] { setEnabled(false); });
        // Bodies move, appear or go: the range follows, the plane stays where it is.
        connect(m_model, &DocumentModel::sceneReady, this, [this] {
            if (m_enabled) {
                refresh(false);
            }
        });
    }
}

void SectionController::attach(Viewport3D* viewport) {
    m_viewport = viewport;
    apply();
}

int SectionController::axisIndex() const noexcept {
    return m_axis == QLatin1String("y") ? 1 : m_axis == QLatin1String("z") ? 2 : 0;
}

bool SectionController::measureRange(double (&lo)[3], double (&hi)[3]) const {
    if (m_model == nullptr || m_kernel == nullptr) {
        return false;
    }
    bool any = false;
    for (const DocumentModel::ExportBody& body : m_model->exportBodies({})) {
        try {
            const geo::Bounds box = m_kernel->exactBounds(body.solid);
            const double a[3] = {box.min.x, box.min.y, box.min.z};
            const double b[3] = {box.max.x, box.max.y, box.max.z};
            for (int i = 0; i < 3; ++i) {
                lo[i] = any ? std::min(lo[i], a[i]) : a[i];
                hi[i] = any ? std::max(hi[i], b[i]) : b[i];
            }
            any = true;
        } catch (const geo::GeometryError&) {
            // a body that cannot be measured does not take part
        }
    }
    return any;
}

void SectionController::refresh(bool centre) {
    double lo[3] = {};
    double hi[3] = {};
    if (!measureRange(lo, hi)) {
        setEnabled(false);
        return;
    }
    for (int i = 0; i < 3; ++i) {
        m_centre[i] = 0.5 * (lo[i] + hi[i]);
    }
    const int axis = axisIndex();
    m_min = lo[axis];
    m_max = hi[axis];
    m_position = centre ? m_centre[axis] : std::clamp(m_position, m_min, m_max);
    apply();
    emit planeChanged();
}

void SectionController::apply() {
    if (!m_enabled) {
        m_caps.reset();
    }
    if (m_viewport == nullptr) {
        return;
    }
    if (!m_enabled) {
        m_viewport->setClipPlane(std::nullopt);
        m_viewport->setSectionCaps(nullptr);
        return;
    }
    const int axis = axisIndex();
    QVector3D point(static_cast<float>(m_centre[0]), static_cast<float>(m_centre[1]),
                    static_cast<float>(m_centre[2]));
    QVector3D normal;
    point[axis] = static_cast<float>(m_position);
    normal[axis] = static_cast<float>(m_side);
    m_viewport->setClipPlane(render::ClipPlane{point, normal});
    requestCaps();
}

void SectionController::requestCaps() {
    if (m_model == nullptr || m_kernel == nullptr || m_viewport == nullptr) {
        return;
    }
    // The old cut faces are wrong for the new place: hide them until the new ones are there.
    m_viewport->setSectionCaps(nullptr);
    std::vector<SectionCapWorker::Body> bodies;
    for (const DocumentModel::ExportBody& body : m_model->exportBodies({})) {
        bodies.push_back(
            {body.id.value, body.solid, QColor(body.color.red, body.color.green, body.color.blue)});
    }
    const int axis = axisIndex();
    geo::Point3d point{m_centre[0], m_centre[1], m_centre[2]};
    geo::Point3d normal{0.0, 0.0, 0.0};
    (axis == 0 ? point.x : axis == 1 ? point.y : point.z) = m_position;
    (axis == 0 ? normal.x : axis == 1 ? normal.y : normal.z) = m_side;
    m_caps.request(m_kernel, std::move(bodies), point, normal);
}

void SectionController::setEnabled(bool enabled) {
    if (enabled == m_enabled) {
        return;
    }
    if (enabled) {
        m_enabled = true;
        m_axis = QStringLiteral("x");
        m_side = 1;
        double lo[3] = {};
        double hi[3] = {};
        if (!measureRange(lo, hi)) {
            m_enabled = false;
            emit nothingToCut();
            return;
        }
        refresh(true);
    } else {
        m_enabled = false;
        apply();
    }
    emit enabledChanged();
}

void SectionController::setAxis(const QString& axis) {
    if (axis == m_axis ||
        (axis != QLatin1String("x") && axis != QLatin1String("y") && axis != QLatin1String("z"))) {
        return;
    }
    m_axis = axis;
    if (m_enabled) {
        refresh(true);
    }
}

void SectionController::setPosition(double position) {
    if (!m_enabled || !std::isfinite(position)) {
        return;
    }
    const double next = std::clamp(position, m_min, m_max);
    if (next == m_position) {
        return;
    }
    m_position = next;
    apply();
    emit planeChanged();
}

void SectionController::flipSide() {
    if (!m_enabled) {
        return;
    }
    m_side = -m_side;
    apply();
    emit planeChanged();
}

} // namespace fission::ui
