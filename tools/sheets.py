#!/usr/bin/env python3
"""The sprite-sheet layout of Bomber Mole: the single source of truth.

Used by tools/make_placeholders.py, tools/cut_ai_sheet.py, the game's asset
build and docs/art-sheets.md (python3 tools/sheets.py --markdown prints the
table). Every sheet is a grid of 16x16 cells on a magenta (#FF00FF)
background. Frames of one entry are laid out left to right.

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
"""
import os
import sys
from collections import namedtuple

CELL = 16
MAGENTA = (255, 0, 255)
SEASONS = ["spring", "summer", "autumn", "winter"]

SHEETS = {
    "characters": {"cols": 16, "rows": 6, "file": "characters.png"},
    "tiles": {"cols": 18, "rows": 4, "file": "tiles.png"},
    "items_fx": {"cols": 16, "rows": 4, "file": "items_fx.png"},
    "props": {"cols": 16, "rows": 10, "file": "props.png"},
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
    E("boss", "characters", 0, 4, 32, 32, 4, "boss_cat", "Spring boss: the barn cat, 32x32: idle, walk, crouch, roar"),
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
    # ---- props.png: level gimmicks ("actuators") and extra bosses ------------------
    # row 0-1: 16x16 terrain-like props (drawn on the terrain layer, BG palette 5)
    E("bridge", "props", 0, 0, 16, 16, 1, "propbg", "Wooden bridge over water (a blast destroys it)"),
    E("ice", "props", 1, 0, 16, 16, 1, "propbg", "Slippery ice (you slide until you hit something)"),
    E("thin_ice", "props", 2, 0, 16, 16, 2, "propbg", "Thin ice: intact, cracked (breaks into water after 2 crossings)"),
    E("mud", "props", 4, 0, 16, 16, 1, "propbg", "Mud (under a puddle; slows you)"),
    E("tall_grass", "props", 5, 0, 16, 16, 1, "propbg", "Tall grass: cover, hides you from cats, burns"),
    E("corn", "props", 6, 0, 16, 16, 1, "propbg", "Corn field (summer cover), burns"),
    E("burnt", "props", 7, 0, 16, 16, 1, "propbg", "Burnt ground (after cover burns)"),
    E("gate", "props", 8, 0, 16, 16, 2, "propbg", "Gate: closed, open"),
    E("plate", "props", 10, 0, 16, 16, 2, "propbg", "Pressure plate: up, pressed"),
    E("lever", "props", 12, 0, 16, 16, 2, "propbg", "Lever: off (left), on (right)"),
    E("steam_vent", "props", 14, 0, 16, 16, 2, "propbg", "Steam vent (summer, depth 2): idle, erupting"),
    E("pipe", "props", 0, 1, 16, 16, 1, "propbg", "Drain pipe opening (teleports to its twin)"),
    E("crate", "props", 1, 1, 16, 16, 1, "propbg", "Tomato crate (farmer boss): breakable"),
    E("splat", "props", 2, 1, 16, 16, 1, "propbg", "Tomato splat on the floor (slippery for a few seconds)"),
    E("beehive", "props", 3, 1, 16, 16, 1, "propbg", "Beehive (summer): bomb it and bees chase the nearest creature"),
    E("well", "props", 4, 1, 16, 16, 1, "propbg", "Well with crank (elevator surface <-> depth 2)"),
    E("mushroom", "props", 5, 1, 16, 16, 1, "propbg", "Bouncy mushroom (autumn): launches you 2 tiles"),
    E("rails_h", "props", 6, 1, 16, 16, 1, "propbg", "Mine-cart rails, horizontal"),
    E("rails_v", "props", 7, 1, 16, 16, 1, "propbg", "Mine-cart rails, vertical"),
    E("apple_tree", "props", 8, 1, 16, 16, 1, "propbg", "Apple tree (autumn): bomb it, apples fall and stun"),
    # row 1-2: 16x16 sprites (sprite palette 6)
    E("sprinkler", "props", 9, 1, 16, 16, 4, "prop", "Garden sprinkler, spraying up, right, down, left"),
    E("spray", "props", 13, 1, 16, 16, 2, "prop", "Water spray / stream current particles"),
    E("log", "props", 15, 1, 16, 16, 1, "prop", "Floating log (pushable bridge on water)"),
    E("wind", "props", 0, 2, 16, 16, 3, "prop", "Wind gust particles (drawn blowing right; flipped)"),
    E("pumpkin", "props", 3, 2, 16, 16, 1, "prop", "Pumpkin (autumn): push it to block enemies"),
    E("snowball", "props", 4, 2, 16, 16, 2, "prop", "Snowball: small, big (rolls and grows when blasted)"),
    E("icicle", "props", 6, 2, 16, 16, 1, "prop", "Icicle (falls after nearby blasts)"),
    E("mine_cart", "props", 7, 2, 16, 16, 1, "prop", "Mine cart"),
    E("bucket", "props", 8, 2, 16, 16, 1, "prop", "Well bucket (elevator)"),
    E("tomato", "props", 9, 2, 16, 16, 2, "prop", "Thrown tomato (2-frame spin)"),
    E("tomato_shadow", "props", 11, 2, 16, 16, 1, "prop", "Target shadow of a falling tomato"),
    E("apple", "props", 12, 2, 16, 16, 1, "prop", "Falling apple"),
    E("zzz", "props", 13, 2, 16, 16, 2, "prop", "Sleeping 'Zz' bubble"),
    E("steam", "props", 15, 2, 16, 16, 1, "prop", "Steam puff (vents)"),
    # row 3: critters (sprite palette 7)
    E("dog_walk_left", "props", 0, 3, 16, 16, 2, "critter", "Guard dog (ally) running left"),
    E("dog_walk_right", "props", 2, 3, 16, 16, 2, "critter", "Guard dog (ally) running right"),
    E("dog_sleep", "props", 4, 3, 16, 16, 1, "critter", "Guard dog asleep"),
    E("bees", "props", 5, 3, 16, 16, 2, "critter", "Swarm of bees"),
    # rows 4-9: 32x32 (bosses: sprite palette 3, loaded per level; windmill: palette 6)
    E("windmill", "props", 0, 4, 32, 32, 4, "prop", "Windmill (spring signature), sails turning (4 frames)"),
    E("fox", "props", 8, 4, 32, 32, 4, "boss_fox", "Autumn boss: the fox (run, run, leap, hurt)"),
    E("farmer", "props", 0, 6, 32, 32, 8, "boss_farmer", "Summer boss: the farmer: idle x2, throw x2, angry x2, hurt x2"),
    E("owl", "props", 0, 8, 32, 32, 4, "boss_owl", "Winter boss: the snowy owl (perch, flap, swoop, hurt)"),
    E("badger", "props", 8, 8, 32, 32, 4, "boss_badger", "Summer mini-boss: the badger (walk x2, dig, hurt)"),
]

