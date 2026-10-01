#!/usr/bin/env python3
"""Pogo Mamie: the art layout, the single source of truth.

Used by make_art.py (the code-drawn art) and build_assets.py (the game's asset build). Every colour of every
sheet comes from the palettes below, in a FIXED order: the asset build maps a pixel to its palette index
exactly (no quantisation), so the C code can recolour palettes per district and at night.

  - BG_TILES: the play layer's 8x8 tiles (BG2), each with its BG palette; the C code composes buildings from
    them by name (T_<NAME>). tiles/bg2.png, 16 tiles per row.
  - SPRITES: the sprite entries (name, frame size, frames, OBJ palette), packed into sprites.png row by row.
  - PANORAMAS: the mid-ground (BG3) and the four district backdrops (BG4), and the title logo (BG1).

    python3 games/pogomamie/tools/pm_sheets.py      prints the tables (Markdown)

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pogomamie/LICENSE.
"""
from collections import namedtuple

MAGENTA = (255, 0, 255)

# ---- palettes (index 0 = transparent; the names are the colour indices 1..15) --------------------------------------
BG_PALS = {
    # 0: the UI (games/common/src/house_ui.c loads it); 5: the title logo (house_ui.c hu_logo)
    # 1: roofs: zinc, terracotta, chimneys, skylights, attics, ropes
    "roof": [("out", (26, 28, 44)), ("zinc_h", (200, 210, 224)), ("zinc_l", (156, 170, 190)), ("zinc_m", (116, 130, 156)),
             ("zinc_d", (80, 90, 116)), ("terra_l", (228, 136, 96)), ("terra_m", (186, 92, 64)), ("terra_d", (124, 56, 44)),
             ("stucco_l", (212, 180, 150)), ("stucco_d", (160, 126, 104)), ("glass_l", (182, 222, 238)),
             ("glass_d", (72, 112, 152)), ("attic", (36, 28, 40)), ("attic_l", (84, 64, 60)), ("rope", (206, 196, 170))],
    # 2: Haussmann stone facades
    "stone": [("out", (44, 36, 44)), ("st_h", (248, 236, 210)), ("st_l", (228, 210, 176)), ("st_m", (198, 176, 140)),
              ("st_d", (150, 128, 102)), ("glass_d", (40, 54, 82)), ("glass_l", (108, 136, 172)), ("iron", (32, 30, 38)),
              ("frame", (238, 236, 226)), ("shop", (140, 38, 44)), ("shop_gold", (232, 190, 92)),
              ("shop_d", (64, 24, 30)), ("door", (98, 66, 46))],
    # 3: Montmartre houses, the Seine's quays and bridges, the bouquinistes' boxes
    "house": [("out", (38, 34, 42)), ("wash_h", (252, 246, 232)), ("wash_l", (234, 222, 198)), ("wash_d", (192, 176, 154)),
              ("shut_l", (98, 164, 108)), ("shut_d", (52, 106, 72)), ("glass_d", (46, 56, 82)), ("glass_l", (112, 142, 172)),
              ("quay_l", (208, 192, 160)), ("quay_m", (172, 154, 124)), ("quay_d", (124, 108, 88)), ("door", (112, 72, 52)),
              ("box_l", (74, 132, 92)), ("box_d", (36, 80, 56)), ("lamp", (230, 200, 120))],
    # 4: the street, the river, the barges
    "ground": [("out", (20, 20, 30)), ("walk_l", (184, 178, 170)), ("walk_d", (142, 138, 132)), ("curb", (212, 208, 198)),
               ("road_l", (94, 94, 104)), ("road_d", (66, 66, 78)), ("water_l", (122, 172, 194)), ("water_m", (72, 122, 154)),
               ("water_d", (42, 82, 114)), ("foam", (222, 238, 244)), ("hull_l", (152, 62, 52)), ("hull_d", (90, 34, 36)),
               ("deck", (172, 122, 72)), ("deck_d", (122, 82, 52)), ("cabin", (236, 230, 214))],
    # 6: the mid-ground roofs (BG3): recoloured per district and at night by the C code (tones 1-4 + windows)
    "mid": [("m1", (120, 110, 150)), ("m2", (98, 88, 130)), ("m3", (78, 70, 112)), ("m4", (60, 54, 92)),
            ("mwin", (240, 200, 120))],
    # 7: the district backdrops (BG4): far silhouettes, tones 1-5 (1 = lightest, the haze), 6 = lights
    "far": [("f1", (200, 170, 190)), ("f2", (170, 142, 170)), ("f3", (140, 116, 150)), ("f4", (112, 92, 130)),
            ("f5", (236, 226, 230)), ("flight", (255, 230, 140))],
}
BG_PAL_INDEX = {"roof": 1, "stone": 2, "house": 3, "ground": 4, "mid": 6, "far": 7}   # 0 UI, 5 logo (the kit)

