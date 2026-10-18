// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/ui/gizmo_controller.h"

#include <QPointF>

#include <algorithm>
#include <cmath>

#include "fission/render/scene.h"

namespace fission::ui {

namespace {

// Room on screen (pixels): the gap between the middle dot and the arrows, the head of an arrow,
// the squares (from and to, along both of their axes), the letter beyond the tip, how far the
// gizmo floats above the body, the half length of the guide line along a dragged axis.
constexpr double kDotGap = 12.0;
constexpr float kHeadLength = 14.0F;
constexpr float kHeadHalfWidth = 6.0F;
constexpr double kSquareFrom = 22.0;
constexpr double kSquareTo = 38.0;
constexpr double kLetterGap = 14.0;
constexpr double kLift = 4.0;
constexpr double kGuidePixels = 900.0;
constexpr float kCalm = 0.35F; // strength of the other parts while one is dragged

const QVector3D kAxes[3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
const QString kAxisNames[3] = {QStringLiteral("x"), QStringLiteral("y"), QStringLiteral("z")};
// Arrow lengths as in the design (mockup: X 1, Y 0.85, Z 1.25 of the base length).
constexpr double kArrowShare[3] = {1.0, 0.85, 1.25};

QColor faded(QColor colour, float strength) {
    colour.setAlphaF(colour.alphaF() * strength);
    return colour;
}

} // namespace

GizmoController::GizmoController(DocumentModel* model, QObject* parent)
    : QObject(parent), m_model(model) {
    if (m_model == nullptr) {
        return;
    }
    connect(m_model, &DocumentModel::stepValuesChanged, this, &GizmoController::refresh);
    connect(m_model, &DocumentModel::sceneReady, this, [this] { refresh(); });
    connect(m_model, &DocumentModel::currentStepChanged, this, &GizmoController::refresh);
}

void GizmoController::attach(Viewport3D* viewport) {
    m_viewport = viewport;
    if (m_viewport != nullptr) {
        connect(m_viewport, &Viewport3D::cameraChanged, this, &GizmoController::refresh);
    }
    refresh();
}

void GizmoController::setEnabled(bool enabled) {
    if (enabled == m_enabled) {
        return;
    }
    if (!enabled && m_dragging && m_moving != QLatin1String("body")) {
        endDrag(); // a drag in progress keeps what it did
    }
    m_enabled = enabled;
    emit enabledChanged();
    refresh();
}

QVariantList GizmoController::offset() const {
    return {m_offset[0], m_offset[1], m_offset[2]};
}

void GizmoController::setColours(const QVariantMap& colours) {
    const auto colour = [&colours](const char* name, const QColor& fallback) {
        const QVariant value = colours.value(QLatin1String(name));
        return value.isValid() ? value.value<QColor>() : fallback;
    };
    m_colours.x = colour("x", m_colours.x);
    m_colours.y = colour("y", m_colours.y);
    m_colours.z = colour("z", m_colours.z);
    m_colours.ink = colour("ink", m_colours.ink);
    m_colours.fill = colour("fill", m_colours.fill);
    m_colourMap = colours;
    emit coloursChanged();
    refresh();
}

void GizmoController::setEditing(bool editing, const QString& moving) {
    const bool changed = editing != m_editing || moving != m_moving;
    m_editing = editing;
    m_moving = moving;
    if (changed) {
        emit editingChanged();
    }
}

std::optional<GizmoController::Box> GizmoController::bodyBox() {
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

double GizmoController::millimetresPerPixel(const QVector3D& point) const {
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

std::optional<QVector3D> GizmoController::anchor() {
    if (m_editing) {
        // The body is calculated a moment later: the gizmo goes where the pointer took it.
        return m_startAnchor + QVector3D(static_cast<float>(m_offset[0]),
                                         static_cast<float>(m_offset[1]),
                                         static_cast<float>(m_offset[2]));
    }
    if (!m_model->currentPosition()) {
        return std::nullopt; // the chosen step cannot be moved
    }
    const auto box = bodyBox();
    if (!box) {
        return std::nullopt;
    }
    QVector3D top((box->low.x() + box->high.x()) / 2.0F, (box->low.y() + box->high.y()) / 2.0F,
                  box->high.z());
    top.setZ(top.z() + static_cast<float>(kLift * millimetresPerPixel(top)));
    return top;
}

void GizmoController::refresh() {
    QVariantList parts;
    QVariantMap bubble;
    render::HandleGraphics graphics;
    const auto at = (m_model != nullptr && m_viewport != nullptr) ? anchor() : std::nullopt;
    if (at) {
        const QPointF centre = m_viewport->project(at->x(), at->y(), at->z());
        // Right of the gizmo, clear of its arrows and squares.
        bubble[QStringLiteral("x")] = centre.x() + 1.3 * kArrowPixels;
        bubble[QStringLiteral("y")] = centre.y() - 15.0;
    }
    if (at && m_enabled) {
        const double perPixel = millimetresPerPixel(*at);
        const QVector3D sight = (*at - m_viewport->eyePosition()).normalized();
        const auto onScreen = [this](const QVector3D& p) {
            return m_viewport->project(p.x(), p.y(), p.z());
        };
        // Short arrows on a body that is small on screen: long ones would reach far beyond it.
        double arrowPixels = kArrowPixels;
        if (const auto box = bodyBox()) {
            const QPointF a = onScreen(box->low);
            const QPointF b = onScreen(box->high);
            if (std::hypot(b.x() - a.x(), b.y() - a.y()) < kSmallBodyPixels) {
                arrowPixels = kSmallArrowPixels;
            }
        }
        const auto world = [perPixel](double pixels) {
            return static_cast<float>(pixels * perPixel);
        };
        const QColor tones[3] = {m_colours.x, m_colours.y, m_colours.z};
        // While a part is dragged, the others step back.
        const auto strength = [this](const QString& part) {
            return m_editing && part != m_moving ? kCalm : 1.0F;
        };

        // The squares first: the arrows are drawn over them.
        const struct {
            const char* part;
            int u;
            int v;
            int normal;
        } squares[3] = {{"xy", 0, 1, 2}, {"xz", 0, 2, 1}, {"yz", 1, 2, 0}};
        // Every part has its place in the list, shown or not: the window keeps one place to grab
        // for each, and a part that is held must never go away under the pointer (perspective
        // turns an arrow towards the viewer while the body moves).
        const auto held = [this](const QString& part) { return m_editing && m_moving == part; };
        for (const auto& square : squares) {
            const QString part = QLatin1String(square.part);
            // Seen edge on, a square is left out (it could not be dragged).
            const bool shown =
                held(part) ||
                std::abs(QVector3D::dotProduct(kAxes[square.normal], sight)) >= kLeastSquareSight;
            const bool pointed = m_pointed == part || (m_editing && m_moving == part);
            const QVector3D u = kAxes[square.u];
            const QVector3D v = kAxes[square.v];
            render::HandleQuad quad;
            quad.corners[0] = *at + u * world(kSquareFrom) + v * world(kSquareFrom);
            quad.corners[1] = *at + u * world(kSquareTo) + v * world(kSquareFrom);
            quad.corners[2] = *at + u * world(kSquareTo) + v * world(kSquareTo);
            quad.corners[3] = *at + u * world(kSquareFrom) + v * world(kSquareTo);
            const QColor tone = tones[square.normal];
            quad.fill = faded(tone, (pointed ? 0.6F : 0.3F) * strength(part));
            quad.line = faded(tone, strength(part));
            quad.lineWidth = pointed ? 2.0F : 1.5F;
            if (shown) {
                graphics.quads.push_back(quad);
            }
            const QPointF middle = onScreen((quad.corners[0] + quad.corners[2]) / 2.0F);
            QVariantMap entry;
            entry[QStringLiteral("part")] = part;
            entry[QStringLiteral("shown")] = shown;
            entry[QStringLiteral("axis")] = kAxisNames[square.normal];
            entry[QStringLiteral("x")] = middle.x();
            entry[QStringLiteral("y")] = middle.y();
            parts.push_back(entry);
        }

        for (int axis = 0; axis < 3; ++axis) {
            const QVector3D way = kAxes[axis];
            const QString& part = kAxisNames[axis];
            // An arrow that points (nearly) at the viewer is a dot on the screen: left out.
            const bool shown =
                held(part) || QVector3D::crossProduct(way, sight).length() >= kLeastArrowSight;
            const bool pointed = m_pointed == part || (m_editing && m_moving == part);
            const QColor tone = faded(tones[axis], strength(part));
            const QVector3D start = *at + way * world(kDotGap);
            const QVector3D tip = *at + way * world(arrowPixels * kArrowShare[axis]);
            const QVector3D shaftEnd = tip - way * world(kHeadLength - 3.0);
            const float grow = pointed ? 1.25F : 1.0F;
            if (shown) {
                graphics.lines.push_back({start, shaftEnd, pointed ? 4.5F : 3.2F, tone});
                graphics.heads.push_back(
                    {tip, way, kHeadLength * grow, kHeadHalfWidth * grow, tone});
            }
            if (m_editing && m_moving == part) {
                // A thin line along the axis: the way the body can go.
                graphics.lines.push_back({*at - way * world(kGuidePixels),
                                          *at + way * world(kGuidePixels), 1.0F,
                                          faded(tones[axis], 0.7F)});
            }
            const QPointF head = onScreen(tip - way * world(kHeadLength / 2.0));
            const QPointF middle = onScreen((start + shaftEnd) / 2.0F);
            const QPointF tipOnScreen = onScreen(tip);
            QPointF out = tipOnScreen - onScreen(*at);
            const double outLength = std::hypot(out.x(), out.y());
            out = outLength > 1e-6 ? out / outLength : QPointF(0.0, -1.0);
            QVariantMap entry;
            entry[QStringLiteral("part")] = part;
            entry[QStringLiteral("shown")] = shown;
            entry[QStringLiteral("axis")] = part;
            entry[QStringLiteral("x")] = head.x();
            entry[QStringLiteral("y")] = head.y();
            entry[QStringLiteral("midX")] = middle.x();
            entry[QStringLiteral("midY")] = middle.y();
            entry[QStringLiteral("labelX")] = tipOnScreen.x() + out.x() * kLetterGap;
            entry[QStringLiteral("labelY")] = tipOnScreen.y() + out.y() * kLetterGap;
            parts.push_back(entry);
        }

        // The dot in the middle: across the bed.
        const QString free = QStringLiteral("free");
        const bool pointed = m_pointed == free ||
                             (m_editing && (m_moving == free || m_moving == QLatin1String("body")));
        render::HandleDot dot;
        dot.centre = *at;
        dot.radius = pointed ? 9.5F : 8.0F;
        const float dotStrength =
            m_editing && m_moving != free && m_moving != QLatin1String("body") ? kCalm : 1.0F;
        dot.fill = faded(m_colours.fill, dotStrength);
        dot.ring = faded(m_colours.ink, dotStrength);
        dot.ringWidth = 1.5F;
        graphics.dots.push_back(dot);
        {
            // The dot moves across the bed: seen from the side (the bed edge on) it is not grabbed.
            const QPointF middle = onScreen(*at);
            QVariantMap entry;
            entry[QStringLiteral("part")] = free;
            entry[QStringLiteral("shown")] =
                held(free) || std::abs(sight.z()) >= interaction::kLeastPlaneSight;
            entry[QStringLiteral("axis")] = QStringLiteral("none");
            entry[QStringLiteral("x")] = middle.x();
            entry[QStringLiteral("y")] = middle.y();
            parts.push_back(entry);
        }
    }
    if (m_viewport != nullptr) {
        m_viewport->setHandleGraphics(std::move(graphics), Viewport3D::HandleSource::gizmo);
    }
    if (parts != m_parts || bubble != m_bubbleAt) {
        m_parts = parts;
        m_bubbleAt = bubble;
        emit partsChanged();
    }
}

void GizmoController::setPointed(const QString& part, bool on) {
    const QString pointed = on ? part : (m_pointed == part ? QString() : m_pointed);
    if (pointed != m_pointed) {
        m_pointed = pointed;
        refresh();
    }
}

bool GizmoController::moves(int axis) const {
    if (m_moving == QLatin1String("free") || m_moving == QLatin1String("body")) {
        return axis < 2; // across the bed
    }
    return m_moving.contains(kAxisNames[axis]);
}

bool GizmoController::startMove(const QString& moving, const QVector3D& anchorPoint) {
    const auto position = m_model->currentPosition();
    if (!position || !m_model->beginMove()) {
        return false;
    }
    m_startPosition = *position;
    m_startAnchor = anchorPoint;
    m_offset = {0.0, 0.0, 0.0};
    m_pixelsPerMillimetre = 1.0 / millimetresPerPixel(anchorPoint);
    m_dragging = true;
    setEditing(true, moving);
    emit offsetChanged();
    refresh();
    return true;
}

bool GizmoController::beginDrag(const QString& part, double x, double y) {
    if (m_model == nullptr || m_viewport == nullptr || m_editing || !m_enabled) {
        return false;
    }
    const auto at = anchor();
    const auto ray = m_viewport->rayAt(x, y);
    if (!at || !ray) {
        return false;
    }
    const interaction::Ray pointer{ray->origin, ray->direction};
    m_axisDrag.reset();
    m_planeDrag.reset();
    for (int axis = 0; axis < 3; ++axis) {
        if (part == kAxisNames[axis]) {
            const auto screen = interaction::AxisDrag::screenOf(
                QPointF(x, y), *at, kAxes[axis], m_viewport->eyePosition(),
                [this](const QVector3D& p) { return m_viewport->project(p.x(), p.y(), p.z()); });
            m_axisDrag = interaction::AxisDrag::grab(pointer, *at, kAxes[axis], screen);
            m_axisPixelsPerMillimetre = screen.pixelsPerMillimetre;
        }
    }
    if (part == QLatin1String("xy") || part == QLatin1String("free")) {
        m_planeDrag = interaction::PlaneDrag::grab(pointer, *at, kAxes[2]);
    } else if (part == QLatin1String("xz")) {
        m_planeDrag = interaction::PlaneDrag::grab(pointer, *at, kAxes[1]);
    } else if (part == QLatin1String("yz")) {
        m_planeDrag = interaction::PlaneDrag::grab(pointer, *at, kAxes[0]);
    }
    if (!m_axisDrag && !m_planeDrag) {
        return false; // seen end on, or not a part
    }
    if (!startMove(part, *at)) {
        return false;
    }
    if (m_axisDrag) {
        m_pixelsPerMillimetre = m_axisPixelsPerMillimetre; // the grid of the arrow as seen
    }
    return true;
}

bool GizmoController::beginBodyDrag(double x, double y) {
    if (m_model == nullptr || m_viewport == nullptr || m_editing) {
        return false;
    }
    const auto ray = m_viewport->rayAt(x, y);
    if (!ray) {
        return false;
    }
    // The level plane through the point of the body under the pointer: that point stays under
    // the pointer while the body moves.
    const auto grabbed = m_viewport->bodyPointAt(x, y);
    const auto box = bodyBox();
    if (!box || !m_model->currentPosition()) {
        return false;
    }
    const QVector3D through = grabbed ? grabbed->point : (box->low + box->high) / 2.0F;
    m_axisDrag.reset();
    m_planeDrag =
        interaction::PlaneDrag::grab({ray->origin, ray->direction}, through, kAxes[2], 0.05);
    if (!m_planeDrag) {
        return false;
    }
    // The gizmo (if shown) goes along from where it is now.
    QVector3D top((box->low.x() + box->high.x()) / 2.0F, (box->low.y() + box->high.y()) / 2.0F,
                  box->high.z());
    top.setZ(top.z() + static_cast<float>(kLift * millimetresPerPixel(top)));
    if (!startMove(QStringLiteral("body"), top)) {
        return false;
    }
    m_pixelsPerMillimetre = 1.0 / millimetresPerPixel(through);
    return true;
}

bool GizmoController::moveTo(std::array<double, 3> target) {
    std::array<std::optional<double>, 3> position;
    for (std::size_t axis = 0; axis < 3; ++axis) {
        if (!moves(static_cast<int>(axis))) {
            target[axis] = m_startPosition[axis];
            continue;
        }
        target[axis] = std::clamp(target[axis], -kFarthestMillimetres, kFarthestMillimetres);
        position[axis] = target[axis];
    }
    const std::array<double, 3> offset{target[0] - m_startPosition[0],
                                       target[1] - m_startPosition[1],
                                       target[2] - m_startPosition[2]};
    if (offset == m_offset) {
        return true;
    }
    if (!m_model->moveCurrent(position)) {
        return false;
    }
    m_offset = offset;
    emit offsetChanged();
    refresh(); // the gizmo goes with the pointer at once
    return true;
}

bool GizmoController::dragTo(double x, double y) {
    if (!m_dragging || m_viewport == nullptr) {
        return false;
    }
    const auto ray = m_viewport->rayAt(x, y);
    if (!ray) {
        return false;
    }
    const interaction::Ray pointer{ray->origin, ray->direction};
    QVector3D moved;
    if (m_axisDrag) {
        const auto along = m_axisDrag->moved(pointer, QPointF(x, y));
        if (!along) {
            return false; // behind the eye
        }
        moved = m_axisDrag->axis() * static_cast<float>(*along);
    } else if (m_planeDrag) {
        const auto across = m_planeDrag->moved(pointer);
        if (!across) {
            return false;
        }
        moved = *across;
    } else {
        return false;
    }
    // The position snaps to the grid of the moment (whole millimetres, finer when zoomed in).
    const double step = interaction::snapStep(m_pixelsPerMillimetre);
    std::array<double, 3> target{};
    for (std::size_t axis = 0; axis < 3; ++axis) {
        target[axis] =
            interaction::snapped(m_startPosition[axis] + moved[static_cast<int>(axis)], step);
    }
    return moveTo(target);
}

bool GizmoController::endDrag() {
    if (!m_dragging) {
        return false;
    }
    m_dragging = false;
    m_model->commitChange();
    setEditing(false, {});
    m_offset = {0.0, 0.0, 0.0};
    emit offsetChanged();
    refresh();
    return true;
}

void GizmoController::cancelDrag() {
    if (!m_dragging) {
        return;
    }
    m_dragging = false;
    m_model->cancelChange();
    setEditing(false, {});
    m_offset = {0.0, 0.0, 0.0};
    emit offsetChanged();
    refresh();
}

bool GizmoController::typeInsteadOfDrag() {
    if (!m_dragging) {
        return false;
    }
    m_dragging = false; // still editing: the same step goes on
    return true;
}

bool GizmoController::setOffset(int axis, double millimetres) {
    if (!m_editing || m_dragging || axis < 0 || axis > 2 || !std::isfinite(millimetres)) {
        return false;
    }
    // A typed axis takes part in the move from now on (Tab goes from X to Y to Z).
    if (!moves(axis)) {
        m_moving = m_moving == QLatin1String("free") || m_moving == QLatin1String("body")
                       ? QStringLiteral("xyz")
                       : m_moving + kAxisNames[axis];
        emit editingChanged();
    }
    std::array<double, 3> target{m_startPosition[0] + m_offset[0], m_startPosition[1] + m_offset[1],
                                 m_startPosition[2] + m_offset[2]};
    target[static_cast<std::size_t>(axis)] =
        m_startPosition[static_cast<std::size_t>(axis)] + millimetres;
    return moveTo(target);
}

bool GizmoController::commitEdit() {
    if (!m_editing || m_dragging) {
        return false;
    }
    m_model->commitChange();
    setEditing(false, {});
    m_offset = {0.0, 0.0, 0.0};
    emit offsetChanged();
    refresh();
    return true;
}

void GizmoController::cancelEdit() {
    if (!m_editing || m_dragging) {
        return;
    }
    m_model->cancelChange();
    setEditing(false, {});
    m_offset = {0.0, 0.0, 0.0};
    emit offsetChanged();
    refresh();
}

bool GizmoController::nudge(int right, int away, int up, bool large) {
    if (m_model == nullptr || m_viewport == nullptr || m_editing) {
        return false;
    }
    const auto position = m_model->currentPosition();
    if (!position) {
        return false;
    }
    // The ways of the screen, laid on the bed: right, and away from the viewer.
    const QVector3D eye = m_viewport->eyePosition();
    const auto box = bodyBox();
    const QVector3D centre = box ? (box->low + box->high) / 2.0F : QVector3D();
    QVector3D forward = centre - eye;
    forward.setZ(0.0F);
    if (forward.lengthSquared() < 1e-9F) {
        // Seen from straight above: "away" is up on the screen.
        const QPointF a = m_viewport->project(centre.x(), centre.y(), centre.z());
        const QPointF b = m_viewport->project(centre.x(), centre.y() + 1.0, centre.z());
        forward = (b.y() < a.y()) ? QVector3D(0, 1, 0) : QVector3D(0, -1, 0);
    }
    forward.normalize();
    const QVector3D rightWay = QVector3D::crossProduct(forward, QVector3D(0, 0, 1));
    // The axis of the bed closest to a way, with its sign.
    const auto nearest = [](const QVector3D& way) {
        return std::abs(way.x()) >= std::abs(way.y())
                   ? QVector3D(way.x() >= 0 ? 1.0F : -1.0F, 0, 0)
                   : QVector3D(0, way.y() >= 0 ? 1.0F : -1.0F, 0);
    };
    const QVector3D step = nearest(rightWay) * static_cast<float>(right) +
                           nearest(forward) * static_cast<float>(away) +
                           QVector3D(0, 0, static_cast<float>(up));
    if (step.lengthSquared() < 1e-9F) {
        return false;
    }
    const double distance = large ? 10.0 : 1.0;
    std::array<std::optional<double>, 3> target;
    for (int axis = 0; axis < 3; ++axis) {
        if (step[axis] != 0.0F) {
            target[static_cast<std::size_t>(axis)] =
                std::clamp((*position)[static_cast<std::size_t>(axis)] + step[axis] * distance,
                           -kFarthestMillimetres, kFarthestMillimetres);
        }
    }
    if (!m_model->beginMove()) {
        return false;
    }
    const bool done = m_model->moveCurrent(target);
    m_model->commitChange();
    return done;
}

} // namespace fission::ui
