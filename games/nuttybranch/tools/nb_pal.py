#!/usr/bin/env python3
"""Nutty Branch: the palettes (house_style ramps, light to dark) and the time-of-day / season bands.

BG 1  the branch (nb_bark.py): a 7-shade bark ramp (B0 the sunlit top .. B6 the underside), moss, lichen, the pale
      wood of scars and broken ends, a knot dark, a shine (wet bark, snow).
OBJ 0 the squirrel (players 2-4: its fur hue rotated, build_assets.py).
OBJ 2 wood: the props and decorations of bark and leaves (they share the branch's bark shades: they sit on it), the
      pale wood, the dark of holes and shadows, a 3-shade leaf ramp, moss, mushrooms. The bark and leaf entries follow
      the band (draw.c blends them).
OBJ 6 colour: the golden walnut, acorns, hazelnuts, sap, white (webs, snow, rain).
OBJ 7 birds and beasts: the woodpecker, the owl, the crow, the bees, the caterpillar and the snake, the balance meter.

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/nuttybranch/LICENSE.
"""

HERO = dict(
    out=(46, 18, 34), f1=(244, 156, 86), f2=(216, 104, 50), f3=(164, 64, 32), f4=(108, 38, 24),
    tip=(255, 214, 160), cream=(250, 228, 192), cream2=(212, 176, 140), white=(250, 250, 250), ink=(18, 8, 26),
    pink=(236, 136, 148), claw=(74, 40, 30))
HERO_PAL = [HERO[k] for k in ("out", "f1", "f2", "f3", "f4", "tip", "cream", "cream2", "white", "ink", "pink",
                              "claw")]
FUR = [HERO["f1"], HERO["f2"], HERO["f3"], HERO["f4"]]
FUR_HUES = (0.0, 0.085)                         # the hues the player swaps turn (not the cream, not the tips)
P2_SHIFT = 0.55                                 # player 2: the red turned blue-grey

# ---- the branch (BG 1): index -> colour of band 0 ---------------------------------------------------------------
BK = dict(rim=1, b0=2, b1=3, b2=4, b3=5, b4=6, b5=7, b6=8, m1=9, m2=10, lichen=11, wl=12, wm=13, knot=14, shine=15)
BARK0 = {1: (26, 16, 12), 2: (184, 146, 100), 3: (152, 116, 78), 4: (124, 92, 62), 5: (98, 72, 48),
         6: (76, 54, 36), 7: (56, 38, 26), 8: (38, 26, 18), 9: (134, 172, 78), 10: (90, 128, 56),
         11: (178, 192, 162), 12: (236, 208, 160), 13: (198, 158, 108), 14: (44, 28, 20), 15: (234, 226, 208)}
BARK_SHADES = [BK["b0"], BK["b1"], BK["b2"], BK["b3"], BK["b4"], BK["b5"], BK["b6"]]

# ---- the wood props (OBJ 2) ------------------------------------------------------------------------------------
WOOD = dict(out=(34, 20, 12), b0=BARK0[2], b1=BARK0[3], b2=BARK0[4], b3=BARK0[5], b4=BARK0[6],
            wl=(236, 208, 160), wm=(196, 156, 106), dark=(26, 20, 32), l0=(170, 214, 96), l1=(112, 170, 62),
            l2=(60, 112, 44), moss=(126, 160, 70), cap=(178, 90, 56), cream=(240, 226, 190))
WOOD_PAL = [WOOD[k] for k in ("out", "b0", "b1", "b2", "b3", "b4", "wl", "wm", "dark", "l0", "l1", "l2", "moss",
                              "cap", "cream")]
R_BARK = [WOOD["b0"], WOOD["b1"], WOOD["b2"], WOOD["b3"]]
R_BARKD = [WOOD["b1"], WOOD["b2"], WOOD["b3"], WOOD["b4"]]
R_WOOD = [WOOD["wl"], WOOD["wl"], WOOD["wm"], WOOD["b3"]]
R_LEAF = [WOOD["l0"], WOOD["l1"], WOOD["l1"], WOOD["l2"]]
R_MOSS = [WOOD["moss"], WOOD["moss"], WOOD["l2"], WOOD["l2"]]
R_CAP = [WOOD["cream"], WOOD["cap"], WOOD["cap"], WOOD["b4"]]
R_DARK = [WOOD["dark"]] * 4