# Character size (owner's choice pending): 16, 24 or 32. The characters sheet
# uses cells of this size (the boss is 2x2 cells), everything else stays 16.
# Set with the environment variable BM_CHAR_SIZE (make CHAR_SIZE=24).
CHAR_SIZE = int(os.environ.get("BM_CHAR_SIZE", "16"))
if CHAR_SIZE not in (16, 24, 32):
    raise SystemExit("BM_CHAR_SIZE must be 16, 24 or 32")
SHEETS["characters"]["cell"] = CHAR_SIZE
ENTRIES = [e._replace(w=e.w * CHAR_SIZE // CELL, h=e.h * CHAR_SIZE // CELL) if e.sheet == "characters" else e
           for e in ENTRIES]


def cell_of(sheet):
    return SHEETS[sheet].get("cell", CELL)


BY_NAME = {e.name: e for e in ENTRIES}
TERRAIN = [e.name for e in ENTRIES if e.sheet == "tiles"]


def entries(sheet):
    return [e for e in ENTRIES if e.sheet == sheet]


def frame_rects(e, season=0):
    """Pixel rectangles (x, y, w, h) of each frame of an entry."""
    c = cell_of(e.sheet)
    row = season if e.row is None else e.row
    cw = e.w // c
    return [((e.col + i * cw) * c, row * c, e.w, e.h) for i in range(e.frames)]


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
    return s["cols"] * cell_of(sheet), s["rows"] * cell_of(sheet)


def validate():
    """No two frames overlap and all fit in their sheet."""
    for sheet in SHEETS:
        W, H = sheet_size(sheet)
        used = {}
        for e, f, s, (x, y, w, h) in reading_order(sheet):
            assert x + w <= W and y + h <= H, (e.name, "outside the sheet")
            c = cell_of(sheet)
            for cy in range(y // c, (y + h) // c):
                for cx in range(x // c, (x + w) // c):
                    k = (cx, cy)
                    assert k not in used, (e.name, "overlaps", used[k])
                    used[k] = e.name
    return True


def markdown():
    lines = ["| Name | Sheet | Column | Row | Size | Frames | Palette group | Description |",
             "|---|---|---|---|---|---|---|---|"]
    for e in ENTRIES:
        row = "0-3 (one per season)" if e.row is None else str(e.row)
        c = cell_of(e.sheet)
        cols = str(e.col) if e.frames == 1 else "%d-%d" % (e.col, e.col + e.frames * (e.w // c) - 1)
        if e.w > c:
            cols = "%d-%d" % (e.col, e.col + e.frames * (e.w // c) - 1)
            row = "%d-%d" % (e.row, e.row + e.h // c - 1)
        lines.append("| %s | %s.png | %s | %s | %dx%d | %d | %s | %s |" %
                     (e.name, e.sheet, cols, row, e.w, e.h, e.frames, e.group, e.desc))
    return "\n".join(lines)


if __name__ == "__main__":
    validate()
    if "--update-doc" in sys.argv:
        # docs/art-sheets.md: replace everything after "## The table" with the current table
        path = sys.argv[sys.argv.index("--update-doc") + 1]
        text = open(path, encoding="utf-8").read()
        head = text.split("## The table")[0]
        with open(path, "w", encoding="utf-8", newline="\n") as f:
            f.write(head + "## The table\n\n" + markdown() + "\n")
        print("updated", path)
        sys.exit(0)
    if "--markdown" in sys.argv:
        print(markdown())
    else:
        for sheet in SHEETS:
            W, H = sheet_size(sheet)
            print("%s: %dx%d px, %d frames" % (sheet, W, H, len(reading_order(sheet))))
