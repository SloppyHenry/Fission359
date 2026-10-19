// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QPointer>
#include <QString>

#include "fission/ui/document_model.h"
#include "fission/ui/viewport_3d.h"

namespace fission::ui {

/// Puts shapes down where the pointer is (AP-1.5): the shape follows the pointer over the print
/// bed and over the flat tops of other bodies and lands on the surface under it, which becomes the
/// work plane. Over a wall, a slope or a curve (AP-1.6b3) its underside lies on that face and it
/// stands out of it; the turn is written into the step as plain angles. With `upright` (the Alt
/// key) it ignores such faces and stays upright on the work plane, as it did before. Beside the
/// bed it stays on the work plane.
///
/// The pointer position (view pixels) is turned into millimetres here, the document is changed
/// through `DocumentModel`. Everything between `begin` and `finish` is one step in the history;
/// `cancel` (Esc) leaves nothing behind.
class PlacementController : public QObject {
    Q_OBJECT
    /// A shape is following the pointer.
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)

public:
    explicit PlacementController(DocumentModel* model, QObject* parent = nullptr);

    void attach(Viewport3D* viewport) { m_viewport = viewport; }
    bool active() const { return m_model != nullptr && m_model->placing(); }

    /// Lengths snap to whole millimetres on the surface (the finer fitting comes with AP-1.6).
    static constexpr double kSnapMillimetres = 1.0;

    /// Starts putting a shape of `type` down at the pointer (view pixels). False if the pointer
    /// is not over anything a shape can stand on.
    Q_INVOKABLE bool begin(const QString& type, bool asHole, double x, double y,
                           bool upright = false);
    /// The pointer moved.
    Q_INVOKABLE void moveTo(double x, double y, bool upright = false);
    /// The pointer was let go: the shape stays.
    Q_INVOKABLE bool finish();
    /// Esc, or the pointer left the view: the shape is taken back.
    Q_INVOKABLE void cancel();
    /// A click on a tile: the shape goes to a free place on the bed.
    Q_INVOKABLE int add(const QString& type, bool asHole);

signals:
    void activeChanged();

private:
    bool landAt(double x, double y, bool upright);

    QPointer<DocumentModel> m_model;
    QPointer<Viewport3D> m_viewport;
};

} // namespace fission::ui
