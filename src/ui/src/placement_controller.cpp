// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/ui/placement_controller.h"

#include <cmath>

namespace fission::ui {

namespace {
double snapped(double value) {
    return std::round(value / PlacementController::kSnapMillimetres) *
           PlacementController::kSnapMillimetres;
}
} // namespace

PlacementController::PlacementController(DocumentModel* model, QObject* parent)
    : QObject(parent), m_model(model) {
    if (m_model != nullptr) {
        connect(m_model, &DocumentModel::placingChanged, this, &PlacementController::activeChanged);
    }
}

bool PlacementController::begin(const QString& type, bool asHole, double x, double y) {
    if (m_model == nullptr || m_viewport == nullptr) {
        return false;
    }
    const auto landing = m_viewport->landingAt(x, y, {}, m_model->workplaneZ());
    if (!landing) {
        return false;
    }
    return m_model->startPlacing(type, asHole, snapped(landing->x), snapped(landing->y),
                                 landing->z) >= 0;
}

bool PlacementController::landAt(double x, double y) {
    // The shape itself is looked through, or it would stand on its own top.
    const auto landing =
        m_viewport->landingAt(x, y, m_model->placingBodyId(), m_model->workplaneZ());
    if (!landing) {
        return false; // the ray misses everything: the shape stays where it was
    }
    m_model->placeAt(snapped(landing->x), snapped(landing->y), landing->z, landing->surface);
    return true;
}

void PlacementController::moveTo(double x, double y) {
    if (m_model == nullptr || m_viewport == nullptr || !m_model->placing()) {
        return;
    }
    landAt(x, y);
}

bool PlacementController::finish() {
    return m_model != nullptr && m_model->finishPlacing();
}

void PlacementController::cancel() {
    if (m_model != nullptr) {
        m_model->cancelPlacing();
    }
}

int PlacementController::add(const QString& type, bool asHole) {
    return m_model != nullptr ? m_model->addShape(type, asHole) : -1;
}

} // namespace fission::ui
