"""Bomber Mole art brief: the style guide and one self-contained prompt per strip.

tools/art_sync.py builds games/bombermole/art/incoming/TODO.md from this file
and from the sheet layout (tools/sheets.py). All rights reserved, 8BCraft.
"""

STYLE = """\
- 16-bit SNES pixel art, top-down 3/4 view like Super Bomberman (characters seen slightly from above,
  front-facing sprites show the face, side sprites the profile).
- Crisp pixels: no blur, no anti-aliasing against the background, no gradients made of noise,
  no soft shadows. Dark outlines (1 pixel at the final size, so about 8 pixels in the image).
- A limited palette: at most about 15 colours per sprite, plus the magenta background.
- Light comes from the top-left: highlights on the top-left, shading on the bottom-right.
- Cute, readable silhouettes that still read when shrunk to 16x16 pixels: big heads, clear poses.
- Background: flat magenta #FF00FF everywhere outside the drawing (sprites), no magenta inside a
  drawing. Tiles fill their whole cell edge to edge (no magenta at all) and tile seamlessly.
- Draw at 8x the final size: a 16x16 frame is about 128x128 pixels, a 32x32 frame about 256x256.
  Frames of one strip sit in ONE horizontal row, evenly spaced, the same size, in the order given.
"""

MODEL_SHEET = """\
- **The mole (hero)**: chubby, soft dark-brown fur, lighter tan belly, big pink star-shaped nose,
  tiny black eyes, large pale pink digging claws, a small RED miner helmet with a round yellow lamp
  on the front. Friendly and brave.
- **The ferret (underground enemy)**: long slim body, cream fur with a dark-brown bandit mask
  around the eyes, dark legs and tail, pink nose, sneaky grin.
- **The cat (surface enemy)**: a lean tabby, grey fur with darker stripes, green eyes, pink nose,
  white muzzle, tail up. Crouches low before pouncing.
- **The barn cat (spring boss, 32x32)**: a huge fat grey (or orange) barn cat, scarred ear, yellow
  eyes, grumpy; roars with big fangs.
- **The farmer (summer boss, 32x32)**: a big man with a wide straw hat, a bushy RED beard, a red
  checked shirt, blue overalls, brown boots; throws rotten tomatoes; red-faced when angry.
- **The badger (summer mini-boss, 32x32)**: stocky grey body, black-and-white striped face, strong
  digging claws.
- **The fox (autumn boss, 32x32)**: sleek bright-orange fox, white chest and tail tip, black legs,
  sly eyes; fast.
- **The snowy owl (winter boss, 32x32)**: round white owl with grey speckles, big yellow eyes,
  wide wings.
- **The guard dog (ally)**: a friendly brown farm dog with floppy ears and a red collar.
"""

SEASONS = """\
- **Spring**: fresh greens, rich brown soil, blue puddles, white and yellow flowers.
- **Summer**: golden yellow-greens, dry sandy soil, warm light, corn and sunflowers.
- **Autumn**: oranges, reds and browns, fallen leaves, dark wet soil.
- **Winter**: blues and whites, snow on everything, frosty grey-blue soil, ice.
"""

