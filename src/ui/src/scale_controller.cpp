// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/ui/scale_controller.h"

#include <algorithm>
#include <cmath>

#include "fission/interaction/snap.h"
#include "fission/render/scene.h"

namespace fission::ui {

namespace {

constexpr double kBubbleGap = 22.0;
constexpr double kDashPixels = 6.0; // a dash of the box, and the gap after it

QColor faded(QColor colour, float strength) {
    colour.setAlphaF(colour.alphaF() * strength);
    return colour;
}

QVector3D toVector(const geo::Point3d& p) {
    return {static_cast<float>(p.x), static_cast<float>(p.y), static_cast<float>(p.z)};
}

} // namespace

ScaleController::ScaleController(DocumentModel* model, QObject* parent)
    : QObject(parent), m_model(model) {
    if (m_model == nullptr) {
        return;
    }
    connect(m_model, &DocumentModel::stepValuesChanged, this, &ScaleController::refresh);
    connect(m_model, &DocumentModel::sceneReady, this, [this] { refresh(); });
    connect(m_model, &DocumentModel::currentStepChanged, this, &ScaleController::refresh);
}

void ScaleController::attach(Viewport3D* viewport) {
    m_viewport = viewport;
    if (m_viewport != nullptr) {
        connect(m_viewport, &Viewport3D::cameraChanged, this, &ScaleController::refresh);
    }
    refresh();
}

void ScaleController::setEnabled(bool enabled) {
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

void ScaleController::setColours(const QVariantMap& colours) {
    const auto colour = [&colours](const char* name, const QColor& fallback) {
        const QVariant value = colours.value(QLatin1String(name));
        return value.isValid() ? value.value<QColor>() : fallback;
    };
    m_colours.handle = colour("handle", m_colours.handle);
    m_colours.ink = colour("ink", m_colours.ink);
    m_colours.line = colour("line", m_colours.line);
    m_colours.accent = colour("accent", m_colours.accent);
    m_colourMap = colours;
    emit coloursChanged();
    refresh();
}

QVariantList ScaleController::size() const {
    if (m_editing) {
        return {(m_startBox.high.x() - m_startBox.low.x()) * m_factor,
                (m_startBox.high.y() - m_startBox.low.y()) * m_factor,
                (m_startBox.high.z() - m_startBox.low.z()) * m_factor};
    }
    return {};
}

void ScaleController::setEditing(bool editing) {
    if (editing != m_editing) {
        m_editing = editing;
        emit editingChanged();
    }
}

std::optional<ScaleController::Box> ScaleController::meshBox() {
    const auto scene = m_model->scene();
    const std::string body = m_model->bodyOfStep(m_model->currentStep()).toStdString();
    if (!scene || body.empty() || !m_model->currentPosition()) {
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

std::optional<ScaleController::Box> ScaleController::shownBox() {
    if (!m_editing) {
        return meshBox();
    }
    // While dragging the box goes with the factor at once; the mesh follows when it is made.
    const auto grown = [this](const QVector3D& p) {
        const QVector3D anchor = toVector(m_anchor);
        return anchor + static_cast<float>(m_factor) * (p - anchor);
    };
    return Box{grown(m_startBox.low), grown(m_startBox.high)};
}

QVector3D ScaleController::topCorner(const Box& box, int corner) {
    // 0 front left, 1 front right, 2 back right, 3 back left (front is -Y).
    const bool right = corner == 1 || corner == 2;
    const bool back = corner == 2 || corner == 3;
    return {right ? box.high.x() : box.low.x(), back ? box.high.y() : box.low.y(), box.high.z()};
}

void ScaleController::refresh() {
    QVariantList corners;
    QVariantMap bubble;
    render::HandleGraphics graphics;
    const auto box = m_enabled && m_model != nullptr && m_viewport != nullptr
                         ? shownBox()
                         : std::optional<Box>();
    for (int corner = 0; corner < 4; ++corner) {
        QVariantMap entry;
        entry[QStringLiteral("corner")] = corner;
        entry[QStringLiteral("shown")] = box.has_value();
        if (box) {
            const QVector3D at = topCorner(*box, corner);
            const QPointF screen = m_viewport->project(at.x(), at.y(), at.z());
            entry[QStringLiteral("x")] = screen.x();
            entry[QStringLiteral("y")] = screen.y();
        }
        corners.push_back(entry);
    }
    if (box) {
        // The box round the body, dashed: where the corners are when the body is round or turned.
        const QVector3D& a = box->low;
        const QVector3D& b = box->high;
        const QVector3D points[8] = {{a.x(), a.y(), a.z()}, {b.x(), a.y(), a.z()},
                                     {b.x(), b.y(), a.z()}, {a.x(), b.y(), a.z()},
                                     {a.x(), a.y(), b.z()}, {b.x(), a.y(), b.z()},
                                     {b.x(), b.y(), b.z()}, {a.x(), b.y(), b.z()}};
        const int edges[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
                                  {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
        const QColor dash = m_editing ? faded(m_colours.accent, 0.8F) : faded(m_colours.line, 0.7F);
        for (const auto& edge : edges) {
            const QVector3D from = points[edge[0]];
            const QVector3D to = points[edge[1]];
            const QPointF p = m_viewport->project(from.x(), from.y(), from.z());
            const QPointF q = m_viewport->project(to.x(), to.y(), to.z());
            const double pixels = std::hypot(q.x() - p.x(), q.y() - p.y());
            const int pieces = std::max(1, static_cast<int>(pixels / kDashPixels));
            for (int i = 0; i < pieces; i += 2) {
                const float s = static_cast<float>(i) / static_cast<float>(pieces);
                const float t =
                    static_cast<float>(std::min(i + 1, pieces)) / static_cast<float>(pieces);
                graphics.lines.push_back(
                    {from + s * (to - from), from + t * (to - from), 1.2F, dash});
            }
        }
        for (int corner = 0; corner < 4; ++corner) {
            const bool held = m_editing && corner == m_corner;
            const bool strong = held || (!m_editing && corner == m_pointed);
            render::HandleDot square;
            square.centre = topCorner(*box, corner);
            square.radius = strong ? kCornerPixels + 1.5F : kCornerPixels;
            square.fill = m_colours.handle;
            square.ring = strong ? m_colours.accent : m_colours.ink;
            square.ringWidth = 2.0F;
            square.square = true;
            // While one is held, the others step back.
            if (m_editing && !held) {
                square.fill = faded(square.fill, 0.4F);
                square.ring = faded(square.ring, 0.4F);
            }
            graphics.dots.push_back(square);
        }
        if (m_editing && m_corner >= 0) {
            const QVector3D at = topCorner(*box, m_corner);
            const QPointF screen = m_viewport->project(at.x(), at.y(), at.z());
            bubble[QStringLiteral("x")] = screen.x() + kBubbleGap;
            bubble[QStringLiteral("y")] = screen.y() - 15.0;
            // The corner itself: the bubble goes to its other side where there is no room.
            bubble[QStringLiteral("cornerX")] = screen.x();
            bubble[QStringLiteral("cornerY")] = screen.y();
        }
    }
    graphics.append(m_snapMarks);
    if (m_viewport != nullptr) {
        m_viewport->setHandleGraphics(std::move(graphics), Viewport3D::HandleSource::corners);
    }
    if (corners != m_corners || bubble != m_bubbleAt) {
        m_corners = corners;
        m_bubbleAt = bubble;
        emit cornersChanged();
    }
}

void ScaleController::setPointed(int corner, bool on) {
    const int pointed = on ? corner : (m_pointed == corner ? -1 : m_pointed);
    if (pointed != m_pointed) {
        m_pointed = pointed;
        refresh();
    }
}

bool ScaleController::beginDrag(int corner, double x, double y) {
    if (m_model == nullptr || m_viewport == nullptr || m_editing || !m_enabled || corner < 0 ||
        corner > 3) {
        return false;
    }
    const auto bounds = m_model->currentBodyBounds();
    const auto ray = m_viewport->rayAt(x, y);
    if (!bounds || !ray) {
        return false;
    }
    // Exact (the mesh of a round body is a little inside it): the anchor stays where it is, to
    // the micrometre, and a body on the bed stays on it.
    const bool right = corner == 1 || corner == 2;
    const bool back = corner == 2 || corner == 3;
    const geo::Point3d held{right ? bounds->max.x : bounds->min.x,
                            back ? bounds->max.y : bounds->min.y, bounds->max.z};
    const geo::Point3d anchor{right ? bounds->min.x : bounds->max.x,
                              back ? bounds->min.y : bounds->max.y, bounds->min.z};
    const double dx = held.x - anchor.x;
    const double dy = held.y - anchor.y;
    const double dz = held.z - anchor.z;
    const double diagonal = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (!(diagonal > 1e-6)) {
        return false;
    }
    const QVector3D origin = toVector(held);
    const QVector3D axis(static_cast<float>(dx / diagonal), static_cast<float>(dy / diagonal),
                         static_cast<float>(dz / diagonal));
    const auto project = [this](const QVector3D& p) {
        return m_viewport->project(p.x(), p.y(), p.z());
    };
    const auto screen = interaction::AxisDrag::screenOf(QPointF(x, y), origin, axis,
                                                        m_viewport->eyePosition(), project);
    // The diagonal of the corner nearest to the viewer points nearly at the eye in the usual
    // views. It is still followed in space down to a steeper angle than an arrow is, so that the
    // corner stays under the pointer; a step of a pixel then means more, but the screen line from
    // the corner across to this one is long enough to aim along.
    m_drag = interaction::AxisDrag::grab({ray->origin, ray->direction}, origin, axis, screen,
                                         interaction::kLeastPlaneSight);
    if (!m_drag || !m_model->beginScale()) {
        m_drag.reset();
        return false;
    }
    m_corner = corner;
    m_startBox = {toVector(bounds->min), toVector(bounds->max)};
    m_anchor = anchor;
    m_held = held;
    m_diagonal = diagonal;
    m_factor = 1.0;
    clearSnap();
    if (const auto scene = m_model->scene()) {
        // The body itself is not a target; its own points are not needed (only the corner held).
        m_snap.begin(*scene, m_model->bodyOfStep(m_model->currentStep()).toStdString(),
                     {bounds->min.x, bounds->min.y, bounds->min.z},
                     {bounds->max.x, bounds->max.y, bounds->max.z});
    }
    m_dragging = true;
    setEditing(true);
    emit factorChanged();
    refresh();
    return true;
}

bool ScaleController::applyFactor(double factor) {
    if (!m_model->scaleCurrent(factor, m_anchor)) {
        return false;
    }
    m_factor = factor;
    emit factorChanged();
    refresh();
    return true;
}

bool ScaleController::dragTo(double x, double y) {
    if (!m_dragging || !m_drag || m_viewport == nullptr) {
        return false;
    }
    const auto ray = m_viewport->rayAt(x, y);
    if (!ray) {
        return false;
    }
    const auto moved = m_drag->moved({ray->origin, ray->direction}, QPointF(x, y));
    if (!moved) {
        return false; // behind the eye, or beyond the vanishing point of the diagonal
    }
    const double asked = 1.0 + *moved / m_diagonal;
    // The corner catches on the points of the others; where it catches on none, the factor rests
    // on whole percent.
    const std::optional<double> caught = snappedFactor(asked);
    double wanted = caught ? *caught : interaction::snapped(asked, kFactorStep);
    // Where a size would become impossible (too small, a wall thicker than the tube), the corner
    // stops there: as close as the shape allows, on whole percent. It never stays behind.
    if (!caught && wanted != m_factor && !m_model->acceptsScale(wanted)) {
        double possible = m_factor;
        double impossible = wanted;
        for (int i = 0; i < 48; ++i) {
            const double middle = (possible + impossible) / 2.0;
            (m_model->acceptsScale(middle) ? possible : impossible) = middle;
        }
        const double onGrid = wanted < m_factor ? std::ceil(possible / kFactorStep) * kFactorStep
                                                : std::floor(possible / kFactorStep) * kFactorStep;
        wanted = m_model->acceptsScale(onGrid) ? onGrid : m_factor;
    }
    if (wanted != m_factor) {
        applyFactor(wanted);
    }
    return true;
}

std::optional<double> ScaleController::snappedFactor(double asked) {
    if (!m_snap.active() || snappingSuppressed()) {
        clearSnap();
        return std::nullopt;
    }
    const QVector3D around =
        toVector(m_anchor) + static_cast<float>(asked) * (toVector(m_held) - toVector(m_anchor));
    const auto caught = interaction::snapScaleFactor(
        {m_anchor.x, m_anchor.y, m_anchor.z}, {m_held.x, m_held.y, m_held.z}, asked,
        m_snap.targets(), snapReachAround(*m_viewport, around));
    if (!caught || !m_model->acceptsScale(caught->factor)) {
        clearSnap(); // a size the shape cannot have: the grid of whole percent
        return std::nullopt;
    }
    const QColor accent = m_viewport->property("accentColor").value<QColor>();
    render::HandleGraphics marks =
        SnapSession::marksOf(caught->result, accent, millimetresPerPixelAt(*m_viewport, around));
    if (!(marks == m_snapMarks)) {
        m_snapMarks = std::move(marks);
        refresh();
    }
    return caught->factor;
}

void ScaleController::clearSnap() {
    const bool had = !m_snapMarks.empty();
    m_snapMarks = {};
    if (had) {
        refresh();
    }
}

void ScaleController::finish() {
    m_snap.end();
    m_snapMarks = {};
    m_dragging = false;
    m_drag.reset();
    m_corner = -1;
    m_factor = 1.0;
    setEditing(false);
    emit factorChanged();
    refresh();
}

bool ScaleController::endDrag() {
    if (!m_dragging) {
        return false;
    }
    m_model->commitChange();
    finish();
    return true;
}

void ScaleController::cancelDrag() {
    if (!m_dragging) {
        return;
    }
    m_model->cancelChange();
    finish();
}

bool ScaleController::typeInsteadOfDrag() {
    if (!m_dragging) {
        return false;
    }
    m_dragging = false; // still editing: the same step goes on
    m_drag.reset();
    m_snap.end(); // typed numbers are not caught on anything
    clearSnap();
    return true;
}

bool ScaleController::setPercent(double percent) {
    if (!m_editing || m_dragging || !std::isfinite(percent)) {
        return false;
    }
    const double factor = percent / 100.0;
    if (!m_model->acceptsScale(factor)) {
        return false;
    }
    return applyFactor(factor);
}

bool ScaleController::commitEdit() {
    if (!m_editing || m_dragging) {
        return false;
    }
    m_model->commitChange();
    finish();
    return true;
}

void ScaleController::cancelEdit() {
    if (!m_editing || m_dragging) {
        return;
    }
    m_model->cancelChange();
    finish();
}

} // namespace fission::ui
