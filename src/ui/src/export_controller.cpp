// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/ui/export_controller.h"

#include <QFileInfo>
#include <QMetaObject>

#include <algorithm>
#include <filesystem>
#include <utility>

#include "fission/geo/error.h"
#include "fission/geo/solid.h"
#include "fission/io/drawing.h"
#include "fission/io/export.h"
#include "fission/ui/document_model.h"

namespace fission::ui {

namespace {

io::ExportQuality qualityOf(const QString& text) {
    if (text == QLatin1String("coarse")) {
        return io::ExportQuality::coarse;
    }
    if (text == QLatin1String("fine")) {
        return io::ExportQuality::fine;
    }
    return io::ExportQuality::normal;
}

} // namespace

ExportController::ExportController(DocumentModel* model,
                                   std::shared_ptr<const geo::GeometryKernel> kernel,
                                   QObject* parent)
    : QObject(parent), m_model(model), m_kernel(std::move(kernel)) {
    m_pool.setMaxThreadCount(1);
}

ExportController::~ExportController() {
    m_cancel->store(true);
    m_pool.waitForDone();
}

int ExportController::exportableCount(const QStringList& onlyIds) const {
    return static_cast<int>(m_model->exportBodies(onlyIds).size());
}

void ExportController::setProgress(double value) {
    m_progress = value;
    emit progressChanged();
}

void ExportController::cancel() {
    m_cancel->store(true);
}

bool ExportController::start(const QString& format, const QString& quality,
                             const QStringList& onlyIds, const QUrl& url,
                             const QVariantMap& names) {
    if (m_exporting) {
        return false;
    }
    QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
    const QString kind = format == QLatin1String("stl")    ? QStringLiteral("stl")
                         : format == QLatin1String("obj")  ? QStringLiteral("obj")
                         : format == QLatin1String("step") ? QStringLiteral("step")
                                                           : QStringLiteral("3mf");
    if (QFileInfo(path).suffix().isEmpty()) {
        path += QLatin1Char('.') + kind;
    }

    // On the interface thread: what is to be exported is decided from what the person sees now.
    std::vector<DocumentModel::ExportBody> bodies = m_model->exportBodies(onlyIds);
    if (bodies.empty()) {
        emit finished(QStringLiteral("nothing"), path, 0);
        return false;
    }
    if (std::any_of(bodies.begin(), bodies.end(),
                    [](const DocumentModel::ExportBody& body) { return body.stale; })) {
        emit finished(QStringLiteral("needsHelp"), path, 0);
        return false;
    }

    m_cancel->store(false);
    m_exporting = true;
    emit exportingChanged();
    setProgress(0.0);

    std::vector<std::string> labels;
    labels.reserve(bodies.size());
    int number = 0;
    for (const DocumentModel::ExportBody& body : bodies) {
        const QString name = names.value(QString::fromStdString(body.id.value)).toString();
        labels.push_back(name.isEmpty() ? "Body " + std::to_string(++number) : name.toStdString());
    }
    const io::ExportQuality chosen = qualityOf(quality);
    m_pool.start([this, bodies = std::move(bodies), labels = std::move(labels), chosen, kind, path,
                  cancel = m_cancel, kernel = m_kernel] {
        const auto finish = [this, path](const QString& code, int count) {
            QMetaObject::invokeMethod(
                this,
                [this, code, path, count] {
                    m_exporting = false;
                    emit exportingChanged();
                    setProgress(code.isEmpty() ? 1.0 : 0.0);
                    emit finished(code, path, count);
                },
                Qt::QueuedConnection);
        };
        const std::filesystem::path target(path.toStdString());
        if (kind == QLatin1String("step")) { // exact bodies, no mesh
            std::vector<geo::ExportBody> exact;
            for (std::size_t i = 0; i < bodies.size(); ++i) {
                const core::Color& color = bodies[i].color;
                exact.push_back({bodies[i].solid, labels[i],
                                 geo::FileColor{color.red, color.green, color.blue}});
            }
            std::string text;
            try {
                text = kernel->exportStep(exact);
            } catch (const geo::GeometryError&) {
                finish(QStringLiteral("needsHelp"), 0);
                return;
            }
            if (cancel->load()) {
                finish(QStringLiteral("cancelled"), 0);
                return;
            }
            const bool ok = io::writeTextFile(text, target);
            finish(ok ? QString() : QStringLiteral("cannotWrite"),
                   ok ? static_cast<int>(exact.size()) : 0);
            return;
        }
        std::vector<io::MeshForExport> meshes;
        const geo::TessellationParams params = io::exportTessellation(chosen);
        for (std::size_t i = 0; i < bodies.size(); ++i) {
            if (cancel->load()) {
                finish(QStringLiteral("cancelled"), 0);
                return;
            }
            try {
                meshes.push_back(
                    {labels[i], bodies[i].color, kernel->tessellate(bodies[i].solid, params)});
            } catch (const geo::GeometryError&) {
                finish(QStringLiteral("needsHelp"), 0); // no kernel text reaches the person
                return;
            }
            QMetaObject::invokeMethod(
                this,
                [this, done = static_cast<double>(i + 1),
                 total = static_cast<double>(bodies.size())] {
                    setProgress(0.9 * done / total); // the writing is the last tenth
                },
                Qt::QueuedConnection);
        }
        if (cancel->load()) {
            finish(QStringLiteral("cancelled"), 0);
            return;
        }
        const bool ok = kind == QLatin1String("stl")   ? io::exportStl(meshes, target)
                        : kind == QLatin1String("obj") ? io::exportObj(meshes, target)
                                                       : io::export3mf(meshes, target);
        finish(ok ? QString() : QStringLiteral("cannotWrite"),
               ok ? static_cast<int>(meshes.size()) : 0);
    });
    return true;
}

bool ExportController::startOutline(const QString& format, const QString& bodyId, int faceNumber,
                                    const QUrl& url) {
    if (m_exporting) {
        return false;
    }
    const bool dxf = format == QLatin1String("dxf");
    QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
    if (QFileInfo(path).suffix().isEmpty()) {
        path += dxf ? QStringLiteral(".dxf") : QStringLiteral(".svg");
    }
    std::vector<DocumentModel::ExportBody> bodies = m_model->exportBodies({bodyId});
    if (bodies.empty() || faceNumber < 1) {
        emit finished(QStringLiteral("nothing"), path, 0);
        return false;
    }
    if (bodies.front().stale) {
        emit finished(QStringLiteral("needsHelp"), path, 0);
        return false;
    }
    // A drawing is quick to make, but it goes through the same steps as the other exports.
    m_cancel->store(false);
    m_exporting = true;
    emit exportingChanged();
    setProgress(0.0);
    m_pool.start([this, solid = bodies.front().solid, faceNumber, dxf, path, kernel = m_kernel] {
        const auto finish = [this, path](const QString& code, int count) {
            QMetaObject::invokeMethod(
                this,
                [this, code, path, count] {
                    m_exporting = false;
                    emit exportingChanged();
                    setProgress(code.isEmpty() ? 1.0 : 0.0);
                    emit finished(code, path, count);
                },
                Qt::QueuedConnection);
        };
        geo::Outline outline;
        try {
            outline = kernel->faceOutline(solid, static_cast<std::uint32_t>(faceNumber));
        } catch (const geo::GeometryError& error) {
            finish(error.code() == geo::GeometryErrorCode::invalidInput
                       ? QStringLiteral("notFlat")
                       : QStringLiteral("needsHelp"),
                   0);
            return;
        }
        const std::filesystem::path target(path.toStdString());
        const bool ok = dxf ? io::exportDxf(outline, target) : io::exportSvg(outline, target);
        finish(ok ? QString() : QStringLiteral("cannotWrite"), ok ? 1 : 0);
    });
    return true;
}

} // namespace fission::ui
