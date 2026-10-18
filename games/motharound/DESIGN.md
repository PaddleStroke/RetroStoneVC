# Moth Around

A tiny moth circles a garden lantern. Leaf wreaths and cobweb rings close inward;
steer Left/Right through their gold-tipped openings. Inspired by the compact
survival play of Super Hexagon, with original garden art and music.

## Play

A starts. Left/Right steer; releasing stops at the current angle. Both directions
together stop too. A run ends when a ring catches the moth. Each cleared ring
scores 10, a passage near the middle of its opening adds 3, and every surviving
second adds 1. One life; A on the result screen returns to the title.

Every moth uses the same 44 px orbit around one lantern, in solo play and in
all multiplayer slots. This is the old four-player innermost radius. Gameplay
uses a 16 px moth in every mode, smaller and closer to the lamp than the old
32 px solo moth at 68 px. The collision core is 6 px; decorative wing tips are
forgiving. Rings have one to three gold-tipped openings. Their widths vary:
primary openings are 48–68 angle units early, 38–58 later; extra openings vary
from 38–56 early to 33–51 later (256 units make a full circle). The initial
radius also varies. Closing speed increases a little with every ring, from
1.05 to 2.3 pixels per frame, capped after 100 rings; later openings gently
rotate. Every ring is visible for at least 70 frames before first contact.
Scheduling leaves at least 67 frames between the previous ring clearing the
shared orbit and the next reaching it, enough for a half-turn
plus reaction time. This applies to all available gap choices. No random
mid-flight jumps and no obstacle kills caused by a rival.

## Presentation

320x240 house pixel style: lavender moth with brass eyespots and animated wings,
warm brass lantern, leafy fence, dotted flight path, gold gap edges. Dusk fades
to moonlight over 40 seconds; stars appear. Original code-drawn sprites and
backgrounds use house_style.py; a D-dorian loop uses house_music.py.

1-4 simultaneous players share one full-screen view and the same ring sequence.
All modes use purpose-drawn 16 px moths. Moths at the same angle overlap;
there is no radial offset or separate difficulty for any player slot. All moths
steer at the same angular speed and meet each ring on the same frame. Course
generation, collision windows and scoring are identical to solo play. One lantern, common
rings and individual score labels appear, with no control instructions during play.
Outside the flight paths, the renderer compresses distant radii to keep the
complete incoming rings and their openings visible within the screen. The moth
orbits and collision zone retain their actual scale. Leaf/cobweb thickness and
gold gap tips stay readable. The control hint remains on the title. Players join/leave on the title; P1 starts.
Enter pauses everyone. The result screen ranks scores, with ties supported.

## Technical and checks

Integer fixed-step 60 Hz simulation, Q16 radii, 256-step orbit, deterministic
seeded patterns; collision tests sweep the ring's previous/current radii.
Battery save has a checked MOTH/version record. Registered state includes the
simulation, title lobby, UI, audio and renderer; battery bests stay outside states.

`make motharound-check`: every starting angle against 1–3 narrow gaps with
rotation, a 24-frame reaction delay and the maximum speed; five-minute runs
with 1–4 players verifying safe contact intervals, size/width/count variation
and the speed cap; all openings' collision/scoring checks; 12 generated courses
through 120 seconds with four ordinary-input bots; a five-minute solo/multiplayer
comparison with identical inputs and independent failures; deterministic replay,
1-4 player shared-orbit placement and small sprite rendering at all angles,
including intentional overlap; real flow/pause/score saves,
libretro/audio, save/load/fresh-process tests and mutable-static audit.
`make motharound-windows`: self-contained `dist/windows/MothAround.exe`.
`make motharound-screenshots`: title/play/pause/results screenshots.

## Widescreen (docs/spec.md "Widescreen")