# ---- the colourful props (OBJ 6) --------------------------------------------------------------------------------
COL = dict(out=(30, 20, 42), g1=(255, 240, 150), g2=(250, 202, 64), g3=(196, 140, 30), a1=(218, 156, 84),
           a2=(178, 110, 52), a3=(126, 74, 36), c1=(152, 120, 72), c2=(104, 78, 46), hz=(226, 178, 112),
           s1=(255, 196, 96), s2=(220, 128, 34), s3=(150, 72, 22), w1=(244, 248, 252), w2=(168, 182, 206))
COL_PAL = [COL[k] for k in ("out", "g1", "g2", "g3", "a1", "a2", "a3", "c1", "c2", "hz", "s1", "s2", "s3", "w1",
                            "w2")]
R_GOLD = [COL["g1"], COL["g2"], COL["g2"], COL["g3"]]
R_ACORN = [COL["a1"], COL["a1"], COL["a2"], COL["a3"]]
R_CAPA = [COL["c1"], COL["c1"], COL["c2"], COL["c2"]]
R_HAZEL = [COL["hz"], COL["a1"], COL["a2"], COL["a3"]]
R_SAP = [COL["w1"], COL["s1"], COL["s2"], COL["s3"]]
R_SNOW = [COL["w1"], COL["w1"], COL["w2"], COL["w2"]]

# ---- birds and beasts (OBJ 7) -----------------------------------------------------------------------------------
BIRD = dict(out=(26, 16, 32), white=(250, 250, 250), black=(36, 32, 46), red=(222, 54, 54), grey=(150, 152, 166),
            o1=(206, 166, 112), o2=(152, 110, 66), o3=(98, 66, 42), ocream=(240, 222, 182), yellow=(255, 206, 44),
            orange=(240, 140, 40), g1=(186, 226, 92), g2=(112, 178, 54), g3=(54, 110, 40), slate=(84, 90, 112))
BIRD_PAL = [BIRD[k] for k in ("out", "white", "black", "red", "grey", "o1", "o2", "o3", "ocream", "yellow",
                              "orange", "g1", "g2", "g3", "slate")]
R_OWL = [BIRD["o1"], BIRD["o1"], BIRD["o2"], BIRD["o3"]]
R_BLACK = [BIRD["slate"], BIRD["black"], BIRD["black"], BIRD["black"]]
R_GREEN = [BIRD["g1"], BIRD["g2"], BIRD["g2"], BIRD["g3"]]
R_YEL = [BIRD["yellow"], BIRD["yellow"], BIRD["orange"], BIRD["orange"]]
R_GREY = [BIRD["white"], BIRD["grey"], BIRD["grey"], BIRD["slate"]]

# the backdrop (BG 2 on layer BG3): canopy 1-4, far forest 5-7, the crowns below 8-10, trunks 11, mist 12-13
SCENE0 = {1: (150, 200, 84), 2: (100, 160, 60), 3: (62, 118, 44), 4: (36, 78, 34), 5: (140, 178, 168),
          6: (116, 156, 150), 7: (96, 136, 134), 8: (112, 150, 110), 9: (88, 128, 94), 10: (66, 104, 80),
          11: (70, 60, 52), 12: (176, 200, 188), 13: (150, 180, 170), 14: (122, 168, 72), 15: (52, 96, 40)}


def lerp(a, b, t):
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(3))


def mul(c, k, add=(0, 0, 0)):
    return tuple(max(0, min(255, int(round(c[i] * k[i] + add[i])))) for i in range(3))


