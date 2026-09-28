#!/usr/bin/env python3
"""The sprite-sheet layout of Bomber Mole: the single source of truth.

Used by tools/make_placeholders.py, tools/cut_ai_sheet.py, the game's asset
build and docs/art-sheets.md (python3 tools/sheets.py --markdown prints the
table). Every sheet is a grid of 16x16 cells on a magenta (#FF00FF)
background. Frames of one entry are laid out left to right.

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
"""
import sys
from collections import namedtuple

CELL = 16
MAGENTA = (255, 0, 255)
SEASONS = ["spring", "summer", "autumn", "winter"]

SHEETS = {
    "characters": {"cols": 16, "rows": 6, "file": "characters.png"},
    "tiles": {"cols": 18, "rows": 4, "file": "tiles.png"},
    "items_fx": {"cols": 16, "rows": 4, "file": "items_fx.png"},
}

# name, sheet, col, row, w, h, frames, group (palette group), description
# For the tiles sheet, `row` is None: one row per season (0 spring .. 3 winter).
Entry = namedtuple("Entry", "name sheet col row w h frames group desc")


def E(name, sheet, col, row, w, h, frames, group, desc):
    return Entry(name, sheet, col, row, w, h, frames, group, desc)


ENTRIES = [
    # ---- characters.png (reading order = the order of the art brief) --------
    E("mole_walk_down", "characters", 0, 0, 16, 16, 3, "mole", "Mole walking toward the viewer (front view); frame 0 = standing"),
    E("mole_walk_up", "characters", 3, 0, 16, 16, 3, "mole", "Mole walking away (back view)"),
    E("mole_walk_left", "characters", 6, 0, 16, 16, 3, "mole", "Mole walking left (side view)"),
    E("mole_walk_right", "characters", 9, 0, 16, 16, 3, "mole", "Mole walking right (side view)"),
    E("mole_dig_down", "characters", 12, 0, 16, 16, 2, "mole", "Mole digging downward (front view), claws out, dirt flying"),
    E("mole_dig_up", "characters", 14, 0, 16, 16, 2, "mole", "Mole digging upward"),
    E("mole_dig_left", "characters", 0, 1, 16, 16, 2, "mole", "Mole digging left, claws forward"),
    E("mole_dig_right", "characters", 2, 1, 16, 16, 2, "mole", "Mole digging right, claws forward"),
    E("mole_place_bomb", "characters", 4, 1, 16, 16, 1, "mole", "Mole dropping a bomb"),
    E("mole_hurt", "characters", 5, 1, 16, 16, 1, "mole", "Mole hurt: flinching, dazed"),
    E("mole_death", "characters", 6, 1, 16, 16, 4, "mole", "Mole knocked out: dazed, falling, flat, ghost"),
    E("mole_victory", "characters", 10, 1, 16, 16, 2, "mole", "Mole cheering, paws up (2-frame loop)"),
    E("ferret_walk_down", "characters", 0, 2, 16, 16, 2, "ferret", "Ferret running toward the viewer"),
    E("ferret_walk_up", "characters", 2, 2, 16, 16, 2, "ferret", "Ferret running away"),
    E("ferret_walk_left", "characters", 4, 2, 16, 16, 2, "ferret", "Ferret running left (long body)"),
    E("ferret_walk_right", "characters", 6, 2, 16, 16, 2, "ferret", "Ferret running right (long body)"),
    E("ferret_stunned", "characters", 8, 2, 16, 16, 1, "ferret", "Ferret stunned: dizzy, stars over the head"),
    E("cat_walk_down", "characters", 0, 3, 16, 16, 2, "cat", "Cat walking toward the viewer"),
    E("cat_walk_up", "characters", 2, 3, 16, 16, 2, "cat", "Cat walking away"),
    E("cat_walk_left", "characters", 4, 3, 16, 16, 2, "cat", "Cat walking left"),
    E("cat_walk_right", "characters", 6, 3, 16, 16, 2, "cat", "Cat walking right"),
    E("cat_pounce", "characters", 8, 3, 16, 16, 2, "cat", "Cat pounce: crouch, then leap (facing right; flipped in game)"),
    E("boss", "characters", 0, 4, 32, 32, 4, "boss", "Boss (big cat in spring), 32x32: idle, walk, crouch, roar"),
    # ---- tiles.png: one row per season ---------------------------------------
    E("grass", "tiles", 0, None, 16, 16, 1, "terrain", "Surface floor: grass (snow in winter)"),
    E("grass_edge", "tiles", 1, None, 16, 16, 1, "terrain", "Surface floor with a shadow/edge along the top (below a wall)"),
    E("soft_dirt", "tiles", 2, None, 16, 16, 1, "terrain", "Soft dirt block (diggable; dry soil in summer)"),
    E("dirt_crack", "tiles", 3, None, 16, 16, 1, "terrain", "Soft dirt half dug, cracked"),
    E("hard_rock", "tiles", 4, None, 16, 16, 1, "terrain", "Hard rock block (bombs only)"),
    E("stone", "tiles", 5, None, 16, 16, 1, "terrain", "Unbreakable stone block"),
    E("roots", "tiles", 6, None, 16, 16, 1, "terrain", "Tree roots blocking the way (a blast clears them, they regrow)"),
    E("tunnel", "tiles", 7, None, 16, 16, 1, "terrain", "Tunnel floor (underground)"),
    E("hole_down", "tiles", 8, None, 16, 16, 1, "terrain", "Burrow hole leading down"),
    E("hole_up", "tiles", 9, None, 16, 16, 1, "terrain", "Hole up: light falling from the depth above"),
    E("ladder", "tiles", 10, None, 16, 16, 1, "terrain", "Ladder leading up"),
    E("thin_floor", "tiles", 11, None, 16, 16, 1, "terrain", "Thin floor: cracked, a bomb breaks it into a hole"),
    E("exit_closed", "tiles", 12, None, 16, 16, 1, "terrain", "Exit molehill, closed"),
    E("exit_open", "tiles", 13, None, 16, 16, 1, "terrain", "Exit molehill, open (glowing)"),
    E("puddle", "tiles", 14, None, 16, 16, 1, "terrain", "Puddle (slows down; stops blasts)"),
    E("frozen_dirt", "tiles", 15, None, 16, 16, 1, "terrain", "Snow and frozen dirt: frozen soil block (cannot be dug; bombs only)"),
    E("leaves", "tiles", 16, None, 16, 16, 1, "terrain", "Pile of leaves (may hide an item; dig or blast it)"),
    E("water", "tiles", 17, None, 16, 16, 1, "terrain", "EXTRA (optional): water, impassable, blasts cross it"),
    # ---- items_fx.png ------------------------------------------------------------
    E("bomb", "items_fx", 0, 0, 16, 16, 3, "bomb", "Bomb with a burning fuse (3-frame pulse)"),
    E("expl_center", "items_fx", 3, 0, 16, 16, 3, "fx", "Explosion centre (3 frames: flash, full, fading)"),
    E("expl_h", "items_fx", 6, 0, 16, 16, 3, "fx", "Explosion arm, horizontal (fills the cell width)"),
    E("expl_v", "items_fx", 9, 0, 16, 16, 3, "fx", "Explosion arm, vertical (fills the cell height)"),
    E("expl_end_left", "items_fx", 12, 0, 16, 16, 3, "fx", "Explosion arm end, tip pointing left"),
    E("expl_end_right", "items_fx", 0, 1, 16, 16, 3, "fx", "Explosion arm end, tip pointing right"),
    E("expl_end_up", "items_fx", 3, 1, 16, 16, 3, "fx", "Explosion arm end, tip pointing up"),
    E("expl_end_down", "items_fx", 6, 1, 16, 16, 3, "fx", "Explosion arm end, tip pointing down"),
    E("grub", "items_fx", 9, 1, 16, 16, 2, "pickup", "Golden grub (2-frame wiggle)"),
    E("pu_bomb", "items_fx", 11, 1, 16, 16, 1, "pickup", "Power-up: one more bomb"),
    E("pu_fire", "items_fx", 12, 1, 16, 16, 1, "pickup", "Power-up: longer blast"),
    E("pu_speed", "items_fx", 13, 1, 16, 16, 1, "pickup", "Power-up: speed (claws)"),
    E("pu_remote", "items_fx", 14, 1, 16, 16, 1, "pickup", "Power-up: remote detonator"),
    E("pu_heart", "items_fx", 15, 1, 16, 16, 1, "pickup", "Power-up: heart (one extra hit)"),
    E("hud_heart", "items_fx", 0, 2, 16, 16, 1, "hud", "HUD icon: heart / lives"),
    E("hud_digit", "items_fx", 1, 2, 16, 16, 10, "hud", "HUD digits 0-9 (big, readable)"),
    E("hud_bomb", "items_fx", 11, 2, 16, 16, 1, "hud", "HUD icon: bombs"),
    E("hud_fire", "items_fx", 12, 2, 16, 16, 1, "hud", "HUD icon: blast range"),
    E("hud_grub", "items_fx", 13, 2, 16, 16, 1, "hud", "HUD icon: golden grubs left"),
    E("hud_speed", "items_fx", 14, 2, 16, 16, 1, "hud", "HUD icon: speed (claws)"),
    E("hud_slash", "items_fx", 15, 2, 16, 16, 1, "hud", "HUD: slash between numbers"),
    E("hud_depth_surface", "items_fx", 0, 3, 16, 16, 1, "hud", "HUD depth icon: surface"),
    E("hud_depth_under1", "items_fx", 1, 3, 16, 16, 1, "hud", "HUD depth icon: underground 1"),
    E("hud_depth_under2", "items_fx", 2, 3, 16, 16, 1, "hud", "HUD depth icon: underground 2"),
    E("hud_danger", "items_fx", 3, 3, 16, 16, 1, "hud", "HUD icon: danger (exclamation)"),
    E("hud_lock", "items_fx", 4, 3, 16, 16, 1, "hud", "Menu icon: locked level"),
    E("hud_check", "items_fx", 5, 3, 16, 16, 1, "hud", "Menu icon: level cleared"),
    E("hud_cursor", "items_fx", 6, 3, 16, 16, 1, "hud", "Menu cursor (arrow pointing right)"),
    E("hud_panel", "items_fx", 7, 3, 16, 16, 1, "hud", "HUD panel background (solid, dark)"),
    E("dust", "items_fx", 8, 3, 16, 16, 3, "bomb", "Dust puff (digging, landing), growing and fading"),
]

