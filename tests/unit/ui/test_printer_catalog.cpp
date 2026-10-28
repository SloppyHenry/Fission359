// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_test_macros.hpp>

#include <set>

#include "fission/ui/printer_catalog.h"

using namespace fission::ui;

TEST_CASE("the printer list is usable", "[ui][printer]") {
    const auto& printers = printerCatalog();
    REQUIRE_FALSE(printers.empty());
    CHECK(printers.front().id == kGenericPrinterId); // the default comes first

    std::set<QString> ids;
    for (const PrinterProfile& printer : printers) {
        CAPTURE(printer.id.toStdString());
        CHECK(ids.insert(printer.id).second); // no id twice
        CHECK(printer.bedX >= 100.0);
        CHECK(printer.bedY >= 100.0);
        CHECK(printer.bedZ >= 100.0);
        CHECK(printer.bedX <= 500.0);
        CHECK(printer.nozzle > 0.0);
        if (printer.id != kGenericPrinterId) {
            CHECK_FALSE(printer.name.isEmpty()); // only the generic entry is named by the interface
        }
    }
    CHECK(findPrinter("prusa-mk4") != nullptr);
    CHECK(findPrinter("nobody") == nullptr);
}

TEST_CASE("the printer list holds the printers of AP-1.11", "[ui][printer]") {
    // The scope of AP-1.11: Bambu A1 / A1 mini / P1S / X1C, Prusa MK4 / Core One / MINI,
    // Creality Ender-3 V3 / K1, Voron. ("Own printer" comes with its own entry in the settings.)
    for (const char* id :
         {"bambu-a1", "bambu-a1-mini", "bambu-p1s", "prusa-mk4", "prusa-core-one", "prusa-mini",
          "creality-ender3", "creality-k1", "voron-24-250", "voron-24-300", "voron-24-350"}) {
        CAPTURE(id);
        CHECK(findPrinter(QString::fromLatin1(id)) != nullptr);
    }
    // The old id of the Ender-3 stays: a saved setting names it.
    const PrinterProfile* ender = findPrinter("creality-ender3");
    REQUIRE(ender != nullptr);
    CHECK(ender->name == "Creality Ender-3 V3");
    const PrinterProfile* core = findPrinter("prusa-core-one");
    REQUIRE(core != nullptr);
    CHECK(core->bedX == 250.0);
    CHECK(core->bedY == 220.0);
    CHECK(core->bedZ == 270.0);
}
