// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QColor>
#include <QObject>
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
#include "fission/render/scene_renderer.h"
#include "fission/ui/document_model.h"
#include "fission/ui/snap_session.h"
#include "fission/ui/viewport_3d.h"

namespace fission::ui {

/// The gizmo of the tool "Move" (AP-1.6a, handle style "Axes"): three arrows along the axes of
/// the bed, three small squares for moving in a plane, and a dot in the middle for moving across
/// the bed. It sits above the middle of the chosen body. A body can also be moved by pressing on
/// it and dragging (`beginBodyDrag`), in every tool but "Select".
///
/// Moving changes the position values of the step that made the body (ADR-0010): one drag (or
/// typing, or both) is one step in the history, Esc takes it back. While dragging, the position
/// snaps to the corners, edge middles, circle centres and face middles of the other bodies and
/// of the bed, and otherwise to the grid of the moment (whole millimetres, finer when zoomed in);
/// Shift switches the snapping off (AP-1.6c). Typing replaces dragging: the number is how
/// far to move along an axis, Tab goes on from X to Y to Z.
///
/// Like the size handles, the gizmo is drawn in the scene (`Viewport3D::setHandleGraphics`):
/// what a body hides of it is faint. The window holds the letters and the places to grab.
class GizmoController : public QObject {
    Q_OBJECT
    /// Shown at all: the tool "Move" is on. (Moving a body by dragging it does not need this.)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    /// The parts on screen (view pixels), empty when nothing is shown:
    /// [{ part, shown, axis, x, y, midX, midY, labelX, labelY }], always all seven parts in the
    /// same order (the window keeps one place to grab for each). `part` is x | y | z (arrows: (x,
    /// y) is the head, (midX, midY) the middle of the shaft, (labelX, labelY) the place of the
    /// letter), xy | xz | yz (squares: (x, y) is the middle) or free (the dot). `axis` is the
    /// colour: x | y | z, or none for the dot. A part seen end on (an arrow pointing at the
    /// viewer, a square seen edge on) is not `shown`, unless it is the one being dragged.
    Q_PROPERTY(QVariantList parts READ parts NOTIFY partsChanged)
    /// A drag or typing is going on.
    Q_PROPERTY(bool editing READ editing NOTIFY editingChanged)
    /// What the drag moves: x | y | z | xy | xz | yz | free | body (empty when nothing).
    Q_PROPERTY(QString moving READ moving NOTIFY editingChanged)
    /// How far the body has moved since the drag began, [x, y, z] in mm.
    Q_PROPERTY(QVariantList offset READ offset NOTIFY offsetChanged)
    /// Where the value bubble goes: next to the middle dot, in view pixels { x, y }.
    Q_PROPERTY(QVariantMap bubbleAt READ bubbleAt NOTIFY partsChanged)
    /// Colours from the theme: { x, y, z (axes), ink (the ring of the dot), fill (inside of it) }.
    Q_PROPERTY(QVariantMap colours READ colours WRITE setColours NOTIFY coloursChanged)

public:
    explicit GizmoController(DocumentModel* model, QObject* parent = nullptr);

    void attach(Viewport3D* viewport);

    bool enabled() const noexcept { return m_enabled; }
    void setEnabled(bool enabled);
    QVariantList parts() const { return m_parts; }
    bool editing() const noexcept { return m_editing; }
    QString moving() const { return m_moving; }
    QVariantList offset() const;
    QVariantMap bubbleAt() const { return m_bubbleAt; }
    QVariantMap colours() const { return m_colourMap; }
    void setColours(const QVariantMap& colours);

    /// Positions are worked out again (the camera moved, the body changed).
    Q_INVOKABLE void refresh();
    /// The pointer is on a part (`on`), or left it: the part is drawn stronger.
    Q_INVOKABLE void setPointed(const QString& part, bool on);

    // --- dragging ---
    /// The pointer went down on a part of the gizmo at (x, y) in view pixels.
    Q_INVOKABLE bool beginDrag(const QString& part, double x, double y);
    /// The pointer went down on the chosen body itself at (x, y) and is dragged: the body moves
    /// across a level plane through the point where it was grabbed (like in Tinkercad).
    Q_INVOKABLE bool beginBodyDrag(double x, double y);
    /// The pointer moved; the body follows.
    Q_INVOKABLE bool dragTo(double x, double y);
    Q_INVOKABLE bool endDrag();
    Q_INVOKABLE void cancelDrag();
    /// Snapping is off for the drag in progress (as with Shift held). For tests and the
    /// developer option `--simulate`: the real key is read from the keyboard.
    Q_INVOKABLE void setSnapSuppressed(bool suppressed) { m_snapSuppressed = suppressed; }

