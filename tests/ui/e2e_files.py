#!/usr/bin/env python3
# SPDX-FileCopyrightText: Fission359 contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""End-to-end test of the file features through the real program (AP-1.10).

Starts the program several times with a data folder of its own and checks what a person would:
save and open again, a crash (kill -9) and the rescue card, quitting a project that was never
saved, and the export files. Usage: e2e_files.py <fission359> [runner...]
"""

import json
import os
import subprocess
import sys
import tempfile
import zipfile

APP = sys.argv[1]
RUNNER = sys.argv[2:]  # e.g. xvfb-run ...; empty on macOS


def run(data, actions, *extra, expect_killed=False, first_run=False, start_page=False):
    """One start of the program: a picture of the window is taken after the actions."""
    picture = os.path.join(data, "picture.png")
    env = dict(os.environ, FISSION_DATA_DIR=data, FISSION_PARK_WINDOW="1")
    if first_run:
        env["FISSION_FIRST_RUN"] = "1"
    if start_page:
        env["FISSION_START_PAGE"] = "1"
    command = RUNNER + [APP, *extra, "--smoke-test", picture, "--language", "en",
                        "--size", "1280x800", "--simulate", actions]
    result = subprocess.run(command, env=env, capture_output=True, text=True, timeout=120)
    if expect_killed:
        # kill -9 shows as -9, or as 137 when a wrapper (xvfb-run) reports it for the program
        if result.returncode not in (-9, 137):
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

    # 6. Settings are kept between starts: mouse style and soft shadows chosen in the menu.
    prefs = os.path.join(data, "prefs")
    os.makedirs(prefs)
    run(prefs, "menu:mouse.blender;menu:softshadows;expectpref:mouseStyle,blender;expectpref:softShadows,false")
    check("mouseStyle=blender" in open(os.path.join(prefs, "settings.ini")).read(), "settings.ini")
    run(prefs, "expectpref:mouseStyle,blender;expectpref:softShadows,false")

    # 7. The language changes while the program runs; the settings page opens without errors.
    lang = os.path.join(data, "lang")
    os.makedirs(lang)
    run(lang, "expecttr:SettingsPage,Settings,Settings;setpref:language,de;"
              "expecttr:SettingsPage,Settings,Einstellungen;setpref:language,en;"
              "expecttr:SettingsPage,Settings,Settings;menu:settings;setpref:uiScale,150;"
              "expectpref:uiScale,150;setpref:uiScale,100")

    # 8. The first start asks language, printer, mouse; skipping or finishing is remembered.
    first = os.path.join(data, "first")
    os.makedirs(first)
    run(first, "expectpref:firstRunDone,false;clickitem:firstRunNext;clickitem:firstRunNext;"
               "clickitem:firstRunNext;expectpref:firstRunDone,true", first_run=True)
    run(first, "expectpref:firstRunDone,true", first_run=True)
    skipped = os.path.join(data, "skipped")
    os.makedirs(skipped)
    run(skipped, "clickitem:firstRunLater;expectpref:firstRunDone,true", first_run=True)

    # 9. The start page: an example opens as a new project, a recent file opens, "New" is empty.
    run(data, "clickitem:startExample_keychain;recipe:box,cylinder,combine;expectmodified:0",
        start_page=True)
    run(data, "clickitem:startRecent_0;recipe:box,cylinder,sphere,cone;expectmodified:0",
        start_page=True)
    run(data, "clickitem:startNew;recipe:;expectmodified:0", start_page=True)
    for example in ("box-with-hole", "star"):
        run(data, "example:%s;expectmodified:0" % example)

    # 10. Several documents: tabs keep their own steps; closing saves; Ctrl+W logic.
    tabs = os.path.join(data, "tabs")
    os.makedirs(tabs)
    one = os.path.join(tabs, "one.fission")
    two = os.path.join(tabs, "two.fission")
    run(tabs, "put:cylinder;save:%s;clickitem:tabNew;recipe:;put:sphere;save:%s;recipe:sphere;"
              "clickitem:tab_0;recipe:box,cylinder;clickitem:tab_1;recipe:sphere;"
              "clickitem:tabClose_1;recipe:box,cylinder;expectmodified:0" % (one, two))
    # Tabs that were never saved stay as rescue work when the program ends.
    run(tabs, "put:cone;clickitem:tabNew;put:star;clickitem:tabNew;recipe:")
    run(tabs, "rescue:restore;rescue:restore;expectmodified:1")

    # 11. The command search finds things by everyday words, through the program's own catalogue.
    run(data, "search:runde ecken,soon.fillet;search:bohren,soon.drill;search:hohl,soon.shell;"
              "search:namen,shape.text;search:verschiben,tool.move;search:grosse,tool.size;"
              "search:round corners,soon.fillet;search:drill,soon.drill;search:rotate,tool.rotate;"
              "search:speichern,menu.save;search:rückgängig,menu.undo;search:kugel,shape.sphere;"
              "runcommand:tool.rotate;runcommand:menu.settings;runcommand:soon.fillet")

    # 12. The hint line and the message catalogue.
    run(data, "menu:new;expecthint:Start with a shape;"
              "expectmessage:parameterOutOfRange,width,must be bigger than zero;"
              "expectmessage:parameterOutOfRange,wall,thinner than the radius;"
              "expectmessage:combineEmpty,,do not overlap;"
              "expectmessage:fontNotFound,Foo,is not on this computer;"
              "expectmessage:cancelled,,newer change")

print("OK")
