// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <Eigen/Dense>

TEST_CASE("Eigen solves a small linear system", "[smoke][eigen]") {
    Eigen::Matrix2d a;
    a << 2.0, 0.0, 0.0, 4.0;
    const Eigen::Vector2d b(2.0, 8.0);

    const Eigen::Vector2d x = a.colPivHouseholderQr().solve(b);

    CHECK(x(0) == Catch::Approx(1.0));
    CHECK(x(1) == Catch::Approx(2.0));
}
