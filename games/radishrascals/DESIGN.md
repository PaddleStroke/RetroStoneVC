# Radish Rascals

A four-direction endless garden defence game: press the matching D-pad direction
to shoo rabbits from the four beds; spare friendly hedgehogs. The farmer holds a
pitchfork, turns to face all four directions and visibly thrusts toward the
pressed bed, then retracts it. Up uses a back view, Down a front view and the
side directions use the existing profile. A single missed rabbit, wrong bed,
early shoo or shooed hedgehog ends that player's run immediately. Later input
on that frame cannot score. Each player has one harvest heart; a mistake does
not end another player's run. Holding a direction cannot silently repeat it.

The opening now uses the former level-eight maximum, the pace reached by roughly
1300 points: visitors every 54 frames and a 40-frame active response window.
After each 12 visitors the interval shrinks by 4 frames and the response window
by 2, down to fixed limits of 38 and 32 frames. The 18-frame soil warning remains
visible before every emergence. Friendly visits occupy a bed and finish naturally;
new visitors only use empty beds. The first warning still comes 25 frames after
start, allowing the player to orient themselves.

The new code-drawn farmer is a 48x48 profile with shaded straw hat, visible nose,
apron, boots and a forward arm during the thrust. Short multiplayer views use a
dedicated 24x32 drawings with the same readable silhouette. Front and back views
have their own idle and thrust drawings at both sizes, with the pitchfork's
hand position matching each orientation. Unused old squash/stretch frames were
removed to keep the enlarged set within the console's VRAM budget. The pitchfork rotates
through all four directions, extends and retracts over the existing 12-frame action
animation, and remains visible in hand between actions. Fork art is presentation:
it does not delay scoring or alter the input timing.

The garden has a layered hedgerow, picket fence, shaded flowering shrubs, lawn
texture and worn paths between the beds. Beds are framed raised planters with
soil, timber sides, front boards, nail highlights and visible radish bulbs/leaves.
The title shows the same planted garden and farmer used for the actual start.
During play graphic hearts and scores replace large per-view panels, instructions
and combo text. Independent split views and full house lobby/results remain.

Validation checks immediate single-mistake elimination, independent multiplayer
loss and no scoring after a fatal miss; all four actual farmer sprite tiles and
their action variants at both sizes; the exact old-cap opening and new fixed cap; warnings, correct
and mistimed inputs, held directions, friendly visitors; four-player deterministic
runs; 24 schedules at the new cap with a 12-frame reaction delay after emergence;
1-4 player sprite visibility and viewport clipping; real frontend input, pause,
checked SRAM, strict rendering, pixel determinism, saved-state replay and audit;
plus native Windows startup/play for one and four players.
