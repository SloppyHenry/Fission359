// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/ui/handle_controller.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <vector>

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
        const QVector3D eye = m_viewport->eyePosition();
        const auto origin = [](const QVariantMap& handle) {
            return QVector3D(static_cast<float>(handle["x"].toDouble()),
                             static_cast<float>(handle["y"].toDouble()),
                             static_cast<float>(handle["z"].toDouble()));
        };

        // One handle for each value, at the place that is nearest to the viewer. While a value is
        // being changed, its place stays (the handle must not jump to another side of the shape).
        QStringList order;
        std::map<QString, int> best;
        for (int i = 0; i < world.size(); ++i) {
            const QVariantMap handle = world[i].toMap();
            const QString parameter = handle["parameter"].toString();
            if (!best.contains(parameter)) {
                order.push_back(parameter);
                best[parameter] = i;
                continue;
            }
            const double here = (origin(handle) - eye).lengthSquared();
            const double before = (origin(world[best[parameter]].toMap()) - eye).lengthSquared();
            if (here < before - 1e-6) {
                best[parameter] = i;
            }
        }
        if (m_editing) {
            for (const QString& parameter : order) {
                const auto kept = m_chosen.find(parameter);
                if (kept != m_chosen.end() && kept->second < world.size() &&
                    world[kept->second].toMap()["parameter"].toString() == parameter) {
                    best[parameter] = kept->second;
                }
            }
        } else {
            m_chosen = best;
        }

        struct Raw {
            int candidate = 0;
            QVariantMap handle;
            QPointF at;    // the measured end, on the shape
            QPointF start; // the other end
            double value = 0.0;
            double length = 0.0; // pixels per millimetre along the handle
            QPointF way;         // the way it grows, on screen (unit vector)
        };
        std::vector<Raw> raws;
        for (int shownIndex = 0; shownIndex < order.size(); ++shownIndex) {
            Raw raw;
            raw.candidate = best[order[shownIndex]];
            raw.handle = world[raw.candidate].toMap();
            QVector3D at = origin(raw.handle);
            const QVector3D direction(static_cast<float>(raw.handle["dx"].toDouble()),
                                      static_cast<float>(raw.handle["dy"].toDouble()),
                                      static_cast<float>(raw.handle["dz"].toDouble()));
            raw.value = raw.handle["value"].toDouble();
            double extent = raw.handle["extent"].toDouble();
            if (m_dragging && shownIndex == m_dragIndex) {
                // The shape is calculated a moment later: the dot goes where the pointer took it
                // at once, so that it does not trail behind.
                raw.value = m_dragValue;
                extent = m_dragExtent + (m_dragValue - m_dragStartValue);
                at = m_dragOrigin +
                     m_dragAxis * static_cast<float>((m_dragValue - m_dragStartValue) * m_dragSign *
                                                     m_dragTravel);
            }
            const QVector3D from = at - direction.normalized() * static_cast<float>(extent);
            raw.at = m_viewport->project(at.x(), at.y(), at.z());
            raw.start = m_viewport->project(from.x(), from.y(), from.z());
            const QVector3D next = at + direction;
            const QPointF step = m_viewport->project(next.x(), next.y(), next.z()) - raw.at;
            raw.length = std::hypot(step.x(), step.y());
            raw.way = raw.length > 1e-9 ? step / raw.length : QPointF(0.0, -1.0);
            raws.push_back(raw);
        }

        // Like the measures of a drawing: the band lies beside the shape, not across it, with
        // lines from its ends to the points it measures. "Beside" is away from the middle of all
        // the handles, and the labels make room for each other.
        QPointF middle(0.0, 0.0);
        for (const Raw& raw : raws) {
            middle += raw.at + raw.start;
        }
        if (!raws.empty()) {
            middle /= 2.0 * static_cast<double>(raws.size());
        }
        constexpr double kOffset = 26.0;
        constexpr double kChipWidth = 112.0;
        constexpr double kChipHeight = 24.0;
        std::vector<QPointF> chips;
        std::vector<QPointF> normals;
        for (const Raw& raw : raws) {
            const double length =
                std::hypot(raw.at.x() - raw.start.x(), raw.at.y() - raw.start.y());
            const QPointF centre = (raw.at + raw.start) / 2.0; // where the band lies
            QPointF normal;
            if (length > 6.0) {
                normal = QPointF(-raw.way.y(), raw.way.x());
                if (QPointF::dotProduct(normal, centre - middle) < 0.0) {
                    normal = -normal;
                }
            } else {
                // Nothing to measure yet (a corner radius of 0): out of the shape, along the way
                // from the middle.
                const QPointF out = raw.at - middle;
                const double outLength = std::hypot(out.x(), out.y());
                normal = outLength > 1e-6 ? out / outLength : QPointF(0.0, -1.0);
            }
            normals.push_back(normal);
            // The label sits beyond the arrow of the handle, where the band points to.
            const QPointF way = raw.way;
            const double reach =
                24.0 + std::abs(way.x()) * kChipWidth / 2.0 + std::abs(way.y()) * kChipHeight / 2.0;
            chips.push_back(raw.at + normal * kOffset + way * reach);
        }
        for (int pass = 0; pass < 4; ++pass) { // pushed apart where they would lie on each other
            for (std::size_t i = 0; i < chips.size(); ++i) {
                for (std::size_t j = i + 1; j < chips.size(); ++j) {
                    const QPointF gap = chips[j] - chips[i];
                    if (std::abs(gap.x()) < kChipWidth && std::abs(gap.y()) < kChipHeight) {
                        const double push = (kChipHeight - std::abs(gap.y())) / 2.0 + 1.0;
                        const double direction = gap.y() >= 0.0 ? 1.0 : -1.0;
                        chips[i].ry() -= direction * push;
                        chips[j].ry() += direction * push;
                    }
                }
            }
        }

        for (std::size_t i = 0; i < raws.size(); ++i) {
            const Raw& raw = raws[i];
            const QPointF shift = normals[i] * kOffset;
            const QPointF dot = raw.at + shift;
            const QPointF bandStart = raw.start + shift;
            const QPointF along = raw.way;
            // The colour of the axis it runs along, whatever the view (round shapes: their own).
            QString axis = QStringLiteral("radial");
            const QString name = raw.handle["parameter"].toString();
            const bool measuresAxis = name != QLatin1String("cornerRadius") &&
                                      name != QLatin1String("wall") &&
                                      name != QLatin1String("tubeRadius");
            if (raw.handle["kind"].toString() != QLatin1String("radius") && measuresAxis) {
                const double ax = std::abs(raw.handle["dx"].toDouble());
                const double ay = std::abs(raw.handle["dy"].toDouble());
                const double az = std::abs(raw.handle["dz"].toDouble());
                axis = az >= ax && az >= ay ? QStringLiteral("z")
                       : ax >= ay           ? QStringLiteral("x")
                                            : QStringLiteral("y");
            }
            QVariantMap entry;
            entry["index"] = static_cast<int>(i);
            entry["candidate"] = raw.candidate;
            entry["parameter"] = raw.handle["parameter"];
            entry["kind"] = raw.handle["kind"];
            entry["axis"] = axis;
            entry["x"] = dot.x(); // the handle
            entry["y"] = dot.y();
            entry["fromX"] = bandStart.x(); // the other end of the band
            entry["fromY"] = bandStart.y();
            entry["pointX"] = raw.at.x(); // the points on the shape that it measures
            entry["pointY"] = raw.at.y();
            entry["startPointX"] = raw.start.x();
            entry["startPointY"] = raw.start.y();
            entry["chipX"] = chips[i].x();
            entry["chipY"] = chips[i].y();
            entry["dirX"] = along.x(); // which way it grows, on screen
            entry["dirY"] = along.y();
            entry["pixelsPerMillimetre"] = raw.length;
            entry["value"] = raw.value;
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
        index >= m_handles.size()) {
        return false;
    }
    const int candidate = m_handles[index].toMap()["candidate"].toInt();
    if (candidate < 0 || candidate >= m_model->stepHandles().size()) {
        return false;
    }
    const QVariantMap world = m_model->stepHandles()[candidate].toMap();
    m_dragExtent = world["extent"].toDouble();
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
    if (m_model == nullptr || m_editing || index < 0 || index >= m_handles.size()) {
        return false;
    }
    m_model->beginChange(m_handles[index].toMap()["parameter"].toString());
    setEditing(true);
    return true;
}

bool HandleController::setValue(int index, const QString& text) {
    if (!m_editing || m_model == nullptr || index < 0 || index >= m_handles.size()) {
        return false;
    }
    return m_model->setStepValue(m_model->currentStep(),
                                 m_handles[index].toMap()["parameter"].toString(), text);
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
