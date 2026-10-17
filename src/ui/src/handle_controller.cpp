// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/ui/handle_controller.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "fission/render/scene.h"

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

namespace {

QVector3D vectorOf(const QVariantMap& handle, const QString& x, const QString& y,
                   const QString& z) {
    return {static_cast<float>(handle[x].toDouble()), static_cast<float>(handle[y].toDouble()),
            static_cast<float>(handle[z].toDouble())};
}

/// The world axis (0 = x, 1 = y, 2 = z) a direction runs along, or -1 when it runs slanted.
int axisOf(const QVector3D& direction) {
    for (int axis = 0; axis < 3; ++axis) {
        if (std::abs(direction[axis]) > 0.999F) {
            return axis;
        }
    }
    return -1;
}

double lengthOf(const QPointF& v) {
    return std::hypot(v.x(), v.y());
}

/// How far a label of this size reaches from its middle in the direction `way` (a unit vector).
double reachOf(const QPointF& way, double width, double height) {
    return std::abs(way.x()) * width / 2.0 + std::abs(way.y()) * height / 2.0;
}

double turn(const QPointF& o, const QPointF& a, const QPointF& b) {
    return (a.x() - o.x()) * (b.y() - o.y()) - (a.y() - o.y()) * (b.x() - o.x());
}

/// The convex hull of some points, counter-clockwise (monotone chain).
std::vector<QPointF> convexHull(std::vector<QPointF> points) {
    if (points.size() < 3) {
        return points;
    }
    std::sort(points.begin(), points.end(), [](const QPointF& a, const QPointF& b) {
        return a.x() < b.x() || (a.x() == b.x() && a.y() < b.y());
    });
    std::vector<QPointF> hull(2 * points.size());
    std::size_t k = 0;
    for (const QPointF& p : points) {
        while (k >= 2 && turn(hull[k - 2], hull[k - 1], p) <= 0.0) {
            --k;
        }
        hull[k++] = p;
    }
    for (std::size_t i = points.size() - 1, lower = k + 1; i-- > 0;) {
        while (k >= lower && turn(hull[k - 2], hull[k - 1], points[i]) <= 0.0) {
            --k;
        }
        hull[k++] = points[i];
    }
    hull.resize(k - 1);
    return hull;
}

/// Whether a point is inside a convex polygon that runs counter-clockwise.
bool inside(const std::vector<QPointF>& polygon, const QPointF& point) {
    if (polygon.size() < 3) {
        return false;
    }
    for (std::size_t i = 0; i < polygon.size(); ++i) {
        if (turn(polygon[i], polygon[(i + 1) % polygon.size()], point) < 0.0) {
            return false;
        }
    }
    return true;
}

} // namespace

bool HandleController::bodyBounds(QVector3D& lo, QVector3D& hi) {
    const auto scene = m_model->scene();
    const std::string body = m_model->bodyOfStep(m_model->currentStep()).toStdString();
    if (!scene || body.empty()) {
        return false;
    }
    for (const render::RenderBody& candidate : scene->bodies) {
        if (candidate.id != body || !candidate.mesh || candidate.mesh->positions.empty()) {
            continue;
        }
        if (candidate.id != m_boundsBody || candidate.meshVersion != m_boundsVersion) {
            QVector3D low(1e30F, 1e30F, 1e30F);
            QVector3D high(-1e30F, -1e30F, -1e30F);
            for (const geo::Point3f& p : candidate.mesh->positions) {
                low = QVector3D(std::min(low.x(), p.x), std::min(low.y(), p.y),
                                std::min(low.z(), p.z));
                high = QVector3D(std::max(high.x(), p.x), std::max(high.y(), p.y),
                                 std::max(high.z(), p.z));
            }
            m_boundsBody = candidate.id;
            m_boundsVersion = candidate.meshVersion;
            m_boundsLow = low;
            m_boundsHigh = high;
        }
        lo = m_boundsLow;
        hi = m_boundsHigh;
        return true;
    }
    return false;
}

