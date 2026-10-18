# Unicorn Hop: design

(c) 2026 Pierre-Louis Boyer (8BCraft), CC BY-NC-SA 4.0 (games/unicornhop/LICENSE). A game for the RetroStone
virtual console (docs/spec.md), in the 8BCraft house style (docs/art-direction.md). The idea is the owner's
daughter's (7): "a unicorn that jumps in clouds".

## Pitch
A little white unicorn with a rainbow mane climbs the sky from cloud to cloud. One button: it jumps straight up, and the
whole game is WHEN: tap so that the cloud sliding above is right under its hooves when it comes down. One miss and it
tumbles out of the sky; one more try to reach the next rainbow, the night and the aurora.

## Rules
The unicorn STANDS on a cloud and waits: nothing happens until A. A press makes it jump straight up on a fixed arc (no
steering, no auto-bounce: presses in the air do nothing). Each level of the sky has one cloud sliding left and right at
its own speed; a normal jump reaches exactly the next level. Coming down, if the hooves cross the top of the cloud above
while over it (a small fair margin past its ends), it LANDS, climbs a level and RIDES that cloud (it moves with it; after
landing on the very end it steps inside). Clouds turn back at the ends of the play band, so a rider is never carried
off the screen. Otherwise it MISSES: a short tumble with a sad sparkle, out of the sky, game over (never back onto a
lower cloud). The camera eases up after each landing: the cloud the unicorn stands on is at line 200, three more levels
show above it.

Special clouds, introduced one by one:
- **pink cotton-candy cloud**: a jump from it is a big bounce of two levels (it lands on the cloud two levels up; the
  level between does not catch it);
- **grey rain cloud**: landing on it starts the rain; it stops, drips, and melts 1.5 s later: still on it, the unicorn
  falls. The cloud right above it is its white twin (same width, moving with it, stopping with it), so the escape is
  always right above: the rain is a hurry, never a trap. It puffs back 2.5 s after melting (for the players behind);
- **thunder cloud**: a small grumpy dark cloud drifting in the gap between two levels; touching it while jumping is a
  zap (a bolt, a hit-stop, a shake) and the fall. Standing right under it is safe.

Stars float in the gaps: flying through one catches it (+2, a chime). Every 25 levels a rainbow rises across the sky
(+10, a jingle, RAINBOW!). The sky changes with the height, one phase per 25 levels (blended over the last 5): the
morning pastel with the castle and the hills far below at the start, the sunset, the starry night with the moon, the
aurora, then the morning again.

The score is the levels climbed plus the bonuses (stars, rainbows). Medals (1 player): bronze 10, silver 30, gold 60,
pearl 120, tuned with the human-like bot (`botskill`, 12 runs each): a careful player (8 px of error) scores about
100, an average one (12-16 px) 15-20, a beginner (24 px) about 9.

Every number is in src/game.h (the tuning table):

| Rule | Value | Where |
|---|---|---|
| level spacing | 64 px | LEVEL_H |
| gravity | 0.20 px/frame^2 | GRAVITY |
| the jump | 5.66 px/frame up: the apex 80 px (a level + 16), it lands 40 frames (0.67 s) after the take-off | JUMP_VY |
| the pink bounce | 7.59 px/frame: the apex 144 px (two levels + 16), it lands after 49 frames | BIG_JUMP_VY |
| the landing margin | the hooves' centre up to 5 px past a cloud's end | LAND_MARGIN |
| stepping inside after landing | to 6 px inside the end, 1 px a frame | SETTLE_IN |
| the play band | stage x 8..312 (the central 320 columns at every width) | BAND_L, BAND_R |
| the start cloud | 160 px wide, still | START_W |
| level 1 | 128 px wide, right above the start, sliding slowly towards player 1 | FIRST_W |
| cloud widths | levels 1-9: 112 or 96 px; 10-19: 96 or 80; 20-34: 80 or 64; 35-59: 64; from 60: 64 or 48 | width_steps (play.c), EASY_LEVELS, RAMP_LEVELS |
| cloud speeds | 0.45 px/frame at level 1, 0.70 at level 10, 1.70 at level 60 and up; odd levels 22% slower and even ones 22% faster (never two in step), +-8% each; the directions alternate | SPEED_MIN, SPEED_EASY, SPEED_MAX, SPEED_ALT, SPEED_JITTER |
| pink clouds | from level 8, 1 level in 7 (never two specials in a row) | PINK_FROM, PINK_CHANCE |
| rain clouds | from level 15, 1 level in 6; melts 90 frames (1.5 s) after the first landing, back 150 frames later | RAIN_FROM, RAIN_CHANCE, RAIN_MELT, RAIN_REFORM |
| thunder clouds | from level 20, in 16% of the gaps rising to 34% at level 60 (never above a rain cloud, never across a pink bounce, never two gaps in a row); drift 0.40 px/frame +-20%; a 22x12 px touch box | THUNDER_FROM, THUNDER_P0, THUNDER_P1, THUNDER_SPEED |
| the zap | an 8-frame hit-stop, then the fall | HIT_FREEZE |
| stars | from level 2, in 35% of the gaps without thunder, 38 px above the cloud top; +2 each | STAR_FROM, STAR_CHANCE, GAP_Y, STAR_POINTS |
| rainbows | every 25 levels: +10, shown 300 frames | RAINBOW_EVERY, RAINBOW_BONUS, RAINBOW_SHOW |
| the camera | the camera's cloud top at line 200; it eases 20/256 of the way a frame | STAND_Y, CAM_EASE |
| 2-4 players | the leader stands at most 2 levels above the camera's level; 3 levels behind = out (off the screen) | CAM_LAG, OUT_BELOW |
| the fall | at most 90 frames (it leaves the screen sooner) | FALL_FRAMES |
| scoring | a level climbed 1, a star 2, a rainbow 10 | play.c score() |
| medals | bronze 10, silver 30, gold 60, pearl 120 | MEDAL_SCORES |

