# SPDX-FileCopyrightText: Fission3D contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""Layout of the disk image (read by dmgbuild; values come in with -D name=value)."""

import os.path

application = defines["app"]  # noqa: F821 - provided by dmgbuild
appname = defines["appname"]  # noqa: F821
help_file = defines["help_file"]  # noqa: F821

format = "UDZO"
size = None  # as small as possible
files = [application, help_file]
symlinks = {"Applications": "/Applications"}
background = defines["background"]  # noqa: F821
icon_size = 128
text_size = 13
window_rect = ((200, 120), (660, 440))
icon_locations = {
    os.path.basename(application): (180, 190),
    "Applications": (480, 190),
    os.path.basename(help_file): (330, 340),
}
