# Goat Gorge: design

(c) 2026 Pierre-Louis Boyer (8BCraft), CC BY-NC-SA 4.0 (games/goatgorge/LICENSE). A game for the RetroStone
virtual console (docs/spec.md), in the 8BCraft house style (docs/art-direction.md). The owner's idea: a pixel-art
precision platformer. The theme, the name, the hero, the art, the music and every room are our own.

## Pitch
A little goat kid in a red scarf climbs a gorge to the summit, one screen at a time. Run, jump, slide down walls and
jump off them, and dash once in the air in any of eight directions; a death puts the kid back at the room's start at
once, and only a counter remembers it. Five chapters, a berry hidden in most rooms for the brave.

## Controls
| | Console (pad) | Phone (touch, apps/launcher/games.cfg) | Keyboard |
|---|---|---|---|
| run | D-pad Left / Right | the Left / Right buttons (bottom left) | arrows |
| aim the dash | the D-pad (8 ways) | Up and the diagonals above the arrows | arrows |
| jump (hold: higher) | A (X) | the A button (bottom right) | X |
| dash | B (Y, L, R) | the B button, left of A | Z |
| fast-fall | Down | - | Down |
| pause | Start or Select | the app's pause button | Enter / Backspace |
| paused: back to the title | B | B | Z |

The title: Left / Right choose the chapter (the next opens when the one before is climbed), A starts it.

## The movement (src/game.h: every number; src/world.c: the rules, no SDK call)
Positions are whole pixels with a 1/256 remainder; every move goes pixel by pixel against the solids. 60 Hz.

| | Value | |
|---|---|---|
| run | 2.0 px/f; 6-7 frames to full speed, 5 to a stop, a turn in 4 | ground and air tuned apart (air 0.7) |
| jump | 3.5 px/f held up to 10 frames, then gravity 0.36 px/f^2 | full: 47 px (3 cells), a 2-frame tap 9 px |
| release | the rise cut to 1.5 px/f | variable height |
| apex | half gravity while jump is held and \|vy\| < 0.69 px/f | a floaty top |
| fall | 4.0 px/f; 5.75 holding Down | terminal velocity, fast-fall |
| coyote time | 6 frames | jump buffer 6 frames, dash buffer 4 |
| corner correction | 4 px (jumps), 5 px (dashes) | a ceiling's corner is slid around |
| ledge pop | 3 px | a lip is stepped onto (falling, dashing) |
| wall slide | 1.125 px/f while pushing into a wall | wall jump: 2.5 px/f away, the input held away 9 frames (a neutral jump is free) |
| dash | 8 px/f for 8 frames (64 px), 8 ways, after a 3-frame freeze | the direction is read at the freeze's end; 3 px/f kept after |
| super jump | a jump during a ground dash: 3.25 px/f along it | |
| refill | landing, a crystal, a spring | the scarf: red = the dash is ready, blue = used |
| hitboxes | the kid 10 x 13; against spikes 6 x 9 (inset); spikes only hurt moving into them | forgiving |

Spring: 4.5 px/f held 13 frames (87 px). Crumbling blocks shake 30 frames, fall, come back after 120. Rafts move at
1 px/f with a 30-frame pause at each end and carry their rider (its motion is added to a jump). Wind pushes the kid's
position (not its speed) by the room's wind, in gusts or always.

Feel: squash on landing, stretch on take-off, dust (jump, landing, wall, running), the dash's freeze with a white glow,
a 2-px shake and a trail of 5 ghosts (added half), a burst on a death, a flash on the respawn.

## The rooms (levels/README.md: the format; levels/chN.txt)
Single screens of 20 x 15 cells of 16 px. A room opens on one side (top, left or right): leaving through it fades to
the next room; falling out of the bottom is a death. Every room is checked and solved by `tests/solver.c` (a
breadth-first search over the touch buttons' inputs, run through the game's own rules): `levels/solutions.txt` holds
the inputs of each room's exit and berry routes, `tests/test_rooms.c` replays them all in `make goatgorge-check`,
and the bot (`--opt bot=1`) plays them in the game.

| Chapter | Rooms | New | Look |
|---|---|---|---|
| 1 the meadow | 12 | steps, gaps, ledges, spikes, wall jumps, the dash, springs | blue sky, green hills, firs, grass |
| 2 the pine wood | 10 | crumbling rocks, refill crystals, longer climbs | misty teal, dark pines, moss |
| 3 the crystal cave | 10 | rafts (sideways, up and down), ceiling spikes, crystals in a row | dark purple, glowing crystals |
| 4 the windy ridge | 10 | wind: steady, in gusts, updrafts and downdrafts | sunset, red sandstone, stone spires |
| 5 the summit | 10 | everything at once, spike-lined shafts | night, snowy granite, snow falling |

52 rooms, a berry in each. The solver's times (an expert's route) are 1-4 s a room; a first climb of a chapter takes
a few minutes and some deaths.

## Screen and layers
320 x 240 at 60 Hz. Backdrop: the chapter's sky (a raster gradient). BG4 far mountains, BG3 the middle ground
(parallax: they lean with the kid), BG2 the room (rock autotiled from 8x8 quarters, the chapter's cap of grass, moss,
crystal, sand or snow; spikes, ledges, crumbling blocks, decorations), BG1 the house kit (title, HUD, tips, results,
PAUSED). Sprites: the kid (OBJ 0 / 1), the props (OBJ 2), the kit (OBJ 3), the trail (OBJ 4, added half), effects
(OBJ 5), the glow (OBJ 6).

**Widescreen**: the room's 320 columns centred, the mountain carried on past its sides. **Tall screen** (a phone held
upright): `rs_screen_tall` (every column: a precision room must never be cropped), the room a third of the way down,
the sky and the HUD above it (pinned), the mountain below it (the touch buttons' band). The title and the results use
house_tall.h's layouts, pinned on the screen. A state saved upright loads in landscape and back (`ht_shape_reset`).

## Sound and music
House synthesiser effects (main.c sound_init): jump, wall jump, dash, land, death, berry, crystal, spring, crumble, a
room cleared, the respawn. A cheerful 4-channel MOD per chapter (tools/make_music.py: square lead, pluck arpeggio,
bass, drums), original.

## Tests
`make goatgorge-check`: the rules and every room's solution (test_rooms), the libretro loader, the smoke tests, the
screens, the save states and their audit, the widths 320 / 427 / 520 and states across them, the tall screen and the
phone turning (tall_test), the frame cost (bench_test), the 22 languages (tools/i18n_test.sh).
