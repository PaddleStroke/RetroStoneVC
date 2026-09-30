#!/usr/bin/env python3
"""Leady Squid: the sheet layout, the single source of truth.

Used by make_art.py (the code-drawn art, final), build_assets.py (the
game's asset build) and art_game.py (the image agent's TODO.md, the import and
the review tool). Sheets are PNGs on a magenta (#FF00FF) background; the frames
of an entry sit left to right from (x, y).

    python3 games/leadysquid/tools/ls_sheets.py      prints the table (Markdown)

All rights reserved, 8BCraft.
"""
from collections import namedtuple

MAGENTA = (255, 0, 255)
THEMES = ["kelp", "coral", "masts", "chains"]

SHEETS = {
    "sprites": {"w": 256, "h": 128, "file": "sprites.png"},
    "obstacles": {"w": 96, "h": 64, "file": "obstacles.png"},
    "props": {"w": 256, "h": 96, "file": "props.png"},
}
# code-drawn panoramas (BG tiles, not in the image agent's TODO by default)
PANORAMAS = {
    "seabed": {"w": 512, "h": 48, "file": "tiles/seabed.png", "layer": "BG2", "y": 192},
    "backdrop": {"w": 512, "h": 192, "file": "tiles/backdrop.png", "layer": "BG4", "y": 0},
    "midground": {"w": 512, "h": 96, "file": "tiles/midground.png", "layer": "BG3", "y": 112},
}
RAYS_END = 112          # backdrop lines 0..111 are the additive rays, 112..191 the opaque reef

# name, sheet, x, y, w, h, frames, group (palette group), description
Entry = namedtuple("Entry", "name sheet x y w h frames group desc")

ENTRIES = [
    # ---- sprites.png --------------------------------------------------------------
    Entry("squid_tilt", "sprites", 0, 0, 32, 32, 6, "squid",
          "the squid swimming right at six angles: +20 (nose up), 0, -22, -45, -67, -90 (nose straight down)"),
    Entry("squid_idle", "sprites", 192, 0, 32, 32, 2, "squid", "get-ready pose, level, tentacles curling (2-frame bob)"),
    Entry("squid_flap", "sprites", 0, 32, 32, 32, 3, "squid", "the jet at +20: squeeze, tentacles fanned out, recover"),
    Entry("squid_hit", "sprites", 96, 32, 32, 32, 1, "squid", "hit: X eyes, tentacles splayed, level"),
    Entry("squid_sink", "sprites", 128, 32, 32, 32, 2, "squid", "sinking nose-down, X eyes, tentacles trailing up"),
    Entry("squid_rest", "sprites", 192, 32, 32, 32, 1, "squid", "knocked out on the seabed"),
    Entry("digits", "sprites", 0, 64, 16, 16, 10, "ui", "big score digits 0-9"),
    Entry("bubble", "sprites", 160, 64, 8, 8, 4, "fx", "bubbles: small, medium, big, pop"),
    Entry("weight", "sprites", 192, 64, 8, 8, 2, "fx", "a lead diving weight: flat, tilted (bounces on the sand)"),
    Entry("sparkle", "sprites", 208, 64, 8, 8, 2, "fx", "medal sparkle, 2 frames"),
    Entry("ink", "sprites", 0, 80, 16, 16, 4, "fx", "the ink and bubble puff of a flap, fading in 4 frames"),
    Entry("hint", "sprites", 64, 80, 16, 16, 2, "ui", "the 'press A' button icon: up, pressed"),
    Entry("medal", "sprites", 0, 96, 24, 24, 4, "ui", "shell medals: bronze, silver, gold, pearl"),
    # ---- obstacles.png: one row per theme (bodies are BG tiles, caps are sprites) ---
] + [e for t, th in enumerate(THEMES) for e in (
    Entry("%s_body" % th, "obstacles", 0, t * 16, 24, 16, 2, "theme_%s" % th,
          "%s obstacle body, repeats vertically (2 variants)" % th),
    Entry("%s_cap_top" % th, "obstacles", 48, t * 16, 24, 16, 1, "theme_%s" % th,
          "%s: the lower end of the top obstacle (its bottom edge is the top of the gap)" % th),
    Entry("%s_cap_bottom" % th, "obstacles", 72, t * 16, 24, 16, 1, "theme_%s" % th,
          "%s: the upper end of the bottom obstacle (its top edge is the bottom of the gap)" % th),
)] + [
    # ---- props.png: the mid-ground, composed into tiles/midground.png ----------------
    Entry("shipwreck", "props", 0, 0, 128, 64, 1, "mid", "a sunken wooden ship, broken mast, lying on its side"),
    Entry("chest", "props", 128, 0, 32, 24, 1, "mid", "a treasure chest, lid ajar, a gold glint"),
    Entry("rock_small", "props", 160, 0, 32, 24, 1, "mid", "a small rock with barnacles"),
    Entry("rock_big", "props", 192, 0, 64, 40, 1, "mid", "a big rock"),
    Entry("coral_clump", "props", 128, 32, 32, 32, 1, "mid", "a clump of coral"),
    Entry("kelp_clump", "props", 160, 32, 16, 48, 1, "mid", "a tall kelp plant"),
    Entry("amphora", "props", 176, 40, 16, 24, 1, "mid", "an old amphora half in the sand"),
]

# where the props sit on the 512x96 mid-ground (x, bottom y); the band starts at screen y 112
MID_LAYOUT = [("rock_big", 8, 88), ("kelp_clump", 70, 90), ("chest", 96, 90), ("coral_clump", 150, 90),
              ("shipwreck", 200, 92), ("rock_small", 336, 90), ("amphora", 374, 92), ("kelp_clump", 400, 92),
              ("coral_clump", 428, 90), ("rock_small", 470, 88)]

GROUPS = {"squid": "OBJ 0 (player 2: OBJ 1, recoloured pink)", "fx": "OBJ 2", "ui": "OBJ 3",
          "mid": "BG 6"}
for _t, _th in enumerate(THEMES):
    GROUPS["theme_%s" % _th] = "BG %d (bodies) + OBJ %d (caps)" % (2 + _t, 4 + _t)


def entry(name):
    for e in ENTRIES:
        if e.name == name:
            return e
    raise KeyError(name)


def entries(sheet):
    return [e for e in ENTRIES if e.sheet == sheet]


def frame_rects(e):
    return [(e.x + i * e.w, e.y, e.w, e.h) for i in range(e.frames)]


def markdown():
    out = ["| Entry | Sheet | Frame | Frames | Palette | Description |", "|---|---|---|---|---|---|"]
    for e in ENTRIES:
        out.append("| %s | %s.png (%d,%d) | %dx%d | %d | %s | %s |" % (
            e.name, e.sheet, e.x, e.y, e.w, e.h, e.frames, GROUPS.get(e.group, e.group), e.desc))
    return "\n".join(out)


if __name__ == "__main__":
    print(markdown())
