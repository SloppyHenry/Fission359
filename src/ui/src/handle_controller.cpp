// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/ui/handle_controller.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "fission/interaction/drag.h"
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
        m_keepChoice = false;
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
        connect(m_viewport, &Viewport3D::cameraChanged, this, [this] {
            m_keepChoice = false; // turned: the handles may go to the side facing the viewer
            refresh();
        });
    }
    refresh();
}

void HandleController::setArmed(bool armed) {
    if (armed != m_armed) {
        m_armed = armed;
        emit armedChanged();
    }
}

void HandleController::setMode(const QString& mode) {
    if (mode == m_mode) {
        return;
    }
    if (m_dragging) {
        endDrag(); // a drag in progress keeps what it did
    }
    m_mode = mode;
    if (m_mode == QLatin1String("off")) {
        setArmed(false); // no handles, nothing to type sizes into (the tools Select and Rotate)
    }
    emit modeChanged();
    refresh();
}

void HandleController::setEditing(bool editing) {
    if (editing != m_editing) {
        m_editing = editing;
        emit editingChanged();
    }
}

double HandleController::snapStep(double pixelsPerMillimetre) {
    return interaction::snapStep(pixelsPerMillimetre);
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

/// How far and which way the handle also moves when its value grows by 1 mm (mm, a vector).
QVector3D riseOf(const QVariantMap& handle) {
    return static_cast<float>(handle["rise"].toDouble()) *
           QVector3D(static_cast<float>(handle["riseDx"].toDouble()),
                     static_cast<float>(handle["riseDy"].toDouble()),
                     static_cast<float>(handle["riseDz"].toDouble()));
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
    render::HandleGraphics graphics;
    bool compact = false;
    if (m_model != nullptr && m_viewport != nullptr && m_mode != QLatin1String("off")) {
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
        // While a value is changed, and after that until the view turns, its place stays: a
        // handle must not jump to another side of the shape under the pointer.
        if (m_editing || m_keepChoice) {
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
            // The same in the scene (mm), for drawing it there, and the stroke across an end.
            QVector3D bandStartWorld;
            QVector3D bandEndWorld;
            QVector3D edgeStartWorld;
            QVector3D edgeEndWorld;
            QVector3D tick;
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
                        place.bandStartWorld = edgeFrom + offset;
                        place.bandEndWorld = edgeTo + offset;
                        place.edgeStartWorld = edgeFrom;
                        place.edgeEndWorld = edgeTo;
                        place.tick = out * static_cast<float>(kTickPixels / perMillimetre);
                        place.bandStart = onScreen(place.bandStartWorld);
                        place.bandEnd = onScreen(place.bandEndWorld);
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

        // Whether a body is in front: the label of a hidden band steps back, like the band.
        const bool solidBodies = m_viewport->displayMode() != QLatin1String("wire");
        for (std::size_t i = 0; i < raws.size(); ++i) {
            const Raw& raw = raws[i];
            const Placed& place = placed[i];
            const QVector3D probe =
                place.band ? (place.bandStartWorld + place.bandEndWorld) / 2.0F : raw.atWorld;
            const bool hidden = solidBodies && hiddenFromEye(probe, eye);
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
            entry["hidden"] = hidden;
            shown.push_back(entry);
            // The arrow shows where the handle really goes (out and up on a sphere).
            const QVector3D goes =
                (raw.axisWorld * static_cast<float>(raw.handle["sign"].toDouble() *
                                                    raw.handle["travel"].toDouble()) +
                 riseOf(raw.handle))
                    .normalized();
            drawHandle(graphics, raw.atWorld, raw.startWorld, goes, place.band,
                       place.edgeStartWorld, place.edgeEndWorld, place.bandStartWorld,
                       place.bandEndWorld, place.tick, static_cast<int>(i), axis,
                       entry["role"].toString(), compact);
        }
    }
    if (m_viewport != nullptr) {
        m_viewport->setHandleGraphics(std::move(graphics));
    }
    if (compact != m_compact) {
        m_compact = compact;
        emit compactChanged();
    }
    // A step without handles for a moment while one is held (being calculated, or failed): the
    // held handle keeps its place in the window. Taking it away under the pointer would leave the
    // drag, and with it the step in the history, open (undo would not work).
    if (m_dragging && shown.isEmpty() && !m_handles.isEmpty()) {
        return;
    }
    if (shown != m_handles) {
        m_handles = shown;
        emit handlesChanged();
    }
}

void HandleController::drawHandle(render::HandleGraphics& graphics, const QVector3D& at,
                                  const QVector3D& start, const QVector3D& axisWorld, bool band,
                                  const QVector3D& edgeStart, const QVector3D& edgeEnd,
                                  const QVector3D& bandStart, const QVector3D& bandEnd,
                                  const QVector3D& tick, int index, const QString& axis,
                                  const QString& role, bool compact) const {
    const bool active = m_editing && m_current == index;
    const bool pointed = m_pointed == index;
    const bool detail = role != QLatin1String("size");
    if (compact && detail && !active) {
        return; // on a small body the details step aside (typing and the panel reach them)
    }
    // With the tool "Move" only the bands are left; the gizmo moves the body.
    const bool dots = m_mode == QLatin1String("size");
    // Lines and labels in the colour of the axis; values without an axis plain, with dark dots
    // and arrows that show on a light body.
    const QColor tone = axis == QLatin1String("x")   ? m_colours.x
                        : axis == QLatin1String("y") ? m_colours.y
                        : axis == QLatin1String("z") ? m_colours.z
                                                     : m_colours.plain;
    const QColor ink = axis == QLatin1String("none") ? m_colours.ink : tone;
    // While another value changes, the rest steps back.
    const float calm = m_editing && !active ? kCalm : 1.0F;
    const auto faded = [calm](QColor color) {
        color.setAlphaF(color.alphaF() * calm);
        return color;
    };
    if (band && (!compact || pointed || active)) {
        // As in a drawing: thin lines from the points it measures, the band between them with a
        // stroke across each end.
        graphics.lines.push_back({edgeStart, bandStart, 1.0F, faded(tone)});
        graphics.lines.push_back({edgeEnd, bandEnd, 1.0F, faded(tone)});
        graphics.lines.push_back({bandStart, bandEnd, 1.5F, faded(tone)});
        graphics.lines.push_back({bandStart - tick, bandStart + tick, 1.5F, faded(tone)});
        graphics.lines.push_back({bandEnd - tick, bandEnd + tick, 1.5F, faded(tone)});
    }
    if (!dots) {
        return;
    }
    if (!band && (pointed || active)) {
        // A radius, a rounding, a wall: what it measures, on the shape (inside a body it shows
        // faint, seen through).
        graphics.lines.push_back({start, at, 3.5F, m_colours.fill});
        graphics.lines.push_back({start, at, 1.5F, ink});
    }
    if (pointed || active) {
        graphics.arrows.push_back({at, axisWorld, ink, m_colours.fill});
    }
    render::HandleDot dot;
    dot.centre = at;
    dot.rounding = role == QLatin1String("rounding");
    dot.radius = (dot.rounding ? 8.0F : detail ? 6.0F : 7.0F) + (pointed || active ? 2.0F : 0.0F);
    dot.fill = faded(active ? ink : m_colours.fill);
    dot.ring = faded(active ? m_colours.fill : ink);
    graphics.dots.push_back(dot);
}

bool HandleController::hiddenFromEye(const QVector3D& point, const QVector3D& eye) {
    const auto scene = m_model != nullptr ? m_model->scene() : nullptr;
    if (!scene) {
        return false;
    }
    const QVector3D way = point - eye;
    const float length = way.length();
    if (length < 1e-6F) {
        return false;
    }
    const QVector3D unit = way / length;
    // Up to a hair before the point: a dot on a face is not hidden by that face.
    const float reach = length - std::max(0.5F, 1e-3F * length);
    // The body of the handles is left out: its bands lie on the side facing the viewer, and its
    // dots on faces turned towards the eye.
    const std::string own = m_model->bodyOfStep(m_model->currentStep()).toStdString();
    for (const render::RenderBody& body : scene->bodies) {
        if (!body.mesh || body.mesh->indices.empty() || body.color.alpha() < 255 ||
            body.id == own) {
            continue; // holes are seen through
        }
        auto& box = m_boxes[body.id];
        if (box.version != body.meshVersion || box.empty) {
            box = {};
            box.version = body.meshVersion;
            for (const geo::Point3f& p : body.mesh->positions) {
                box.low = QVector3D(std::min(box.low.x(), p.x), std::min(box.low.y(), p.y),
                                    std::min(box.low.z(), p.z));
                box.high = QVector3D(std::max(box.high.x(), p.x), std::max(box.high.y(), p.y),
                                     std::max(box.high.z(), p.z));
            }
            box.empty = body.mesh->positions.empty();
        }
        // Does the line of sight pass the box round the body at all (slab test)?
        float enter = 0.0F;
        float leave = reach;
        for (int a = 0; a < 3 && enter <= leave; ++a) {
            if (std::abs(unit[a]) < 1e-9F) {
                if (eye[a] < box.low[a] || eye[a] > box.high[a]) {
                    leave = -1.0F;
                }
                continue;
            }
            const float t0 = (box.low[a] - eye[a]) / unit[a];
            const float t1 = (box.high[a] - eye[a]) / unit[a];
            enter = std::max(enter, std::min(t0, t1));
            leave = std::min(leave, std::max(t0, t1));
        }
        if (enter > leave) {
            continue;
        }
        // Then its triangles (Moeller-Trumbore).
        const auto& positions = body.mesh->positions;
        const auto& indices = body.mesh->indices;
        for (std::size_t t = 0; t + 2 < indices.size(); t += 3) {
            const geo::Point3f& p0 = positions[indices[t]];
            const geo::Point3f& p1 = positions[indices[t + 1]];
            const geo::Point3f& p2 = positions[indices[t + 2]];
            const QVector3D v0(p0.x, p0.y, p0.z);
            const QVector3D e1 = QVector3D(p1.x, p1.y, p1.z) - v0;
            const QVector3D e2 = QVector3D(p2.x, p2.y, p2.z) - v0;
            const QVector3D h = QVector3D::crossProduct(unit, e2);
            const float det = QVector3D::dotProduct(e1, h);
            if (std::abs(det) < 1e-12F) {
                continue;
            }
            const QVector3D s = eye - v0;
            const float u = QVector3D::dotProduct(s, h) / det;
            if (u < 0.0F || u > 1.0F) {
                continue;
            }
            const QVector3D q = QVector3D::crossProduct(s, e1);
            const float v = QVector3D::dotProduct(unit, q) / det;
            if (v < 0.0F || u + v > 1.0F) {
                continue;
            }
            const float hit = QVector3D::dotProduct(e2, q) / det;
            if (hit > 0.0F && hit < reach) {
                return true;
            }
        }
    }
    return false;
}

void HandleController::setPointed(int index, bool on) {
    const int pointed = on ? index : (m_pointed == index ? -1 : m_pointed);
    if (pointed != m_pointed) {
        m_pointed = pointed;
        refresh();
    }
}

void HandleController::setCurrent(int index) {
    if (index != m_current) {
        m_current = index;
        refresh();
    }
}

void HandleController::setColours(const QVariantMap& colours) {
    const auto colour = [&colours](const char* name, const QColor& fallback) {
        const QVariant value = colours.value(QLatin1String(name));
        return value.isValid() ? value.value<QColor>() : fallback;
    };
    m_colours.x = colour("x", m_colours.x);
    m_colours.y = colour("y", m_colours.y);
    m_colours.z = colour("z", m_colours.z);
    m_colours.plain = colour("plain", m_colours.plain);
    m_colours.ink = colour("ink", m_colours.ink);
    m_colours.fill = colour("fill", m_colours.fill);
    m_colourMap = colours;
    emit coloursChanged();
    refresh();
}

bool HandleController::beginDrag(int index, double x, double y) {
    if (m_model == nullptr || m_viewport == nullptr || m_editing || index < 0 ||
        index >= m_handles.size() || m_mode != QLatin1String("size")) {
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
    // The way the handle really goes when the value grows: along its direction, and up as well
    // where the shape rises with it (a sphere). The pointer follows that way.
    const QVector3D way =
        m_dragAxis * static_cast<float>(world["sign"].toDouble() *
                                        std::max(world["travel"].toDouble(), 0.05)) +
        riseOf(world);
    m_dragAxis = way.normalized();
    m_dragSign = 1.0;
    m_dragTravel = way.length();
    m_dragParameter = world["parameter"].toString();
    m_dragIndex = index;

    // The pointer is laid on the plane through the axis that faces the viewer most; an axis
    // that points at the viewer is dragged on the screen (AxisDrag).
    const auto screen = interaction::AxisDrag::screenOf(
        QPointF(x, y), m_dragOrigin, m_dragAxis, m_viewport->eyePosition(),
        [this](const QVector3D& p) { return m_viewport->project(p.x(), p.y(), p.z()); });
    m_axisDrag.reset();
    if (const auto ray = m_viewport->rayAt(x, y)) {
        m_axisDrag = interaction::AxisDrag::grab({ray->origin, ray->direction}, m_dragOrigin,
                                                 m_dragAxis, screen);
    }
    m_dragPixelsPerMillimetre = screen.pixelsPerMillimetre;

    m_dragging = true;
    m_model->beginChange(m_dragParameter);
    setEditing(true);
    return true;
}

double HandleController::dragTo(double x, double y) {
    if (!m_dragging || m_model == nullptr || !m_axisDrag) {
        return m_dragValue;
    }
    const auto ray = m_viewport->rayAt(x, y);
    if (!ray) {
        return m_dragValue;
    }
    // How far along the axis the pointer has gone since it grabbed the handle. The face under the
    // pointer follows it one to one; a shape that grows to both sides changes its value by more.
    const auto moved = m_axisDrag->moved({ray->origin, ray->direction}, QPointF(x, y));
    if (!moved) {
        return m_dragValue; // behind the eye, or beyond the vanishing point of the axis
    }
    const double step = m_viewport->snapStepFor(m_dragPixelsPerMillimetre);
    double wanted =
        interaction::snapped(m_dragStartValue + m_dragSign * *moved / m_dragTravel, step);
    wanted = std::min(wanted, kLargestMillimetres);
    // Where the shape stops being possible (a corner radius wider than half the box, no wall left,
    // a size of zero), the handle stops there, too: as close as the shape allows, on the grid.
    // (It once stayed where it was, so that pulling a box smaller than possible did nothing.)
    if (wanted != m_dragValue && !m_model->acceptsValue(m_dragParameter, wanted)) {
        double possible = m_dragValue;
        double impossible = wanted;
        for (int i = 0; i < 48; ++i) {
            const double middle = (possible + impossible) / 2.0;
            (m_model->acceptsValue(m_dragParameter, middle) ? possible : impossible) = middle;
        }
        const double onGrid = wanted < m_dragValue ? std::ceil(possible / step) * step
                                                   : std::floor(possible / step) * step;
        wanted = m_model->acceptsValue(m_dragParameter, onGrid) ? onGrid : m_dragValue;
    }
    if (wanted != m_dragValue && m_model->setStepValue(m_model->currentStep(), m_dragParameter,
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
    m_keepChoice = true;
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
