# Frog Reach

A one-button endless crossing game in a quiet swamp. Hold a face button to extend
the frog's tongue toward the next lily pad. Release while its tip is over the
pad to latch on and pull the frog across. Too short or too long means a splash.
A release within four world pixels of the fly catches it for a perfect bonus.
Inspired by Stick Hero's simple distance judgement; the tongue, swamp, frog,
moving lily pads, code, sound and artwork are original.

Pads score 10; a perfect fly adds 5. The first two pads are 64 and 56 pixels wide;
later widths vary by seed between 24 and 64, narrowing with distance. Distances
also vary. From pad 4, every third pad drifts sideways by 8–16 pixels. A moving
pad carries its resting frog; a caught target continues drifting through the leap.
Waiting too long sinks the current pad: 5.5 seconds initially, falling to 3 seconds later.
The tongue grows 4 pixels/frame, rising to 6 with distance. The jump is a 28-frame
arc; the tongue stays visibly tethered to the caught pad. One life, endless score.

The simulation is fixed-step Q16. Release validation uses the same position and
width shown by the renderer. The bot uses ordinary held/released button input
and the current target's predicted next position; it does not bypass misses.
Twelve course seeds test short, overlong, held and accurately released reaches,
perfect bonuses, determinism and four-player scoring.

The title draws the actual starting pond, pads and frog. Starting preserves the
course and camera position. A brief starting tap leaves the frog resting safely;
a continuing hold begins extending after a two-frame guard. Later presses extend
immediately.

Up to four players join on the shared title. Each has an independent course view
and progress; falling behind a rival never eliminates a player. Two use top and
bottom views, three a top half and two lower panels, four quadrants. Two-player
panels use the full-width, 32-pixel frog. Narrow panels show a wider world area
with a compact 16-pixel frog. Multiplayer overlays only a small player label and
score in each view, leaving the pond visible without status panels or repeated
instructions. Pause freezes all worlds and cameras.

Art is code-drawn by tools/make_art.py using the shared house_style kit. Big eye
bulbs, folded hind legs, cream throats and a long mouth make the frog recognizable.
The swamp has hazy reeds, clouds and water glints; runtime foreground pixels draw
exact lily-pad widths and a three-shade pink tongue. Flies flap, catches sparkle,
and failures splash. The font, title, lobby, results, medals, pause, sounds and
tracker music use games/common, matching Leady Squid and the other house games.

Build: make frogreach / frogreach-windows / frogreach-check / frogreach-screenshots /
frogreach-art. Windows output: dist/windows/FrogReach.exe, statically linked SDL2.
Code MIT; art, music, sound and design CC BY-NC-SA 4.0, as stated in LICENSE.
