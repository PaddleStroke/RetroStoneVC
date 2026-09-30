"""Leady Squid art brief: the style guide, the model sheet and one self-contained prompt per strip.

games/leadysquid/tools/art_game.py (run by `tools/art_sync.py --game leadysquid todo`) builds
games/leadysquid/art/incoming/TODO.md from this file and from the sheet layout (ls_sheets.py).
MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/leadysquid/LICENSE.
"""

TITLE = "Leady Squid"

STYLE = """\
- 16-bit SNES pixel art, an underwater world seen from the side (a side-scrolling game), in the spirit
  of the underwater stages of SNES platformers: clean shapes, rich but limited colours.
- Crisp pixels: no blur, no anti-aliasing against the background, no gradients made of noise, no soft
  shadows, no dithering noise. Dark outlines (1 pixel at the final size, so about 8 pixels in the image).
- Flat shades: 2 to 4 shades per material, large clear shapes, readable when shrunk to the final size.
- A limited palette: at most about 15 colours per sprite (and per obstacle theme), plus the magenta
  background.
- Light comes from the top-left (the water surface): highlights on the top-left, shading on the
  bottom-right.
- Background: flat magenta #FF00FF everywhere outside the drawing, no magenta inside a drawing. Tiles
  (obstacle bodies) fill their whole cell edge to edge and repeat seamlessly from top to bottom.
- Draw at 8x the final size: a 32x32 frame is about 256x256 pixels, a 24x16 frame about 192x128.
  Frames of one strip sit in ONE horizontal row, evenly spaced, the same size, in the order given.
- No text, no logos, no characters or art from existing games. Everything here is original.
"""

MODEL_SHEET = """\
- **The squid (hero)**: a small, chubby, grumpy-cute squid, seen from the SIDE, swimming to the RIGHT,
  mantle first. About 24x20 pixels in the game, inside a 32x32 frame, centred on its body.
  - Body: a big rounded purple mantle (the pointed end, on the right) with two small triangular fins at
    its tip, one above and one below; a round head behind it (left of the mantle); four short thick
    tentacles trailing behind (to the left), slightly curled at the tips.
  - Colours: medium purple body (#9652C2), light lilac highlights on the top-left (#C286E6, #EAC8FA),
    dark purple shading (#642E8A), three small pink spots on the mantle (#DE6EB0), dark purple outline.
  - Face: ONE big eye visible (side view) on the head, white with a black pupil looking forward (to the
    right), a HEAVY LID covering its upper half and a slanted frowning brow (grumpy, a bit sleepy), a tiny
    pouting mouth under it near the tentacles. Cute, not scary.
  - **The lead weights**: a dark grey belt around the middle of the mantle with a small gold buckle, and
    grey lead diving-weight blocks on the belt sticking out above and below the body (like a scuba
    diver's weight belt). They are why it sinks so fast: make them readable.
  - The hitbox is the body (mantle and head); keep the body the same size in every frame.
- **Player 2** is the same squid recoloured pink by the game (do not draw it).
- **Obstacle themes** (columns 24 px wide, a top one hanging from the surface and a bottom one rising
  from the seabed, a gap between them): kelp (olive-green stalks, leaf blades, golden air bladders),
  coral (pink and coral-red branching coral, orange polyps, a brain-coral dome), sunken ship masts (dark
  wood with iron hoops, rope shrouds and ratlines; a yard with a torn sail, a crow's nest), anchor
  chains (big rusty links, a grey iron anchor hanging at the end, a rusty mooring buoy).
- **Mid-ground props**: muted, darker, blue-green tones (they sit far behind the obstacles): a shipwreck,
  a treasure chest with a gold glint, rocks with barnacles, coral and kelp clumps, an amphora.
"""

PALETTES = """\
- **Squid**: purples and lilacs, pink spots, lead greys, a gold buckle.
- **Kelp**: olive and yellow-greens, dark green outline, golden bladders.
- **Coral**: pinks, coral red, orange polyps, dark red outline.
- **Masts**: dark to light browns, iron greys, pale sail cloth.
- **Chains**: rust browns and oranges, iron greys.
- **Mid-ground**: deep blue-greens, a muted brown for wood, one gold glint.
- **UI**: white digits with a navy outline; bronze, silver, gold and pearl-pink shells.
"""

