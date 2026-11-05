// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>
#include <QThreadPool>
#include <QUrl>
#include <QVariantMap>

#include <atomic>
#include <memory>

#include "fission/geo/kernel.h"

namespace fission::ui {

class DocumentModel;

/// Imports STEP and IGES files (AP-2.13). The file is read in a background thread (rule 9); then
/// it is embedded in the project and each of its bodies becomes a step of the recipe, as one
/// step of the history. Nothing is changed when the file cannot be used.
class ImportController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool importing READ importing NOTIFY importingChanged)

public:
    ImportController(DocumentModel* model, std::shared_ptr<const geo::GeometryKernel> kernel,
                     QObject* parent = nullptr);
    ~ImportController() override;

    bool importing() const noexcept { return m_importing; }

    /// True for a file name this controller reads (STEP and IGES, by the ending).
    Q_INVOKABLE bool canImport(const QUrl& url) const;
    /// Starts reading the file. False when it did not start (another import runs, or the file
    /// type is not known: `finished` then says "unsupported").
    Q_INVOKABLE bool importFrom(const QUrl& url);
    /// "It was inches": reads the embedded file again with all lengths ×25.4.
    Q_INVOKABLE bool setInches(const QString& asset);

signals:
    void importingChanged();
    /// `code`: "" (done) | "unsupported" | "cannotRead" | "unreadable" (not a usable STEP/IGES
    /// file) | "empty" (no solid body in it). `notes` (only for ""): { asset, repaired, skipped,
    /// assembly, unitAssumed }.
    void finished(const QString& code, const QString& fileName, int bodies,
                  const QVariantMap& notes);

private:
    DocumentModel* m_model;
    std::shared_ptr<const geo::GeometryKernel> m_kernel;
    QThreadPool m_pool;
    bool m_importing = false;
};

} // namespace fission::ui
