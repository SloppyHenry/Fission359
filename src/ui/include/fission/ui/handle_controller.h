// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QColor>
#include <QObject>
#include <QPointF>
#include <QPointer>
#include <QVariantList>
#include <QVariantMap>
#include <QVector3D>

#include <cstdint>
#include <map>
#include <optional>
#include <string>

#include "fission/interaction/drag.h"
#include "fission/render/scene_renderer.h"
#include "fission/ui/document_model.h"
#include "fission/ui/snap_session.h"
#include "fission/ui/viewport_3d.h"

namespace fission::ui {

/// The size handles of the chosen shape (AP-1.5): points on the shape that change one value when
/// dragged (rule 1) and can be typed instead (rule 2). A handle changes the value, never scales the
/// geometry (rule 11): a box with rounded corners that is pulled wider keeps its 5 mm corners.
///
/// The controller turns what the shape says (`FeatureType::handles`, in mm) into screen points,
/// turns a drag of the pointer into a value, and changes the document through `DocumentModel`.
/// Everything between `beginEdit` and `commitEdit` (a drag, or typing with Tab from value to
/// value) is one step in the history; `cancelEdit` (Esc) takes it all back.
///
/// A handle that moves a face along a bed axis (AP-1.6c2) catches on the corners, edge middles and
/// face middles of other bodies and of the bed, before it falls back on the grid; Shift switches
/// catching and grid off. A turned body, a sphere and the details (corner radius, wall) have only
/// the grid.
class HandleController : public QObject {
    Q_OBJECT
    /// The handles on screen, one for each value, in the order they are shown (view pixels):
    /// [{ index, candidate, parameter, kind, role, axis, x, y, startX, startY, band, bandStartX,
    /// bandStartY, bandEndX, bandEndY, edgeStartX, edgeStartY, edgeEndX, edgeEndY, chipX, chipY,
    /// dirX, dirY, value, pixelsPerMillimetre }].
    /// The dot (x, y) sits on the shape, on the face it moves; (startX, startY) is the other end
    /// of what it measures, and (dirX, dirY) the way it grows on screen (a unit vector). A main
    /// size along an axis has a measure band like in a drawing (`band`): it lies outside the
    /// body, on the bed in front of it or beside its outline, from bandStart to bandEnd, with
    /// lines from the points it measures (edgeStart, edgeEnd). `chipX`, `chipY` is where the
    /// label goes: on the band, or beside the dot when there is none. `role` is size | detail |
    /// rounding, `axis` is x | y | z | none (the colour of the handle). `candidate` is the place
    /// in `DocumentModel::stepHandles`. `hidden`: a body is in front of its band (or its dot), so
    /// its label steps back. Empty when nothing is shown.
    ///
    /// The dots, bands and arrows themselves are drawn in the scene (`Viewport3D::
    /// setHandleGraphics`): what a body hides of them is drawn faint. The labels and the places to
    /// grab are left to the window.
    Q_PROPERTY(QVariantList handles READ handles NOTIFY handlesChanged)
    /// A drag or typing is going on.
    Q_PROPERTY(bool editing READ editing NOTIFY editingChanged)
    /// Digits start typing: a shape was just put down and nothing else was done since.
    Q_PROPERTY(bool armed READ armed WRITE setArmed NOTIFY armedChanged)
    /// The body is small on screen: only the dots are shown, a band and its label when the pointer
    /// is on the dot.
    Q_PROPERTY(bool compact READ compact NOTIFY compactChanged)
    /// The colours of the handles, from the theme: { x, y, z (axes), plain (lines without an
    /// axis), ink (dots and arrows without an axis), fill (inside of a dot, light line beneath) }.
    Q_PROPERTY(QVariantMap colours READ colours WRITE setColours NOTIFY coloursChanged)
    /// What is shown, after the tool of the palette (AP-1.6): `size` (tool "Size": dots to drag,
    /// bands and labels), `labels` (tool "Move": the bands and their labels only, which can still
    /// be clicked to type a size; the dots would sit where the arrows of the gizmo are) or `off`.
    Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY modeChanged)

public:
    explicit HandleController(DocumentModel* model, QObject* parent = nullptr);

