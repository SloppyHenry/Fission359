// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QColor>
#include <QObject>
#include <QPointF>
#include <QPointer>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QVector3D>

#include <array>
#include <cstdint>
#include <optional>
#include <string>

#include "fission/interaction/drag.h"
#include "fission/ui/document_model.h"
#include "fission/ui/viewport_3d.h"

namespace fission::ui {

/// The corner handles of the tool "Size" (AP-1.6b2, docs/02-ux-konzept.md §7.2): a square on each
/// of the four top corners of the box round the chosen body, its edges in the directions of the
/// bed (for a round or a turned body the corners float beside it, and the box is drawn as a
/// dashed line). Pulling a corner makes the body larger or smaller in all directions alike: the
/// corner across from it at the bottom stays where it is, so the body stays on the bed, and the
/// corner that is held stays under the pointer.
///
/// Every size of the shape changes by the same factor: its values, never the geometry (rule 11).
/// A box with 5 mm corners at 200 % has 10 mm corners, round. The factor rests on whole percent;
/// typing replaces dragging (a number is the percentage), Enter accepts, Esc puts it back. Where a
/// size would become impossible, the corner stops at the nearest possible factor. One drag is
/// one step in the history ("Box made larger to 150 %"), no new step in the recipe.
///
/// Like the rings, the handles are drawn in the scene with depth (`Viewport3D::setHandleGraphics`);
/// the window holds the places to grab and the value bubble.
class ScaleController : public QObject {
    Q_OBJECT
    /// Shown at all: the tool "Size" is on.
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    /// The four corners, always in the same order (front left, front right, back right, back
    /// left; the window keeps one place to grab for each): [{ corner, shown, x, y }] in view
    /// pixels.
    Q_PROPERTY(QVariantList corners READ corners NOTIFY cornersChanged)
    /// A drag or typing is going on.
    Q_PROPERTY(bool editing READ editing NOTIFY editingChanged)
    /// How large the body is now, compared with when the drag began (1.5 = 150 %).
    Q_PROPERTY(double factor READ factor NOTIFY factorChanged)
    /// The size of the box round the body now, [x, y, z] in mm.
    Q_PROPERTY(QVariantList size READ size NOTIFY factorChanged)
    /// Where the value bubble goes, in view pixels { x, y }, and the corner held it belongs to
    /// { cornerX, cornerY }.
    Q_PROPERTY(QVariantMap bubbleAt READ bubbleAt NOTIFY cornersChanged)
    Q_PROPERTY(QVariantMap colours READ colours WRITE setColours NOTIFY coloursChanged)

public:
    explicit ScaleController(DocumentModel* model, QObject* parent = nullptr);

    void attach(Viewport3D* viewport);

    bool enabled() const noexcept { return m_enabled; }
    void setEnabled(bool enabled);
    QVariantList corners() const { return m_corners; }
    bool editing() const noexcept { return m_editing; }
    double factor() const noexcept { return m_factor; }
    QVariantList size() const;
    QVariantMap bubbleAt() const { return m_bubbleAt; }
    QVariantMap colours() const { return m_colourMap; }
    void setColours(const QVariantMap& colours);

    /// Positions are worked out again (the camera moved, the body changed).
    Q_INVOKABLE void refresh();
    /// The pointer is on corner `corner` (`on`) or left it: it is drawn stronger.
    Q_INVOKABLE void setPointed(int corner, bool on);

    // --- dragging ---
    /// The pointer went down on corner `corner` (0 to 3) at (x, y) in view pixels.
    Q_INVOKABLE bool beginDrag(int corner, double x, double y);
    Q_INVOKABLE bool dragTo(double x, double y);
    Q_INVOKABLE bool endDrag();
    Q_INVOKABLE void cancelDrag();

    // --- typing ---
    /// A digit was typed while dragging: the drag turns into typing, still the same step.
    Q_INVOKABLE bool typeInsteadOfDrag();
    /// How large the body is to be, in percent of its size when the drag began. False (nothing
    /// changes) for a size the shape cannot have.
    Q_INVOKABLE bool setPercent(double percent);
    Q_INVOKABLE bool commitEdit();
    Q_INVOKABLE void cancelEdit();

    /// The factor rests on whole percent.
    static constexpr double kFactorStep = 0.01;
    /// Half the size of a corner square (pixels) and how far around it counts as on it.
    static constexpr float kCornerPixels = 6.0F;

signals:
    void enabledChanged();
    void cornersChanged();
    void editingChanged();
    void factorChanged();
    void coloursChanged();

private:
    struct Box {
        QVector3D low;
        QVector3D high;
    };
    /// The box round the body as drawn (from its mesh), kept until the mesh changes.
    std::optional<Box> meshBox();
    /// The box shown now: while dragging, the box of the start grown about the anchor.
    std::optional<Box> shownBox();
    static QVector3D topCorner(const Box& box, int corner);
    bool applyFactor(double factor);
    void setEditing(bool editing);
    void finish();

    QPointer<DocumentModel> m_model;
    QPointer<Viewport3D> m_viewport;
    bool m_enabled = false;
    QVariantList m_corners;
    QVariantMap m_bubbleAt;
    int m_pointed = -1;
    bool m_editing = false;
    bool m_dragging = false;
    struct Colours {
        QColor handle{0xFF, 0xFF, 0xFF};
        QColor ink{0x2A, 0x2E, 0x35};
        QColor line{0x9A, 0xA0, 0xA8};
        QColor accent{0x4C, 0x8D, 0xFF};
    } m_colours;
    QVariantMap m_colourMap;

    // The box of the body from its mesh, kept until the mesh changes.
    std::string m_boxBody;
    std::uint64_t m_boxVersion = 0;
    Box m_box;

    // The drag in progress.
    int m_corner = -1;       ///< the corner held
    Box m_startBox;          ///< the exact box round the body when it began
    geo::Point3d m_anchor;   ///< the corner across, at the bottom: it stays where it is (mm)
    double m_diagonal = 0.0; ///< from the anchor to the corner held (mm)
    double m_factor = 1.0;   ///< what was written last
    std::optional<interaction::AxisDrag> m_drag;
};

} // namespace fission::ui
