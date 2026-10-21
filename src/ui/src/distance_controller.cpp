// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/ui/distance_controller.h"

#include <QPointF>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <set>

#include "fission/render/scene.h"
#include "fission/render/scene_renderer.h"
#include "fission/ui/snap_session.h"

namespace fission::ui {

namespace {

constexpr float kCalm = 0.55F;     // strength of the bands that are not pointed at
constexpr float kBandWidth = 1.5F; // pixels
constexpr float kPointedWidth = 2.5F;
constexpr float kExtensionWidth = 1.0F;

QString nameOf(interaction::Side side) {
    switch (side) {
    case interaction::Side::left:
        return QStringLiteral("left");
    case interaction::Side::right:
        return QStringLiteral("right");
    case interaction::Side::front:
        return QStringLiteral("front");
    case interaction::Side::back:
        return QStringLiteral("back");
    }
    return {};
}

QVector3D toVector(const interaction::Vec3& p) {
    return {static_cast<float>(p[0]), static_cast<float>(p[1]), static_cast<float>(p[2])};
}

QColor faded(QColor colour, float strength) {
    colour.setAlphaF(colour.alphaF() * strength);
    return colour;
}

} // namespace

DistanceController::DistanceController(DocumentModel* model, QObject* parent)
    : QObject(parent), m_model(model) {
    if (m_model == nullptr) {
        return;
    }
    connect(m_model, &DocumentModel::sceneReady, this, [this] { refresh(); });
    connect(m_model, &DocumentModel::currentStepChanged, this, &DistanceController::refresh);
}

void DistanceController::attach(Viewport3D* viewport) {
    m_viewport = viewport;
    if (m_viewport != nullptr) {
        connect(m_viewport, &Viewport3D::cameraChanged, this, &DistanceController::refresh);
    }
    refresh();
}

void DistanceController::attachGizmo(GizmoController* gizmo) {
    m_gizmo = gizmo;
    if (m_gizmo != nullptr) {
        connect(m_gizmo, &GizmoController::offsetChanged, this, &DistanceController::refresh);
        connect(m_gizmo, &GizmoController::editingChanged, this, [this] {
            if (m_gizmo->editing() && !m_following) {
                moverBox(); // the box as it stands when the move begins
                m_following = true;
            } else if (!m_gizmo->editing()) {
                m_following = false;
            }
            refresh();
        });
    }
}

void DistanceController::setEnabled(bool enabled) {
    if (enabled == m_enabled) {
        return;
    }
    m_enabled = enabled;
    emit enabledChanged();
    refresh();
}

void DistanceController::setColours(const QVariantMap& colours) {
    const QVariant value = colours.value(QStringLiteral("line"));
    if (value.isValid()) {
        m_colours.line = value.value<QColor>();
    }
    m_colourMap = colours;
    emit coloursChanged();
    refresh();
}

void DistanceController::setPointed(int index, bool on) {
    const int pointed = on ? index : (m_pointed == index ? -1 : m_pointed);
    if (pointed != m_pointed) {
        m_pointed = pointed;
        refresh();
    }
}

std::optional<interaction::Box3> DistanceController::moverBox() {
    if (m_model == nullptr || !m_model->currentPosition()) {
        return std::nullopt; // the chosen step cannot be moved
    }
    // While the gizmo moves the body, the box of the start is kept: the results that arrive are
    // behind the pointer.
    if (!m_following) {
        const auto result = m_model->result();
        if (result != m_boundsResult || m_model->currentStep() != m_boundsStep) {
            m_bounds = m_model->currentBodyBounds();
            m_boundsResult = result;
            m_boundsStep = m_model->currentStep();
        }
    }
    if (!m_bounds) {
        return std::nullopt;
    }
    interaction::Box3 box{{m_bounds->min.x, m_bounds->min.y, m_bounds->min.z},
                          {m_bounds->max.x, m_bounds->max.y, m_bounds->max.z}};
    if (m_following && m_gizmo != nullptr) {
        const QVariantList moved = m_gizmo->offset();
        for (int axis = 0; axis < 3 && axis < moved.size(); ++axis) {
            const double by = moved[axis].toDouble();
            box.low[static_cast<std::size_t>(axis)] += by;
            box.high[static_cast<std::size_t>(axis)] += by;
        }
    }
    for (std::size_t axis = 0; axis < 3; ++axis) {
        box.low[axis] = interaction::cleanedCoordinate(box.low[axis]);
        box.high[axis] = interaction::cleanedCoordinate(box.high[axis]);
    }
    return box;
}

std::vector<std::pair<std::string, interaction::Box3>> DistanceController::otherBoxes() {
    std::vector<std::pair<std::string, interaction::Box3>> boxes;
    const auto scene = m_model->scene();
    if (!scene) {
        return boxes;
    }
    const std::string own = m_model->bodyOfStep(m_model->currentStep()).toStdString();
    std::set<std::string> present;
    for (const render::RenderBody& body : scene->bodies) {
        present.insert(body.id);
        // A hole is seen through; it is no neighbour. The body of the step itself is the mover.
        if (body.id == own || !body.mesh || body.mesh->positions.empty() ||
            body.color.alpha() < 255) {
            continue;
        }
        Cached& kept = m_boxes[body.id];
        if (!kept.valid || kept.version != body.meshVersion) {
            interaction::Box3 box{{1e30, 1e30, 1e30}, {-1e30, -1e30, -1e30}};
            for (const geo::Point3f& p : body.mesh->positions) {
                const double v[3] = {p.x, p.y, p.z};
                for (std::size_t a = 0; a < 3; ++a) {
                    box.low[a] = std::min(box.low[a], v[a]);
                    box.high[a] = std::max(box.high[a], v[a]);
                }
            }
            for (std::size_t a = 0; a < 3; ++a) {
                box.low[a] = interaction::cleanedCoordinate(box.low[a]);
                box.high[a] = interaction::cleanedCoordinate(box.high[a]);
            }
            kept.box = box;
            kept.version = body.meshVersion;
            kept.valid = true;
        }
        boxes.emplace_back(body.id, kept.box);
    }
    // Bodies that are gone do not stay in the cache.
    for (auto it = m_boxes.begin(); it != m_boxes.end();) {
        it = present.count(it->first) != 0 ? std::next(it) : m_boxes.erase(it);
    }
    return boxes;
}

std::vector<interaction::Gap> DistanceController::measure() {
    if (m_model == nullptr || !m_enabled) {
        return {};
    }
    const auto box = moverBox();
    const auto scene = m_model->scene();
    if (!box || !scene) {
        return {};
    }
    return interaction::nearestGaps(*box, otherBoxes(), scene->bed.sizeX, scene->bed.sizeY);
}

void DistanceController::refresh() {
    QVariantList labels;
    render::HandleGraphics graphics;
    m_gaps.clear();
    if (m_model != nullptr && m_viewport != nullptr) {
        m_gaps = measure();
    }
    for (std::size_t i = 0; i < m_gaps.size() && m_viewport != nullptr; ++i) {
        const interaction::Gap& gap = m_gaps[i];
        const QVector3D from = toVector(gap.from);
        const QVector3D to = toVector(gap.to);
        const QVector3D middle = (from + to) / 2.0F;
        const QPointF screen = m_viewport->project(middle.x(), middle.y(), middle.z());
        QVariantMap entry;
        entry[QStringLiteral("index")] = static_cast<int>(i);
        entry[QStringLiteral("side")] = nameOf(gap.side);
        entry[QStringLiteral("x")] = screen.x();
        entry[QStringLiteral("y")] = screen.y();
        entry[QStringLiteral("value")] = gap.distance;
        entry[QStringLiteral("toBed")] = gap.toBed;
        labels.push_back(entry);

        // The band like a measure in a drawing: the line between the two edges, a stroke across
        // each end and an arrow head from each end inwards.
        const bool pointed = m_pointed == static_cast<int>(i);
        const QColor colour = faded(m_colours.line, pointed || m_pointed < 0 ? 1.0F : kCalm);
        const double perPixel = millimetresPerPixelAt(*m_viewport, middle);
        const bool alongX =
            gap.side == interaction::Side::left || gap.side == interaction::Side::right;
        const QVector3D across = alongX ? QVector3D(0, 1, 0) : QVector3D(1, 0, 0);
        const QVector3D tick = across * static_cast<float>(kTickPixels * perPixel);
        graphics.lines.push_back({from, to, pointed ? kPointedWidth : kBandWidth, colour});
        graphics.lines.push_back({from - tick, from + tick, kExtensionWidth, colour});
        graphics.lines.push_back({to - tick, to + tick, kExtensionWidth, colour});
        const QPointF a = m_viewport->project(from.x(), from.y(), from.z());
        const QPointF b = m_viewport->project(to.x(), to.y(), to.z());
        if (std::hypot(b.x() - a.x(), b.y() - a.y()) > 2.5 * kHeadLength) {
            const QVector3D way = (to - from).normalized();
            graphics.heads.push_back({from, -way, kHeadLength, kHeadHalfWidth, colour});
            graphics.heads.push_back({to, way, kHeadLength, kHeadHalfWidth, colour});
        }
    }
    if (m_viewport != nullptr) {
        m_viewport->setHandleGraphics(std::move(graphics), Viewport3D::HandleSource::distances);
    }
    if (labels != m_labels) {
        m_labels = labels;
        emit labelsChanged();
    }
}

int DistanceController::indexOfSide(const QString& side) const {
    for (const QVariant& entry : m_labels) {
        if (entry.toMap().value(QStringLiteral("side")).toString() == side) {
            return entry.toMap().value(QStringLiteral("index")).toInt();
        }
    }
    return -1;
}

bool DistanceController::setDistance(int index, double millimetres) {
    if (m_model == nullptr || !m_enabled || !std::isfinite(millimetres) ||
        (m_gizmo != nullptr && m_gizmo->editing())) {
        return false;
    }
    // The measures as they are now, not as they were when the number was shown.
    const std::vector<interaction::Gap> gaps = measure();
    if (index < 0 || index >= static_cast<int>(gaps.size())) {
        return false;
    }
    const auto move =
        interaction::offsetForDistance(gaps[static_cast<std::size_t>(index)], millimetres);
    const auto position = m_model->currentPosition();
    if (!move || !position) {
        return false;
    }
    // Only the value that changes is written (a formula in the other one stays).
    std::array<std::optional<double>, 3> target;
    for (std::size_t axis = 0; axis < 3; ++axis) {
        if ((*move)[axis] != 0.0) {
            target[axis] = (*position)[axis] + (*move)[axis];
        }
    }
    if (!target[0] && !target[1] && !target[2]) {
        return true; // it is where it was asked to be
    }
    if (!m_model->beginMove()) {
        return false;
    }
    const bool done = m_model->moveCurrent(target);
    if (done) {
        m_model->commitChange();
    } else {
        m_model->cancelChange();
    }
    return done;
}

} // namespace fission::ui
