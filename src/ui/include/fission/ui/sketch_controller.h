// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QColor>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVariantMap>

#include <array>
#include <optional>
#include <vector>

#include "fission/core/sketch.h"
#include "fission/interaction/sketch_edit.h"
#include "fission/interaction/sketch_offset.h"
#include "fission/interaction/sketch_pattern.h"
#include "fission/interaction/sketch_snap.h"
#include "fission/interaction/sketch_tool.h"
#include "fission/render/camera.h"

namespace fission::ui {

class DocumentModel;
class SelectionController;
class Viewport3D;

/// The sketch mode (AP-2.1b, ADR-0013): choosing the plane to draw on, being in the sketch, and
/// leaving it again.
///
///  - `start()` (the button "Sketch"): the next click on a flat face, or on the bed, makes a new
///    sketch step on it. A click on a curved face makes no step and says why (`notice`).
///  - In the sketch: the view looks squarely at the plane, the other bodies are pale, a grid lies
///    on the plane and the pointer catches on the sketch (a marker shows where). The drawing
///    tools (AP-2.1c1: line, rectangle, circle, point, helper line) are `chooseTool`; the logic of
///    each is `interaction::SketchTool`, this class wires it to the view and the document.
///  - `finish()` (the button "Done", Esc): an empty sketch is removed again (a normal command), the
///    view returns to where it was.
/// The controller changes the document only through `DocumentModel` (commands).
class SketchController : public QObject {
    Q_OBJECT
    /// Waiting for the click that chooses the plane.
    Q_PROPERTY(bool picking READ picking NOTIFY stateChanged)
    /// In a sketch (the plane may still be being calculated, see `planeReady`).
    Q_PROPERTY(bool active READ active NOTIFY stateChanged)
    /// Picking or in a sketch: Esc, the hint line and the view belong to the sketch mode.
    Q_PROPERTY(bool engaged READ engaged NOTIFY stateChanged)
    /// A drawing tool is on; Esc ends it before it ends the sketch.
    Q_PROPERTY(bool toolActive READ toolActive NOTIFY toolChanged)
    /// The tool: "" (none), "line", "rectangle", "circle", "point", "arc", "slot", "polygon",
    /// "spline" (the curve), "trim" or "extend" (AP-2.1e1), "offset" or "fillet" (AP-2.1e2:
    /// variant 0 rounds a corner, 1 chamfers it), "mirror" or "pattern" (AP-2.1e3: variant 0 a row,
    /// 1 round a point); and
    /// its variant (rectangle: 0 two corners, 1 middle and corner; circle: 0 middle and radius,
    /// 1 three points; arc: 0 three points, 1 middle, start and end; polygon: 0 corner, 1 middle
    /// of a side).
    Q_PROPERTY(QString tool READ toolName NOTIFY toolChanged)
    Q_PROPERTY(int toolVariant READ toolVariant NOTIFY toolChanged)
    /// The switch "Helper line": what is drawn is an aid (dashed). Alt does the same while held.
    Q_PROPERTY(bool helperLine READ helperLine WRITE setHelperLine NOTIFY toolChanged)
    /// An element is being pulled (the first click was made).
    Q_PROPERTY(bool drawing READ drawing NOTIFY draftChanged)
    /// How many clicks of the element being pulled are made (0 when none): the hint line.
    Q_PROPERTY(int placed READ placed NOTIFY draftChanged)
    /// An arc is being pulled from the end of the last line (AP-2.1c2): the hint line.
    Q_PROPERTY(bool pullingArc READ pullingArc NOTIFY draftChanged)
    /// The numbers of the element being pulled: { valid, x, y (item pixels, where they belong),
    /// fields: [{ name, value, typed }] } with the names "length", "angle", "width", "height",
    /// "radius", "sides"; just { valid: false } when nothing is pulled.
    Q_PROPERTY(QVariantMap readout READ readout NOTIFY draftChanged)
    /// The plane of the sketch is known, grid and catching work.
    Q_PROPERTY(bool planeReady READ planeReady NOTIFY stateChanged)
    /// The id of the step being sketched in, empty when none.
    Q_PROPERTY(QString step READ step NOTIFY stateChanged)
    /// Where the pointer catches: { valid, u, v, kind (none | point | middle | alignment | grid),
    /// element, guides }; just { valid: false } when it is not over the plane.
    Q_PROPERTY(QVariantMap snap READ snap NOTIFY snapChanged)
    /// The distance (mm) of the grid lines now, 0 without a plane.
    Q_PROPERTY(double gridStep READ gridStep NOTIFY snapChanged)
    Q_PROPERTY(QColor gridColour MEMBER m_gridColour NOTIFY coloursChanged)
    Q_PROPERTY(QColor gridMajorColour MEMBER m_gridMajorColour NOTIFY coloursChanged)
    Q_PROPERTY(QColor elementColour MEMBER m_elementColour NOTIFY coloursChanged)
    Q_PROPERTY(QColor guideColour MEMBER m_guideColour NOTIFY coloursChanged)
    Q_PROPERTY(QColor markerColour MEMBER m_markerColour NOTIFY coloursChanged)
    Q_PROPERTY(QColor draftColour MEMBER m_draftColour NOTIFY coloursChanged)
    /// The piece that trimming would take away (AP-2.1e1).
    Q_PROPERTY(QColor trimColour MEMBER m_trimColour NOTIFY coloursChanged)
    /// A number that belongs to the last `notice` (the biggest radius that fits, in mm).
    Q_PROPERTY(double noticeNumber READ noticeNumber NOTIFY noticeNumberChanged)

public:
    SketchController(DocumentModel* model, SelectionController* selection,
                     QObject* parent = nullptr);

