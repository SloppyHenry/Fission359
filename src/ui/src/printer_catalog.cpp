// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/ui/printer_catalog.h"

namespace fission::ui {

const std::vector<PrinterProfile>& printerCatalog() {
    // Sizes of the print bed (X x Y x height, mm), checked on 2026-10-28 against several shop and
    // review pages that agree with each other; a page of the maker was not reached for any of
    // them, so check against the data sheet before a release (pflege.md P4.3). The id of an
    // entry never changes, a saved setting names it.
    static const std::vector<PrinterProfile> printers = {
        {QStringLiteral("generic"), QString(), 256.0, 256.0, 256.0, 0.4},
        // Prusa: MK4 and MK4S are the same size.
        {QStringLiteral("prusa-mk4"), QStringLiteral("Prusa MK4"), 250.0, 210.0, 220.0, 0.4},
        {QStringLiteral("prusa-mini"), QStringLiteral("Prusa MINI+"), 180.0, 180.0, 180.0, 0.4},
        {QStringLiteral("prusa-xl"), QStringLiteral("Prusa XL"), 360.0, 360.0, 360.0, 0.4},
        {QStringLiteral("prusa-core-one"), QStringLiteral("Prusa Core One"), 250.0, 220.0, 270.0,
         0.4},
        // Bambu Lab: A1 and P1S / X1C 256 mm cube, A1 mini 180 mm cube.
        {QStringLiteral("bambu-a1"), QStringLiteral("Bambu Lab A1"), 256.0, 256.0, 256.0, 0.4},
        {QStringLiteral("bambu-a1-mini"), QStringLiteral("Bambu Lab A1 mini"), 180.0, 180.0, 180.0,
         0.4},
        {QStringLiteral("bambu-p1s"), QStringLiteral("Bambu Lab P1S / X1C"), 256.0, 256.0, 256.0,
         0.4},
        // Creality: the id "creality-ender3" stays, the model is the Ender-3 V3 (220 x 220 x 250).
        {QStringLiteral("creality-ender3"), QStringLiteral("Creality Ender-3 V3"), 220.0, 220.0,
         250.0, 0.4},
        {QStringLiteral("creality-k1"), QStringLiteral("Creality K1"), 220.0, 220.0, 250.0, 0.4},
        {QStringLiteral("creality-k1-max"), QStringLiteral("Creality K1 Max"), 300.0, 300.0, 300.0,
         0.4},
        {QStringLiteral("elegoo-neptune4"), QStringLiteral("Elegoo Neptune 4"), 225.0, 225.0, 265.0,
         0.4},
        {QStringLiteral("anycubic-kobra2"), QStringLiteral("Anycubic Kobra 2"), 220.0, 220.0, 250.0,
         0.4},
        // Voron 2.4 is built in three sizes (250, 300, 350 mm cube); a part can only be as tall as
        // the bed can move (230, 280, 330 mm, from the Voron forum), so that is the height here.
        {QStringLiteral("voron-24-250"), QStringLiteral("Voron 2.4 (250)"), 250.0, 250.0, 230.0,
         0.4},
        {QStringLiteral("voron-24-300"), QStringLiteral("Voron 2.4 (300)"), 300.0, 300.0, 280.0,
         0.4},
        {QStringLiteral("voron-24-350"), QStringLiteral("Voron 2.4 (350)"), 350.0, 350.0, 330.0,
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