# ---- the time of day and the seasons (every 400 m: src/draw.c blends them) -------------------------------------
BANDS = [
    dict(name="spring morning", sky=[(104, 168, 228), (232, 228, 204), (172, 200, 168), (84, 120, 96)],
         leaves=[(170, 214, 96), (118, 176, 66), (72, 132, 50), (40, 86, 38)],
         far=[(150, 188, 176), (124, 164, 156), (100, 140, 136)], crowns=[(120, 160, 112), (94, 136, 96), (70, 110, 82)],
         mist=[(184, 206, 190), (156, 184, 172)], bark=(1.0, 1.0, 1.0), light=(0, 0, 0)),
    dict(name="summer noon", sky=[(64, 146, 232), (196, 228, 250), (150, 196, 150), (60, 104, 70)],
         leaves=[(140, 200, 70), (86, 156, 50), (48, 112, 40), (26, 70, 30)],
         far=[(130, 176, 170), (104, 152, 146), (80, 128, 124)], crowns=[(96, 150, 90), (70, 124, 74), (50, 98, 62)],
         mist=[(170, 206, 196), (140, 184, 170)], bark=(1.06, 1.04, 1.0), light=(6, 4, 0)),
    dict(name="autumn afternoon", sky=[(96, 146, 210), (250, 214, 160), (206, 172, 120), (110, 80, 60)],
         leaves=[(250, 190, 70), (230, 120, 40), (180, 66, 30), (110, 40, 26)],
         far=[(204, 168, 130), (176, 136, 104), (146, 108, 84)], crowns=[(200, 130, 60), (164, 96, 46), (120, 70, 40)],
         mist=[(226, 196, 156), (196, 164, 128)], bark=(1.04, 0.96, 0.9), light=(8, 0, 0)),
    dict(name="dusk", sky=[(56, 58, 120), (250, 150, 110), (150, 96, 110), (52, 40, 70)],
         leaves=[(196, 120, 96), (144, 76, 80), (96, 48, 70), (58, 30, 54)],
         far=[(150, 104, 130), (120, 80, 112), (94, 62, 96)], crowns=[(116, 72, 96), (90, 56, 84), (66, 42, 70)],
         mist=[(200, 140, 140), (160, 110, 124)], bark=(0.86, 0.76, 0.84), light=(0, 0, 10)),
    dict(name="night", sky=[(12, 18, 46), (44, 58, 104), (36, 50, 84), (10, 14, 30)],
         leaves=[(70, 110, 120), (46, 80, 96), (30, 56, 76), (18, 34, 54)],
         far=[(52, 72, 104), (40, 58, 90), (30, 46, 76)], crowns=[(40, 64, 84), (30, 50, 72), (22, 38, 60)],
         mist=[(60, 80, 116), (44, 62, 98)], bark=(0.56, 0.62, 0.86), light=(0, 4, 18)),
    dict(name="winter dawn", sky=[(150, 170, 212), (252, 220, 214), (210, 214, 226), (120, 130, 156)],
         leaves=[(246, 248, 252), (206, 216, 232), (150, 160, 186), (92, 98, 124)],
         far=[(196, 204, 220), (172, 182, 204), (150, 160, 186)], crowns=[(214, 220, 234), (180, 188, 210), (140, 150, 176)],
         mist=[(232, 230, 238), (206, 208, 222)], bark=(0.9, 0.9, 0.98), light=(10, 10, 16)),
]


def band_bark(b):
    """The branch palette (BG 1, 16 entries) of band b."""
    t = BANDS[b]
    out = [(0, 0, 0)] * 16
    for i, c in BARK0.items():
        if i in (9, 10):                                      # the moss follows the leaves
            c = lerp(c, t["leaves"][1 if i == 9 else 2], 0.5)
        if b == 5 and i in (2, 3):                            # snow on the sunlit top
            c = lerp(c, (240, 242, 250), 0.6 if i == 2 else 0.3)
        out[i] = mul(c, t["bark"], t["light"]) if i != 15 else c
    return out


def band_scene(b):
    t = BANDS[b]
    out = [(0, 0, 0)] * 16
    for i in range(4):
        out[1 + i] = t["leaves"][i]
    for i in range(3):
        out[5 + i] = t["far"][i]
        out[8 + i] = t["crowns"][i]
    out[11] = mul(SCENE0[11], t["bark"], t["light"])
    out[12], out[13] = t["mist"]
    out[14] = lerp(t["leaves"][0], t["leaves"][1], 0.5)
    out[15] = lerp(t["leaves"][2], t["leaves"][3], 0.5)
    return out


def band_wood(b):
    """The wood props (OBJ 2): the bark entries are the branch's, the leaves the band's."""
    bk = band_bark(b)
    t = BANDS[b]
    out = [(0, 0, 0)] + list(WOOD_PAL)
    for i, j in ((2, 2), (3, 3), (4, 4), (5, 5), (6, 6)):
        out[i] = bk[j]
    out[7] = mul(WOOD["wl"], t["bark"], t["light"])
    out[8] = mul(WOOD["wm"], t["bark"], t["light"])
    out[10], out[11], out[12] = t["leaves"][0], t["leaves"][1], t["leaves"][2]
    out[13] = lerp(t["leaves"][1], WOOD["moss"], 0.5)
    return out


def band_col(b):
    return [(0, 0, 0)] + list(COL_PAL)