# per entry: (what it is, [frame 1, frame 2, ...])
S = {
    "mole_walk_down": ("The mole walking toward the viewer (front view)",
                       ["standing, both feet down", "left foot forward", "right foot forward"]),
    "mole_walk_up": ("The mole walking away from the viewer (back view, helmet seen from behind)",
                     ["standing", "left foot forward", "right foot forward"]),
    "mole_walk_left": ("The mole walking LEFT (side view, nose pointing left)",
                       ["standing", "left foot forward", "right foot forward"]),
    "mole_walk_right": ("The mole walking RIGHT (side view, nose pointing right)",
                        ["standing", "left foot forward", "right foot forward"]),
    "mole_dig_down": ("The mole digging DOWNWARD (front view), claws scraping the ground in front, dirt clods flying",
                      ["claws raised", "claws down, dirt flying"]),
    "mole_dig_up": ("The mole digging UPWARD (back view), claws scraping above, dirt falling",
                    ["claws raised", "claws down, dirt flying"]),
    "mole_dig_left": ("The mole digging to the LEFT (side view), claws forward, dirt flying",
                      ["claws back", "claws forward, dirt flying"]),
    "mole_dig_right": ("The mole digging to the RIGHT (side view), claws forward, dirt flying",
                       ["claws back", "claws forward, dirt flying"]),
    "mole_place_bomb": ("The mole (front view) setting a round black bomb down in front of its feet", ["one frame"]),
    "mole_hurt": ("The mole (front view) hurt: flinching, eyes squeezed shut, small yellow stars around the helmet",
                  ["one frame"]),
    "mole_death": ("The mole knocked out",
                   ["dazed, spiral eyes, stars", "tipping over backwards", "lying flat on its back",
                    "a little ghost of the mole floating up with a halo"]),
    "mole_victory": ("The mole cheering (front view), both paws up",
                     ["paws up, mouth open", "jumping, paws up, sparkles"]),
    "ferret_walk_down": ("The ferret running toward the viewer", ["left legs forward", "right legs forward"]),
    "ferret_walk_up": ("The ferret running away (back view, tail visible)", ["left legs forward", "right legs forward"]),
    "ferret_walk_left": ("The ferret running LEFT (side view, long body)", ["legs stretched", "legs gathered"]),
    "ferret_walk_right": ("The ferret running RIGHT (side view, long body)", ["legs stretched", "legs gathered"]),
    "ferret_stunned": ("The ferret stunned: sitting, dizzy spiral eyes, little stars circling its head", ["one frame"]),
    "cat_walk_down": ("The tabby cat walking toward the viewer", ["left paw forward", "right paw forward"]),
    "cat_walk_up": ("The tabby cat walking away (back view, tail up)", ["left paw forward", "right paw forward"]),
    "cat_walk_left": ("The tabby cat walking LEFT (side view)", ["left paws forward", "right paws forward"]),
    "cat_walk_right": ("The tabby cat walking RIGHT (side view)", ["left paws forward", "right paws forward"]),
    "cat_pounce": ("The tabby cat pouncing to the RIGHT", ["crouched low, ready to spring", "in mid-leap, legs stretched"]),
    "boss": ("The barn cat boss (spring), 32x32 frames, front view",
             ["standing, grumpy", "walking, one paw lifted", "crouching, about to pounce", "roaring, fangs showing"]),
    # items and effects
    "bomb": ("A round black cartoon bomb with a short fuse and a spark", ["small yellow spark", "bigger red-yellow spark, bomb slightly swollen", "white flash spark"]),
    "grub": ("A golden grub (the collectible): plump, segmented, shiny gold, cute face", ["curled one way", "curled the other way"]),
    "pu_bomb": ("Power-up icon: a bomb with a '+' on a rounded blue tile", ["one frame"]),
    "pu_fire": ("Power-up icon: a flame on a rounded red tile", ["one frame"]),
    "pu_speed": ("Power-up icon: speed (mole claws or a winged shoe) on a rounded green tile", ["one frame"]),
    "pu_remote": ("Power-up icon: a remote detonator with a red button and an antenna on a rounded purple tile", ["one frame"]),
    "pu_heart": ("Power-up icon: a red heart on a rounded white tile", ["one frame"]),
    "dust": ("A puff of dust (digging, landing), light beige", ["small puff", "bigger puff", "fading, breaking up"]),
    "expl_center": ("Explosion CENTRE piece: a plus-shaped blast whose four arms touch the four cell edges; white core, yellow, orange rim",
                    ["bright flash, thinner", "full blast", "fading, darker reds, thinner"]),
    "expl_h": ("Explosion HORIZONTAL arm: a band of fire going from the left edge to the right edge of the cell",
               ["bright, thinner", "full", "fading, darker"]),
    "expl_v": ("Explosion VERTICAL arm: a band of fire going from the top edge to the bottom edge of the cell",
               ["bright, thinner", "full", "fading, darker"]),
    "expl_end_left": ("Explosion arm END pointing LEFT: connects to the right edge, rounded tip on the left",
                      ["bright", "full", "fading"]),
    "expl_end_right": ("Explosion arm END pointing RIGHT: connects to the left edge, rounded tip on the right",
                       ["bright", "full", "fading"]),
    "expl_end_up": ("Explosion arm END pointing UP: connects to the bottom edge, rounded tip at the top",
                    ["bright", "full", "fading"]),
    "expl_end_down": ("Explosion arm END pointing DOWN: connects to the top edge, rounded tip at the bottom",
                      ["bright", "full", "fading"]),
    # HUD
    "hud_heart": ("HUD icon: a small red heart", ["one frame"]),
    "hud_digit": ("HUD digits 0 to 9: chunky, very readable, white or gold with a dark outline",
                  ["0", "1", "2", "3", "4", "5", "6", "7", "8", "9"]),
    "hud_bomb": ("HUD icon: a small black bomb", ["one frame"]),
    "hud_fire": ("HUD icon: a small flame", ["one frame"]),
    "hud_grub": ("HUD icon: a small golden grub", ["one frame"]),
    "hud_speed": ("HUD icon: speed (claws or a winged shoe)", ["one frame"]),
    "hud_slash": ("HUD: a slash '/' in the style of the digits", ["one frame"]),
    "hud_depth_surface": ("HUD depth icon SURFACE: a tiny square picture of sky over grass over soil", ["one frame"]),
    "hud_depth_under1": ("HUD depth icon UNDERGROUND 1: a tiny square picture of soil with one tunnel", ["one frame"]),
    "hud_depth_under2": ("HUD depth icon UNDERGROUND 2: a tiny square picture of dark deep soil with a tunnel", ["one frame"]),
    "hud_danger": ("HUD icon: danger, a red warning triangle with a white '!'", ["one frame"]),
    "hud_lock": ("Menu icon: a grey padlock (locked level)", ["one frame"]),
    "hud_check": ("Menu icon: a green check mark (level cleared)", ["one frame"]),
    "hud_cursor": ("Menu cursor: a yellow arrow pointing right", ["one frame"]),
    "hud_panel": ("HUD panel background: a solid dark navy tile (fills the whole cell) with a lighter line at the bottom", ["one frame"]),
    # props: terrain-like
    "bridge": ("A wooden plank bridge seen from above, crossing water left to right (fills the cell)", ["one frame"]),
    "ice": ("Slippery ice floor (fills the cell, tiles seamlessly), pale blue with white glints", ["one frame"]),
    "thin_ice": ("Thin ice (fills the cell), darker blue-grey, water showing through", ["intact", "cracked"]),
    "mud": ("Wet brown mud with puddles (fills the cell, tiles seamlessly)", ["one frame"]),
    "tall_grass": ("Tall grass, dense and green, seen from above (fills the cell)", ["one frame"]),
    "corn": ("Corn plants seen from above, green stalks with yellow ears (fills the cell)", ["one frame"]),
    "burnt": ("Burnt ground: black ash and charred grass (fills the cell)", ["one frame"]),
    "gate": ("A metal garden gate in a fence, seen from above (fills the cell)", ["closed", "open"]),
    "plate": ("A stone pressure plate set in the ground (fills the cell)", ["up", "pressed down"]),
    "lever": ("A lever on a small stone base (fills the cell)", ["off: tilted left, red knob", "on: tilted right, green knob"]),
    "steam_vent": ("A steam vent: a hole in rocky soil (fills the cell)", ["idle, faint wisp", "erupting, a burst of white steam"]),
    "pipe": ("The opening of a big drain pipe in the ground, seen from above (fills the cell)", ["one frame"]),
    "crate": ("A wooden crate full of red tomatoes (fills the cell)", ["one frame"]),
    "splat": ("A squashed tomato splat on the ground (fills the cell, on grass)", ["one frame"]),
    "beehive": ("A straw beehive on grass (fills the cell)", ["one frame"]),
    "well": ("A stone well with a wooden crank, seen from above (fills the cell)", ["one frame"]),
    "mushroom": ("A big red bouncy mushroom with white spots on moss (fills the cell)", ["one frame"]),
    "rails_h": ("Mine-cart rails on wooden sleepers, horizontal (fills the cell)", ["one frame"]),
    "rails_v": ("Mine-cart rails on wooden sleepers, vertical (fills the cell)", ["one frame"]),
    "apple_tree": ("A small apple tree seen from above with red apples (fills the cell)", ["one frame"]),
    # props: sprites
    "sprinkler": ("A garden sprinkler head on a small base, nozzle turning", ["spraying up", "spraying right", "spraying down", "spraying left"]),
    "spray": ("Water spray droplets / stream current particles (light blue and white)", ["droplets", "droplets moved on"]),
    "log": ("A floating log (horizontal), bark and a cut end", ["one frame"]),
    "wind": ("Wind gust streaks blowing to the RIGHT (white and pale curls)", ["streaks left", "streaks middle", "streaks right"]),
    "pumpkin": ("A big orange pumpkin", ["one frame"]),
    "snowball": ("A snowball", ["small", "big"]),
    "icicle": ("An icicle hanging, pointing down", ["one frame"]),
    "mine_cart": ("An empty mine cart", ["one frame"]),
    "bucket": ("A wooden well bucket with a rope handle", ["one frame"]),
    "tomato": ("A flying rotten tomato", ["spinning, stem up", "spinning, stem sideways"]),
    "tomato_shadow": ("A small dark oval shadow on the ground (where a tomato will land)", ["one frame"]),
    "apple": ("A falling red apple", ["one frame"]),
    "zzz": ("A sleeping 'Zz' bubble (white letters)", ["Zz low", "Zz higher"]),
    "steam": ("A white puff of steam", ["one frame"]),
    "dog_walk_left": ("The guard dog running LEFT (side view)", ["legs stretched", "legs gathered"]),
    "dog_walk_right": ("The guard dog running RIGHT (side view)", ["legs stretched", "legs gathered"]),
    "dog_sleep": ("The guard dog curled up asleep", ["one frame"]),
    "bees": ("A small swarm of bees", ["wings up", "wings down"]),
    "windmill": ("A small countryside windmill (spring signature), 32x32 frames: tower with a red roof, four white sails",
                 ["sails at 0 degrees", "sails turned 22 degrees", "sails turned 45 degrees", "sails turned 67 degrees"]),
    "fox": ("The fox (autumn boss), 32x32, side view facing LEFT",
            ["running", "running, other legs", "leaping", "hurt, eyes shut"]),
    "farmer": ("The farmer (summer boss), 32x32, front view",
               ["idle", "idle, breathing", "throw: arm up holding a tomato", "throw: arm forward, tomato released",
                "angry, red face", "angry, stomping", "hurt, dizzy", "hurt, hat askew"]),
    "owl": ("The snowy owl (winter boss), 32x32, front view",
            ["perched, wings folded", "flapping, wings half open", "swooping, wings spread", "hurt, eyes shut"]),
    "badger": ("The badger (summer mini-boss), 32x32, front view",
               ["walking", "walking, other paws", "digging, dirt flying", "hurt"]),
}