    // --- typing ---
    /// A digit was typed while dragging: the drag turns into typing, still the same step.
    Q_INVOKABLE bool typeInsteadOfDrag();
    /// How far to move along `axis` (0 = x, 1 = y, 2 = z), in mm from where the drag began.
    Q_INVOKABLE bool setOffset(int axis, double millimetres);
    Q_INVOKABLE bool commitEdit();
    Q_INVOKABLE void cancelEdit();

    // --- keys ---
    /// Moves the chosen body by one step with the arrow keys: `right` and `away` are -1, 0 or 1
    /// on the screen (left/right, towards/away from the viewer on the bed), `up` lifts or lowers.
    /// The way on screen is turned into the axis of the bed closest to it. `large`: 10 mm instead
    /// of 1 mm. One step in the history.
    Q_INVOKABLE bool nudge(int right, int away, int up, bool large);

    /// No position goes beyond this (mm) in any direction: a slip of the hand must not throw a
    /// body out of sight.
    static constexpr double kFarthestMillimetres = 2000.0;
    /// Arrow length on screen (pixels) and on a small body.
    static constexpr double kArrowPixels = 84.0;
    static constexpr double kSmallArrowPixels = 52.0;
    /// Below this size on screen (diagonal of the body, pixels) the arrows are short.
    static constexpr double kSmallBodyPixels = 120.0;
    /// A square seen more edge on than this (sine of the angle) is left out.
    static constexpr double kLeastSquareSight = 0.25;
    /// An arrow closer than this to the line of sight (sine of the angle) is left out: on the
    /// screen it is hardly more than a dot. (A held arrow stays; it can still be dragged.)
    static constexpr double kLeastArrowSight = 0.2;

signals:
    void enabledChanged();
    void partsChanged();
    void editingChanged();
    void offsetChanged();
    void coloursChanged();

private:
    struct Box {
        QVector3D low;
        QVector3D high;
    };
    /// The box round the body of the chosen step as last calculated (mm).
    std::optional<Box> bodyBox();
    /// Where the gizmo sits: a little above the middle of the top of the body.
    std::optional<QVector3D> anchor();
    /// The world size of one pixel at `point` (mm), across the line of sight.
    double millimetresPerPixel(const QVector3D& point) const;
    bool startMove(const QString& moving, const QVector3D& anchorPoint);
    /// Whether the move in progress changes the position along `axis` (0 = x).
    bool moves(int axis) const;
    /// Puts the body at `target` (only the axes the move changes are written).
    bool moveTo(std::array<double, 3> target);
    void setEditing(bool editing, const QString& moving);
    /// Shift is held, or snapping was switched off for tests.
    bool snappingOff() const;
    /// How far (mm) a point may be from another on each axis and still catch: a fixed number of
    /// pixels on the screen, so it depends on how the axis is seen at `around`.
    interaction::Vec3 snapReach(const QVector3D& around) const;
    /// Takes the catch away: marks and grid of the drag are gone.
    void clearSnap();
    /// Gets the points to catch on for the drag that begins.
    void beginSnap();

    QPointer<DocumentModel> m_model;
    QPointer<Viewport3D> m_viewport;
    bool m_enabled = false;
    QVariantList m_parts;
    QVariantMap m_bubbleAt;
    QString m_pointed;
    bool m_editing = false;
    bool m_dragging = false;
    QString m_moving;
    struct Colours {
        QColor x{0xE5, 0x53, 0x4B};
        QColor y{0x57, 0xAB, 0x5A};
        QColor z{0x5B, 0x97, 0xFF};
        QColor ink{0x16, 0x18, 0x1B};
        QColor fill{0xFF, 0xFF, 0xFF};
    } m_colours;
    QVariantMap m_colourMap;

    // The box of the body, kept until its mesh changes.
    std::string m_boxBody;
    std::uint64_t m_boxVersion = 0;
    Box m_box;

    // The move in progress.
    std::array<double, 3> m_startPosition{}; ///< the position values when it began
    QVector3D m_startAnchor;                 ///< where the gizmo sat when it began
    std::array<double, 3> m_offset{};        ///< how far it has moved (mm)
    double m_pixelsPerMillimetre = 1.0;
    double m_axisPixelsPerMillimetre = 1.0; ///< the rate of the arrow that was grabbed
    std::optional<interaction::AxisDrag> m_axisDrag;
    std::optional<interaction::PlaneDrag> m_planeDrag;

    // Snapping (AP-1.6c)
    SnapSession m_snap;
    render::HandleGraphics m_snapMarks;
    bool m_snapSuppressed = false;
};

} // namespace fission::ui