# per strip: (what it is, [frame 1, frame 2, ...])
S = {
    "squid_tilt": ("The squid swimming to the RIGHT, the whole body rotated around its centre (SNES "
                   "pre-rendered rotation), tentacles trailing straight behind",
                   ["tilted nose UP by 20 degrees", "level (horizontal)", "nose down 22 degrees",
                    "nose down 45 degrees", "nose down 67 degrees",
                    "nose pointing straight DOWN (90 degrees): the tentacles trail UPWARD"]),
    "squid_idle": ("The squid level, floating in place ('get ready' pose), looking a bit grumpy",
                   ["tentacles curled down", "tentacles curled up (a gentle bob)"]),
    "squid_flap": ("The squid's jet of water, tilted nose up by 20 degrees",
                   ["SQUEEZE: the mantle narrower and longer, tentacles bunched tight together",
                    "JET: tentacles fanned wide open behind it", "RECOVER: halfway back to normal"]),
    "squid_hit": ("The squid just hit something: dazed X-shaped eye, tentacles splayed, level",
                  ["hit"]),
    "squid_sink": ("The dazed squid sinking nose-down (pointing straight down), X eye, limp tentacles "
                   "trailing upward", ["tentacles to the left", "tentacles to the right"]),
    "squid_rest": ("The dazed squid knocked out on the seabed, nose down in the sand, X eye", ["resting"]),
    "digits": ("Big score digits 0 to 9, chunky and rounded, white with a light-blue lower half and a navy "
               "outline, about 12x16 pixels each inside 16x16",
               ["0", "1", "2", "3", "4", "5", "6", "7", "8", "9"]),
    "bubble": ("Air bubbles: a light-cyan ring with a white highlight, transparent inside",
               ["small (3 px)", "medium (5 px)", "big (7 px)", "popping (a broken ring)"]),
    "weight": ("A small grey lead diving weight block", ["lying flat", "tilted, bouncing"]),
    "sparkle": ("A small yellow-white four-pointed sparkle", ["big", "small, with diagonals"]),
    "ink": ("The puff of dark purple ink and tiny bubbles left behind by a flap, fading",
            ["a dense small cloud", "bigger and lighter", "thin and dithered", "almost gone"]),
    "hint": ("A round silver game-pad button with a dark letter A", ["up", "pressed (1 px lower)"]),
    "medal": ("Shell medals: a scallop shell (a fan with ribs, a hinge at the bottom) with a white shine",
              ["bronze", "silver", "gold", "pearl: a pale pink open shell holding a white pearl"]),
    "shipwreck": ("A sunken wooden sailing ship lying half in the sand, broken mast leaning, muted "
                  "blue-grey-brown tones (mid-ground, low contrast)", ["the wreck"]),
    "chest": ("A treasure chest, lid ajar, a thin line of gold inside, muted", ["chest"]),
    "rock_small": ("A small rounded rock with a few barnacles, muted blue-grey", ["rock"]),
    "rock_big": ("A big rounded rock, muted blue-grey, lit from the top-left", ["rock"]),
    "coral_clump": ("A clump of branching coral, muted pink-grey", ["coral"]),
    "kelp_clump": ("A tall kelp plant, muted blue-green", ["kelp"]),
    "amphora": ("An old clay amphora half buried in the sand, muted brown", ["amphora"]),
}
THEME_S = {
    "kelp": ("Kelp column", "a bundle of three twisting olive-green kelp stalks with leaf blades filling "
             "the width", "a crown of kelp blades with golden air bladders", ),
    "coral": ("Coral pillar", "a pillar of pink branching coral with grooves, knobs and orange polyps",
              "a rounded brain-coral dome"),
    "masts": ("Sunken ship mast", "a dark wooden mast with iron hoops, rope shrouds and ratlines on both "
              "sides filling the width", None),
    "chains": ("Anchor chains", "two rusty anchor chains side by side, big links, filling the width", None),
}
CAPS = {
    "masts": ("a wooden yard (a horizontal spar) with a torn pale sail hanging down from it",
              "a wooden crow's nest (a round lookout barrel with an iron rim)"),
    "chains": ("a grey iron anchor hanging from the chains (stock, shank, curved arms and flukes)",
               "a rusty round mooring buoy with rivets, the chain attached below it"),
}
for _t, (_name, _body, _cap) in THEME_S.items():
    S["%s_body" % _t] = ("%s, the repeating body: %s. It must repeat seamlessly from top to bottom and "
                         "fill the 24-px width (the whole column is solid)" % (_name, _body),
                         ["variant 1", "variant 2"])
    top, bottom = CAPS.get(_t, ("%s, hanging DOWN (the lower end of the top column)" % _cap,
                                "%s, pointing UP (the upper end of the bottom column)" % _cap))
    S["%s_cap_top" % _t] = ("%s: the LOWER END of the column hanging from the surface: %s. Its bottom edge is "
                            "the top of the gap; the upper half joins the body" % (_name, top), ["cap"])
    S["%s_cap_bottom" % _t] = ("%s: the UPPER END of the column rising from the seabed: %s. Its top edge is "
                               "the bottom of the gap; the lower half joins the body" % (_name, bottom), ["cap"])

LOGO = ("title_logo", "Title logo 'LEADY SQUID': chunky SNES-style letters, 'LEADY' in heavy riveted "
        "lead-grey metal, 'SQUID' in rounded glossy purple, a dark outline and a navy drop shadow, a few "
        "bubbles around. 256x64 in the game: draw it at 4x (1024x256) on flat magenta")

# rows grouped by priority; "*" takes the rest
GROUPS = [
    ("1. The squid (the hero, every frame of play)", ["squid_*"]),
    ("2. Obstacle caps (sprites at the gap edges)", ["*_cap_*"]),
    ("3. Score, medals, effects", ["digits", "medal", "hint", "bubble", "ink", "weight", "sparkle"]),
    ("4. Title logo", ["title_logo"]),
    ("5. Mid-ground props", ["shipwreck", "chest", "rock_*", "coral_clump", "kelp_clump", "amphora"]),
    ("6. Obstacle bodies (BG tiles: code-drawn by default, AI versions optional)", ["*_body"]),
]