TILES = {
    "grass": "Surface floor: short grass (in WINTER: snow-covered ground), fills the cell, tiles seamlessly",
    "grass_edge": "Surface floor with a darker shadow band along the TOP edge (a wall stands above), fills the cell",
    "soft_dirt": "A block of soft diggable soil, slightly raised (in SUMMER: dry cracked sandy soil), fills the cell",
    "dirt_crack": "The same soil block, half dug: deep cracks, fills the cell",
    "hard_rock": "A hard grey boulder block (bombs only), fills the cell",
    "stone": "An unbreakable block: dark stone bricks, fills the cell",
    "roots": "Tangled tree roots across soil, blocking the way, fills the cell",
    "tunnel": "Underground tunnel floor: packed dark earth with pebbles, fills the cell, tiles seamlessly",
    "hole_down": "A burrow hole going down: black hole with an earthen rim, on tunnel floor, fills the cell",
    "hole_up": "A hole up: light falling from above onto the tunnel floor, fills the cell",
    "ladder": "A wooden ladder going up, on tunnel floor, fills the cell",
    "thin_floor": "A thin floor: old cracked planks over a void, fills the cell",
    "exit_closed": "The exit molehill, closed: a mound of soil on grass, fills the cell",
    "exit_open": "The exit molehill, open: the mound with a glowing golden hole, fills the cell",
    "puddle": "A puddle of water on grass, fills the cell",
    "frozen_dirt": "Snow and frozen dirt: a frosty frozen soil block with snow on top (bombs only), fills the cell",
    "leaves": "A pile of fallen leaves on grass (may hide an item), fills the cell",
    "water": "Water (river), with small waves, fills the cell, tiles seamlessly",
}