OBJ_PALS = {
    # the hero: the house skin ramp, the game's pink accent (house_style.ACCENTS["pink"]) on the headscarf
    "mamie": [("out", (40, 16, 58)), ("skin_l", (255, 222, 196)), ("skin_d", (206, 140, 108)), ("scarf_l", (246, 150, 190)),
              ("scarf_d", (214, 92, 146)), ("cardi_l", (150, 110, 196)), ("cardi_d", (100, 66, 142)), ("skirt", (72, 74, 98)),
              ("bag_l", (150, 96, 54)), ("bag_d", (100, 62, 38)), ("metal_l", (214, 222, 232)), ("metal_d", (120, 130, 152)),
              ("hair", (200, 200, 212)), ("white", (250, 250, 252)), ("spring", (230, 190, 70))],
    "animals": [("out", (28, 24, 32)), ("cat_l", (246, 170, 78)), ("cat_m", (206, 118, 44)), ("cat_d", (132, 70, 32)),
                ("white", (250, 248, 240)), ("pink", (240, 140, 150)), ("pg_l", (176, 182, 198)), ("pg_m", (128, 134, 154)),
                ("pg_d", (84, 90, 110)), ("pg_green", (96, 160, 128)), ("pg_purple", (130, 96, 160)),
                ("beak", (236, 150, 120)), ("eye", (250, 214, 70))],
    "props": [("out", (30, 26, 34)), ("red", (214, 60, 62)), ("red_d", (148, 34, 42)), ("white", (246, 240, 230)),
              ("white_d", (196, 188, 178)), ("pot_l", (206, 112, 72)), ("pot_d", (140, 70, 50)), ("flower", (244, 110, 160)),
              ("leaf_l", (100, 168, 84)), ("leaf_d", (52, 112, 62)), ("wood_l", (170, 118, 66)), ("wood_d", (112, 72, 42)),
              ("metal_l", (178, 184, 198)), ("metal_d", (96, 102, 118)), ("blue", (96, 140, 206))],
    "items": [("out", (26, 22, 34)), ("umb_l", (92, 110, 190)), ("umb_d", (44, 54, 110)), ("handle", (140, 86, 44)),
              ("bread_l", (240, 196, 118)), ("bread_m", (206, 144, 70)), ("bread_d", (146, 94, 44)), ("yarn_l", (240, 124, 170)),
              ("yarn_d", (192, 66, 116)), ("needle", (214, 216, 226)), ("glow", (255, 250, 210)), ("glow_d", (250, 220, 120)),
              ("white", (255, 255, 255))],
    "fx": [("out", (30, 26, 36)), ("dust_l", (236, 230, 216)), ("dust_d", (194, 188, 174)), ("glass_l", (206, 238, 250)),
           ("glass_d", (130, 180, 212)), ("deb_l", (196, 104, 72)), ("deb_d", (132, 72, 50)), ("wind_l", (255, 255, 255)),
           ("wind_d", (206, 226, 246)), ("water_l", (190, 226, 244)), ("water_d", (110, 170, 214)), ("white", (252, 252, 252)),
           ("curse_r", (222, 44, 44)), ("curse_y", (250, 212, 60)), ("star", (255, 236, 150))],
    # the kit sprites' palette (games/common/src/house_ui.c hu_obj_palette), exactly: our own UI sprites share it
    "ui": [("out", (22, 18, 40)), ("white", (250, 250, 250)), ("shade", (168, 200, 232)), ("bd", (138, 76, 38)),
           ("bl", (210, 142, 82)), ("sd", (128, 138, 156)), ("sl", (222, 230, 238)), ("gd", (186, 128, 22)),
           ("gl", (252, 216, 72)), ("pd", (200, 138, 170)), ("pl", (242, 202, 214)), ("pearl", (255, 250, 238)),
           ("spark", (255, 238, 150)), ("sparkw", (255, 255, 255))],
}
# The power-ups are drawn in the kit sprites' colours (OBJ 3) and the café in the props' (OBJ 7): two palettes free
# for players 3 and 4. Their art keeps its own colour names, aliases of those palettes' colours.
_UI, _PROPS = dict(OBJ_PALS["ui"]), dict(OBJ_PALS["props"])
OBJ_PALS["items"] = [(k, _UI[v]) for k, v in (
    ("out", "out"), ("umb_l", "shade"), ("umb_d", "sd"), ("handle", "bd"), ("bread_l", "gl"), ("bread_m", "bl"),
    ("bread_d", "bd"), ("yarn_l", "pl"), ("yarn_d", "pd"), ("needle", "sl"), ("glow", "spark"), ("glow_d", "gd"),
    ("white", "white"))]
