#!/usr/bin/env python3
"""The launcher's translated text (docs/i18n.md), with the games' tool, tools/i18n_build.py.

    launcher_text.py --out build/bundle/gen/launcher [--unifont FILE.hex]

apps/launcher/strings/<lang>.txt (the 22 languages, en.txt the source) and the house kit's strings -> game_text.c and
game_text.h in --out, exactly as for a game (TR(KEY) in launcher.c). The launcher is not in games/, so the tool is run
on a copy of its strings under a scratch "games/launcher/" folder; the wide glyphs (CJK) that the games' font subset
(games/common/fonts/unifont-subset.hex) lacks are taken from the full GNU Unifont (SDL2's source release has it:
build/deps/src/SDL2-*/test/unifont-13.0.06.hex, tools/fetch_sdl2_src.sh).
The bundle compiles game_text.c with -Drs_i18n_register=rsb_launcher_i18n_capture: the runtime installs the launcher's
pack when the launcher starts (rsb_runtime.c), like each game's.
MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
"""
import argparse
import glob
import os
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import i18n_build as ib  # noqa: E402

STRINGS = os.path.join(ROOT, "apps", "launcher", "strings")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--unifont")
    a = ap.parse_args()
    out = os.path.abspath(a.out)
    work = os.path.join(out, "i18n-work")
    shutil.rmtree(work, ignore_errors=True)
    os.makedirs(os.path.join(work, "games", "launcher", "strings"))
    for f in glob.glob(os.path.join(STRINGS, "*.txt")):
        shutil.copy(f, os.path.join(work, "games", "launcher", "strings"))

    # the font: the games' subset, plus the launcher's wide characters it lacks
    need = ib.wide_chars(ib.load_set(STRINGS), ib.load_set(ib.KIT_DIR))
    subset = ib.load_unifont(ib.FONT_SUBSET) if os.path.exists(ib.FONT_SUBSET) else {}
    missing = sorted(c for c in (ord(ch) for ch in need) if c not in subset)
    font_path = ib.FONT_SUBSET
    if missing:
        hexes = [a.unifont] if a.unifont else sorted(glob.glob(os.path.join(ROOT, "build", "deps", "src", "SDL2-*",
                                                                            "test", "unifont-*.hex")))
        if not hexes or not os.path.exists(hexes[-1]):
            raise SystemExit("launcher_text: no full Unifont .hex for %d wide characters (run tools/fetch_sdl2_src.sh)"
                             % len(missing))
        full = ib.load_unifont(hexes[-1])
        lost = [c for c in missing if c not in full]
        if lost:
            raise SystemExit("launcher_text: Unifont has no glyph for " + " ".join("U+%04X" % c for c in lost))
        font_path = os.path.join(work, "unifont.hex")
        with open(font_path, "w", encoding="utf-8") as f:
            f.write("# the games' subset and the launcher's characters (GNU Unifont 13.0.06, SIL OFL 1.1)\n")
            for c in sorted(set(subset) | set(missing)):
                f.write("%04X:%s\n" % (c, subset.get(c) or full[c]))
    ib.FONT_SUBSET = font_path
    ib.ROOT = work
    ib.gen_game("launcher", out)
    shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    main()
