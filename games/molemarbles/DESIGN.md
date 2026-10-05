# Mole Marbles

A Pang-inspired endless arena starring a miner mole and bouncing gemstones.
One full 320x240 cavern, four equal-sized players and no player collision.
Left/right moves at 2.7 pixels/frame; A sends a harpoon vertically upward.
Both tip and trailing rope hit balls, and holding A repeats after its shot ends.
Shots climb 6 pixels/frame and last at most 56 frames; 8-frame refire interval.

Four sphere radii: 22,15,10,6 pixels. Each larger ball splits into two of the
next size, with opposite horizontal velocities. Smallest balls disappear in
visible crystal fragments. Split = 10 points, final shatter = 15 points.
The opening is one tiny radius-6 ball, frozen behind the title and retained
when A starts. All ball sizes have 30-percent slower initial horizontal and
vertical motion. Gravity is reduced proportionally to preserve approximately
50-percent higher bounce arcs: tiny 82 pixels, small107, medium140 (previously
54/70/92). Largest bounce uses available cavern height and respects the ceiling.
Floor y208; ceiling y24. Moles have a16x22 collision body. Ball/hero collision
interpolates both trajectories; rope collision sweeps balls. Walls reflect;
each marble casts a floor shadow.

Timed arrivals run independently of destruction. Normal opening arrivals are
single tiny balls roughly4.5-6.5seconds apart, including1.5seconds of visible
ceiling warning. An empty cavern schedules a replacement after exactly1second.
Warnings show every incoming position when a wave contains several marbles.
Splits remain harmless42frames; opening marble harmless60frames. Protection
pulses the gemstone shading, with no plus/minus symbols beside the balls.

Difficulty uses the best current player's score so multiplayer does not speed
up solely because more people joined. At60points, two-piece balls enter the
random size pool; at180four-piece balls; at420giants, which remain uncommon.
Groups begin with one ball, then include randomized pairs from45points and
one/two/three arrivals from150points. New arrivals choose varied sizes, colours,
positions and timer spacing. Split children still conserve mass. Weighted mass
budget rises from6to20; a full arena skips a scheduled wave instead of waiting
for the previous giant to be destroyed. The next clock continues independently.

All existing balls gradually accelerate with score, from70percent of the old
opening velocity to120percent at1500points. Gravity/bounce are scaled together
so faster play retains useful arc height. Arrival cadence also rises gradually
to about2.5-4.5seconds. Both speed and density have firm caps.

Hit feedback is immediate. Dead player is removed after a brief hit pose;
survivors continue. Eight-frame hitstop freezes shared motion. Everyone down
leads to the house results panel, records, medals and return to the lobby.
Pause, four gamepad ports, keyboard players1/2, SRAM and registered save states.
Run-start best snapshot keeps NEW BEST presentation deterministic after state
load without rolling back battery records.

Code-drawn art: stout mole, digging claws, mining helmet/light; hue-swapped
players keep eyes, metal and light colors. Navy-violet cavern uses distant
arches, stone ribs and muted crystal clusters; foreground gems have saturated
three-shade facets, a dark round outline and glints. No instructions during
play; scores use a compact top row in multiplayer. All moles use the same
32x32 cell in every mode, with a body approximately24pixels wide.

Validation: standalone physics fixtures cover frozen title, finite held-shot
repeat, all bounce apices, swept collisions, mass-conserving splits, visible
grace, normal90-frame warnings and60-frame empty refills, individual player elimination and survivors.
Fixtures measure the actual bounce increase, opening speed, independent timed
arrivals, randomized late groups and score-based acceleration. Twelve ordinary-input four-player bot runs compare entire worlds every frame
for deterministic replay and check capped density. Bots predict visible ball
arcs and issue only left/right/A. Full integration suite covers actual scripted
pads, lobby/pause/results, SRAM, libretro, screenshots and save-state audits.