The game opts in (`max_width` = 560, `WIDESCREEN_motharound = 1`): 320 x 240 is unchanged pixel for pixel; at 427
(16:9), 520 or 560 (phones) **the garden is wider, the lantern and its rings are the same**.
- **A fixed screen, centred**: `rs_screen_origin(rs_screen_margin())` (draw_init). The lantern stays at stage x 160,
  the orbits, the rings (and their compressed approach, which never looked at the screen's width), the moths and the
  HUD keep their places; the screen shows stage x -margin .. 320 + margin. The one full view
  (rs_viewport_layout) now starts at stage x -margin: the ring layer's pixels are drawn in the view's columns (a
  buffer as wide as the widest screen), the sprites in stage coordinates (in the multiplayer frames, whose view is a
  viewport, relative to its corner: spr_dx), the score centred on the view, PAUSED on the screen.
- **The wider garden** (make_art.py `hills_wide`; build_assets.py): BG3 becomes a 128-tile map of a 1024 x 160 garden
  (screen y 80..239) with its own tiles and palette; over the lantern's 320 columns its bottom 96 rows are the classic
  garden and nothing is above them (build_assets.py checks it). The hills, the fence and the hedge go on round in
  stage coordinates; above the hedge, beyond the 320 columns: foxgloves and a rose arch on the left, a birdhouse on
  its pole and a nodding sunflower on the right. Six more stars come out in the wider sky.
- **The same game**: the rules never look at the width; tests/wide_test.sh compares the whole logs (scores, game
  overs, the final state and its hash) of 1-player and 4-player bot runs at 320, 427, 520 and 560, changes the width
  three times during runs (`--resize`) and loads states saved at one width at the others.
- **Save states load at any width**: `state_loaded` (draw_relayout) compares the width the maps were laid out for
  (draw.laid_w, saved) with this one; another width sets this width's maps and the garden's tiles up again (every
  frame draws the whole screen anyway).
- `make motharound-check` runs the libretro test, tests/check_game.py (strict mode included) and the state tests
  again at 427 and 520, then tests/wide_test.sh and the language test at 320, 427 and 520;
  `make motharound-screenshots` adds title/play/gameover-427/520.png and play-4-players-520.png (tools/wide_shots.sh).

## Tall screen (docs/spec.md "Tall screen", the zoomed recipe)
A phone held upright: `rs_screen_zoom(ZOOM_H)` (512 lines, game.h) first in `game_init`. The RetroStone app scales the
512 lines to the display's height and crops the sides: 236 columns on a 1080x2340 phone, 218 on a 1080x2520 one, so
the lantern, the rings and the moths are about 1.5x the size of the old 320-column strip. The rings never reach more
than 107 px from the lantern (draw.c `visible_radius`, the gold tips included): they stay whole in the columns shown.
The 240 lines of the stage are the same pixels; the stage sits low (`rs_screen_origin_v`: TALL_BELOW = 40 lines of the
hedge's shade under it, the rest above).
- **The HUD at the screen's top** during a run: the score (sprites at `rs_screen_top()` + 8) and the rings line (BG1's
  top 48 lines, `rs_bg_pin`); with 2-4 players the tags go two a row (rows 1 and 3) inside the columns shown.
- **One look**: the title, the pause and the game-over / results panels stay in the run's zoomed view
  (`rs_screen_fill(1)` always on a tall screen), laid out in the columns shown (games/common/src/house_tall.h, when
  `ht_narrow()`), the house's standard calls at their default rows, as every game: `ht_logo` (MOTH over AROUND when
  one line does not fit) from row 2, `ht_title_draw` from row 15 (the slots, the prompt, BEST, the join line, the
  credits) with the hint wrapped on row 17; `ht_banner`, `ht_gameover_panel`, the `ht_results_*` ranking,
  `ht_retry_line_at`; PAUSED with `ht_big` (`ht_pause` draws only when the pause changes; this UI is redrawn
  every frame). The UI takes the stage's rows, so on the zoomed title the moth, the lantern and the
  leaves (decor) move up into the sky above the stage (draw.c `title_sky_dy`: the lantern mid-sky, its halo off the
  stage), with the moon and the stars; the panels get the moon and the stars too. 2-4 players share the one view, so
  nothing needs every column.
- **Decor**: the raster (draw.c `raster_out`) draws the lines past the stage: above, the dusk or night sky going on,
  deeper toward the top; below, the hedge's shade, darker with depth. BG2 and BG3 are hidden there (window 1), BG1
  too past the HUD's band; line 0 puts the stage's settings back. In the sky above the stage: the moon in the columns
  shown, the first stars from the start and more as the night falls, and three leaves drifting down from the screen's
  top into the hills, behind the rings (sprites culled at the screen's edges, not the stage's).
- Landscape and 4:3 are the same pixels as before (checked at 2340x1080 and 320x240).