    void attach(Viewport3D* viewport);

    QVariantList handles() const { return m_handles; }
    bool editing() const noexcept { return m_editing; }
    bool armed() const noexcept { return m_armed; }
    bool compact() const noexcept { return m_compact; }
    QVariantMap colours() const { return m_colourMap; }
    void setColours(const QVariantMap& colours);
    QString mode() const { return m_mode; }
    void setMode(const QString& mode);
    /// "axes" (round dots, a measure band beside every size) or "classic" (white squares; a
    /// measure band and its box appear when the pointer is on a square, as in Tinkercad). Both
    /// drag the same values with the same commands (AP-1.11f).
    Q_PROPERTY(QString style READ style WRITE setStyle NOTIFY styleChanged)
    QString style() const { return m_style; }
    void setStyle(const QString& style);

    /// The pointer is on handle `index` (`on`), or left it: its band, arrow and label show.
    Q_INVOKABLE void setPointed(int index, bool on);
    /// The handle that is dragged or typed (-1: none).
    Q_INVOKABLE void setCurrent(int index);
    void setArmed(bool armed);

    /// Positions are worked out again (the camera moved, the shape changed).
    Q_INVOKABLE void refresh();

    // --- dragging ---
    /// The pointer went down on handle `index` at (x, y) in view pixels.
    Q_INVOKABLE bool beginDrag(int index, double x, double y);
    /// The pointer moved. Returns the value the handle has now.
    Q_INVOKABLE double dragTo(double x, double y);
    Q_INVOKABLE bool endDrag();
    Q_INVOKABLE void cancelDrag();

    // --- typing ---
    /// A digit was typed while a handle is dragged: the drag turns into typing (rule 2: typing
    /// replaces dragging). The step in the history stays the same one. False if nothing is dragged.
    Q_INVOKABLE bool typeInsteadOfDrag();
    /// Starts changing values by typing (one step in the history until `commitEdit`).
    Q_INVOKABLE bool beginEdit(int index);
    /// Sets the value of handle `index` to what was typed (a number or a formula).
    Q_INVOKABLE bool setValue(int index, const QString& text);
    Q_INVOKABLE bool commitEdit();
    Q_INVOKABLE void cancelEdit();

    /// The step of a value: whole millimetres, finer when zoomed in, coarser when far away.
    static double snapStep(double pixelsPerMillimetre);
    /// No length of a shape is taken beyond this (mm): a slip of the hand must not make a body
    /// the size of a house.
    static constexpr double kLargestMillimetres = 2000.0;
    /// A handle whose axis is closer than this (sine of the angle, 0.5 = 30 degrees) to the line
    /// of sight cannot be dragged: the pointer would have to go ever so far for a little change.
    /// (Typing and turning the view still work.)
    static constexpr double kLeastSight = interaction::kLeastSight;
    /// Below this size on screen (diagonal of the body, pixels) the handles are `compact`.
    static constexpr double kCompactPixels = 120.0;

signals:
    void handlesChanged();
    void editingChanged();
    void armedChanged();
    void compactChanged();
    void coloursChanged();
    void modeChanged();
    void styleChanged();

private:
    void setEditing(bool editing);
    /// Prepares catching for the handle grabbed (`m_dragAxis`, `m_dragOrigin` are set).
    void beginSnap(const QVariantMap& world);
    /// Ends catching: marks and grid of the view go.
    void clearSnap();
    /// The value the handle takes where it catches on something, or nothing. `wanted` is the
    /// value the pointer asks for (not rounded).
    std::optional<double> snappedValue(double wanted);
    /// The box round the body of the current step as last calculated (mm); false if none.
    bool bodyBounds(QVector3D& low, QVector3D& high);
    /// Whether a solid body is between the eye and `point`.
    bool hiddenFromEye(const QVector3D& point, const QVector3D& eye);
    /// Adds the dot, band, line and arrow of the shown handle `index` to `graphics`.
    void drawHandle(render::HandleGraphics& graphics, const QVector3D& at, const QVector3D& start,
                    const QVector3D& axisWorld, bool band, const QVector3D& edgeStart,
                    const QVector3D& edgeEnd, const QVector3D& bandStart, const QVector3D& bandEnd,
                    const QVector3D& tick, int index, const QString& axis, const QString& role,
                    bool compact) const;