BY_NAME = {e.name: e for e in ENTRIES}
TERRAIN = [e.name for e in ENTRIES if e.sheet == "tiles"]


def entries(sheet):
    return [e for e in ENTRIES if e.sheet == sheet]


def frame_rects(e, season=0):
    """Pixel rectangles (x, y, w, h) of each frame of an entry."""
    row = season if e.row is None else e.row
    cw = e.w // CELL
    return [((e.col + i * cw) * CELL, row * CELL, e.w, e.h) for i in range(e.frames)]


def reading_order(sheet):
    """Every frame of a sheet in table order: (entry, frame, season, rect).
    AI images are expected to show the sprites in this order."""
    out = []
    if sheet == "tiles":
        for s in range(len(SEASONS)):
            for e in entries(sheet):
                out.append((e, 0, s, frame_rects(e, s)[0]))
    else:
        for e in entries(sheet):
            for i, r in enumerate(frame_rects(e)):
                out.append((e, i, 0, r))
    return out


def sheet_size(sheet):
    s = SHEETS[sheet]
    return s["cols"] * CELL, s["rows"] * CELL


def validate():
    """No two frames overlap and all fit in their sheet."""
    for sheet in SHEETS:
        W, H = sheet_size(sheet)
        used = {}
        for e, f, s, (x, y, w, h) in reading_order(sheet):
            assert x + w <= W and y + h <= H, (e.name, "outside the sheet")
            for cy in range(y // CELL, (y + h) // CELL):
                for cx in range(x // CELL, (x + w) // CELL):
                    k = (cx, cy)
                    assert k not in used, (e.name, "overlaps", used[k])
                    used[k] = e.name
    return True


def markdown():
    lines = ["| Name | Sheet | Column | Row | Size | Frames | Palette group | Description |",
             "|---|---|---|---|---|---|---|---|"]
    for e in ENTRIES:
        row = "0-3 (one per season)" if e.row is None else str(e.row)
        cols = str(e.col) if e.frames == 1 else "%d-%d" % (e.col, e.col + e.frames * (e.w // CELL) - 1)
        if e.w > CELL:
            cols = "%d-%d" % (e.col, e.col + e.frames * (e.w // CELL) - 1)
            row = "%d-%d" % (e.row, e.row + e.h // CELL - 1)
        lines.append("| %s | %s.png | %s | %s | %dx%d | %d | %s | %s |" %
                     (e.name, e.sheet, cols, row, e.w, e.h, e.frames, e.group, e.desc))
    return "\n".join(lines)


if __name__ == "__main__":
    validate()
    if "--markdown" in sys.argv:
        print(markdown())
    else:
        for sheet in SHEETS:
            W, H = sheet_size(sheet)
            print("%s: %dx%d px, %d frames" % (sheet, W, H, len(reading_order(sheet))))
