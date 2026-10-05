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
