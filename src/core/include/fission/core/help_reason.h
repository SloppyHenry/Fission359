// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace fission::core {

/// Why a reference could not be resolved safely (codes for the message catalogue, AP-2.14).
enum class HelpReason {
    none,
    split,    ///< The face was cut into pieces.
    consumed, ///< A step used the element up.
    turned,   ///< The name points to an element that turned by more than 45°.
    swapped,  ///< Two equal elements probably changed places.
    notFound  ///< Nothing is clearly the element any more.
};

} // namespace fission::core
