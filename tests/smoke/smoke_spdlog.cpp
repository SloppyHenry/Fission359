// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <sstream>

#include <spdlog/logger.h>
#include <spdlog/sinks/ostream_sink.h>

TEST_CASE("spdlog formats a log line", "[smoke][spdlog]") {
    std::ostringstream output;
    auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(output);
    spdlog::logger logger("smoke", sink);

    logger.info("answer {}", 42);
    logger.flush();

    CHECK(output.str().find("answer 42") != std::string::npos);
}