    // Room on screen (pixels): the size of a label, how far a band lies off the body beyond what
    // its label needs, how far a label sits off its dot, the shortest band worth drawing.
    static constexpr double kChipWidth = 76.0;
    static constexpr double kChipHeight = 24.0;
    static constexpr double kBandGap = 10.0;
    static constexpr double kDotGap = 16.0;
    static constexpr double kShortestBand = 14.0;
    static constexpr double kTickPixels = 5.0; ///< half the stroke across the end of a band
    static constexpr float kCalm = 0.35F;      ///< strength of the others while one is changed

    QPointer<DocumentModel> m_model;
    QPointer<Viewport3D> m_viewport;
    QVariantList m_handles;
    std::map<QString, int> m_chosen; ///< the place of each value while it is being changed
    bool m_keepChoice = false;       ///< after a drag: the places stay until the view turns
    QString m_mode{QStringLiteral("size")};
    QString m_style{QStringLiteral("axes")};
    bool m_editing = false;
    bool m_armed = false;
    bool m_compact = false;
    int m_pointed = -1;
    int m_current = -1;
    struct Colours {
        QColor x{0xE5, 0x53, 0x4B};
        QColor y{0x57, 0xAB, 0x5A};
        QColor z{0x5B, 0x97, 0xFF};
        QColor plain{0xB8, 0xBC, 0xC4};
        QColor ink{0x16, 0x18, 0x1B};
        QColor fill{0xFF, 0xFF, 0xFF};
    } m_colours;
    QVariantMap m_colourMap;
    /// The boxes round the bodies, for `hiddenFromEye`, by body and mesh.
    struct Box {
        std::uint64_t version = 0;
        bool empty = true;
        QVector3D low{1e30F, 1e30F, 1e30F};
        QVector3D high{-1e30F, -1e30F, -1e30F};
    };
    std::map<std::string, Box> m_boxes;
    std::string m_boundsBody; ///< whose box `m_boundsLow`/`m_boundsHigh` are, and of which mesh
    std::uint64_t m_boundsVersion = 0;
    QVector3D m_boundsLow;
    QVector3D m_boundsHigh;
    int m_armedStep = -1;

    // The drag in progress. The pointer is a ray into the scene; it is laid on a plane that holds
    // the axis of the handle and faces the viewer, and the handle follows where the ray meets it.
    bool m_dragging = false;
    int m_dragIndex = -1;      ///< the handle as shown (position in `handles`)
    double m_dragExtent = 0.0; ///< the length it measured when grabbed
    QVector3D m_dragOrigin;    ///< where the handle sat when it was grabbed (mm)
    QVector3D m_dragAxis;      ///< unit vector along which it moves
    /// The pointer laid on the axis; empty while the axis is (nearly) along the line of sight.
    std::optional<interaction::AxisDrag> m_axisDrag;
    double m_dragPixelsPerMillimetre = 1.0;
    double m_dragStartValue = 0.0;
    double m_dragSign = 1.0;
    double m_dragTravel = 1.0;
    double m_dragValue = 0.0;
    QString m_dragParameter;

    // Catching (AP-1.6c2): the face the handle moves, on a bed axis. `m_snapAxis` is -1 for a
    // handle that only has the grid.
    SnapSession m_snap;
    int m_snapAxis = -1;
    render::HandleGraphics m_snapMarks;
};

} // namespace fission::ui
