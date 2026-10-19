# Lemur Leap: design

(c) 2026 Pierre-Louis Boyer (8BCraft), CC BY-NC-SA 4.0 (games/lemurleap/LICENSE). A game for the RetroStone
virtual console (docs/spec.md), in the 8BCraft house style (docs/art-direction.md). The owner's idea: a vertical
endless jumper, the genre where the hero bounces by itself and the player only steers. The theme, the name, the hero,
the leaves, the pests, the art, the music and every number are our own; nothing else is taken from any game.

## Pitch
A little ring-tailed lemur in a red scarf hops up a giant beanstalk, leaf after leaf, out of the garden, through the
clouds and the sunset sky into outer space. It bounces by itself on every leaf it lands on: steer it left and right
(off one side, in on the other), stomp or shoot the pests, grab the coins and the power-ups, and do not fall. A marker
shows your record as you pass it. Coins buy hats and furs in the wardrobe on the title.

## Controls
| | Console (pad) | Phone (touch, the game reads the fingers) | Keyboard |
|---|---|---|---|
| steer | D-pad Left / Right (held: faster, a little inertia) | hold the left / right half of the screen | arrows |
| shoot a seed up | A (B, X, Y, Up too; held: one every 0.37 s) | a second finger going down, or a quick tap | X |
| pause | Start or Select | the app's pause button | Enter / Backspace |
| paused: back to the title (the climb kept) | B | tap B: TITLE (a tap elsewhere: go on) | Z |
| title: the wardrobe | Left / Right | tap the look's line (its left / right half) | arrows |
| title: buy the look shown | X, Y or Select | tap the price's line | V, C |
| title: climb / continue | A or Start | tap the prompt (or anywhere else) | X / Enter |
| title: a new climb (when one is kept) | B | tap B: NEW CLIMB | Z |

`rs_game.touch_native = 1`: the app passes the fingers (apps/launcher/games.cfg has no zone for it). The finger down
the longest steers by the half it is on, so a thumb can hold a side while the other taps to shoot.

## The climb (src/world.c: the rules, no SDK call; src/game.h: every number)
Positions in 1/256 px (Q8), 60 Hz, deterministic (the world's own xorshift RNG, seeded from the start's frame or
`--opt seed`). The column is 192 px wide and wraps. The camera only rises: it keeps the lemur's head at least 104 px
down its 240-line window (the stage); feet more than 250 px down it: the lemur has fallen out, the camera follows the
fall for 70 frames, then the results. The window is the same at every screen size (a run is the same in landscape, 4:3
and upright, frame for frame; a tall screen shows 240 more lines above it).

| | Value |
|---|---|
| bounce (a leaf) | 5.8 px/f up, gravity 0.2 px/f^2: 80 px high (8 m), 28 frames up, 56 a hop |
| mushroom / spring / cloud | 7.9 px/f (152 px) / 10.2 px/f (258 px, a somersault) / 4.6 px/f (52 px, soft) |
| steering | 2.75 px/f at most, 8-9 frames to full speed, 18 to stop when released, a turn at once |
| fall | 7.4 px/f at most |
| height | 10 px a metre: the score is the height of the highest feet |
| seeds | 8 px/f up, 44 frames, 4 at most, a press every 12 frames, every 22 held |
| propeller / rocket | 5.5 px/f up for 2.5 s / 8.75 px/f for 2.8 s; pests met while flying pop |
| shield / magnet | a bubble that takes one pest's touch (12 s) / coins within 80 px fly in (10 s) |
| pests | stomped from above: they pop and the lemur bounces (92 px); met from the side or below: knocked, it tumbles out; a seed pops them; 3 coins each |

### The leaves and the generator
Each row of the climb holds one leaf of the **path**: the step from the row below is within that leaf's bounce
(64 px at most after a leaf, 38 after a cloud) and its run (a moving leaf's sweep, a cloud's drift) comes within the
lemur's reach sideways (across the wrap) in the time the bounce gives, 16 px spare; a moving leaf can be waited for.
The extras come on top: more leaves low down (a gentle start), dry leaves between the rows (they crack and fall: no
bounce), coins (a column over a leaf, an arc across a gap), the power-ups, the pests (hovering in a gap, clear of the
columns of the two leaves round it: a straight bounce never meets one).

