# Puffin Plunge: design

(c) 2026 Pierre-Louis Boyer (8BCraft), CC BY-NC-SA 4.0 (games/puffinplunge/LICENSE). A game for the RetroStone
virtual console (docs/spec.md), in the 8BCraft house style (docs/art-direction.md). The owner's idea: a vertical fall
shooter. The theme, the name, the hero, the critters, the art, the music and every chunk of the well are our own.

## Pitch
A little puffin in bubble boots dives down a deep well. It runs and jumps on the ledges; in the air the same button
fires bubbles straight down from its boots: they pop the critters below and each shot slows the fall. The bubbles run
out; landing refills them, and so does a stomp on a soft critter (it bounces the puffin up). Critters popped one after
the other without touching the rock make a combo, paid in gems on landing. Gems buy upgrades in the shop at each
level's bottom and in the little side rooms on the way down. Four zones of three levels, deeper and harder, and the
Grumpy Walrus at the very bottom.

## Controls
| | Console (pad) | Phone (touch, apps/launcher/games.cfg) | Keyboard |
|---|---|---|---|
| run | D-pad Left / Right | the drawn Left / Right buttons (bottom left) | arrows |
| jump (on a ledge; released early: lower) | A (B, X, Y too) | the big A button (bottom right) | X |
| fire down (in the air; held: again and again) | A (B, X, Y) | the big A button | X |
| a side room | push into its door from the ledge | the same | |
| shops: choose, buy | the D-pad, A; the last line dives on | Left / Right, A | arrows, X |
| pause | Start or Select (paused: A goes on, B the title) | the app's pause button | Enter / Backspace |

The title: A dives. A dive is kept in save RAM at each level's start: the title then says IN PROGRESS: LEVEL 2-1 and
offers CONTINUE THE DIVE or NEW DIVE (Left / Right choose, A goes; B: a new dive). A level starts with its card (LEVEL
1-2, the zone's name); a first dive shows the controls' hint.

## The puffin (src/game.h: every number; src/world.c: the rules, no SDK call)
Positions are whole pixels with a 1/256 remainder; every move goes pixel by pixel against the rock. 60 Hz.

| | Value | |
|---|---|---|
| run | 2.0 px/f; full speed in 4 frames, a stop in 3-4, a turn in 2-3 | the air: 0.375 px/f^2, a third of the drag |
| jump | 4.6 px/f, gravity 0.28 px/f^2 | about 38 px (2.4 cells); released while rising: cut to 1.5 px/f |
| fall | 4.0 px/f | terminal velocity |
| coyote time | 5 frames | A just after leaving a ledge still jumps (not fires) |
| jump buffer | 6 frames | A just before landing with no bubbles left: a jump on landing |
| fire | a new press in the air, then held: again every `period` frames | each shot sets the fall to `hover` (a little up) |
| bubbles | 8 (the tank: up to 16) | refilled on landing and by a stomp |
| stomp | 4.0 px/f up (A held: 4.8), 2 damage | only on soft critters; a spiky one stings and bounces |
| hurt | one heart, 90 frames safe (blinking), a 6-frame hit-stop, a small kick up | the bubble shield takes one hit a level |
| hearts | 4 (up to 8) | |
| gems | fly to the puffin from 22 px (the magnet: 64) | a big gem is 5 |

The boots' modules (one at a time; the shops sell them):

| | Period | Bubbles | Damage | Hover | |
|---|---|---|---|---|---|
| bubbles | 7 | 1 | 1 | -0.6 px/f | one straight down, 7 px/f |
| triple bubbles | 11 | 1 | 1 each | -0.2 | three in a fan |
| sparkle beam | 16 | 2 | 3 | -1.6 | a ray 176 px down through every critter, breaks a crystal |
| rapid bubbles | 4 | 1 | 1 | -0.4 | |

Combos: each critter popped while the feet are off the rock counts; landing pays combo x 2 gems from 4 (combo x 3
from 8) and a heart from 10. The camera keeps the puffin a third of the way down the screen and looks up to 36 px
further ahead of a fall (smoothed in the world: the same at every screen size).

## The well (levels/README.md: the chunks' format; levels/chunks.txt)
A level piles up 9 + its number of chunks of 10 x 12 cells (16 px), picked among its zone's (24 chunks, 18-19 a zone,
some shared), maybe mirrored, between a landing ledge and the bottom's floor. Critter letters bring one 58% of the time
on level 1-1, 3% more each level (92% at most); the critters go 5% faster each level. Side rooms: a door in a wall on a
ledge (70% of the `D` letters, at most one a chunk, four a level), a little shop (2 offers, 20% cheaper) or a gift (one
of two free). Every chunk is checked passable from every column of its top (tests/test_world.c).

