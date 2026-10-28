// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/ui/placement_controller.h"

#include <QColor>
#include <QVector3D>

#include <array>
#include <cmath>
#include <vector>

#include "fission/geo/seat.h"
#include "fission/interaction/snap.h"

namespace fission::ui {

namespace {
double snapped(double value, double step) {
    return std::round(value / step) * step;
}

// The point on a face, moved along the face onto whole millimetres of the shape's own X and Y
// (on a wall that runs along an axis these are millimetres of the world as well). The distance
// from the face stays what it is.
geo::Point3d snappedOnFace(const Viewport3D::Landing& landing, double step) {
    const geo::Point3d normal{landing.nx, landing.ny, landing.nz};
    const geo::Matrix3 turn = geo::turnOntoFace(normal);
    geo::Point3d point{landing.x, landing.y, landing.z};
    for (int column = 0; column < 2; ++column) {
        const geo::Point3d axis{turn.m[0][column], turn.m[1][column], turn.m[2][column]};
        const double along = point.x * axis.x + point.y * axis.y + point.z * axis.z;
        const double off = along - snapped(along, step);
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

double PlacementController::snapStep() const {
    const double chosen = m_viewport != nullptr ? m_viewport->fixedSnapStep() : 0.0;
    return chosen > 0.0 ? chosen : kSnapMillimetres;
}

void PlacementController::clearSnap() {
    m_snap.end();
    if (m_marked && m_viewport != nullptr) {
        m_viewport->setHandleGraphics({}, Viewport3D::HandleSource::placement);
    }
    m_marked = false;
}

std::pair<double, double> PlacementController::flatSpot(const Viewport3D::Landing& landing) {
    if (snappingSuppressed()) {
        clearSnap();
        return {landing.x, landing.y};
    }
    const double step = snapStep();
    const std::pair<double, double> onGrid{snapped(landing.x, step), snapped(landing.y, step)};
    // The size of the shape is known a moment after it appeared: the very first place is the grid.
    const auto bounds = m_model->placingBodyBounds();
    const auto scene = m_model->scene();
    if (!bounds || !scene) {
        clearSnap();
        return onGrid;
    }
    // The shape stands on its underside, centred on the place (rule of the shapes): its footprint
    // is the 9 points of the lower side of the box round it, as they would be at the origin.
    const double halfX = (bounds->max.x - bounds->min.x) / 2.0;
    const double halfY = (bounds->max.y - bounds->min.y) / 2.0;
    const double height = bounds->max.z - bounds->min.z;
    auto movers =
        interaction::snapPointsOfBoxSide({-halfX, -halfY, 0.0}, {halfX, halfY, height}, 2, false);
    if (m_snap.active()) {
        m_snap.setMovers(std::move(movers));
    } else {
        m_snap.beginWithMovers(*scene, m_model->placingBodyId().toStdString(), std::move(movers));
    }
    const QVector3D around(static_cast<float>(landing.x), static_cast<float>(landing.y),
                           static_cast<float>(landing.z));
    const interaction::SnapResult caught =
        m_snap.snap({landing.x, landing.y, landing.z}, {true, true, false},
                    snapReachAround(*m_viewport, around));
    render::HandleGraphics marks;
    if (caught.caught()) {
        marks = SnapSession::marksOf(caught, m_viewport->property("accentColor").value<QColor>(),
                                     millimetresPerPixelAt(*m_viewport, around));
    }
    m_viewport->setHandleGraphics(marks, Viewport3D::HandleSource::placement);
    m_marked = !marks.empty();
    return {caught.axes[0] ? caught.offset[0] : onGrid.first,
            caught.axes[1] ? caught.offset[1] : onGrid.second};
}

bool PlacementController::begin(const QString& type, bool asHole, double x, double y,
                                bool upright) {
    if (m_model == nullptr || m_viewport == nullptr) {
        return false;
    }
    clearSnap();
    const auto landing = m_viewport->landingAt(x, y, {}, m_model->workplaneZ(), !upright);
    if (!landing) {
        return false;
    }
    // (The shape does not exist yet: nothing to catch with, only the grid, or free with Shift.)
    const auto spot = flatSpot(*landing);
    if (m_model->startPlacing(type, asHole, spot.first, spot.second, landing->z) < 0) {
        return false;
    }
    if (landing->tilted) {
        m_model->placeOnFace(snappedOnFace(*landing, snapStep()),
                             {landing->nx, landing->ny, landing->nz});
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
        clearSnap(); // on a wall only the grid along the face
        m_model->placeOnFace(snappedOnFace(*landing, snapStep()),
                             {landing->nx, landing->ny, landing->nz});
    } else {
        const auto spot = flatSpot(*landing);
        m_model->placeAt(spot.first, spot.second, landing->z, landing->surface);
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
    clearSnap();
    return m_model != nullptr && m_model->finishPlacing();
}

void PlacementController::cancel() {
    clearSnap();
    if (m_model != nullptr) {
        m_model->cancelPlacing();
    }
}

bool PlacementController::seatChosen(double x, double y) {
    if (m_model == nullptr || m_viewport == nullptr || m_model->placing()) {
        return false;
    }
    const QString body = m_model->bodyOfStep(m_model->currentStep());
    if (body.isEmpty()) {
        return false;
    }
    // The body itself is looked through, or it would lie on its own top.
    const auto landing = m_viewport->landingAt(x, y, body, m_model->workplaneZ(), true);
    if (!landing) {
        return false;
    }
    const double step = snapStep();
    const geo::Point3d normal{landing->nx, landing->ny, landing->nz};
    const geo::Point3d point =
        landing->tilted
            ? snappedOnFace(*landing, step)
            : geo::Point3d{snapped(landing->x, step), snapped(landing->y, step), landing->z};
    return m_model->seatBodyOnFace(body, point, normal);
}

int PlacementController::add(const QString& type, bool asHole) {
    return m_model != nullptr ? m_model->addShape(type, asHole) : -1;
}

} // namespace fission::ui
