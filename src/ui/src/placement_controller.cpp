// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/ui/placement_controller.h"

#include <cmath>

#include "fission/geo/seat.h"

namespace fission::ui {

namespace {
double snapped(double value) {
    return std::round(value / PlacementController::kSnapMillimetres) *
           PlacementController::kSnapMillimetres;
}

// The point on a face, moved along the face onto whole millimetres of the shape's own X and Y
// (on a wall that runs along an axis these are millimetres of the world as well). The distance
// from the face stays what it is.
geo::Point3d snappedOnFace(const Viewport3D::Landing& landing) {
    const geo::Point3d normal{landing.nx, landing.ny, landing.nz};
    const geo::Matrix3 turn = geo::turnOntoFace(normal);
    geo::Point3d point{landing.x, landing.y, landing.z};
    for (int column = 0; column < 2; ++column) {
        const geo::Point3d axis{turn.m[0][column], turn.m[1][column], turn.m[2][column]};
        const double along = point.x * axis.x + point.y * axis.y + point.z * axis.z;
        const double off = along - snapped(along);
        point = {point.x - off * axis.x, point.y - off * axis.y, point.z - off * axis.z};
    }
    return point;
}

} // namespace

PlacementController::PlacementController(DocumentModel* model, QObject* parent)
    : QObject(parent), m_model(model) {
    if (m_model != nullptr) {
        connect(m_model, &DocumentModel::placingChanged, this, &PlacementController::activeChanged);
    }
}

bool PlacementController::begin(const QString& type, bool asHole, double x, double y,
                                bool upright) {
    if (m_model == nullptr || m_viewport == nullptr) {
        return false;
    }
    const auto landing = m_viewport->landingAt(x, y, {}, m_model->workplaneZ(), !upright);
    if (!landing) {
        return false;
    }
    if (m_model->startPlacing(type, asHole, snapped(landing->x), snapped(landing->y), landing->z) <
        0) {
        return false;
    }
    if (landing->tilted) {
        m_model->placeOnFace(snappedOnFace(*landing), {landing->nx, landing->ny, landing->nz});
    }
    return true;
}

bool PlacementController::landAt(double x, double y, bool upright) {
    // The shape itself is looked through, or it would stand on its own top.
    const auto landing =
        m_viewport->landingAt(x, y, m_model->placingBodyId(), m_model->workplaneZ(), !upright);
    if (!landing) {
        return false; // the ray misses everything: the shape stays where it was
    }
    if (landing->tilted) {
        m_model->placeOnFace(snappedOnFace(*landing), {landing->nx, landing->ny, landing->nz});
    } else {
        m_model->placeAt(snapped(landing->x), snapped(landing->y), landing->z, landing->surface);
    }
    return true;
}

void PlacementController::moveTo(double x, double y, bool upright) {
    if (m_model == nullptr || m_viewport == nullptr || !m_model->placing()) {
        return;
    }
    landAt(x, y, upright);
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
