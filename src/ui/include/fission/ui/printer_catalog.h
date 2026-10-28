// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// A short list of common printers: name and print bed. It stands in for the printer profiles of
// AP-3.1 and only decides how big the print bed in the 3D view is.

#pragma once

#include <QString>

#include <vector>

namespace fission::ui {

struct PrinterProfile {
    QString id;          ///< stable key, e.g. "prusa-mk4"
    QString name;        ///< brand and model; empty for the generic entry (the interface names it)
    double bedX = 0.0;   ///< mm
    double bedY = 0.0;   ///< mm
    double bedZ = 0.0;   ///< mm, how tall a part may be
    double nozzle = 0.4; ///< mm
};

/// The generic printer (256 x 256 mm) is first and the default.
const std::vector<PrinterProfile>& printerCatalog();
inline const QString kGenericPrinterId = QStringLiteral("generic");

/// nullptr if there is no printer with this id.
const PrinterProfile* findPrinter(const QString& id);

} // namespace fission::ui
