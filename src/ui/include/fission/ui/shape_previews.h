// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QImage>
#include <QQuickImageProvider>

#include <map>
#include <memory>
#include <mutex>

#include "fission/geo/kernel.h"
#include "fission/geo/mesh.h"

namespace fission::ui {

/// The pictures on the tiles of the shape gallery (AP-1.5): the real shape, made by the geometry
/// kernel with its default sizes and drawn from the front left above, like the pictures in the
/// mockup. They come as `image://shapes/<type>` (e.g. `image://shapes/sphere`); with a
/// `/hole` behind the name the shape is drawn as a see-through ghost.
///
/// The pictures are made when first asked for, in a thread of their own, and then remembered.
class ShapePreviews : public QQuickImageProvider {
public:
    explicit ShapePreviews(std::shared_ptr<const geo::GeometryKernel> kernel);

    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;

    /// The shape drawn into an image of the given size (tests, other callers). A shape the kernel
    /// cannot make gives a plain placeholder.
    QImage render(const QString& type, bool hole, const QSize& size);

    /// The mesh of the shape with its default sizes; empty if the kernel could not make it.
    geo::TriangleMesh meshOf(const QString& type);

private:
    std::shared_ptr<const geo::GeometryKernel> m_kernel;
    std::mutex m_mutex;
    std::map<QString, geo::TriangleMesh> m_meshes;
};

/// Draws a mesh from the front left above (flat shaded, painter's algorithm) in `color`. Public
/// for the test.
QImage drawShapePicture(const geo::TriangleMesh& mesh, const QColor& color, bool hole,
                        const QSize& size);

} // namespace fission::ui
