#!/usr/bin/env python3
"""Create a new RetroStone VC game from games/_template, in the 8BCraft house style.

    python3 tools/new_game.py ID "Display Name" [--exe ExeName] [--magic ABCD] [--no-art]

ID is the folder and file-name id (lowercase letters and digits: pogomamie), the display name is shown in the
frontends and drawn as the title logo ("Pogo Mamie"). It writes games/ID/ (game.mk with the ID, ID-check,
ID-dist, ID-windows, ID-armhf, ID-screenshots and ID-art targets; src/ with the house flow; tools/ with
make_art.py, make_music.py and build_assets.py on the house kit; tests/; DESIGN.md; README-windows.txt;
art/incoming/TODO.md) and draws the art (tools/make_art.py). Then:

    make ID-check        build and test it;   make ID    the SDL2 build: build/host/ID

The template's placeholders: @ID@, @UP@ (ID in capitals, for make variables), @NAME@, @TITLE@ (the name in
capitals: the logo), @EXE@ (the exe name: PogoMamie), @MAGIC@ (4 letters: the save RAM's magic).

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
"""
import argparse
import os
import re
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TEMPLATE = os.path.join(ROOT, "games", "_template")
TEXT_EXT = {".c", ".h", ".py", ".sh", ".md", ".txt", ".in", ""}


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("id")
    ap.add_argument("name")
    ap.add_argument("--exe", help="the exe name (default: the name without spaces)")
    ap.add_argument("--magic", help="4 letters for the save RAM (default: the first 4 of the id)")
    ap.add_argument("--no-art", action="store_true", help="do not run make_art.py")
    a = ap.parse_args(argv)
    if not re.match(r"^[a-z][a-z0-9]{1,30}$", a.id):
        sys.exit("the id must be lowercase letters and digits (e.g. pogomamie)")
    title = a.name.upper()
    if not re.match(r"^[A-Z0-9 !?.-]+$", title):
        sys.exit("the name must use letters, digits, spaces and !?.- (the logo font)")
    dest = os.path.join(ROOT, "games", a.id)
    if os.path.exists(dest):
        sys.exit("%s exists already" % dest)
    exe = a.exe or re.sub(r"[^A-Za-z0-9]", "", a.name.title())
    magic = (a.magic or (a.id.upper() + "XXXX"))[:4]
    subst = {"@ID@": a.id, "@UP@": a.id.upper(), "@NAME@": a.name, "@TITLE@": title, "@EXE@": exe,
             "@MAGIC@": magic}
    shutil.copytree(TEMPLATE, dest, ignore=shutil.ignore_patterns("__pycache__", "*.pyc"))
    for base, _dirs, files in os.walk(dest):
        for f in files:
            p = os.path.join(base, f)
            if os.path.splitext(f)[1] not in TEXT_EXT:
                continue
            s = open(p, encoding="utf-8").read()
            for k, v in subst.items():
                s = s.replace(k, v)
            open(p, "w", encoding="utf-8", newline="\n").write(s)
    os.rename(os.path.join(dest, "game.mk.in"), os.path.join(dest, "game.mk"))
    for sh in ("tests/smoke_test.sh", "tests/ui_test.sh", "tests/state_test.sh"):
        os.chmod(os.path.join(dest, sh), 0o755)
    print("created games/%s (%s, %s.exe)" % (a.id, a.name, exe))
    if not a.no_art:
        subprocess.check_call([sys.executable, os.path.join(dest, "tools", "make_art.py")])
    print("next: make %s-check" % a.id)


if __name__ == "__main__":
    main()
