#!/usr/bin/env python3
# SPDX-FileCopyrightText: Fission3D contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""End-to-end test of the keychain, flow A of docs/02-ux-konzept.md section 15 (milestone M1).

Plays the steps through the real program and reads the exported 3MF with the eyes of a slicer:
two objects, two colours, the hole cut out of the plate. Left out on purpose: step 3 (round the
vertical edges, AP-2.5) and step 7 (print check, phase 3). Steps 4 and 5 use values for the
place of the text and the hole instead of clicking on the top face (the placement tests cover
that). The distance measure ("5 mm from the edge") is not used: it measures only to bodies
beside the hole, and on the plate it shows the distance to the edge of the print bed. Usage: e2e_keychain.py <fission3d> [runner...]
"""

import math
import os
import re
import subprocess
import sys
import tempfile
import zipfile

APP = sys.argv[1]
RUNNER = sys.argv[2:]  # e.g. xvfb-run ...; empty on macOS

# The plate 50 x 20 x 4 mm (step 2: typed), text LENA of 1.5 mm height on top of it (step 4), a
# cylinder of 5 mm across as a hole, its left side 5 mm from the left edge (step 5), the text red (step 6),
# then export as 3MF (step 8, without the slicer).
ACTIONS = ";".join([
    "step:0", "type:50,tab,20,tab,4,enter", "value:x,0", "value:y,0",
    "put:text", "set:text,LENA", "value:size,6", "value:height,1.5", "value:x,0", "value:y,0", "value:z,4",
    "color:#e53935",
    "puthole:cylinder", "value:radius,2.5", "value:x,-17.5", "value:y,0", "value:z,-1",
    "value:height,5",
    "recipe:box,text,cylinder,combine",
])


def fail(message):
    print("FAILED:", message)
    sys.exit(1)


def check(condition, message):
    if not condition:
        fail(message)


def volume(object_xml):
    """Volume of an object from its triangles (signed, divergence theorem)."""
    vertices = [tuple(float(v) for v in m) for m in re.findall(
        r'<vertex x="([^"]+)" y="([^"]+)" z="([^"]+)"', object_xml)]
    total = 0.0
    for a, b, c in re.findall(r'<triangle v1="(\d+)" v2="(\d+)" v3="(\d+)"', object_xml):
        p, q, r = vertices[int(a)], vertices[int(b)], vertices[int(c)]
        total += (p[0] * (q[1] * r[2] - q[2] * r[1]) - p[1] * (q[0] * r[2] - q[2] * r[0])
                  + p[2] * (q[0] * r[1] - q[1] * r[0])) / 6.0
    return abs(total)


with tempfile.TemporaryDirectory() as data:
    out = os.path.join(data, "keychain.3mf")
    picture = os.path.join(data, "picture.png")
    env = dict(os.environ, FISSION_DATA_DIR=data, FISSION_PARK_WINDOW="1")
    command = RUNNER + [APP, "--smoke-test", picture, "--language", "en", "--size", "1280x800",
                        "--camera=0,60,160", "--simulate", ACTIONS + ";export:" + out]
    result = subprocess.run(command, env=env, capture_output=True, text=True, timeout=180)
    check(result.returncode == 0, "exit code %s\n%s%s" % (result.returncode, result.stdout,
                                                         result.stderr))
    check(os.path.exists(out), "the 3MF was not written")
    with zipfile.ZipFile(out) as archive:
        text = archive.read("3D/3dmodel.model").decode()
    objects = re.findall(r"<object .*?</object>", text, re.S)
    check(len(objects) == 2, "expected two objects (plate and text), found %d" % len(objects))
    colours = set(re.findall(r'displaycolor="(#[0-9A-Fa-f]{6,8})"', text))
    check(len(colours) == 2, "expected two colours, found %s" % sorted(colours))
    check(any(c.upper().startswith("#E53935") for c in colours), "the text is not red: %s" % colours)
    volumes = sorted(volume(o) for o in objects)  # the text is the smaller one
    text_volume, plate_volume = volumes
    hole = math.pi * 2.5 ** 2 * 4
    check(text_volume > 0.0, "the text has no volume")
    check(plate_volume < 50 * 20 * 4 - 0.5 * hole,
          "the hole is not cut out: volume %.1f of %.1f" % (plate_volume, 50 * 20 * 4))
    check(plate_volume > 50 * 20 * 4 - 2 * hole, "too much is cut away: volume %.1f" % plate_volume)
    print("OK: plate %.1f mm3 (box 4000 minus hole %.1f), text %.1f mm3, colours %s"
          % (plate_volume, hole, text_volume, sorted(colours)))
