// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/ui/shape_previews.h"

#include <QColor>
#include <QPainter>

#include <algorithm>
#include <cmath>
#include <map>
#include <numbers>
#include <set>
#include <vector>

#include "fission/core/document.h"
#include "fission/core/feature_type.h"
#include "fission/core/recompute.h"
#include "fission/features/builtin.h"
#include "fission/geo/error.h"

namespace fission::ui {

namespace {

// The colours of the pictures: the palette of the mockup (docs/mockups/oberflaeche.html).
QColor colorOf(const QString& type) {
    static const std::map<QString, QColor> colors{
        {QStringLiteral("box"), {70, 136, 206}},
        {QStringLiteral("cylinder"), {236, 128, 52}},
        {QStringLiteral("sphere"), {146, 112, 200}},
        {QStringLiteral("hemisphere"), {146, 112, 200}},
        {QStringLiteral("cone"), {242, 183, 56}},
        {QStringLiteral("torus"), {66, 156, 96}},
        {QStringLiteral("tube"), {52, 166, 160}},
        {QStringLiteral("prism"), {52, 166, 160}},
        {QStringLiteral("pyramid"), {242, 183, 56}},
        {QStringLiteral("wedge"), {236, 128, 52}},
        {QStringLiteral("star"), {242, 183, 56}},
        {QStringLiteral("text"), {214, 66, 56}},
    };
    const auto found = colors.find(type);
    return found != colors.end() ? found->second : QColor(150, 156, 166);
}

struct Vec {
    double x = 0.0, y = 0.0, z = 0.0;
};

Vec cross(const Vec& a, const Vec& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
double dot(const Vec& a, const Vec& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
Vec normalized(const Vec& v) {
    const double length = std::sqrt(dot(v, v));
    return length > 1e-12 ? Vec{v.x / length, v.y / length, v.z / length} : Vec{0.0, 0.0, 1.0};
}

} // namespace

QImage drawShapePicture(const geo::TriangleMesh& mesh, const QColor& color, bool hole,
                        const QSize& size) {
    QImage result(size, QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::transparent);
    if (mesh.empty() || size.isEmpty()) {
        return result;
    }

    // The camera: yaw -30 degrees, 30 degrees above the bed, orthographic. `right` and `up`
    // span the picture, `toEye` points to the eye (larger depth = nearer).
    constexpr double kPi = std::numbers::pi;
    const double yaw = -30.0 * kPi / 180.0;
    const double pitch = 30.0 * kPi / 180.0;
    const Vec toEye{std::sin(yaw) * std::cos(pitch), -std::cos(yaw) * std::cos(pitch),
                    std::sin(pitch)};
    const Vec right = normalized(cross({0.0, 0.0, 1.0}, toEye));
    const Vec up = cross(toEye, right);
    const auto project = [&](double x, double y, double z) {
        const Vec v{x, y, z};
        return Vec{dot(v, right), dot(v, up), dot(v, toEye)};
    };

    // Fit the picture into the image with a margin.
    double minX = 1e30, maxX = -1e30, minY = 1e30, maxY = -1e30, minZ = 1e30, maxZ = -1e30;
    std::vector<Vec> projected;
    projected.reserve(mesh.positions.size());
    for (const geo::Point3f& p : mesh.positions) {
        const Vec v = project(p.x, p.y, p.z);
        projected.push_back(v);
        minX = std::min(minX, v.x);
        maxX = std::max(maxX, v.x);
        minY = std::min(minY, v.y);
        maxY = std::max(maxY, v.y);
        minZ = std::min(minZ, v.z);
        maxZ = std::max(maxZ, v.z);
    }

    // Drawn at three times the size into a depth buffer, then averaged down: smooth edges and
    // correct hiding of one part by another, also for thin letters and the inside of a tube.
    constexpr int kScale = 3;
    const int width = size.width() * kScale;
    const int height = size.height() * kScale;
    constexpr double kMargin = 0.08;
    const double scale = std::min(width * (1.0 - 2.0 * kMargin) / std::max(maxX - minX, 1e-9),
                                  height * (1.0 - 2.0 * kMargin) / std::max(maxY - minY, 1e-9));
    const double centerX = (minX + maxX) / 2.0;
    const double centerY = (minY + maxY) / 2.0;
    const auto toBuffer = [&](const Vec& v) {
        return Vec{width / 2.0 + (v.x - centerX) * scale, height / 2.0 - (v.y - centerY) * scale,
                   v.z};
    };

    std::vector<float> depth(static_cast<std::size_t>(width) * height, -1e30F);
    std::vector<QRgb> pixels(static_cast<std::size_t>(width) * height, 0);
    const Vec lightDirection = normalized({-0.45, -0.55, 0.75});
    const auto lightAt = [&](std::uint32_t i) {
        const Vec normal{mesh.normals[i].x, mesh.normals[i].y, mesh.normals[i].z};
        return 0.52 + 0.48 * std::max(dot(normalized(normal), lightDirection), 0.0);
    };

    for (std::size_t t = 0; t < mesh.triangleCount(); ++t) {
        const std::uint32_t index[3] = {mesh.indices[3 * t], mesh.indices[3 * t + 1],
                                        mesh.indices[3 * t + 2]};
        const Vec p0 = toBuffer(projected[index[0]]);
        const Vec p1 = toBuffer(projected[index[1]]);
        const Vec p2 = toBuffer(projected[index[2]]);
        const double area = (p1.x - p0.x) * (p2.y - p0.y) - (p2.x - p0.x) * (p1.y - p0.y);
        if (std::abs(area) < 1e-9) {
            continue;
        }
        const double light[3] = {lightAt(index[0]), lightAt(index[1]), lightAt(index[2])};
        const int x0 = std::max(0, static_cast<int>(std::floor(std::min({p0.x, p1.x, p2.x}))));
        const int x1 =
            std::min(width - 1, static_cast<int>(std::ceil(std::max({p0.x, p1.x, p2.x}))));
        const int y0 = std::max(0, static_cast<int>(std::floor(std::min({p0.y, p1.y, p2.y}))));
        const int y1 =
            std::min(height - 1, static_cast<int>(std::ceil(std::max({p0.y, p1.y, p2.y}))));
        for (int y = y0; y <= y1; ++y) {
            for (int x = x0; x <= x1; ++x) {
                const double px = x + 0.5;
                const double py = y + 0.5;
                const double w0 = ((p1.x - px) * (p2.y - py) - (p2.x - px) * (p1.y - py)) / area;
                const double w1 = ((p2.x - px) * (p0.y - py) - (p0.x - px) * (p2.y - py)) / area;
                const double w2 = 1.0 - w0 - w1;
                if (w0 < -1e-9 || w1 < -1e-9 || w2 < -1e-9) {
                    continue;
                }
                const auto z = static_cast<float>(w0 * p0.z + w1 * p1.z + w2 * p2.z);
                const std::size_t at = static_cast<std::size_t>(y) * width + x;
                if (z <= depth[at]) {
                    continue;
                }
                depth[at] = z;
                const double lit = w0 * light[0] + w1 * light[1] + w2 * light[2];
                pixels[at] = qRgb(std::min(255, static_cast<int>(color.red() * lit)),
                                  std::min(255, static_cast<int>(color.green() * lit)),
                                  std::min(255, static_cast<int>(color.blue() * lit)));
            }
        }
    }

    // The hard edges, thin and dark, where nothing hides them: an edge is drawn when a flat face
    // beside it turns towards us (exact for the corners of boxes and the rims of cylinders; the
    // seams of round faces are left out).
    std::set<std::uint32_t> flatFacing;
    {
        std::map<std::uint32_t, Vec> firstNormal;
        std::set<std::uint32_t> curved;
        for (std::size_t t = 0; t < mesh.triangleCount(); ++t) {
            const std::uint32_t face = mesh.faceIds[t];
            for (int corner = 0; corner < 3; ++corner) {
                const geo::Point3f& n = mesh.normals[mesh.indices[3 * t + corner]];
                const Vec normal{n.x, n.y, n.z};
                const auto first = firstNormal.emplace(face, normal).first;
                if (dot(first->second, normal) < 0.99) {
                    curved.insert(face);
                }
            }
        }
        for (const auto& [face, normal] : firstNormal) {
            if (!curved.contains(face) && dot(normal, toEye) > 0.0) {
                flatFacing.insert(face);
            }
        }
    }
    const double bias = 0.01 * std::max(maxZ - minZ, 1e-9) + 0.5 / scale;
    constexpr int kEdgeRadius = 1; // 3 pixels wide at three times the size: one pixel in the end
    for (const geo::EdgePolyline& line : mesh.edges) {
        if (line.points.size() < 2 || line.faces[0] == line.faces[1] ||
            (!flatFacing.contains(line.faces[0]) && !flatFacing.contains(line.faces[1]))) {
            continue;
        }
        for (std::size_t i = 0; i + 1 < line.points.size(); ++i) {
            const Vec a = toBuffer(project(line.points[i].x, line.points[i].y, line.points[i].z));
            const Vec b =
                toBuffer(project(line.points[i + 1].x, line.points[i + 1].y, line.points[i + 1].z));
            const int steps = std::max(
                1, static_cast<int>(std::ceil(std::max(std::abs(b.x - a.x), std::abs(b.y - a.y)))));
            for (int step = 0; step <= steps; ++step) {
                const double f = static_cast<double>(step) / steps;
                const int x = static_cast<int>(std::lround(a.x + (b.x - a.x) * f));
                const int y = static_cast<int>(std::lround(a.y + (b.y - a.y) * f));
                const double z = a.z + (b.z - a.z) * f;
                for (int dy = -kEdgeRadius; dy <= kEdgeRadius; ++dy) {
                    for (int dx = -kEdgeRadius; dx <= kEdgeRadius; ++dx) {
                        const int px = x + dx;
                        const int py = y + dy;
                        if (px < 0 || py < 0 || px >= width || py >= height) {
                            continue;
                        }
                        const std::size_t at = static_cast<std::size_t>(py) * width + px;
                        if (z + bias >= depth[at] && depth[at] > -1e29F) {
                            const QRgb old = pixels[at];
                            pixels[at] = qRgb(qRed(old) * 40 / 100, qGreen(old) * 40 / 100,
                                              qBlue(old) * 40 / 100);
                        }
                    }
                }
            }
        }
    }

    // Average three by three pixels down to one; a ghost is faded as a whole (fading every
    // triangle would pile up).
    const double fade = hole ? 0.42 : 1.0;
    for (int y = 0; y < size.height(); ++y) {
        auto* row = reinterpret_cast<QRgb*>(result.scanLine(y));
        for (int x = 0; x < size.width(); ++x) {
            int red = 0, green = 0, blue = 0, covered = 0;
            for (int sy = 0; sy < kScale; ++sy) {
                for (int sx = 0; sx < kScale; ++sx) {
                    const std::size_t at =
                        static_cast<std::size_t>(y * kScale + sy) * width + x * kScale + sx;
                    if (depth[at] > -1e29F) {
                        red += qRed(pixels[at]);
                        green += qGreen(pixels[at]);
                        blue += qBlue(pixels[at]);
                        ++covered;
                    }
                }
            }
            if (covered == 0) {
                continue;
            }
            const double coverage = static_cast<double>(covered) / (kScale * kScale) * fade;
            // Premultiplied: the colours of the uncovered pixels count as nothing, so the sums
            // are already scaled by the coverage.
            const auto channel = [&](int sum) {
                return static_cast<int>(
                    std::lround(static_cast<double>(sum) / (kScale * kScale) * fade));
            };
            row[x] = qRgba(channel(red), channel(green), channel(blue),
                           static_cast<int>(std::lround(coverage * 255.0)));
        }
    }
    return result;
}

ShapePreviews::ShapePreviews(std::shared_ptr<const geo::GeometryKernel> kernel)
    : QQuickImageProvider(QQuickImageProvider::Image), m_kernel(std::move(kernel)) {}

geo::TriangleMesh ShapePreviews::meshOf(const QString& type) {
    {
        const std::lock_guard lock(m_mutex);
        const auto known = m_meshes.find(type);
        if (known != m_meshes.end()) {
            return known->second;
        }
    }
    geo::TriangleMesh mesh;
    try {
        // The same way the document makes it: a step of that type with its default values.
        core::FeatureRegistry registry;
        features::registerBuiltinFeatures(registry);
        const core::FeatureType* featureType = registry.find(type.toStdString());
        if (featureType != nullptr) {
            core::Document document;
            core::FeatureDraft draft = core::defaultDraft(*featureType);
            if (type == QLatin1String("text")) {
                draft.texts["text"] = "Aa"; // two letters read better on a tile than "Text"
                draft.params["size"].expression = "16";
                draft.params["height"].expression = "5";
            }
            document.addFeature(std::move(draft));
            core::RecomputeCache cache;
            const auto result = core::recompute(document, registry, *m_kernel, cache);
            if (result && !result->bodies.empty()) {
                mesh = m_kernel->tessellate(result->bodies.begin()->second.solid,
                                            {0.05, 0.2617993877991494});
            }
        }
    } catch (const geo::GeometryError&) {
        mesh = {}; // a placeholder is drawn
    }
    const std::lock_guard lock(m_mutex);
    return m_meshes.emplace(type, std::move(mesh)).first->second;
}

QImage ShapePreviews::render(const QString& type, bool hole, const QSize& size) {
    const geo::TriangleMesh mesh = meshOf(type);
    if (mesh.empty()) {
        QImage placeholder(size, QImage::Format_ARGB32_Premultiplied);
        placeholder.fill(Qt::transparent);
        QPainter painter(&placeholder);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(QColor(150, 156, 166), 1.5, Qt::DashLine));
        painter.drawRoundedRect(QRectF(QPointF(size.width() * 0.2, size.height() * 0.2),
                                       QPointF(size.width() * 0.8, size.height() * 0.8)),
                                6, 6);
        return placeholder;
    }
    return drawShapePicture(mesh, colorOf(type), hole, size);
}

QImage ShapePreviews::requestImage(const QString& id, QSize* size, const QSize& requestedSize) {
    const bool hole = id.endsWith(QLatin1String("/hole"));
    const QString type = hole ? id.left(id.size() - 5) : id;
    const QSize wanted = requestedSize.isValid() && !requestedSize.isEmpty()
                             ? requestedSize
                             : QSize(186, 156); // 3 x the tile, sharp on a 2x screen
    if (size != nullptr) {
        *size = wanted;
    }
    return render(type, hole, wanted);
}

} // namespace fission::ui
