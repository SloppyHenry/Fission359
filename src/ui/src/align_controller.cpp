// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/ui/align_controller.h"

#include <QPointF>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <set>

#include "fission/render/scene_renderer.h"

namespace fission::ui {

namespace {

constexpr const char* kAxisNames[3] = {"x", "y", "z"};
constexpr const char* kLineNames[3] = {"min", "centre", "max"};

QVector3D toVector(const interaction::Vec3& p) {
    return {static_cast<float>(p[0]), static_cast<float>(p[1]), static_cast<float>(p[2])};
}

QColor faded(QColor colour, float strength) {
    colour.setAlphaF(colour.alphaF() * strength);
    return colour;
}

} // namespace

AlignController::AlignController(DocumentModel* model, SelectionController* selection,
                                 QObject* parent)
    : QObject(parent), m_model(model), m_selection(selection) {
    if (m_model != nullptr) {
        connect(m_model, &DocumentModel::sceneReady, this, [this] { refresh(); });
    }
    if (m_selection != nullptr) {
        connect(m_selection, &SelectionController::changed, this, &AlignController::refresh);
    }
}

void AlignController::attach(Viewport3D* viewport) {
    m_viewport = viewport;
    if (m_viewport != nullptr) {
        connect(m_viewport, &Viewport3D::cameraChanged, this, &AlignController::refresh);
    }
    refresh();
}

void AlignController::setEnabled(bool enabled) {
    if (enabled == m_enabled) {
        return;
    }
    m_enabled = enabled;
    emit enabledChanged();
    refresh();
}

void AlignController::setColours(const QVariantMap& colours) {
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

void AlignController::setPointed(int index, bool on) {
    const int pointed = on ? index : (m_pointed == index ? -1 : m_pointed);
    if (pointed != m_pointed) {
        m_pointed = pointed;
        refresh();
    }
}

std::optional<AlignController::Group> AlignController::group() const {
    if (m_model == nullptr || m_selection == nullptr) {
        return std::nullopt;
    }
    Group found;
    found.box = {{1e30, 1e30, 1e30}, {-1e30, -1e30, -1e30}};
    std::set<QString> seen;
    for (const QVariant& item : m_selection->items()) {
        const QString body = item.toMap().value(QStringLiteral("body")).toString();
        if (!seen.insert(body).second) {
            continue;
        }
        const auto bounds = m_model->bodyBounds(body);
        if (!bounds) {
            continue; // not calculated (yet)
        }
        found.bodies.push_back(body);
        const double low[3] = {bounds->min.x, bounds->min.y, bounds->min.z};
        const double high[3] = {bounds->max.x, bounds->max.y, bounds->max.z};
        for (std::size_t a = 0; a < 3; ++a) {
            found.box.low[a] = std::min(found.box.low[a], low[a]);
            found.box.high[a] = std::max(found.box.high[a], high[a]);
        }
    }
    if (found.bodies.size() < 2) {
        return std::nullopt;
    }
    return found;
}

void AlignController::refresh() {
    QVariantList points;
    render::HandleGraphics graphics;
    const auto chosen = m_enabled && m_viewport != nullptr ? group() : std::nullopt;
    if (chosen) {
        const interaction::Box3& box = chosen->box;
        const QVector3D eye = m_viewport->eyePosition();
        const QColor tones[3] = {m_colours.x, m_colours.y, m_colours.z};
        const auto onScreen = [this](const QVector3D& p) {
            return m_viewport->project(p.x(), p.y(), p.z());
        };
        // The edge of the box each direction is on: across X on the side of the box nearer to the
        // viewer, on the bed; across Z at the bottom corner that is furthest left on screen.
        const double floor = eye.z() >= box.low[2] ? box.low[2] : box.high[2];
        const double nearY = eye.y() > (box.low[1] + box.high[1]) / 2.0 ? box.high[1] : box.low[1];
        const double nearX = eye.x() > (box.low[0] + box.high[0]) / 2.0 ? box.high[0] : box.low[0];
        double leftX = box.low[0];
        double leftY = box.low[1];
        double leftMost = 1e30;
        for (int corner = 0; corner < 4; ++corner) {
            const double cx = (corner & 1) != 0 ? box.high[0] : box.low[0];
            const double cy = (corner & 2) != 0 ? box.high[1] : box.low[1];
            const double sx = onScreen(QVector3D(static_cast<float>(cx), static_cast<float>(cy),
                                                 static_cast<float>(floor)))
                                  .x();
            if (sx < leftMost) {
                leftMost = sx;
                leftX = cx;
                leftY = cy;
            }
        }
        const interaction::Vec3 edges[3] = {
            {0.0, nearY, floor}, {nearX, 0.0, floor}, {leftX, leftY, 0.0}};
        int index = 0;
        for (std::size_t a = 0; a < 3; ++a) {
            const double low = box.low[a];
            const double high = box.high[a];
            interaction::Vec3 start = edges[a];
            interaction::Vec3 end = edges[a];
            start[a] = low;
            end[a] = high;
            const float strength =
                m_pointed >= 0 && m_pointed / 3 != static_cast<int>(a) ? 0.5F : 1.0F;
            // The edge itself, thin, in the colour of the direction.
            graphics.lines.push_back(
                {toVector(start), toVector(end), 1.5F, faded(tones[a], strength)});
            for (int line = 0; line < 3; ++line, ++index) {
                interaction::Vec3 at = edges[a];
                at[a] = line == 0 ? low : (line == 1 ? (low + high) / 2.0 : high);
                const bool pointed = m_pointed == index;
                render::HandleDot dot;
                dot.centre = toVector(at);
                dot.radius = kPointRadius + (pointed ? 2.0F : 0.0F);
                dot.fill = pointed ? tones[a] : faded(m_colours.fill, strength);
                dot.ring = faded(pointed ? m_colours.fill : tones[a], strength);
                dot.ringWidth = 2.0F;
                graphics.dots.push_back(dot);
                const QPointF screen = onScreen(toVector(at));
                QVariantMap entry;
                entry[QStringLiteral("index")] = index;
                entry[QStringLiteral("axis")] = QLatin1String(kAxisNames[a]);
                entry[QStringLiteral("to")] = QLatin1String(kLineNames[line]);
                entry[QStringLiteral("x")] = screen.x();
                entry[QStringLiteral("y")] = screen.y();
                points.push_back(entry);

                if (pointed) {
                    // The plane the bodies will line up on, across the box round them.
                    const std::size_t u = (a + 1) % 3;
                    const std::size_t v = (a + 2) % 3;
                    const double where = at[a];
                    const auto corner = [&](double cu, double cv) {
                        interaction::Vec3 p{};
                        p[a] = where;
                        p[u] = cu;
                        p[v] = cv;
                        return toVector(p);
                    };
                    render::HandleQuad quad;
                    quad.corners[0] = corner(box.low[u], box.low[v]);
                    quad.corners[1] = corner(box.high[u], box.low[v]);
                    quad.corners[2] = corner(box.high[u], box.high[v]);
                    quad.corners[3] = corner(box.low[u], box.high[v]);
                    quad.fill = faded(tones[a], 0.12F);
                    quad.line = faded(tones[a], 0.9F);
                    quad.lineWidth = 1.5F;
                    graphics.quads.push_back(quad);
                }
            }
        }
    }
    if (m_viewport != nullptr) {
        m_viewport->setHandleGraphics(std::move(graphics), Viewport3D::HandleSource::align);
    }
    if (points != m_points) {
        m_points = points;
        emit pointsChanged();
    }
}

int AlignController::indexOf(const QString& axis, const QString& to) const {
    for (const QVariant& entry : m_points) {
        const QVariantMap point = entry.toMap();
        if (point.value(QStringLiteral("axis")).toString() == axis &&
            point.value(QStringLiteral("to")).toString() == to) {
            return point.value(QStringLiteral("index")).toInt();
        }
    }
    return -1;
}

bool AlignController::align(int index) {
    if (m_model == nullptr || index < 0 || index >= m_points.size()) {
        return false;
    }
    const auto chosen = group();
    if (!chosen) {
        return false;
    }
    const QVariantMap point = m_points[index].toMap();
    const QString axis = point.value(QStringLiteral("axis")).toString();
    const int a = axis == QLatin1String("x") ? 0 : axis == QLatin1String("y") ? 1 : 2;
    return m_model->alignBodies(chosen->bodies, a, point.value(QStringLiteral("to")).toString());
}

} // namespace fission::ui
