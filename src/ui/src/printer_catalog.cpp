// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/ui/printer_catalog.h"

namespace fission::ui {

const std::vector<PrinterProfile>& printerCatalog() {
    // Sizes of the print bed as the makers state them (X x Y x height, mm).
    static const std::vector<PrinterProfile> printers = {
        {QStringLiteral("generic"), QString(), 256.0, 256.0, 256.0, 0.4},
        {QStringLiteral("prusa-mk4"), QStringLiteral("Prusa MK4"), 250.0, 210.0, 220.0, 0.4},
        {QStringLiteral("prusa-mini"), QStringLiteral("Prusa MINI+"), 180.0, 180.0, 180.0, 0.4},
        {QStringLiteral("prusa-xl"), QStringLiteral("Prusa XL"), 360.0, 360.0, 360.0, 0.4},
        {QStringLiteral("bambu-a1"), QStringLiteral("Bambu Lab A1"), 256.0, 256.0, 256.0, 0.4},
        {QStringLiteral("bambu-a1-mini"), QStringLiteral("Bambu Lab A1 mini"), 180.0, 180.0, 180.0,
         0.4},
        {QStringLiteral("bambu-p1s"), QStringLiteral("Bambu Lab P1S / X1C"), 256.0, 256.0, 256.0,
         0.4},
        {QStringLiteral("creality-ender3"), QStringLiteral("Creality Ender-3"), 220.0, 220.0, 250.0,
         0.4},
        {QStringLiteral("creality-k1"), QStringLiteral("Creality K1"), 220.0, 220.0, 250.0, 0.4},
        {QStringLiteral("creality-k1-max"), QStringLiteral("Creality K1 Max"), 300.0, 300.0, 300.0,
         0.4},
        {QStringLiteral("elegoo-neptune4"), QStringLiteral("Elegoo Neptune 4"), 225.0, 225.0, 265.0,
         0.4},
        {QStringLiteral("anycubic-kobra2"), QStringLiteral("Anycubic Kobra 2"), 220.0, 220.0, 250.0,
         0.4},
    };
    return printers;
}

const PrinterProfile* findPrinter(const QString& id) {
    for (const PrinterProfile& printer : printerCatalog()) {
        if (printer.id == id) {
            return &printer;
        }
    }
    return nullptr;
}

} // namespace fission::ui
