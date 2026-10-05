# Spider Swing

An endless climb using release timing and steering in flight. A releases the
current web without changing the spider's position or velocity. Left / Right
steers only during free flight. An 88-pixel loose silk strand stays attached to
the spider, lifting overhead and trailing its movement. The visible strand is
the catch surface: touching any higher hanging flower pins the silk to it at once,
even when the body is far below. The released flower cannot catch again. Both
strand endpoints and the area swept between frames are checked against the
flower's 12-pixel radius. There is no narrow body zone or hidden flight timer.
The full attached web appears immediately with a blossom sparkle and catch sound.
The spider keeps its position and incoming velocity, and the rope eases toward
the normal swing length. Holding A does not repeat releases, and holding
a direction on a web never advances to another flower.

Swing motion uses a fixed-point rope constraint with gravity and a gentle
automatic pump. The pump stops near turning points so the swing can reverse
naturally. A release keeps the actual tangent velocity; it adds no launch boost.
Flight uses gravity and bounded horizontal acceleration. Attachment preserves
position and incoming velocity, with no phase matching or teleports. The camera
eases in both axes and keeps the complete spider visible below the HUD.

Seeded flower positions vary in height and horizontal position, including runs
in the same direction. Flowers remain fresh for seven seconds initially and
five seconds later. Catches score 10; dewdrops add 3. Bees appear after the
introductory climb, patrolling an outer lane away from the main transfer route.
There are no closing flytraps in this revision. A missed catch, bee collision,
or waiting until a flower wilts ends the run.

Art follows docs/art-direction.md and uses house_style.py. The original spider
is retained. Five hanging flower shapes—fuchsia, bell, star, rosette and orchid—
each come in pink, lilac and amber for fifteen combinations. Shape and colour
are independently seeded and stay fixed at each anchor. All flowers use the
same catch radius and lifetime. Each shape plays a distinct D-major pentatonic
chime on a successful catch: D5, E5, F-sharp5, A5 and B5, respectively. Colour
changes preserve the shape's note; dewdrops keep their separate reward sound.
The catch chime replaces the overlapping generic ding and landing scuff.
Flowers have calyces above their petals;
curved branches attach each blossom to a shaded leafy vine in world coordinates.
All vines, branches, blossoms and leaves use fixed world coordinates. Background
lianas scroll at the same speed as the blossoms, including in small viewports;
foreground vine samples and leaves are anchored to world heights. Loose silk
follows the spider throughout flight and visibly pins at contact. Small flight
trails clarify free movement. Compact sprites keep
small multiplayer panes readable.

Players 2-4 join on the title and receive independent cameras and scores. The
shared house kit supplies the lobby, pause, results, medals, saved records,
font, effects and music. Checked battery scores stay outside save states; the
title's displayed record is cached so restored titles replay consistently.

Validation: strand and swept-strand contacts, body-distance and timing regressions,
twelve courses with broad opening release windows using simple steering,
twelve seeded two-minute climbs using ordinary release and steering
inputs; continuous world motion; correct release velocity; no held-input
shortcuts; real keyboard flight steering; 1-4 player view clipping and maximum
screen movement through release/catch transitions; deterministic pixels,
pausing, battery persistence, libretro/audio and fresh-process state replay.

Build: make spiderswing-check / spiderswing-windows / spiderswing-screenshots.
Windows: dist/windows/SpiderSwing.exe. Code MIT; original art/music/sound/design
CC BY-NC-SA 4.0, as stated in LICENSE.
