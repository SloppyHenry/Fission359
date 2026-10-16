// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QPointF>
#include <QPointer>
#include <QVariantList>

#include "fission/ui/document_model.h"
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
class HandleController : public QObject {
    Q_OBJECT
    /// The handles on screen: [{ index, parameter, kind, x, y, dirX, dirY, value,
    /// pixelsPerMillimetre }] (x, y in view pixels, dir a unit vector on screen). Empty when
    /// nothing is shown.
    Q_PROPERTY(QVariantList handles READ handles NOTIFY handlesChanged)
    /// A drag or typing is going on.
    Q_PROPERTY(bool editing READ editing NOTIFY editingChanged)
    /// Digits start typing: a shape was just put down and nothing else was done since.
    Q_PROPERTY(bool armed READ armed WRITE setArmed NOTIFY armedChanged)

public:
    explicit HandleController(DocumentModel* model, QObject* parent = nullptr);

    void attach(Viewport3D* viewport);

    QVariantList handles() const { return m_handles; }
    bool editing() const noexcept { return m_editing; }
    bool armed() const noexcept { return m_armed; }
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

signals:
    void handlesChanged();
    void editingChanged();
    void armedChanged();

private:
    void setEditing(bool editing);

    QPointer<DocumentModel> m_model;
    QPointer<Viewport3D> m_viewport;
    QVariantList m_handles;
    bool m_editing = false;
    bool m_armed = false;
    int m_armedStep = -1;

    // The drag in progress.
    bool m_dragging = false;
    int m_dragIndex = -1;
    QPointF m_dragStart;
    QPointF m_dragPerMillimetre; ///< screen movement for one millimetre along the handle
    double m_dragStartValue = 0.0;
    double m_dragSign = 1.0;
    double m_dragValue = 0.0;
    QString m_dragParameter;
};

} // namespace fission::ui