    void attach(Viewport3D* viewport);

    bool picking() const noexcept { return m_mode == Mode::picking; }
    bool active() const noexcept { return m_mode == Mode::sketching; }
    bool engaged() const noexcept { return m_mode != Mode::idle; }
    bool toolActive() const noexcept {
        return m_mode == Mode::sketching && m_tool.kind() != interaction::SketchToolKind::none;
    }
    QString toolName() const;
    int toolVariant() const noexcept { return m_tool.variant(); }
    bool helperLine() const noexcept { return m_tool.helperLine(); }
    void setHelperLine(bool on);
    bool drawing() const noexcept { return toolActive() && (m_tool.busy() || editDrafting()); }
    int placed() const noexcept {
        if (copyTool()) {
            return copyPlaced();
        }
        return editDrafting() && m_tool.kind() == interaction::SketchToolKind::offset
                   ? 1
                   : static_cast<int>(m_tool.placedCount());
    }
    double noticeNumber() const noexcept { return m_noticeNumber; }
    bool pullingArc() const noexcept { return drawing() && m_tool.dragging(); }
    QVariantMap readout() const { return m_readout; }
    bool planeReady() const noexcept { return m_mode == Mode::sketching && m_planeReady; }
    QString step() const { return m_step; }
    QVariantMap snap() const { return m_snapMap; }
    double gridStep() const noexcept { return m_gridStep; }

    /// Starts choosing the plane. Does nothing while a sketch mode is on.
    Q_INVOKABLE void start();
    /// The click on a flat face of a body (`body`, face `number`) or on the bed. Return true when a
    /// sketch step was made and the sketch mode is on.
    Q_INVOKABLE bool pickFace(const QString& body, int number);
    Q_INVOKABLE bool pickGround();
    /// "Done": ends the sketch mode. An empty sketch disappears.
    Q_INVOKABLE void finish();
    /// Esc: ends a tool, else the sketch, else the choosing of the plane.
    Q_INVOKABLE void escape();
    /// Double click on a sketch step in the recipe: opens the sketch mode on it. False (with a
    /// `notice`) when the step has no plane, e.g. because its face cannot be found any more.
    Q_INVOKABLE bool reopen(int stepIndex);

    /// The pointer is over the plane along this line of sight; `pixelsPerMillimetre` is the zoom.
    /// What the pointer catches shows as the marker and the aid lines. (The view calls this when
    /// the pointer moves; tests call it directly.)
    void pointAt(const interaction::PlaneRay& ray, double pixelsPerMillimetre);
    /// The pointer left the plane.
    void pointerGone();