# priority groups, in order (what spring levels 1-8 need first)
GROUPS = [
    ("1. Spring levels: the characters and the title logo",
     ["title_logo", "mole_*", "ferret_*", "cat_*", "boss"]),
    ("2. Spring tiles", ["tile_spring_*"]),
    ("3. Items and explosions", ["bomb", "grub", "pu_*", "dust", "expl_*"]),
    ("4. HUD and menus", ["hud_*"]),
    ("5. Spring gimmicks", ["windmill", "wind", "sprinkler", "spray", "log", "bridge", "tall_grass", "mud",
                            "gate", "plate", "lever", "zzz", "dog_*"]),
    ("6. Summer, autumn and winter level 1", ["tile_summer_*", "tile_autumn_*", "tile_winter_*", "steam_vent",
                                              "steam", "corn", "ice", "thin_ice"]),
    ("7. The other bosses", ["farmer", "tomato", "tomato_shadow", "crate", "splat", "badger", "fox", "owl"]),
    ("8. Later gimmicks (planned levels)", ["*"]),
]

LOGO = ("title_logo", "Title logo 'BOMBER MOLE': chunky SNES-style 3D letters (gold with a dark outline and a "
        "red shadow), the mole with its miner helmet peeking over the letters holding a lit bomb. "
        "Final size 256x64 pixels; draw it at 4x (1024x256) on flat magenta. It is downscaled as a whole "
        "(area filter + 15 colours), not cut into frames.")