OBJ_PALS["cafe"] = [(k, _PROPS[v]) for k, v in (
    ("out", "out"), ("green", "red"), ("green_d", "red_d"), ("cream", "white"), ("cream_d", "white_d"), ("gold", "blue"),
    ("ring_r", "red"), ("ring_w", "white"))]
# the house layout (docs/art-direction.md): players 1-4 on OBJ 0, 1, 4, 5; 2 effects, 3 the kit sprites (and our
# power-ups), 6-7 the game's own
OBJ_PAL_INDEX = {"mamie": 0, "papi": 1, "fx": 2, "ui": 3, "tata": 4, "tonton": 5, "animals": 6, "props": 7}

# The family (players 2-4): Mamie's palette recoloured. Papi: the headscarf -> grey hair under a navy beret overlay,
# the cardigan -> a brown jacket, the handbag -> a baguette-coloured satchel. Tata (the aunt): a sunflower
# headscarf, a green cardigan. Tonton (the uncle): white hair under Papi's beret in bottle green, a brick jacket.
PAPI = {"scarf_l": (206, 206, 216), "scarf_d": (150, 150, 166), "cardi_l": (150, 112, 76), "cardi_d": (98, 70, 48),
        "skirt": (58, 62, 84), "bag_l": (226, 180, 106), "bag_d": (168, 120, 60), "spring": (120, 200, 230)}
TATA = {"scarf_l": (252, 214, 84), "scarf_d": (214, 156, 36), "cardi_l": (96, 176, 112), "cardi_d": (52, 116, 76),
        "skirt": (92, 60, 48), "bag_l": (90, 120, 200), "bag_d": (52, 72, 140), "spring": (240, 120, 170)}
TONTON = {"scarf_l": (236, 236, 240), "scarf_d": (180, 180, 192), "cardi_l": (214, 112, 72), "cardi_d": (150, 66, 46),
          "skirt": (40, 92, 64), "bag_l": (160, 160, 172), "bag_d": (100, 100, 116), "spring": (150, 220, 100)}
PLAYER_PALS = {"papi": PAPI, "tata": TATA, "tonton": TONTON}
PLAYER_NAMES = ["MAMIE", "PAPI", "TATA", "TONTON"]


def pal_rgb(pals, name):
    return dict(pals[name])