void HandleController::refresh() {
    QVariantList shown;
    bool compact = false;
    if (m_model != nullptr && m_viewport != nullptr) {
        const QVariantList world = m_model->stepHandles();
        const QVector3D eye = m_viewport->eyePosition();
        const auto origin = [](const QVariantMap& handle) {
            return vectorOf(handle, QStringLiteral("x"), QStringLiteral("y"), QStringLiteral("z"));
        };
        const auto direction = [](const QVariantMap& handle) {
            return vectorOf(handle, QStringLiteral("dx"), QStringLiteral("dy"),
                            QStringLiteral("dz"));
        };
        const auto onScreen = [this](const QVector3D& p) {
            return m_viewport->project(p.x(), p.y(), p.z());
        };

        // One handle for each value, at the place that suits the viewer best. A handle on a face
        // (a width, a height) is taken on the face nearest to the viewer. A handle round an axis
        // (a radius) is taken at the outline, on the side facing the viewer, where it is seen
        // from the side and moves the most on screen (the right one of two). A rounding is taken
        // at the corner furthest right: the nearest corner is in the middle of the body on
        // screen, with no room for its label. While a value is
        // being changed, its place stays (the handle must not jump to another side of the shape).
        struct Choice {
            int index = -1;
            double near = 0.0;   // distance to the eye, squared
            double length = 0.0; // pixels per millimetre along the handle
            double right = 0.0;  // screen x
        };
        QStringList order;
        std::map<QString, Choice> best;
        for (int i = 0; i < world.size(); ++i) {
            const QVariantMap handle = world[i].toMap();
            const QString parameter = handle["parameter"].toString();
            const QVector3D at = origin(handle);
            const QPointF here = onScreen(at);
            Choice choice;
            choice.index = i;
            choice.near = (at - eye).lengthSquared();
            choice.length = lengthOf(onScreen(at + direction(handle)) - here);
            choice.right = here.x();
            const bool round = handle["kind"].toString() == QLatin1String("radius");
            const bool rounding = handle["role"].toString() == QLatin1String("rounding");
            const bool facing = QVector3D::dotProduct(direction(handle), eye - at) >= 0.0F;
            if (!best.contains(parameter)) {
                order.push_back(parameter);
                best[parameter] = choice;
                continue;
            }
            const Choice& before = best[parameter];
            const QVariantMap other = world[before.index].toMap();
            const bool otherFacing =
                QVector3D::dotProduct(direction(other), eye - origin(other)) >= 0.0F;
            bool better = choice.near < before.near - 1e-6;
            if (rounding) {
                better = choice.right > before.right;
            } else if (round) {
                if (facing != otherFacing) {
                    better = facing;
                } else if (std::abs(choice.length - before.length) > 0.1 * before.length) {
                    better = choice.length > before.length;
                } else {
                    better = choice.right > before.right;
                }
            }
            if (better) {
                best[parameter] = choice;
            }
        }
        if (m_editing) {
            for (const QString& parameter : order) {
                const auto kept = m_chosen.find(parameter);
                if (kept != m_chosen.end() && kept->second < world.size() &&
                    world[kept->second].toMap()["parameter"].toString() == parameter) {
                    best[parameter].index = kept->second;
                }
            }
        } else {
            m_chosen.clear();
            for (const auto& [parameter, choice] : best) {
                m_chosen[parameter] = choice.index;
            }
        }

        struct Raw {
            int candidate = 0;
            QVariantMap handle;
            QVector3D atWorld;    // the measured end, on the shape
            QVector3D startWorld; // the other end
            QVector3D axisWorld;  // unit vector the handle moves along
            QPointF at;
            QPointF start;
            double value = 0.0;
            double length = 0.0; // pixels per millimetre along the handle
            QPointF way;         // the way it grows, on screen (unit vector)
            QString role;
        };
        std::vector<Raw> raws;
        QVector3D low(1e30F, 1e30F, 1e30F);
        QVector3D high(-1e30F, -1e30F, -1e30F);
        const auto include = [&low, &high](const QVector3D& p) {
            low = QVector3D(std::min(low.x(), p.x()), std::min(low.y(), p.y()),
                            std::min(low.z(), p.z()));
            high = QVector3D(std::max(high.x(), p.x()), std::max(high.y(), p.y()),
                             std::max(high.z(), p.z()));
        };
        for (int shownIndex = 0; shownIndex < order.size(); ++shownIndex) {
            Raw raw;
            raw.candidate = best[order[shownIndex]].index;
            raw.handle = world[raw.candidate].toMap();
            raw.role = raw.handle["role"].toString();
            raw.atWorld = origin(raw.handle);
            raw.axisWorld = direction(raw.handle).normalized();
            raw.value = raw.handle["value"].toDouble();
            double extent = raw.handle["extent"].toDouble();
            if (m_dragging && shownIndex == m_dragIndex) {
                // The shape is calculated a moment later: the dot goes where the pointer took it
                // at once, so that it does not trail behind.
                raw.value = m_dragValue;
                extent = m_dragExtent + (m_dragValue - m_dragStartValue);
                raw.atWorld = m_dragOrigin +
                              m_dragAxis * static_cast<float>((m_dragValue - m_dragStartValue) *
                                                              m_dragSign * m_dragTravel);
            }
            raw.startWorld = raw.atWorld - raw.axisWorld * static_cast<float>(extent);
            raw.at = onScreen(raw.atWorld);
            raw.start = onScreen(raw.startWorld);
            const QPointF step = onScreen(raw.atWorld + raw.axisWorld) - raw.at;
            raw.length = lengthOf(step);
            raw.way = raw.length > 1e-9 ? step / raw.length : QPointF(0.0, -1.0);
            include(raw.atWorld);
            include(raw.startWorld);
            raws.push_back(raw);
        }

        // The box round the body: the measure bands lie outside it, like the measures of a
        // drawing, so that they never cross the shape. The body as last calculated, and the
        // handles as they are now (a drag is ahead of the calculation).
        QVector3D bodyLow;
        QVector3D bodyHigh;
        if (!raws.empty() && bodyBounds(bodyLow, bodyHigh)) {
            include(bodyLow);
            include(bodyHigh);
        }
        const QVector3D centre = (low + high) / 2.0F;
        QPointF screenLow(1e30, 1e30);
        QPointF screenHigh(-1e30, -1e30);
        for (int corner = 0; corner < 8 && !raws.empty(); ++corner) {
            const QPointF p = onScreen({(corner & 1) != 0 ? high.x() : low.x(),
                                        (corner & 2) != 0 ? high.y() : low.y(),
                                        (corner & 4) != 0 ? high.z() : low.z()});
            screenLow = QPointF(std::min(screenLow.x(), p.x()), std::min(screenLow.y(), p.y()));
            screenHigh = QPointF(std::max(screenHigh.x(), p.x()), std::max(screenHigh.y(), p.y()));
        }
        const QPointF screenCentre = onScreen(centre);
        // The outline of that box on screen (convex hull of its corners), and whether a label
        // there would lie on it.
        std::vector<QPointF> outline;
        for (int corner = 0; corner < 8 && !raws.empty(); ++corner) {
            outline.push_back(onScreen({(corner & 1) != 0 ? high.x() : low.x(),
                                        (corner & 2) != 0 ? high.y() : low.y(),
                                        (corner & 4) != 0 ? high.z() : low.z()}));
        }
        outline = convexHull(outline);
        const auto coversBody = [&outline](const QPointF& chip) {
            const double w = kChipWidth / 2.0;
            const double h = kChipHeight / 2.0;
            for (const QPointF& p : {chip, chip + QPointF(-w, -h), chip + QPointF(w, -h),
                                     chip + QPointF(-w, h), chip + QPointF(w, h)}) {
                if (inside(outline, p)) {
                    return true;
                }
            }
            return false;
        };
        // A body this small on screen keeps only its dots; a band shows when the pointer is on
        // its dot. Everything at once would cover the body.
        compact = !raws.empty() && lengthOf(screenHigh - screenLow) < kCompactPixels;

        struct Placed {
            bool band = false;
            QPointF bandStart; // the band, beside the body
            QPointF bandEnd;
            QPointF edgeStart; // the points on the body it measures
            QPointF edgeEnd;
            QPointF chip;
        };
        std::vector<Placed> placed;
        for (const Raw& raw : raws) {
            Placed place;
            const int axis = axisOf(raw.axisWorld);
            // Looked at nearly end on, a band would be a short slanted stroke: none then, as
            // for dragging (kLeastSight).
            const QVector3D sight = (raw.atWorld - eye).normalized();
            const bool seen = QVector3D::crossProduct(raw.axisWorld, sight).length() >= kLeastSight;
            const bool linear = raw.role == QLatin1String("size") &&
                                raw.handle["kind"].toString() == QLatin1String("distance") &&
                                axis >= 0 && seen;
            if (linear) {
                // The edge of the box the band runs along, and the way out of the box from it.
                const float from = raw.startWorld[axis];
                const float to = raw.atWorld[axis];
                QVector3D base;
                QVector3D out;
                if (axis == 2) {
                    // Upright: at the corner furthest left on screen, out of the corner (a radius
                    // has its label on the right).
                    float bestLeft = 1e30F;
                    for (int corner = 0; corner < 4; ++corner) {
                        const QVector3D p((corner & 1) != 0 ? high.x() : low.x(),
                                          (corner & 2) != 0 ? high.y() : low.y(), centre.z());
                        const auto left = static_cast<float>(onScreen(p).x());
                        if (left < bestLeft) {
                            bestLeft = left;
                            base = p;
                        }
                    }
                    out = QVector3D(base.x() - centre.x(), base.y() - centre.y(), 0.0F);
                    out = out.lengthSquared() > 1e-12F ? out.normalized() : QVector3D(1, 0, 0);
                } else {
                    // Flat: on the bed (the bottom of the body), on the side facing the viewer.
                    // Seen straight from above, the width goes in front and the depth right.
                    const int across = axis == 0 ? 1 : 0;
                    base = centre;
                    base[2] = eye.z() >= low.z() ? low.z() : high.z();
                    const bool near =
                        across == 1 ? eye.y() > centre.y() + 1.0F : eye.x() > centre.x() - 1.0F;
                    base[across] = near ? high[across] : low[across];
                    out = QVector3D();
                    out[across] = near ? 1.0F : -1.0F;
                }
                QVector3D edgeFrom = base;
                QVector3D edgeTo = base;
                edgeFrom[axis] = from;
                edgeTo[axis] = to;
                place.edgeStart = onScreen(edgeFrom);
                place.edgeEnd = onScreen(edgeTo);
                const QPointF along = place.edgeEnd - place.edgeStart;
                const double bandLength = lengthOf(along);
                const QVector3D middle = (edgeFrom + edgeTo) / 2.0F;
                if (bandLength > kShortestBand) {
                    const QPointF unit = along / bandLength;
                    const QPointF normal(-unit.y(), unit.x());
                    auto aside = [&](const QVector3D& way) {
                        return std::abs(
                            QPointF::dotProduct(onScreen(middle + way) - onScreen(middle), normal));
                    };
                    // Looking along the way out (straight at the face), the band would lie on
                    // the edge: then it goes below the body, as in a front view of a drawing.
                    if (axis != 2 && aside(out) < 0.2 * raw.length) {
                        out = QVector3D(0.0F, 0.0F, base.z() <= centre.z() ? -1.0F : 1.0F);
                    }
                    const double perMillimetre = aside(out);
                    if (perMillimetre > 1e-3) {
                        // Far enough out that its label, which sits on the band, stays off the
                        // body.
                        const double wanted = kBandGap + reachOf(normal, kChipWidth, kChipHeight);
                        const auto offset = out * static_cast<float>(wanted / perMillimetre);
                        place.band = true;
                        place.bandStart = onScreen(edgeFrom + offset);
                        place.bandEnd = onScreen(edgeTo + offset);
                        place.chip = (place.bandStart + place.bandEnd) / 2.0;
                    }
                }
            }
            if (!place.band) {
                // A radius, a rounding, a wall, or a size seen end on: the label goes beside the
                // dot, out of the body.
                QPointF out = raw.at - screenCentre;
                const double outLength = lengthOf(out);
                out = outLength > 4.0 ? out / outLength : raw.way;
                if (outLength <= 4.0 && raw.length < 1e-3) {
                    out = QPointF(1.0, 0.0); // a dot in the middle, pointing at the viewer
                }
                place.chip = raw.at + out * (kDotGap + reachOf(out, kChipWidth, kChipHeight));
                // A label on the body would hide it: further out, until it is clear of it.
                for (int step = 0; step < 60 && coversBody(place.chip); ++step) {
                    place.chip += out * 6.0;
                }
            }
            placed.push_back(place);
        }
        for (int pass = 0; pass < 4; ++pass) { // pushed apart where they would lie on each other
            for (std::size_t i = 0; i < placed.size(); ++i) {
                for (std::size_t j = i + 1; j < placed.size(); ++j) {
                    const QPointF gap = placed[j].chip - placed[i].chip;
                    if (std::abs(gap.x()) < kChipWidth && std::abs(gap.y()) < kChipHeight) {
                        const double push = (kChipHeight - std::abs(gap.y())) / 2.0 + 1.0;
                        const double sign = gap.y() >= 0.0 ? 1.0 : -1.0;
                        placed[i].chip.ry() -= sign * push;
                        placed[j].chip.ry() += sign * push;
                    }
                }
            }
        }

        for (std::size_t i = 0; i < raws.size(); ++i) {
            const Raw& raw = raws[i];
            const Placed& place = placed[i];
            // The colour of the axis it runs along, whatever the view; round values and details
            // have no axis and are drawn plain.
            QString axis = QStringLiteral("none");
            const int worldAxis = axisOf(raw.axisWorld);
            if (raw.role == QLatin1String("size") &&
                raw.handle["kind"].toString() == QLatin1String("distance") && worldAxis >= 0) {
                axis = worldAxis == 0   ? QStringLiteral("x")
                       : worldAxis == 1 ? QStringLiteral("y")
                                        : QStringLiteral("z");
            }
            QVariantMap entry;
            entry["index"] = static_cast<int>(i);
            entry["candidate"] = raw.candidate;
            entry["parameter"] = raw.handle["parameter"];
            entry["kind"] = raw.handle["kind"];
            entry["role"] = raw.role.isEmpty() ? QStringLiteral("size") : raw.role;
            entry["axis"] = axis;
            entry["x"] = raw.at.x(); // the dot, on the shape
            entry["y"] = raw.at.y();
            entry["startX"] = raw.start.x(); // the other end of what it measures
            entry["startY"] = raw.start.y();
            entry["band"] = place.band;
            entry["bandStartX"] = place.bandStart.x();
            entry["bandStartY"] = place.bandStart.y();
            entry["bandEndX"] = place.bandEnd.x();
            entry["bandEndY"] = place.bandEnd.y();
            entry["edgeStartX"] = place.edgeStart.x();
            entry["edgeStartY"] = place.edgeStart.y();
            entry["edgeEndX"] = place.edgeEnd.x();
            entry["edgeEndY"] = place.edgeEnd.y();
            entry["chipX"] = place.chip.x();
            entry["chipY"] = place.chip.y();
            entry["dirX"] = raw.way.x(); // which way it grows, on screen
            entry["dirY"] = raw.way.y();
            entry["pixelsPerMillimetre"] = raw.length;
            entry["value"] = raw.value;
            shown.push_back(entry);
        }
    }
    if (compact != m_compact) {
        m_compact = compact;
        emit compactChanged();
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
