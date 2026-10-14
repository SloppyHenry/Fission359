// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <compare>
#include <cstdint>
#include <string>

namespace fission::core {

/// A stable identifier (UUID text). The tag keeps feature ids and body ids apart in the type
/// system.
template <class Tag> struct Id {
    std::string value;

    bool empty() const noexcept { return value.empty(); }
    auto operator<=>(const Id&) const = default;
};

struct FeatureTag {};
struct BodyTag {};
using FeatureId = Id<FeatureTag>;
using BodyId = Id<BodyTag>;

/// Makes UUIDs (version 4 layout). A document owns one, so there is no global state and tests
/// can hand in a seed to get the same ids every time.
class IdSource {
public:
    /// Seeded from the system's random device.
    IdSource();
    /// Deterministic: the same seed gives the same ids.
    explicit IdSource(std::uint64_t seed) noexcept : state_(seed) {}

    std::string nextUuid();

private:
    std::uint64_t nextWord() noexcept;

    std::uint64_t state_;
};

} // namespace fission::core
