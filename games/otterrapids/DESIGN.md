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
