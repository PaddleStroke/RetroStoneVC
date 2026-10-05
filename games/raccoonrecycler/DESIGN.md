# Raccoon Recycler

An endless, single-screen conveyor sorter for 1-4 players. Nine toys combine
ducks, sea stars and snails with yellow, white and blue. Snail shells carry
their colour; their bodies stay neutral. Press Left, Right or Up/Down to set your
junction; it stays selected. Sorting happens when the toy reaches the fork,
not when the button is pressed. A correct route earns one point; the first wrong
route immediately eliminates that player. Each player has an independent score
and route. There are no hearts or extra lives.

The three bin pictures are the rule. Neutral duck/sea-star/snail signs mean sort
by species; yellow/white/blue rectangular signs mean sort by colour. In each
mode, read the actual signs: the category order reshuffles at every rule change.
All six permutations are possible, and the new order differs from the previous
one. Up and Down both select the centre bin. The opening uses duck/star/snail
order to introduce the station; subsequent orders are seeded and random.
The rule alternates every
12 deliveries. New deliveries stop, every existing toy finishes its conveyor
and chute journey, then the empty station pauses for 1.5 seconds. The signs change
with a three-note chime after 0.5 seconds, changing both the rule and bin order,
giving a full second to read them before
the next delivery starts. No toy is judged against a rule that changed in transit.

The initial toy is already on the belt in the frozen title. Starting preserves
that exact position and seed. The belt is continuous fixed-point motion, with
new objects arriving every 96 frames initially, reaching 48 frames after
36 deliveries and 36 frames after 72. Spacing accounts for belt speed: the
physical gap shrinks from 96 to 72 pixels over the first 36 deliveries, then
stays tight as the belt accelerates. Belt speed rises from 1 to 2 pixels/frame.
The maximum pace leaves 0.6 seconds between toys and roughly a full second of
unobstructed belt below the HUD before the junction. Moving treads follow the same fixed-point speed as
objects and pause during rule changes. Items are emitted before treads in OAM
so conveyor stripes cannot overwrite their pixels. The junction is lower at
y=160; nearby bins have short 64-pixel side chutes and a straight centre chute.
Delivered copies
follow a connected chute, then descend behind the physical bin lip. Multiplayer
uses the same incoming toys and coloured overlapping junction strips; once routed,
personal copies follow each player's selected side. Full-size raccoons stay visible.

During play only centered scores appear above the scene;
there are no item names, countdown bars, streak labels, instruction text or textual
bin labels. House lobby, pause, results, medals, save RAM and state registration remain.

Validation covers arrival-time scoring and last-frame switching, persistent gates,
single-miss elimination and surviving multiplayer opponents, all three persistent
routes across every bin permutation, no mid-delivery order or
rule changes, monotonically decreasing delivery intervals, shrinking physical
gaps and enough separation for every routed toy to finish its chute animation,
16 seeded long runs with four bots using a 12-frame reaction delay
surviving the pace cap and over 30 rule changes,
continuous object positions, strict rendering, frozen title, real input, SRAM,
deterministic screenshots, save-state replay and native Windows 1P/4P execution.
The current bin order belongs to the registered world and restores with the
exact items, rule-change timing and seeded random state.