The bot (`--opt bot=N`: tests, screenshots and card frames) plays from the world: it plays each jump ahead on copies of
the clouds and jumps when it would land 6 px inside the next cloud without touching a thunder cloud (BOT_SAFE). With
2-4 bots, a bot two levels ahead waits for the others and one left behind hurries (the four stay on the screen
together). `botstop=S` makes it miss on purpose once its score reaches S, `botskill=E` misjudges every landing by up to
E px (a human-like player, to tune the medals). A perfect bot climbs about 110 levels in 12000 frames and never falls.

## Screen and layers
320x240 at 60 Hz, fixed step. The house layout (docs/art-direction.md "Backgrounds"):

| Layer | Contents | Scroll |
|---|---|---|
| backdrop | the sky gradient, one colour per line (raster callback), by the height: morning, sunset, night, aurora (green and violet ribbons waving); a 2-line dither step every 8 lines | - |
| BG4 | the far wisps and the stars (their two colours are set per line: the sky's colour by day, lit and twinkling at night) | 1/4 vertically, drifting |
| BG3 | the start scene far below (hills, the castle with pink roofs, round trees), sinking out at the start; later the rainbow arc (its tiles reloaded over the scene's), translucent (colour math: half the sky), rising for 40 frames, standing, sinking | 1/2 (the scene), fixed (the rainbow) |
| BG2 | soft cloud banks, no outline, recoloured with the sky (they melt into it: never mistaken for a cloud to land on) | 1/2, drifting |
| BG1 | the house UI kit: the logo, the title's lines, the panels, PAUSED, RAINBOW! | fixed |
| OBJ | the unicorns (P2-P4 = the body recoloured: OBJ 1, 4, 5), the clouds (OBJ 2; pink OBJ 6, grey rain OBJ 7: the same tiles; moonlit at night), stars, sparkles, the thunder clouds, the moon, the kit's digits, glyphs and medals | |

**Widescreen** (docs/spec.md "Widescreen"): the game opts in (`max_width`, `WIDESCREEN_unicornhop = 1`). A wider screen
(427 for 16:9, 520 for phones) shows more sky on both sides: the decor layers and the start scene are drawn wider
(maps of 128 tiles beyond 512 columns, the second half mirrored); the clouds, stars and thunder clouds stay in the
central play band, so the game is the same at every width. A state saved at one width loads at another.

## Screens (the house flow: no "get ready", the title is the only menu)
title (the logo, a unicorn bobbing on a little cloud, the castle far below, the player slots, PRESS A TO HOP with the A
glyph, BEST, the join line, the copyright line) -> P1 presses A (or B, X, Y, Up, Start; the press is also the first
jump: level 1 starts right above) -> play (the score at the top; Select, or Start, pauses) -> a miss, a melt or a zap:
the tumble -> game over (the banner, the panel sliding up: score, best / new best, the medal, the stars caught; 2-4
players: the ranking by height with medals) -> one press: back to the title (the same players start again with A).
Touch: the apps turn a tap anywhere into A (the default for a game that is not touch-native).

## Up to four players
Pads 2-4 join with A on the title (B leaves); everyone starts on the start cloud and climbs the same clouds, each on
their own (several can ride the same cloud). Each falls out on its own miss. The camera follows the leader: when the
leader is more than two levels above the lowest unicorn, the camera rises with it and a unicorn left below the screen's
bottom is out. The unicorns are white (P1), lilac (P2), pink (P3) and mint (P4): the body ramp recoloured, the mane,
the horn and the eyes the same (tools/build_assets.py). The HUD shows each player's height (3-4 players in the
corners); the results rank everyone by height (the same height, the same place) with medals.

## Tall screen (docs/spec.md "Tall screen", the zoomed recipe)
A phone held upright: `rs_screen_zoom(ZOOM_H)` (520 lines, game.h) first in `game_init`. The RetroStone app scales the
520 lines to the display's height and crops the sides: 222 columns on a 9:21 phone, 240 on a 9:19.5 one (1.4x the
size of the plain tall screen). The 240 lines of the stage are the same pixels; the stage sits low
(`rs_screen_origin_v`: TALL_BELOW, 40 lines under it), so the screen shows about six levels ahead of the unicorn up to
its top, one behind.
- **The columns shown** (1 player): centred on the unicorn (`rs_screen_focus_x`, clamped to the screen): it rides its
  cloud and jumps straight up, so the view follows it smoothly; the cloud above shows over about 110 px on each side,
  more than it slides during a jump (70 px at the fastest).
