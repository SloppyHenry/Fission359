// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QThreadPool>
#include <QUrl>
#include <QVariantMap>

#include <atomic>
#include <memory>

#include "fission/geo/kernel.h"

namespace fission::ui {

class DocumentModel;

/// Export to 3MF, STL, OBJ and STEP (AP-1.10e, AP-2.13), and the outline of a flat face as SVG or
/// DXF. The bodies are meshed and written in a background thread; `progress` follows and `cancel`
/// stops it (rule 9). Holes, bodies that are part of another body and hidden bodies are not
/// exported, and neither is a body whose step has an error (rule 6: never quietly write something
/// that is not what the person sees).
class ExportController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool exporting READ exporting NOTIFY exportingChanged)
    /// 0 to 1 while exporting.
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)

public:
    ExportController(DocumentModel* model, std::shared_ptr<const geo::GeometryKernel> kernel,
                     QObject* parent = nullptr);
    ~ExportController() override;

    bool exporting() const noexcept { return m_exporting; }
    double progress() const noexcept { return m_progress; }

    /// How many bodies an export of these would hold (all of them when `onlyIds` is empty).
    Q_INVOKABLE int exportableCount(const QStringList& onlyIds) const;
    /// Starts the export. `format`: "3mf" | "stl" | "obj" | "step"; `quality` (not used by "step"):
    /// "coarse" | "normal" | "fine"; `names`: body id to the name the person sees (used for the
    /// objects of a 3MF). False when it did not start; `finished` then says why ("nothing",
    /// "needsHelp").
    Q_INVOKABLE bool start(const QString& format, const QString& quality,
                           const QStringList& onlyIds, const QUrl& url, const QVariantMap& names);
    /// Writes the outline of one flat face of a body as a drawing for a laser cutter: `format`
    /// "svg" | "dxf", `bodyId` and `faceNumber` as in the choice (`Selection.items`). False when
    /// it did not start; `finished` then says why ("nothing", "needsHelp", "notFlat").
    Q_INVOKABLE bool startOutline(const QString& format, const QString& bodyId, int faceNumber,
                                  const QUrl& url);
    Q_INVOKABLE void cancel();

signals:
    void exportingChanged();
    void progressChanged();
    /// `code`: "" (done) | "cancelled" | "nothing" (no body to export) | "needsHelp" (a body has a
    /// problem in its recipe) | "notFlat" (the chosen face is not flat) | "cannotWrite".
    void finished(const QString& code, const QString& path, int bodies);

private:
    void setProgress(double value);

    DocumentModel* m_model;
    std::shared_ptr<const geo::GeometryKernel> m_kernel;
    QThreadPool m_pool;
    std::shared_ptr<std::atomic<bool>> m_cancel = std::make_shared<std::atomic<bool>>(false);
    bool m_exporting = false;
    double m_progress = 0.0;
};

} // namespace fission::ui
