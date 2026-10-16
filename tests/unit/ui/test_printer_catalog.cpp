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