| | the garden 0-200 m | the clouds 200-500 m | the sunset sky 500-900 m | space 900 m + |
|---|---|---|---|---|
| step between rows | 22 px at 0 m, growing to 58 px at 2000 m (+-6) | | | |
| moving leaves | from 50 m, up to 45% weight | | | +15 |
| dandelion puffs (one bounce) | from 150 m, up to 35 | | | |
| clouds (soft) | - | 45 | 15 | - |
| mushrooms, springs | 5 / 5-8% of the leaves | | | |
| dry leaves | from 30 m, 8% + 4% every 100 m, 45% at most | | | |
| pests | from 120 m, one every 900 px at first, every 220 at last | bee | storm cloud / bat | saucer |
| power-ups | from 60 m, one every 500 px at least, 4.5% of the leaves | propeller, magnet, shield, rocket (400 m+) | | |

`tests/test_world.c` checks the path of 1000 seeds up to 2500 m (every step within the bounce and the reach, never a
dry leaf), and the bot (`bot_input`, world.c: it steers for the highest leaf it can still land on, waits on its leaf for
a moving one, shoots the pests above) climbs 1500 m on every seed with the pests harmless, and far with them.

## Screen and layers
320 x 240 at 60 Hz. The column (stage x 64..255) in the middle, the side panels at its sides.

| Layer | Contents | Scroll |
|---|---|---|
| backdrop | the sky, one colour per line (the raster) by the altitude of that line: garden blue, the clouds' pale blue, the sunset's orange and pink, the violet dusk, the night of space | - |
| BG4 | far clouds (their colours by zone: white, pink, a violet nebula) | 1/4 |
| BG3 | stars, a ringed planet, a moon: from 650 m, fading in | 1/8 |
| sprites | prio 2: the leaves, the ground, coins, power-ups, pests, seeds, the lemur and its hat, the effects | |
| BG2 | the side panels: the giant beanstalk's stalks, their leaves, a vine on the column's edge; high priority: they hide what passes outside the column (the lemur crossing the wrap shows on both sides, cut at the edges); their background takes the zone's colours | 1/2 |
| BG1 | the house kit: the HUD in boxes on the panels (the height, the record, the coins, the power-up's bar: icons and digits only), the title, the results, PAUSED | |

**Widescreen**: wider panels (more beanstalk). **Tall screen** (a phone held upright): `rs_screen_zoom(480)`: 480
lines fill the display's height, 204 to 220 columns shown (the column and a strip of each stalk); the stage at the
screen's bottom, so the 240 lines above it show the climb ahead; the HUD pinned at the top (the height big in the
middle, the coins and the record at the sides, clear of the app's corner buttons). The title and the results use
house_tall.h's layouts, pinned on the screen; the lemur hops on the garden's ground at the bottom of the title.

## Screens
title (the logo, the lemur hopping on a leaf wearing the look shown, the wardrobe's line, its price or WORN, the
coins, the climb kept, PRESS A TO CLIMB, the record) -> the climb (THE CLOUDS, THE SUNSET SKY, OUTER SPACE as each
zone begins; NEW BEST! as the record's rope is passed) -> the fall -> GAME OVER: HEIGHT, BEST (or NEW BEST), COINS
earned, the medal (bronze 100 m, silver 250, gold 500, pearl 900) -> one press: the title.

## The wardrobe
Cosmetic only. Hats: none, leaf cap (30 coins), flower crown (60), party hat (100), beanie (150), pirate hat (250),
crown (400), space helmet (600); furs: grey, golden (200), snow (300), midnight (500). A look bought is worn as soon as
it is shown; one not bought is tried on (the title's lemur wears it) with its price.

## Sound and music
House synthesiser effects (main.c sound_init): bounce, spring (the mushroom: the same lower), a dry leaf's crack, a
puff (the cloud: lower, the shield: higher), a coin (higher with each one in a row), a seed, a pest popping (the stomp:
lower), the knock, a power-up, the fall, the propeller's whirr, a new zone's chime. Two 4-channel MODs
(tools/make_music.py, original): "Beanstalk Hop" (C major, the title, the garden, the clouds) and "Starry Leaves"
(A dorian, the sunset and space).

## Save RAM
The record (metres, offset 8: games.cfg's best), the coins, the looks bought and worn, the climb kept (the whole world,
saved every 30 s and when left from the pause; the title offers to CONTINUE it).

## Tests
`make lemurleap-check`: test_world (the rules, the path of 1000 seeds, the bot's climbs, determinism), the libretro
loader, the smoke tests (the flow, the wardrobe, the touches, pause, the climb kept, the fall and the record, the bot,
determinism), the screens, the save states and their audit, the widths 320 / 427 / 520 and states across them, the
tall screen and the phone turning (tall_test), the frame cost (bench_test), the 22 languages (tools/i18n_test.sh).