    // --- drawing (AP-2.1c1) ---
    /// Chooses a tool by its name ("line", "rectangle", "circle", "point"; "" for none) and
    /// variant (-1: the one used last with that tool). Does nothing outside a sketch. Choosing
    /// the tool that is on drops the element being pulled. The tool stays on after each element.
    Q_INVOKABLE void chooseTool(const QString& name, int variant = -1);
    Q_INVOKABLE void toggleHelperLine();
    /// A click on the plane along this line of sight (`alt`: the key that makes a helper line is
    /// held). The point is the one the pointer catches (Shift off). The view calls this for each
    /// click; tests call it directly.
    void clickAt(const interaction::PlaneRay& ray, double pixelsPerMillimetre, bool alt = false);
    /// A double click: ends a chain of lines.
    Q_INVOKABLE void doubleClick();
    /// The left button goes down / the pointer moves with it held / it goes up (AP-2.1c2). Pressed
    /// on the end of the last line of a chain and pulled on (more than 6 pixels), the line gets a
    /// tangential arc; letting go sets it. The view calls these; tests call them directly.
    void pressAt(const interaction::PlaneRay& ray, double pixelsPerMillimetre);
    void dragTo(const interaction::PlaneRay& ray, double pixelsPerMillimetre);
    /// `dragged` = the pointer moved far enough to count as a drag (else a click follows).
    void releaseAt(const interaction::PlaneRay& ray, double pixelsPerMillimetre, bool alt = false,
                   bool dragged = true);
    /// A number typed for field 0 or 1 of the element being pulled (millimetres, or degrees for
    /// the angle); the element follows at once. NaN takes the number back.
    Q_INVOKABLE void setTyped(int field, double value);
    /// Enter in the mirror and pattern tools (the button "Continue" of the hint line): goes on from
    /// choosing the elements to the second step. Nothing without a choice (AP-2.1e3).
    Q_INVOKABLE void proceed();
    /// Backspace: takes the last point of the curve being drawn back (AP-2.1d).
    Q_INVOKABLE void removeLastPoint();
    /// Takes all typed numbers back (the element follows the pointer again).
    Q_INVOKABLE void clearTyped();
    /// Enter: sets the element as the typed numbers and the pointer make it.
    Q_INVOKABLE void acceptTyped();
    /// How many numbers can be typed now (0 when nothing is pulled).
    Q_INVOKABLE int fieldCount() const {
        return editDrafting() ? (patternReadout() ? 2 : 1) : drawing() ? m_tool.fieldCount() : 0;
    }
    /// The elements of the sketch as the controller knows them (the document's; for tests).
    const std::vector<core::SketchElement>& elements() const noexcept { return m_elements; }

signals:
    void stateChanged();
    void snapChanged();
    void coloursChanged();
    /// The tool, its variant or the helper switch changed.
    void toolChanged();
    /// The element being pulled (or its numbers) changed.
    void draftChanged();
    /// An element was finished and is in the document; typing ends.
    void elementDone();
    /// A sentence to tell the person, as a hint (never a dialog): "curvedFace" (sketches only go
    /// on flat faces), "faceUnusable" (this face cannot be drawn on), "needsHelp" (the sketch has
    /// no plane at the moment), "collinear" (three points on one line make no circle), "tooSmall"
    /// (shorter than 0.01 mm: nothing was drawn), "badSides" (the polygon has 3 to 64 sides),
    /// "curveFailed" (the kernel could make no curve through the points; nothing was added),
    /// "curvePartial" (a curve that is crossed cannot be trimmed in part yet), "nothingToExtend"
    /// (no curve in the way of the end), "editFailed" (the crossings could not be calculated).
    /// AP-2.1e2: "curveOffset", "offsetTooBig", "offsetCrossing" (offsetting), "curveCorner",
    /// "noCorner" (nothing near), "notACorner" (they go on smoothly), "cornerTooBig" and
    /// "chamferTooBig" (with `noticeNumber`: the biggest that fits).
    void notice(const QString& code);
    void noticeNumberChanged();

private:
    enum class Mode { idle, picking, sketching };

    void beginOn(const QString& stepId);
    void leave(bool removeIfEmpty);
    void onModelChanged();
    void onPicked(int intent, bool extend, const QString& body, int kind, int number);
    void onPointerMoved(double x, double y);
    void refresh();
    const std::vector<core::SketchPoint>* curveOf(const core::SketchElement& element);
    double baseStep() const;
    void applyStep(const interaction::SketchToolStep& step);
    void updateDraft();
    // trimming and extending (AP-2.1e1)
    bool editingTool() const noexcept;
    const interaction::SketchTrimmer* trimmer();
    void updateEditHover(const core::SketchPoint& raw);
    void markAlong(const core::SketchPoint& from, const core::SketchPoint& to);
    void commitTrim();
    void commitExtend();
    bool applyEdit(const interaction::SketchEdit& edit, const QString& action);
    // offsetting and rounding or chamfering corners (AP-2.1e2)
    bool offsetTool() const noexcept {
        return toolActive() && m_tool.kind() == interaction::SketchToolKind::offset;
    }
    bool cornerTool() const noexcept {
        return toolActive() && m_tool.kind() == interaction::SketchToolKind::fillet;
    }
    bool chamfering() const noexcept { return cornerTool() && m_tool.variant() == 1; }
    /// A chain is picked, or a corner is under the pointer: a number can be typed.
    bool editDrafting() const noexcept {
        return (offsetTool() && m_chain) || patternReadout() ||
               (cornerTool() && (m_pressedCorner || (m_hoverCorner && !m_hoverCorner->curve)));
    }
    // mirroring and patterns (AP-2.1e3)
    bool copyTool() const noexcept {
        return toolActive() && (m_tool.kind() == interaction::SketchToolKind::mirror ||
                                m_tool.kind() == interaction::SketchToolKind::pattern);
    }
    bool patternTool() const noexcept {
        return toolActive() && m_tool.kind() == interaction::SketchToolKind::pattern;
    }
    /// The pattern's second step with its start (or middle) set: count and distance can be typed.
    bool patternReadout() const noexcept { return patternTool() && m_second && m_anchor; }
    /// For the hint line: 0 nothing chosen, 1 something chosen (Enter goes on), 2 second step, 3
    /// second step with the start (middle) of the pattern set.
    int copyPlaced() const noexcept {
        return !m_second ? (m_picked.empty() ? 0 : 1) : (m_anchor ? 3 : 2);
    }
    std::vector<std::string> chainIds(const std::string& id) const;
    void updateCopyHover(const core::SketchPoint& raw);
    interaction::SketchCopyResult patternResult(const core::SketchPoint& pointer);
    std::optional<std::string> axisNear(const core::SketchPoint& raw) const;
    void commitCopy();
    void applyCopy(const interaction::SketchCopyResult& result, const QString& action);
    void forgetCopy();
    void updateOffsetHover(const core::SketchPoint& raw);
    void updateCornerHover(const core::SketchPoint& raw);
    double cornerValue(const interaction::SketchCorner& corner, const core::SketchPoint& raw);
    void commitOffset();
    void commitCorner();
    void say(const QString& code, double number = 0.0);
    void forgetEdit();
    void onClicked(double x, double y);
    void onPressed(double x, double y);
    void onDragged(double x, double y);
    void onReleased(double x, double y, bool dragged);
    void onDoubleClicked(double x, double y);

