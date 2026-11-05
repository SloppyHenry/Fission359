// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/ui/import_controller.h"

#include <QFile>
#include <QFileInfo>
#include <QMetaObject>

#include <utility>

#include "fission/geo/error.h"
#include "fission/geo/exchange.h"
#include "fission/ui/document_model.h"

namespace fission::ui {

namespace {

enum class Kind { unknown, step, iges };

Kind kindOfName(const QString& path) {
    const QString ending = QFileInfo(path).suffix().toLower();
    if (ending == QLatin1String("step") || ending == QLatin1String("stp")) {
        return Kind::step;
    }
    if (ending == QLatin1String("iges") || ending == QLatin1String("igs")) {
        return Kind::iges;
    }
    return Kind::unknown;
}

// The ending can be wrong (a .step that holds IGES): look at the beginning too. A STEP file
// starts with "ISO-10303-21;", an IGES file has 80-character lines whose 73rd character is the
// section letter.
Kind kindOfContent(const QByteArray& bytes, Kind byName) {
    if (bytes.trimmed().startsWith("ISO-10303-21")) {
        return Kind::step;
    }
    const int firstLine = bytes.indexOf('\n');
    const QByteArray line = bytes.left(firstLine < 0 ? 80 : firstLine);
    if (line.size() >= 73 && line[72] == 'S') {
        return Kind::iges;
    }
    return byName;
}

QString localPath(const QUrl& url) {
    return url.isLocalFile() ? url.toLocalFile() : url.toString();
}

} // namespace

ImportController::ImportController(DocumentModel* model,
                                   std::shared_ptr<const geo::GeometryKernel> kernel,
                                   QObject* parent)
    : QObject(parent), m_model(model), m_kernel(std::move(kernel)) {
    m_pool.setMaxThreadCount(1);
}

ImportController::~ImportController() {
    m_pool.waitForDone();
}

bool ImportController::canImport(const QUrl& url) const {
    return kindOfName(localPath(url)) != Kind::unknown;
}

bool ImportController::importFrom(const QUrl& url) {
    const QString path = localPath(url);
    const QString name = QFileInfo(path).fileName();
    if (m_importing) {
        return false;
    }
    const Kind byName = kindOfName(path);
    if (byName == Kind::unknown) {
        emit finished(QStringLiteral("unsupported"), name, 0, {});
        return false;
    }
    m_importing = true;
    emit importingChanged();
    m_pool.start([this, path, name, byName, kernel = m_kernel] {
        const auto end = [this, name](const QString& code, int bodies, const QVariantMap& notes) {
            QMetaObject::invokeMethod(
                this,
                [this, code, name, bodies, notes] {
                    m_importing = false;
                    emit importingChanged();
                    emit finished(code, name, bodies, notes);
                },
                Qt::QueuedConnection);
        };
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            end(QStringLiteral("cannotRead"), 0, {});
            return;
        }
        const QByteArray content = file.readAll();
        if (file.error() != QFileDevice::NoError) {
            end(QStringLiteral("cannotRead"), 0, {});
            return;
        }
        const bool iges = kindOfContent(content, byName) == Kind::iges;
        std::vector<std::uint8_t> bytes(content.begin(), content.end());
        geo::ImportResult read;
        try {
            read = kernel->importFile(iges ? geo::ImportFormat::iges : geo::ImportFormat::step,
                                      bytes, {});
        } catch (const geo::GeometryError& error) {
            end(error.code() == geo::GeometryErrorCode::importEmpty ? QStringLiteral("empty")
                                                                    : QStringLiteral("unreadable"),
                0, {});
            return;
        }
        // Back on the interface thread: the document is changed there, and only there.
        QMetaObject::invokeMethod(
            this,
            [this, read = std::move(read), bytes = std::move(bytes), iges, path, name] {
                const QString base = QFileInfo(path).completeBaseName();
                std::vector<DocumentModel::ImportedBodyInfo> infos;
                int repaired = 0;
                for (std::size_t i = 0; i < read.bodies.size(); ++i) {
                    const geo::ImportedBody& body = read.bodies[i];
                    DocumentModel::ImportedBodyInfo info;
                    info.name = !body.name.empty() ? body.name
                                : read.bodies.size() == 1
                                    ? base.toStdString()
                                    : base.toStdString() + " " + std::to_string(i + 1);
                    if (body.color) {
                        info.color =
                            core::Color{body.color->red, body.color->green, body.color->blue};
                    }
                    repaired += body.repaired ? 1 : 0;
                    infos.push_back(std::move(info));
                }
                const QString asset = m_model->addImportedFile(name, bytes, iges, infos);
                m_importing = false;
                emit importingChanged();
                if (asset.isEmpty()) {
                    emit finished(QStringLiteral("unreadable"), name, 0, {});
                    return;
                }
                QVariantMap notes;
                notes[QStringLiteral("asset")] = asset;
                notes[QStringLiteral("repaired")] = repaired;
                notes[QStringLiteral("skipped")] = read.skipped;
                notes[QStringLiteral("assembly")] = read.assembly;
                notes[QStringLiteral("unitAssumed")] = read.unitAssumed;
                emit finished(QString(), name, static_cast<int>(read.bodies.size()), notes);
            },
            Qt::QueuedConnection);
    });
    return true;
}

bool ImportController::setInches(const QString& asset) {
    return m_model->setImportUnitFactor(asset, 25.4);
}

} // namespace fission::ui
