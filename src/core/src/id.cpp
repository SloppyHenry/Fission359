// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/core/id.h"

#include <random>

namespace fission::core {

IdSource::IdSource() {
    std::random_device device;
    state_ = (static_cast<std::uint64_t>(device()) << 32) ^ device();
}

// SplitMix64: small, fast, and gives the same numbers on every platform.
std::uint64_t IdSource::nextWord() noexcept {
    state_ += 0x9E3779B97F4A7C15ULL;
    std::uint64_t z = state_;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

std::string IdSource::nextUuid() {
    std::uint64_t high = nextWord();
    std::uint64_t low = nextWord();
    high = (high & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL; // version 4
    low = (low & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;   // variant 1

    static constexpr char digits[] = "0123456789abcdef";
    std::string text;
    text.reserve(36);
    const auto put = [&](std::uint64_t word, int firstNibble, int lastNibble) {
        for (int nibble = firstNibble; nibble >= lastNibble; --nibble) {
            text += digits[(word >> (nibble * 4)) & 0xF];
        }
    };
    put(high, 15, 8);
    text += '-';
    put(high, 7, 4);
    text += '-';
    put(high, 3, 0);
    text += '-';
    put(low, 15, 12);
    text += '-';
    put(low, 11, 0);
    return text;
}

} // namespace fission::core