| Zone | Levels | Critters | Look |
|---|---|---|---|
| 1 the crystal grotto | 1-1 .. 1-3 | snails (walk the ledges), bats (wake and flutter at the puffin), crystal urchins (spiky) | violet rock, crystal shards, cyan glints |
| 2 the coral trench | 2-1 .. 2-3 | crabs (fast), fish (swim across), jellyfish (sting, bob), pufferfish (soft, spiky when puffed), sea urchins | blue rock, pink coral |
| 3 the mushroom cave | 3-1 .. 3-3 | beetles (3 hits), moths (weaving), puffcaps (shoot spores up), jellyfish, thorny burrs | brown rock, moss, glowing mushrooms |
| 4 the frozen deep | 4-1 .. 4-3 | woolly grubs, ice bats (fast), icicles (drop when the puffin passes under), frost wisps (drift through the rock to it), ice spikes | icy rock, snow, frozen fish |

The bottom (level 4-3 ends in a cave): the Grumpy Walrus (40 hits): it waits for the puffin, slides along the floor,
rears up and throws snowballs at it (three when angry, under half its health), bounces on its belly. Stomps hurt it
(soft), bubbles too; its snowballs pop. Beaten: the results (YOU DID IT!).

## The shops
At each level's bottom: three offers among those the puffin can use (a heal always when hurt), Up / Down (or Left /
Right) choose, A buys, the last line (DIVE ON) goes on. Prices: base + per level (src/world.c PRICE): extra heart 24 +
5, warm soup (2 hearts) 12 + 3, bigger tank (+2 bubbles) 16 + 4, triple bubbles 30 + 5, sparkle beam 36 + 6, rapid
bubbles 30 + 5, gem magnet 24 + 4, spring boots (bigger stomps) 24 + 4, feather cape (a slower fall firing) 24 + 4,
bubble shield (a hit a level) 28 + 4, lucky clover (half as many gems more) 24 + 4.

## Score
Gems picked up (1, a big one 5), 5 a critter, 2 a crystal, combo bonuses x 2, 100 a level, 500 the walrus. The best
score in save RAM (games.cfg best=8); medals at 300, 900, 1800, 3000.

## Screen and layers
320 x 240 at 60 Hz. BG3 the back wall (the zone's dark rock, at half the speed), BG2 the shaft (the rock autotiled from
8x8 quarters with the zone's cap, crystals, doors, the floor) and the rock beside it (a darker palette, the zone's
decorations), BG1 the house kit (cards, shops, results, PAUSED). Sprites: the puffin (OBJ 0), the critters (OBJ 1, the
zone's palette), items and the HUD (OBJ 2), the kit (OBJ 3), the effects (OBJ 4), the walrus (OBJ 5), the hit flash
(OBJ 6: white). The HUD: hearts, the gems (big digits, a pulse when one arrives, gems zip to it), the depth (2-1), the
combo, the bubbles' gauge beside the shaft.

**Widescreen**: the shaft (192 px) centred, more rock and decorations on its sides; the HUD stays by the shaft.
**Tall screen** (a phone held upright): `rs_screen_zoom(480)`: 202-220 columns shown, the shaft fills them; the stage's
bottom at the screen's bottom (the touch buttons' band, over the shaft far below), the puffin at 30% of the height:
about 300 px of the well below it. The HUD at the screen's top (sprites), the bubbles' gauge over the right wall. The
title, the cards, the shops and the results in house_tall.h's layouts, pinned on the screen. A state saved upright
loads in landscape and back (`ht_shape_reset`).

## Feel
Squash on landing, stretch on the jump, dust (jump, landing, stomps), a muzzle flash and a spent shell spinning away at
every shot, a white flash on every hit, a poof and sparks and a 2-px shake on a pop, shards on a crystal, the combo
counter popping over the puffin and by the HUD, gems zipping to the counter, hit-stop on a stomp (2 frames) and a hurt
(6), a 3-px shake on a hurt.

## Sound and music
House synthesiser effects (main.c sound_init): jump, bubble shot, beam, hit, pop, stomp, gem (pitched up along a
chain), landing, hurt, crystal, empty, combo (pitched up), buy, door, the walrus. A 4-channel MOD per zone and one for
the walrus (tools/make_music.py: square lead, pluck arpeggio, bass, drums), original.

## Tests
`make puffinplunge-check`: the rules (tests/test_world.c: every chunk passable, the levels, the bot diving through
levels 1-2 on 40 seeds and how deep it goes, determinism), the libretro loader, the smoke tests (the flow, the shops,
the side rooms, pause, the dive kept and continued, the game over, the bot through levels 1-2 with strict mode, the
deep zones and the walrus, determinism), the screens, the save states and their audit, the widths 320 / 427 / 520 and
states across them, the tall screen and the phone turning (tall_test), the frame cost (bench_test), the languages
(tools/i18n_test.sh).