    QPointer<DocumentModel> m_model;
    QPointer<SelectionController> m_selection;
    QPointer<Viewport3D> m_viewport;
    Mode m_mode = Mode::idle;
    QString m_step;
    bool m_planeReady = false;
    bool m_cameraTurned = false;
    core::SketchPlane m_plane;
    std::vector<core::SketchElement> m_elements;
    std::optional<render::CameraPose> m_previousPose;
    double m_gridStep = 0.0;
    double m_pixelsPerMillimetre = 2.0;
    std::optional<interaction::SketchSnap> m_caught;
    QVariantMap m_snapMap;
    interaction::SketchTool m_tool;
    std::array<int, 16> m_variants{}; ///< the variant last used with each tool
    struct CachedCurve {
        std::vector<core::SketchPoint> through;
        std::vector<core::SketchPoint> line; ///< empty: the kernel refused
    };
    std::vector<CachedCurve> m_curves; ///< the curves of splines drawn lately
    interaction::SketchDraft m_draft;
    QVariantMap m_readout;
    // trimming and extending (AP-2.1e1)
    std::optional<interaction::SketchTrimmer> m_trimmer; ///< of the elements as they are now
    std::optional<interaction::SketchPiece> m_hoverPiece;
    std::vector<interaction::SketchPiece> m_marked; ///< the pieces a stroke has gone over
    std::optional<interaction::SketchExtension> m_hoverExtension;
    std::optional<core::SketchPoint> m_rawPointer; ///< the pointer on the plane, not caught
    bool m_stroking = false;
    // offsetting and corners (AP-2.1e2)
    std::optional<interaction::SketchChain> m_chain; ///< the chain picked to offset
    std::vector<std::string> m_hoverIds;             ///< the elements to show under the pointer
    std::optional<interaction::SketchCorner> m_hoverCorner;
    std::optional<interaction::SketchCorner> m_pressedCorner;
    std::optional<core::SketchPoint> m_pressAt;
    bool m_cornerDragged = false;
    std::optional<double> m_typedValue;         ///< the number typed (distance, radius)
    double m_editValue = 0.0;                   ///< the number shown in the bubble
    double m_lastRadius = 2.0;                  ///< the radius used last (mm)
    double m_lastChamfer = 2.0;                 ///< the chamfer distance used last (mm)
    std::vector<core::SketchElement> m_preview; ///< what the edit would add
    // mirroring and patterns (AP-2.1e3)
    std::vector<std::string> m_picked;         ///< the elements to copy
    bool m_second = false;                     ///< the second step (the axis, the row, the circle)
    std::optional<core::SketchPoint> m_anchor; ///< pattern: the start of the row, the middle
    std::optional<double> m_patTyped[2];       ///< count, then distance (row) or angle (circle)
    std::array<int, 2> m_patCount{3, 6};       ///< the count used last: row, circle
    int m_shownCount = 0;
    double m_noticeNumber = 0.0;

    QColor m_gridColour{0x3A, 0x3D, 0x43};
    QColor m_gridMajorColour{0x55, 0x59, 0x61};
    QColor m_elementColour{0xE8, 0xEA, 0xED};
    QColor m_guideColour{0x4C, 0x8D, 0xFF};
    QColor m_markerColour{0x4C, 0x8D, 0xFF};
    QColor m_draftColour{0x4C, 0x8D, 0xFF};
    QColor m_trimColour{0xF0, 0x5A, 0x5A};
};

} // namespace fission::ui
