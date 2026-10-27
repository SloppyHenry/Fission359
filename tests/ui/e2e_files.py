#!/usr/bin/env python3
# SPDX-FileCopyrightText: Fission3D contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""End-to-end test of the file features through the real program (AP-1.10).

Starts the program several times with a data folder of its own and checks what a person would:
save and open again, a crash (kill -9) and the rescue card, quitting a project that was never
saved, and the export files. Usage: e2e_files.py <fission3d> [runner...]
"""

import json
import os
import subprocess
import sys
import tempfile
import zipfile

APP = sys.argv[1]
RUNNER = sys.argv[2:]  # e.g. xvfb-run ...; empty on macOS


def run(data, actions, *extra, expect_killed=False):
    """One start of the program: a picture of the window is taken after the actions."""
    picture = os.path.join(data, "picture.png")
    env = dict(os.environ, FISSION_DATA_DIR=data, FISSION_PARK_WINDOW="1")
    command = RUNNER + [APP, *extra, "--smoke-test", picture, "--language", "en",
                        "--size", "1280x800", "--simulate", actions]
    result = subprocess.run(command, env=env, capture_output=True, text=True, timeout=120)
    if expect_killed:
        if result.returncode != -9:
            fail("the program should have been killed (-9), exit code %s\n%s%s"
                 % (result.returncode, result.stdout, result.stderr))
    elif result.returncode != 0:
        fail("exit code %s for %r\n%s%s" % (result.returncode, actions, result.stdout, result.stderr))
    return result


def fail(message):
    print("FAILED:", message)
    sys.exit(1)


def check(condition, message):
    if not condition:
        fail(message)


with tempfile.TemporaryDirectory() as data:
    model = os.path.join(data, "model.fission")

    # 1. Save, close, open again: the same recipe, nothing left to save.
    run(data, "put:cylinder;put:sphere;save:%s;expectmodified:0" % model)
    check(os.path.exists(model), "the file was not written")
    check(not [f for f in os.listdir(data) if ".tmp-" in f], "a temporary file was left")
    with zipfile.ZipFile(model) as archive:
        names = archive.namelist()
        check({"manifest.json", "document.json", "preview.png"} <= set(names), "parts missing: %s" % names)
        check(archive.read("preview.png")[:4] == b"\x89PNG", "the preview is not a picture")
        manifest = json.loads(archive.read("manifest.json"))
        check(manifest["formatVersion"] == 1 and "document.json" in manifest["checksums"], "manifest")
        document = json.loads(archive.read("document.json"))
        check([f["type"] for f in document["features"]] == ["box", "cylinder", "sphere"], "recipe in file")
    run(data, "recipe:box,cylinder,sphere;expectmodified:0", model)       # opened by argument
    run(data, "open:%s;recipe:box,cylinder,sphere;expectmodified:0" % model)  # opened by the menu path

    # 2. A change after opening is saved on a clean end (no question asked) ...
    run(data, "put:cone;expectmodified:1", model)
    run(data, "recipe:box,cylinder,sphere,cone;expectmodified:0", model)

    # 3. kill -9 after the autosave: the next start offers the work, restoring brings it back.
    crash_data = os.path.join(data, "crash")
    os.makedirs(crash_data)
    run(crash_data, "put:cylinder;put:cone;autosave;crash", expect_killed=True)
    rescue = os.path.join(crash_data, "recovery")
    check(any(f.endswith(".fission") for f in os.listdir(rescue)), "no rescue file after the crash")
    run(crash_data, "rescue:restore;recipe:box,cylinder,cone;expectmodified:1")

    # 4. Quitting a project that was never saved keeps it as rescue work.
    quit_data = os.path.join(data, "quit")
    os.makedirs(quit_data)
    run(quit_data, "put:cylinder;expectmodified:1")
    run(quit_data, "rescue:restore;recipe:box,cylinder")

    # 5. Export: the files are what a slicer expects.
    out3mf = os.path.join(data, "out.3mf")
    outstl = os.path.join(data, "out.stl")
    run(data, "put:cylinder;export:%s;export:%s" % (out3mf, outstl))
    with zipfile.ZipFile(out3mf) as archive:
        text = archive.read("3D/3dmodel.model").decode()
        check(text.count("<object ") == 2, "3MF should hold two objects")
        check("basematerials" in text and "displaycolor" in text, "3MF has no colours")
    size = os.path.getsize(outstl)
    with open(outstl, "rb") as stl:
        stl.seek(80)
        count = int.from_bytes(stl.read(4), "little")
    check(size == 84 + 50 * count and count > 12, "STL size does not fit its triangle count")

print("OK")
