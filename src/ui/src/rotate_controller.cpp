// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/ui/rotate_controller.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "fission/render/scene.h"

namespace fission::ui {

namespace {

constexpr float kCalm = 0.35F; // strength of the other rings while one is dragged
constexpr double kPi = 3.14159265358979323846;
constexpr double kBubbleGap = 22.0;

const QVector3D kAxes[3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
const QString kAxisNames[3] = {QStringLiteral("x"), QStringLiteral("y"), QStringLiteral("z")};

QColor faded(QColor colour, float strength) {
    colour.setAlphaF(colour.alphaF() * strength);
    return colour;
}

double distanceToSegment(const QPointF& p, const QPointF& a, const QPointF& b) {
    const QPointF ab = b - a;
    const double lengthSquared = ab.x() * ab.x() + ab.y() * ab.y();
    double t = lengthSquared > 1e-12
                   ? ((p.x() - a.x()) * ab.x() + (p.y() - a.y()) * ab.y()) / lengthSquared
                   : 0.0;
    t = std::clamp(t, 0.0, 1.0);
    const QPointF nearest = a + ab * t;
    return std::hypot(p.x() - nearest.x(), p.y() - nearest.y());
}

int ringOf(const QString& part) {
    for (int axis = 0; axis < 3; ++axis) {
        if (part == kAxisNames[axis]) {
            return axis;
        }
    }
    return -1;
}

} // namespace

RingMask::RingMask(RotateController* owner, int ring)
    : QObject(owner), m_owner(owner), m_ring(ring) {}

bool RingMask::contains(const QPointF& point) const {
    return m_owner != nullptr && m_owner->ringContains(m_ring, point);
}

RotateController::RotateController(DocumentModel* model, QObject* parent)
    : QObject(parent), m_model(model) {
    for (int ring = 0; ring < 3; ++ring) {
        // Qt parent ownership: the controller deletes them.
        m_masks[static_cast<std::size_t>(ring)] = new RingMask(this, ring);
    }
    if (m_model == nullptr) {
        return;
    }
    connect(m_model, &DocumentModel::stepValuesChanged, this, &RotateController::refresh);
    connect(m_model, &DocumentModel::sceneReady, this, [this] { refresh(); });
    connect(m_model, &DocumentModel::currentStepChanged, this, &RotateController::refresh);
}

void RotateController::attach(Viewport3D* viewport) {
    m_viewport = viewport;
    if (m_viewport != nullptr) {
        connect(m_viewport, &Viewport3D::cameraChanged, this, &RotateController::refresh);
    }
    refresh();
}

void RotateController::setEnabled(bool enabled) {
    if (enabled == m_enabled) {
        return;
    }
    if (!enabled && m_dragging) {
        endDrag(); // a drag in progress keeps what it did
    }
    m_enabled = enabled;
    emit enabledChanged();
    refresh();
}

QVariantList RotateController::turn() const {
    return {m_delta[0], m_delta[1], m_delta[2]};
}

QObject* RotateController::mask(int ring) {
    return ring >= 0 && ring < 3 ? m_masks[static_cast<std::size_t>(ring)] : nullptr;
}

bool RotateController::ringContains(int ring, const QPointF& point) const {
    if (ring < 0 || ring > 2 || !m_enabled) {
        return false;
    }
    const auto& line = m_screen[static_cast<std::size_t>(ring)];
    for (std::size_t i = 0; i + 1 < line.size(); ++i) {
        if (distanceToSegment(point, line[i], line[i + 1]) <= kGrabPixels) {
            return true;
        }
    }
    return false;
}

void RotateController::setColours(const QVariantMap& colours) {
    const auto colour = [&colours](const char* name, const QColor& fallback) {
        const QVariant value = colours.value(QLatin1String(name));
        return value.isValid() ? value.value<QColor>() : fallback;
    };
    m_colours.x = colour("x", m_colours.x);
    m_colours.y = colour("y", m_colours.y);
    m_colours.z = colour("z", m_colours.z);
    m_colours.accent = colour("accent", m_colours.accent);
    m_colourMap = colours;
    emit coloursChanged();
    refresh();
}

void RotateController::setEditing(bool editing, const QString& turning) {
    const bool changed = editing != m_editing || turning != m_turning;
    m_editing = editing;
    m_turning = turning;
    if (changed) {
        emit editingChanged();
    }
}

std::optional<RotateController::Box> RotateController::bodyBox() {
    const auto scene = m_model->scene();
    const std::string body = m_model->bodyOfStep(m_model->currentStep()).toStdString();
    if (!scene || body.empty()) {
        return std::nullopt;
    }
    for (const render::RenderBody& candidate : scene->bodies) {
        if (candidate.id != body || !candidate.mesh || candidate.mesh->positions.empty()) {
            continue;
        }
        if (candidate.id != m_boxBody || candidate.meshVersion != m_boxVersion) {
            Box box{{1e30F, 1e30F, 1e30F}, {-1e30F, -1e30F, -1e30F}};
            for (const geo::Point3f& p : candidate.mesh->positions) {
                box.low = QVector3D(std::min(box.low.x(), p.x), std::min(box.low.y(), p.y),
                                    std::min(box.low.z(), p.z));
                box.high = QVector3D(std::max(box.high.x(), p.x), std::max(box.high.y(), p.y),
                                     std::max(box.high.z(), p.z));
            }
            m_boxBody = candidate.id;
            m_boxVersion = candidate.meshVersion;
            m_box = box;
        }
        return m_box;
    }
    return std::nullopt;
}

double RotateController::millimetresPerPixel(const QVector3D& point) const {
    QVector3D sight = point - m_viewport->eyePosition();
    sight = sight.lengthSquared() > 1e-12F ? sight.normalized() : QVector3D(0, 0, -1);
    QVector3D across = QVector3D::crossProduct(sight, QVector3D(0, 0, 1));
    if (across.lengthSquared() < 1e-6F) {
        across = QVector3D::crossProduct(sight, QVector3D(1, 0, 0));
    }
    across.normalize();
    const QPointF here = m_viewport->project(point.x(), point.y(), point.z());
    const QVector3D next = point + across;
    const QPointF there = m_viewport->project(next.x(), next.y(), next.z());
    const double pixels = std::hypot(there.x() - here.x(), there.y() - here.y());
    return pixels > 1e-9 ? 1.0 / pixels : 1.0;
}

double RotateController::ringRadius(const QVector3D& centre) {
    const double perPixel = millimetresPerPixel(centre);
    double radius = kLeastRingPixels * perPixel;
    if (const auto box = bodyBox()) {
        // Round the whole body, a little off it.
        radius = std::max(radius, 0.5 * (box->high - box->low).length() + kRingMargin * perPixel);
    }
    return radius;
}

QPointF RotateController::ringPoint(const QString& part, double degrees) {
    const int ring = ringOf(part);
    const auto turnCentre = m_model != nullptr ? m_model->currentTurnCentre() : std::nullopt;
    if (ring < 0 || !turnCentre || m_viewport == nullptr) {
        return {};
    }
    const QVector3D centre =
        m_editing ? m_centre
                  : QVector3D(static_cast<float>(turnCentre->x), static_cast<float>(turnCentre->y),
                              static_cast<float>(turnCentre->z));
    QVector3D u;
    QVector3D v;
    interaction::RingDrag::frameOf(kAxes[ring], u, v);
    const double radius = m_editing ? m_radius : ringRadius(centre);
    const double angle = degrees / 180.0 * kPi;
    const QVector3D point =
        centre + static_cast<float>(radius) * (static_cast<float>(std::cos(angle)) * u +
                                               static_cast<float>(std::sin(angle)) * v);
    return m_viewport->project(point.x(), point.y(), point.z());
}

namespace {

// The place on each ring that is easiest to take: on the half of the ring nearer to the viewer,
// and as far as can be from the other two rings on the screen (where rings cross, the wrong one
// would be grabbed). `screen` and `distance` hold the points of the three rings: the screen place
// of each and its distance from the eye (squared). Gives the index of the point for each ring.
std::array<int, 3> easiestToTake(const std::array<std::vector<QPointF>, 3>& screen,
                                 const std::array<std::vector<double>, 3>& distance) {
    std::array<int, 3> best{0, 0, 0};
    for (std::size_t ring = 0; ring < 3; ++ring) {
        std::vector<double> sorted = distance[ring];
        std::sort(sorted.begin(), sorted.end());
        const double median = sorted[sorted.size() / 2];
        double bestRoom = -1.0;
        for (std::size_t i = 0; i + 1 < screen[ring].size(); ++i) {
            if (distance[ring][i] > median) {
                continue;
            }
            double room = 60.0; // far enough is far enough
            for (std::size_t other = 0; other < 3; ++other) {
                if (other == ring) {
                    continue;
                }
                const auto& line = screen[other];
                for (std::size_t k = 0; k + 1 < line.size(); ++k) {
                    room = std::min(room, distanceToSegment(screen[ring][i], line[k], line[k + 1]));
                }
            }
            // Of equally roomy places the nearest to the viewer.
            if (room > bestRoom + 1e-9 ||
                (std::abs(room - bestRoom) <= 1e-9 &&
                 distance[ring][i] < distance[ring][static_cast<std::size_t>(best[ring])])) {
                bestRoom = room;
                best[ring] = static_cast<int>(i);
            }
        }
    }
    return best;
}

} // namespace

double RotateController::grabAngle(const QString& part) {
    const int ring = ringOf(part);
    const auto turnCentre = m_model != nullptr ? m_model->currentTurnCentre() : std::nullopt;
    if (ring < 0 || !turnCentre || m_viewport == nullptr) {
        return 0.0;
    }
    const QVector3D centre(static_cast<float>(turnCentre->x), static_cast<float>(turnCentre->y),
                           static_cast<float>(turnCentre->z));
    const double radius = ringRadius(centre);
    const QVector3D eye = m_viewport->eyePosition();
    std::array<std::vector<QPointF>, 3> screen;
    std::array<std::vector<double>, 3> distance;
    for (int axis = 0; axis < 3; ++axis) {
        QVector3D u;
        QVector3D v;
        interaction::RingDrag::frameOf(kAxes[axis], u, v);
        for (int i = 0; i <= kRingSegments; ++i) {
            const double angle = 2.0 * kPi * i / kRingSegments;
            const QVector3D point =
                centre + static_cast<float>(radius) * (static_cast<float>(std::cos(angle)) * u +
                                                       static_cast<float>(std::sin(angle)) * v);
            screen[static_cast<std::size_t>(axis)].push_back(
                m_viewport->project(point.x(), point.y(), point.z()));
            distance[static_cast<std::size_t>(axis)].push_back((point - eye).lengthSquared());
        }
    }
    return 360.0 * easiestToTake(screen, distance)[static_cast<std::size_t>(ring)] / kRingSegments;
}

void RotateController::refresh() {
    QVariantList rings;
    QVariantMap bubble;
    render::HandleGraphics graphics;
    std::optional<QVector3D> centre;
    if (m_model != nullptr && m_viewport != nullptr) {
        if (m_editing) {
            centre = m_centre;
        } else if (const auto turnCentre = m_model->currentTurnCentre()) {
            centre = QVector3D(static_cast<float>(turnCentre->x), static_cast<float>(turnCentre->y),
                               static_cast<float>(turnCentre->z));
        }
    }
    for (auto& line : m_screen) {
        line.clear();
    }
    if (centre && m_enabled) {
        const double radius = m_editing ? m_radius : ringRadius(*centre);
        const QColor tones[3] = {m_colours.x, m_colours.y, m_colours.z};
        const QVector3D eye = m_viewport->eyePosition();
        const auto strength = [this](int ring) {
            return m_editing && ring != m_ring && m_delta[static_cast<std::size_t>(ring)] == 0.0
                       ? kCalm
                       : 1.0F;
        };
        // The points of the rings first (on the screen, and how far from the eye), so that the
        // place to take each one can keep away from the others.
        std::array<std::vector<QVector3D>, 3> world;
        std::array<std::vector<double>, 3> distance;
        for (int axis = 0; axis < 3; ++axis) {
            QVector3D u;
            QVector3D v;
            interaction::RingDrag::frameOf(kAxes[axis], u, v);
            const auto ring = static_cast<std::size_t>(axis);
            for (int i = 0; i <= kRingSegments; ++i) {
                const double angle = 2.0 * kPi * i / kRingSegments;
                const QVector3D point = *centre + static_cast<float>(radius) *
                                                      (static_cast<float>(std::cos(angle)) * u +
                                                       static_cast<float>(std::sin(angle)) * v);
                world[ring].push_back(point);
                m_screen[ring].push_back(m_viewport->project(point.x(), point.y(), point.z()));
                distance[ring].push_back((point - eye).lengthSquared());
            }
        }
        const std::array<int, 3> grabAt = easiestToTake(m_screen, distance);
        for (int axis = 0; axis < 3; ++axis) {
            QVector3D u;
            QVector3D v;
            interaction::RingDrag::frameOf(kAxes[axis], u, v);
            const auto ring = static_cast<std::size_t>(axis);
            const bool pointed = m_pointed == kAxisNames[axis] || (m_editing && m_ring == axis);
            const QColor tone = faded(tones[axis], strength(axis));
            for (std::size_t i = 1; i < world[ring].size(); ++i) {
                graphics.lines.push_back(
                    {world[ring][i - 1], world[ring][i], pointed ? 4.5F : 3.0F, tone});
            }
            const QPointF grab = m_screen[ring][static_cast<std::size_t>(grabAt[ring])];
            QVariantMap entry;
            entry[QStringLiteral("part")] = kAxisNames[axis];
            entry[QStringLiteral("shown")] = true;
            entry[QStringLiteral("axis")] = kAxisNames[axis];
            entry[QStringLiteral("grabX")] = grab.x();
            entry[QStringLiteral("grabY")] = grab.y();
            rings.push_back(entry);

            // How far it has turned: a wedge from where the ring was taken (or from the start
            // of the ring where a number was typed).
            const double degrees = m_delta[static_cast<std::size_t>(axis)];
            if (m_editing && degrees != 0.0) {
                const double from = axis == m_ring ? m_grabbedAt : 0.0;
                const int pieces =
                    std::max(2, static_cast<int>(std::ceil(std::abs(degrees) / 6.0)));
                for (int i = 0; i < pieces; ++i) {
                    const double a = from + degrees / 180.0 * kPi * i / pieces;
                    const double b = from + degrees / 180.0 * kPi * (i + 1) / pieces;
                    const auto onRing = [&](double angle) {
                        return *centre + static_cast<float>(radius) *
                                             (static_cast<float>(std::cos(angle)) * u +
                                              static_cast<float>(std::sin(angle)) * v);
                    };
                    render::HandleQuad wedge;
                    wedge.corners[0] = *centre;
                    wedge.corners[1] = onRing(a);
                    wedge.corners[2] = onRing(b);
                    wedge.corners[3] = *centre;
                    wedge.fill = faded(tones[axis], 0.28F);
                    wedge.line = QColor(0, 0, 0, 0);
                    wedge.lineWidth = 0.0F;
                    graphics.quads.push_back(wedge);
                }
                // The two edges of the wedge.
                graphics.lines.push_back({*centre,
                                          *centre + static_cast<float>(radius) *
                                                        (static_cast<float>(std::cos(from)) * u +
                                                         static_cast<float>(std::sin(from)) * v),
                                          1.2F, faded(tones[axis], 0.6F)});
                const double to = from + degrees / 180.0 * kPi;
                graphics.lines.push_back(
                    {*centre,
                     *centre + static_cast<float>(radius) * (static_cast<float>(std::cos(to)) * u +
                                                             static_cast<float>(std::sin(to)) * v),
                     2.0F, tones[axis]});
            }
        }
        const QPointF middle = m_viewport->project(centre->x(), centre->y(), centre->z());
        const double pixels = radius / millimetresPerPixel(*centre);
        bubble[QStringLiteral("x")] = middle.x() + pixels + kBubbleGap;
        bubble[QStringLiteral("y")] = middle.y() - 15.0;
    }
    if (m_viewport != nullptr) {
        m_viewport->setHandleGraphics(std::move(graphics), Viewport3D::HandleSource::rings);
    }
    if (rings != m_rings || bubble != m_bubbleAt) {
        m_rings = rings;
        m_bubbleAt = bubble;
        emit ringsChanged();
    }
}

void RotateController::setPointed(const QString& part, bool on) {
    const QString pointed = on ? part : (m_pointed == part ? QString() : m_pointed);
    if (pointed != m_pointed) {
        m_pointed = pointed;
        refresh();
    }
}

bool RotateController::beginDrag(const QString& part, double x, double y) {
    const int ring = ringOf(part);
    if (m_model == nullptr || m_viewport == nullptr || m_editing || !m_enabled || ring < 0) {
        return false;
    }
    const auto turnCentre = m_model->currentTurnCentre();
    const auto angles = m_model->currentTurn();
    const auto ray = m_viewport->rayAt(x, y);
    if (!turnCentre || !angles || !ray) {
        return false;
    }
    const QVector3D centre(static_cast<float>(turnCentre->x), static_cast<float>(turnCentre->y),
                           static_cast<float>(turnCentre->z));
    const double radius = ringRadius(centre);
    m_drag = interaction::RingDrag::grab(
        {ray->origin, ray->direction}, QPointF(x, y), centre, kAxes[ring],
        [this](const QVector3D& p) { return m_viewport->project(p.x(), p.y(), p.z()); }, radius);
    if (!m_drag || !m_model->beginTurn()) {
        m_drag.reset();
        return false;
    }
    m_centre = centre;
    m_radius = radius;
    m_startAngles = *angles;
    m_lastAngles = *angles;
    m_delta = {0.0, 0.0, 0.0};
    m_ring = ring;
    m_grabbedAt = m_drag->grabbedAt();
    m_dragging = true;
    setEditing(true, kAxisNames[ring]);
    emit turnChanged();
    refresh();
    return true;
}

bool RotateController::applyTurn() {
    const geo::Matrix3 turn = geo::axisRotation(2, m_delta[2]) * geo::axisRotation(1, m_delta[1]) *
                              geo::axisRotation(0, m_delta[0]) * geo::rotationOf(m_startAngles);
    const geo::Angles angles = geo::anglesOf(turn, m_lastAngles);
    if (!m_model->turnCurrent(angles)) {
        return false;
    }
    m_lastAngles = angles;
    emit turnChanged();
    refresh();
    return true;
}

bool RotateController::dragTo(double x, double y) {
    if (!m_dragging || !m_drag || m_viewport == nullptr) {
        return false;
    }
    const auto ray = m_viewport->rayAt(x, y);
    if (!ray) {
        return false;
    }
    const auto degrees = m_drag->moved({ray->origin, ray->direction}, QPointF(x, y));
    if (!degrees) {
        return false; // behind the eye
    }
    const double snapped = interaction::snapped(*degrees, interaction::kTurnStepDegrees);
    auto& delta = m_delta[static_cast<std::size_t>(m_ring)];
    if (snapped == delta) {
        return true;
    }
    const double before = delta;
    delta = snapped;
    if (!applyTurn()) {
        delta = before;
        return false;
    }
    return true;
}

void RotateController::finish() {
    m_dragging = false;
    m_drag.reset();
    m_ring = -1;
    setEditing(false, {});
    m_delta = {0.0, 0.0, 0.0};
    emit turnChanged();
    refresh();
}

bool RotateController::endDrag() {
    if (!m_dragging) {
        return false;
    }
    m_model->commitChange();
    finish();
    return true;
}

void RotateController::cancelDrag() {
    if (!m_dragging) {
        return;
    }
    m_model->cancelChange();
    finish();
}

bool RotateController::typeInsteadOfDrag() {
    if (!m_dragging) {
        return false;
    }
    m_dragging = false; // still editing: the same step goes on
    m_drag.reset();
    return true;
}

bool RotateController::setTurn(int axis, double degrees) {
    if (!m_editing || m_dragging || axis < 0 || axis > 2 || !std::isfinite(degrees)) {
        return false;
    }
    // A typed axis takes part in the turn from now on (Tab goes from X to Y to Z).
    if (!m_turning.contains(kAxisNames[axis])) {
        m_turning += kAxisNames[axis];
        emit editingChanged();
    }
    const double before = m_delta[static_cast<std::size_t>(axis)];
    m_delta[static_cast<std::size_t>(axis)] = degrees;
    if (!applyTurn()) {
        m_delta[static_cast<std::size_t>(axis)] = before;
        return false;
    }
    return true;
}

bool RotateController::commitEdit() {
    if (!m_editing || m_dragging) {
        return false;
    }
    m_model->commitChange();
    finish();
    return true;
}

void RotateController::cancelEdit() {
    if (!m_editing || m_dragging) {
        return;
    }
    m_model->cancelChange();
    finish();
}

} // namespace fission::ui
