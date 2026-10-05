# Crab Coconut

Move Left / Right to bounce your coconut with the crab's claws. Where it hits
changes the rebound angle. Break the raining shells before they reach the crab.
Missing your coconut ends your run; shells reaching the sand end the shared round.

The full 320x240 screen is one court, with walls at the screen edges. Coconut
centres bounce between x8 and x312 and can rise to y8. Solo crabs rest at y224
with their catch line at y209. Every player uses the same full 32x32 crab sprite,
including multiplayer. Multiplayer crabs are staggered by three pixels in height,
ending at y224 so the entire sprite fits; catch lines follow their actual claws.
The initial eighteen shells are scattered across x18–302 and y40–126, with no
overlaps, columns or shared rows. Score labels overlay the upper edge without a large HUD panel.
The expanded height increases the flight time between shells and crab.

Coconuts start at a total speed of 1.8 pixels per frame. Speed increases smoothly
with elapsed play time to 4.6 pixels per frame after two minutes. Rebounds preserve
that total speed: edge catches change direction without an abrupt speed boost.
Fixed-point vector normalization and four collision substeps prevent tunnelling.

One to four players share a single shell array and camera. Each has its own
crab, coconut, movement, score and life. A coconut rebounds only from its owner.
Shell damage is immediately visible to every player, and a destroyed shell scores
once for the player who broke it. Coconut outlines match the four player colours.
Crabs and coconuts can cross each other without collisions. Surviving players
continue after another player's miss.

Shells rain down individually from above the top edge at seeded random horizontal
positions and intervals. Each has its own falling speed, 0.035–0.055 pixels per
frame. Entry positions avoid overlapping nearby shells. Arrivals become more
frequent over time and when fewer than ten shells remain. An empty board gets
one falling shell immediately; it never refills with an ordered formation.
After a minute, occasional tougher shells require two hits.
If shared shells reach y196, every remaining player loses. Individual rebounds,
shell damage and score bonuses use ordinary steering inputs and seeded boards.
Shells render in front of the sand so they remain visible near the failure line.
After a round ends, remaining shells accelerate downward and continue behind
the results transition. A shell is removed only after its entire 16 px sprite
has passed below the screen at y248. This animation awards no extra points and
spawns no new targets.

A starts; Start/Select pauses all players. Players 2–4 join and leave on the
title, with P1 starting the run. Results return to the title with the lobby intact.
Best scores use the existing checked battery format. Simulation, renderer,
title, UI and audio are registered for deterministic save-state replay.

The original crab, coconut, shells, beach, music and house presentation are
preserved. Artwork is code-drawn using house_style.py and runtime court drawing.
Physics remains integer Q16 at 60 Hz, independent of artwork.

Validation covers scattered non-overlapping starts, individual arrivals and
empty-board replenishment, falling motion, full-size crab/claw alignment,
shell visibility across the sand and bottom edge, off-screen retirement,
gradual speed, rebound directions, full-court boundaries,
misses, low-shell failure, a shared shell hit scoring only once, independent
coconuts, twelve two-minute four-player courses, deterministic replay, one board
drawn on the full screen, actual controls, pause, SRAM, libretro and save states.

Build: make crabcoconut-check / crabcoconut-windows / crabcoconut-screenshots.
Windows output: dist/windows/CrabCoconut.exe, statically linked SDL2.
Code MIT; art, music, sound and design CC BY-NC-SA 4.0, as stated in LICENSE.
