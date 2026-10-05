# Otter Rapids

Endless river navigation for 1-4 full-size otters on one shared screen. Hold
Left/Right to paddle, counter the visible currents and avoid scattered rocks,
floating timber, whirlpools and submerged crocodiles. Islands split the river
into two channels and taper away before the channels rejoin.

Both banks curve independently between seeded shoreline positions, changing
the river's width from broad reaches to narrower channels. Island lengths,
widths and lateral offsets vary by seed and section. Sand, grass, reeds and
island foliage use the same world coordinates as the physical shoreline.
Hazards also appear in the water beside islands. Their full depth is checked
against both banks, the island and protected routes through each branch.
Neither branch can be blocked; fish resume only after a generous merge buffer.

Catch every golden fish. A catch earns 15 bonus points; missing a fish ends that
player's run, just like touching an obstacle or the island. Fish are claimed
independently in multiplayer, so every otter can catch the same fish. Mandatory
fish appear only in the common river, with generous buffers before a fork and
after its merge. Players can freely choose either branch; neither requires
crossing land to reach a fish. Survival also adds distance points.

Course generation uses seeded waypoints spaced 200-280 world pixels apart.
The former left/right slalom pairs are replaced by 1-3 individually placed
hazards per section, each with an independent lateral position and depth offset.
Placement reserves a swimmable route through common stretches and a route through
both sides of every fork, considering obstacle dimensions and the otter's hull.
There are no mandatory gates. Routes are generation constraints, not drawn guides.
The otter steers continuously with ordinary D-pad input, not automatic route choice.
The banks constrain lateral movement using their actual changing positions.
Geometry is cached only by seed and section ID, and reconstructs identically
after any save-state restore or new run.

River speed grows from 2.4 to 3.8 pixels/frame with distance, with a fixed cap.
Local currents push sideways by up to 0.46 pixels/frame; directional surface
streaks show them. Paddling reaches 2.7 pixels/frame, so currents remain manageable.
The island shape and collision use the same seed and world coordinates. The
multiplayer camera conversion is scroll + HERO_LINE - player_y; collisions and
fish catches now exactly match each otter's drawn vertical position.

Crocodiles show a muted, full-size underwater silhouette. Touching one records
its location, surfaces it with ripples, opens its toothed mouth and snaps it shut.
The bite animation continues after loss. All bite records belong to the registered
world, so save-state replay preserves the animation. The otter's head, muzzle,
body, paddling paws and trailing tail have one consistent upright orientation.

A starts the actual frozen title scene. Other pads join or leave there. Pause
freezes the river, current effects, swimmers and bite animation. Existing checked
SRAM is preserved. A registered best-score snapshot keeps UI replays deterministic
without rolling back the battery save. House results, medals and retry remain.

Validation includes 64 seeds at both opening and maximum pace (128 courses), four
players using both branches and catching every fish, geometric route clearance,
mandatory fish exclusion around forks, independent fish claims, missed-fish loss,
crocodile surfacing triggers, island collisions, strict rendering, native Windows
1P/4P play, real controls/lobby/pause/SRAM and deterministic state/picture replay.

## Widescreen

A vertical scroller on one shared screen: the game opts in (`max_width` = `RS_SCREEN_W_MAX`,
`WIDESCREEN_otterrapids = 1`), the 320-px stage is centred (`rs_screen_origin(rs_screen_margin())`) and 320 is
unchanged pixel for pixel. The river, its banks, islands, hazards, currents, fish, collisions, the camera and the bot
are those of 320: the rules never read the width (the same logs at 320, 427, 520 and 560). A wider screen shows more
of the land beyond the banks, which never leave the stage. The foreground is drawn 16 px wider on each side, so the
banks' bushes overhang the stage's edges instead of being cut. Behind it, the land layer (BG3, only on a wide screen)
continues the grass: a meadow next to the river (bushes, rocks, stones, logs, ponds with reeds and cattails, stumps,
flowers), then a forest of round canopies. Its tiles are code-drawn once (draw.c `land_art`, the banks' and the
river's style and palette); its map is rewritten each frame from the world's tile rows, so the land scrolls with the
river. Each cell's object is seeded by the course and the cell, so the land is the same at every width. The UI layer
is 128 tiles wide beyond 512 px; the HUD, title, pause and results stay centred (the house kit). Two to four players
share the same screen, the otters where they were. Saved objects do not depend on the width: `state_loaded`
(`draw_layout`) sets the layers up for the new width.

A run starts the same at every width: the first hazards and fish are at the same distance from the otters as at
320, and the stage (the middle 320 columns of the picture) is the 320 picture from the first frame on.

Tests: the check target runs at 320, then at 427 and 520 (the libretro core at 16:9 and Wide, check_game.py, the UI
screens, the save states); tests/wide_test.sh checks the picture widths, the same logs at 320/427/520/560, the stage
the same picture as at 320 (the start of a run, its first hazards, four otters, the title, later in a run), `--resize`
during a run and states loaded across widths (`--any-width`); the language test runs at the three widths.
Screenshots: title, play and game over at 427 and 520.