# ---- BG2 tiles ----------------------------------------------------------------------------------------------------
# name, palette. The C code refers to them as T_<NAME>. Shapes that repeat mirrored use the map's h-flip.
BG_TILES = [
    ("blank", "roof"),
    # zinc and terracotta roofs
    ("zinc_top", "roof"), ("zinc_top_end", "roof"), ("mans_diag", "roof"), ("mans_face", "roof"),
    ("mans_dormer_t", "roof"), ("mans_dormer_b", "roof"), ("mans_face_b", "roof"),
    ("pitch_a", "roof"), ("pitch_b", "roof"), ("pitch_fill", "roof"), ("pitch_eave", "roof"),
    ("chim_top", "roof"), ("chim_body", "roof"), ("chim_base", "roof"),
    ("sky_glass", "roof"), ("sky_broken", "roof"), ("attic", "roof"), ("attic_floor", "roof"),
    ("rope", "roof"), ("davit_arm", "roof"), ("davit_end", "roof"),
    # Haussmann stone
    ("cornice", "stone"), ("st_wall", "stone"), ("st_edge", "stone"), ("st_band", "stone"),
    ("st_win_t", "stone"), ("st_win_b", "stone"), ("st_balc", "stone"), ("st_balc_wall", "stone"),
    ("shop_t", "stone"), ("shop_b", "stone"), ("st_door_t", "stone"), ("st_door_b", "stone"),
    # houses
    ("h_eave", "house"), ("h_wall", "house"), ("h_edge", "house"), ("h_win_t", "house"), ("h_win_b", "house"),
    ("h_door_t", "house"), ("h_door_b", "house"), ("h_base", "house"),
    # quays, bridges, the bouquinistes' boxes
    ("q_top", "house"), ("q_wall", "house"), ("q_ring", "house"),
    ("br_top", "house"), ("br_band", "house"), ("br_wall", "house"), ("br_arch_c", "house"), ("br_arch_s", "house"),
    ("br_lamp", "house"),
    ("box_top", "house"), ("box_top_end", "house"), ("box_body", "house"), ("box_body_end", "house"),
    # the ground, the barges
    ("walk", "ground"), ("curb", "ground"), ("road", "ground"), ("road2", "ground"),
    ("water_top", "ground"), ("water", "ground"), ("water2", "ground"),
    ("deck", "ground"), ("deck_end", "ground"), ("hull", "ground"), ("hull_water", "ground"),
    ("cabin_top", "ground"), ("cabin_top_end", "ground"), ("cabin_win", "ground"), ("cabin_end", "ground"),
]
BG_TILE_INDEX = {n: i for i, (n, _) in enumerate(BG_TILES)}
# tiles the bot and the tests treat as NOT a roof (ropes, the davit)
BG_TILE_NONSOLID = {"blank", "rope", "davit_arm", "davit_end"}
# tiles of the ground (a landing there is a fall)
BG_TILE_GROUND = {"walk", "curb", "road", "road2", "water_top", "water", "water2"}
BG_TILE_GLASS = {"sky_glass"}
# chimneys, boxes, cabins: you land on them but they are not walls
BG_TILE_BUMP = {"chim_top", "chim_body", "box_top", "box_top_end", "box_body", "box_body_end", "cabin_top",
                "cabin_top_end", "cabin_win", "cabin_end"}
# a broken skylight's frame and the attic under it: open (the attic's floor is the surface)
BG_TILE_OPEN = {"sky_broken", "attic"}

# ---- sprites ------------------------------------------------------------------------------------------------------
# name, frame w, h, frames, OBJ palette, surface row (landing props: the row her foot stands on; -1 = none)
Sprite = namedtuple("Sprite", "name w h frames pal surface desc")
SPRITES = [
    Sprite("mamie", 24, 32, 13, "mamie", -1,
           "Mamie on her pogo: idle, squash 1-2, stretch, rise, fall, big (knees up), stumble, flail 1-2, sit, "
           "afloat in a ring, hanging from the yarn"),
    Sprite("papi_head", 16, 16, 1, "mamie", -1, "Papi's (Tonton's) beret and moustache (over Mamie's frames)"),
    Sprite("umbrella", 24, 16, 2, "ui", -1, "the open umbrella over her head, 2 frames (sway)"),
    Sprite("cat", 16, 16, 4, "animals", -1, "the cat: sitting looking back, tail flick, leaping, landing"),
    Sprite("pigeon", 16, 16, 7, "animals", -1, "a pigeon: walk 1-2, peck, flap 1-2, glide, dive"),
    Sprite("awning", 24, 16, 2, "props", 0, "a striped awning over a window: taut, pressed"),
    Sprite("pot", 16, 16, 1, "props", 8, "a window box of geraniums (the box's top at row 8)"),
    Sprite("cradle", 32, 16, 1, "props", 0, "a window-cleaner's cradle"),
    Sprite("ledge", 8, 8, 3, "props", 0, "crumbling tiles: left end, middle, right end"),
    Sprite("rope", 8, 16, 16, "props", -1, "a clothesline chord falling 0..15 px over 8 (rising: v-flipped)"),
    Sprite("hook", 8, 8, 1, "props", -1, "the clothesline's hook on a wall"),
    Sprite("clothes", 8, 8, 12, "props", -1, "a shirt, a sock, bloomers, a towel: hanging, swinging left, right"),
    Sprite("antenna", 16, 32, 3, "props", -1, "a TV antenna: still, wobbling left, right"),
    Sprite("beacon", 8, 8, 2, "props", -1, "the antenna's warning light: lit, dim"),
    Sprite("balloon", 32, 32, 2, "props", 0, "a hot-air balloon's envelope (the top is bouncy): round, pressed"),
    Sprite("basket", 16, 16, 1, "props", -1, "the balloon's ropes and basket (a hazard)"),
    Sprite("baguette", 8, 8, 3, "ui", 0, "the baguette plank: left end, middle, right end"),
    Sprite("item", 16, 16, 4, "ui", -1, "power-ups: umbrella, baguette, knitting yarn, croissant"),
    Sprite("icon", 8, 8, 4, "ui", -1, "small HUD icons of the power-ups"),
    Sprite("glow", 16, 16, 2, "ui", -1, "the glow around a power-up"),
    Sprite("dust", 8, 8, 3, "fx", -1, "a dust puff (landing)"),
    Sprite("shard", 8, 8, 2, "fx", -1, "glass shards, tile debris"),
    Sprite("debris", 8, 8, 2, "fx", -1, "tile and pot debris"),
    Sprite("wind", 16, 8, 2, "fx", -1, "wind streaks"),
    Sprite("splash", 16, 16, 3, "fx", -1, "a splash"),
    Sprite("curse", 32, 16, 2, "fx", -1, "the cursing bubble: #@$%!"),
    Sprite("star", 8, 8, 2, "fx", -1, "a stunt sparkle"),
    Sprite("small", 8, 8, 12, "fx", -1, "small digits 0-9, plus, times (the stunt points)"),
    # (the score digits, the A glyph and the sparkle are the house kit's sprites: house_ui.c)
    Sprite("unit_m", 16, 16, 1, "ui", -1, "the 'm' after the distance, in the kit digits' style"),
    Sprite("medal", 24, 24, 4, "ui", -1, "cat-catch medals in the house tiers (bronze, silver, gold, pearl): whiskers"),
    Sprite("arrow", 8, 8, 1, "ui", -1, "Mamie above the screen: an arrow at the top edge"),
    Sprite("cafe", 32, 16, 3, "props", -1, "the café awning below a fall: left, right half, pressed"),
    Sprite("feather", 8, 8, 2, "fx", -1, "a feather, drifting down"),
]
SPRITE_SHEET_W = 320