- **The HUD at the screen's top**: the score (sprites at `rs_screen_top()`); 2-4 players: a row of each player's icon
  and height (no corner tags there).
- **One view**: the title and the panels stay zoomed like the play (never the letterboxed full width), laid out in
  the columns shown by games/common/src/house_tall.h: the logo as UNICORN over HOP (`ht_logo`), the unicorn on its
  cloud under it, the title's lines wrapped and stacked from row 19 (the slots closer), GAME OVER and the score panel
  narrower (each value under its label, the stars caught under them, the panel as wide as the retry line: draw.c
  `over_panel`), the ranking narrower. Climbing, the UI layer is scrolled so that its middle is on the columns shown:
  PAUSED and RAINBOW! (1x when its 2x line is wider than the columns) follow the unicorn. Only 2-4 players climbing
  show every column (`rs_screen_fill(0)`): the unicorns spread over the whole band.
- **Decor**: the raster (draw.c `raster_out`) carries the gradient on (its top colour deepening toward the screen's
  top, the horizon's colour below), the stars and the decor layers go on (their maps wrap, as when the camera climbs),
  the UI layer is hidden there, and the start scene's land goes on below the stage (its last row repeated); the
  rainbow stays on the stage. The play clouds of every level the screen shows are drawn there too (the world already
  keeps 8 levels above the camera and 8 below: no rule changes). The moon stays in the columns shown. Line 0 puts the
  UI layer and BG3's scroll back.
- Landscape and 4:3 are the same pixels as before (checked at 2340x1080 and 320x240).

## Art
Code-drawn only (tools/make_art.py with games/common/tools/house_style.py): no image generation. The unicorn: a white
body shaded lavender (light from the top-left), a pastel rainbow mane and tail (pink, peach, mint, sky, lilac), a
striped golden horn and hooves, the house eye, a blush; a violet-navy outline. Frames: two idle (the mane flutters),
squash (take-off, landing), stretch (rising), falling (legs reaching), hurt (X eyes) and the hurt turned a quarter (the
tumble spins with flips). The clouds: pieces 16x24 (two ends, three middles with different bumps; any order joins
without a seam), white with lilac shading and a violet outline. Also: puffs, rain drops, a grumpy thunder cloud
(crackling frame), its bolt, a smiling turning star, sparkles in the mane's colours, a tear, a sleepy crescent moon.
The title logo: UNICORN in pink, HOP in sky blue.

## Sound and music
The house synthesiser (games/common/src/house_audio.c) for the effects (main.c sound_init): a soft rising "boing" with
a sparkle (the jump), a longer springier one (the pink bounce), a fluffy puff (landing), a two-note chime (a star, A6
and E7, with echo), a sad slide down (a miss or a melt), a crackle and buzz (the zap, the only shake), a plip (the rain
starts), a fizzle (the melt), a six-note arpeggio C6-G7 (the rainbow); the house sounds for the menus, the medal and the
game over. The lengths keep the samples under the 64 KiB guideline with the house set.

Music: "Cloud Hopper" (tools/make_music.py): F major, speed 6, A A' A B (about 31 s): the house pluck in arpeggios,
an oom-pah bass, the soft pad, the breathy lead with a tune that hops up by thirds and floats down; A' doubles it an
octave up like a music box. Two alternatives (tools/make_music_alt.py -> music-alt/alt1.mod, alt2.mod; the owner picks):
"Cotton Candy Waltz" (a music-box waltz in B flat) and "Rainbow Gallop" (a chiptune gallop in G with drums). To switch,
call make_music_alt.py OUTDIR N from tools/build_assets.py instead of make_music.py.

## Tests
`make unicornhop-check` (about 2.5 minutes): the rules alone (tests/test_rules.c: the arcs, waiting, the first jump, no
steering, the landing margin to the pixel, never landing on a lower cloud, riding and the band, the pink bounce, the rain
cloud and its twin, thunder, stars, the rainbow, the camera and the players left below, determinism, the bot), the
libretro loader, the smoke tests (flow, start inputs, waiting, a miss and back to the title, pause, save RAM, joining and
leaving, 4 players, the bot, determinism with 1 and 4 players), the UI screens (tests/ui_test.sh: the title with 0-3
players joined, play, pause, 2 and 4 players, the game over with a medal, the results, and at 320 the rainbow, the night
and the aurora), the save states (the title's lobby, mid-climb, high up, a race, 4 players, paused, the game over, the
results) and their audit, all at 320, 427 and 520 columns, states across widths, and the 22 languages
(tests/i18n_runs.txt).
