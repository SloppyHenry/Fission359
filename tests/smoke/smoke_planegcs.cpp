// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <planegcs/GCS.h>

TEST_CASE("PlaneGCS solves a point at a given distance on the x axis", "[smoke][planegcs]") {
    // Point 1 is fixed at the origin, point 2 must lie on y = 0 at distance 5 from it.
    double x1 = 0.0;
    double y1 = 0.0;
    double x2 = 3.0; // deliberately wrong start values
    double y2 = 1.0;
    double fixedX = 0.0;
    double fixedY = 0.0;
    double lineY = 0.0;
    double distance = 5.0;

    GCS::Point p1;
    p1.x = &x1;
    p1.y = &y1;
    GCS::Point p2;
    p2.x = &x2;
    p2.y = &y2;

    GCS::System system;
    system.addConstraintCoordinateX(p1, &fixedX, 1);
    system.addConstraintCoordinateY(p1, &fixedY, 2);
    system.addConstraintCoordinateY(p2, &lineY, 3);
    system.addConstraintP2PDistance(p1, p2, &distance, 4);

    GCS::VEC_pD unknowns{&x1, &y1, &x2, &y2};
    system.declareUnknowns(unknowns);
    system.initSolution();
    const int status = system.solve();
    system.applySolution();

    REQUIRE(status == GCS::Success);
    CHECK(x2 == Catch::Approx(5.0).margin(1e-8));
    CHECK(y2 == Catch::Approx(0.0).margin(1e-8));
}
