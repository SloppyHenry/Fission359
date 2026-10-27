// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <manifold/manifold.h>

TEST_CASE("Manifold subtracts a sphere from a cube", "[smoke][manifold]") {
    using manifold::Manifold;

    const Manifold cube = Manifold::Cube({10.0, 10.0, 10.0}, /*center=*/true);
    const Manifold sphere = Manifold::Sphere(4.0, 128);
    const Manifold hollow = cube - sphere;

    REQUIRE(hollow.Status() == Manifold::Error::NoError);
    CHECK(hollow.NumTri() > 0);

    // Analytic: cube minus sphere. The tessellated sphere is slightly smaller than the true
    // one, so allow 0.5 %.
    const double expected = 1000.0 - 4.0 / 3.0 * 3.14159265358979323846 * 64.0;
    CHECK(hollow.Volume() == Catch::Approx(expected).epsilon(0.005));
}