def sprite_layout():
    """(x, y) of each sprite entry's first frame on sprites.png: frames left to right, entries packed in rows."""
    out, x, y, row_h = {}, 0, 0, 0
    for s in SPRITES:
        w = s.w * s.frames
        if x + w > SPRITE_SHEET_W:
            x, y, row_h = 0, y + row_h, 0
        out[s.name] = (x, y)
        x += w
        row_h = max(row_h, s.h)
    return out, y + row_h


def sprite(name):
    for s in SPRITES:
        if s.name == name:
            return s
    raise KeyError(name)


def frame_rects(name):
    s = sprite(name)
    lay, _ = sprite_layout()
    x, y = lay[name]
    return [(x + i * s.w, y, s.w, s.h) for i in range(s.frames)]


# ---- panoramas ----------------------------------------------------------------------------------------------------
MID_W, MID_H, MID_Y = 512, 128, 128      # BG3: content rows 128..255 of a 64x32 map
FAR_W, FAR_H, FAR_Y = 512, 152, 104      # BG4: content rows 104..255, one panorama per district
DISTRICTS = ["montmartre", "seine", "haussmann", "eiffel"]
# the title logo is drawn at run time by the kit (house_ui.c hu_logo): "POGO" in lead grey, "MAMIE" in the pink accent
LOGO_RAMPS = [[(208, 214, 226), (160, 166, 180), (116, 122, 138)], [(255, 208, 226), (246, 150, 190), (214, 92, 146)]]

# Mamie's frames, in order (the C code's MF_*), and the head offset of each (Papi's overlay: x, y in the frame)
MAMIE_FRAMES = ["idle", "squash1", "squash2", "stretch", "rise", "fall", "big", "stumble", "flail1", "flail2", "sit",
                "float", "hang"]


def markdown():
    out = ["| Sprite | Frame | Frames | Palette | Description |", "|---|---|---|---|---|"]
    for s in SPRITES:
        out.append("| %s | %dx%d | %d | %s | %s |" % (s.name, s.w, s.h, s.frames, s.pal, s.desc))
    out += ["", "| BG2 tile | Palette |", "|---|---|"] + ["| %s | %s |" % t for t in BG_TILES]
    return "\n".join(out)


if __name__ == "__main__":
    print(markdown())
