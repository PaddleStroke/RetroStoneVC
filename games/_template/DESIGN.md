# @NAME@: design

(c) 2026 Pierre-Louis Boyer (8BCraft), CC BY-NC-SA 4.0 (games/@ID@/LICENSE). A game for the RetroStone
virtual console (docs/spec.md), in the 8BCraft house style (docs/art-direction.md).

> Made from games/_template by `tools/new_game.py`. Fill in every TODO, delete this note.

## Pitch
TODO: three sentences. Who the hero is, the one verb the button does, why one more try.
One button, a quick restart from the title, a best score to beat.

## Rules
TODO: the mechanics, with every number in src/game.h (the tuning table: one place for every gameplay number).

| Rule | Value | Where |
|---|---|---|
| gravity | TODO | GRAVITY |
| the action (jump, flap, hop...) | TODO | JUMP_VY |
| scroll / speed | TODO | SCROLL_SPEED |
| scoring | one point per TODO | play.c |
| medals | bronze 10, silver 20, gold 30, pearl 40 | MEDAL_SCORES |

## Screen and layers
320x240 at 60 Hz, fixed step. The house layout (docs/art-direction.md "Backgrounds"):

| Layer | Contents | Scroll |
|---|---|---|
| backdrop | the sky (or water) gradient, one colour per line (raster callback) | - |
| BG3 | the far layer: low-contrast silhouettes, no outline | 1/4 |
| BG2 | the playfield front: ground, obstacles | 1 |
| BG1 | the house UI kit: the logo, the title's lines, the panels, PAUSED | fixed |
| OBJ | the heroes (P2-P4 = palette swaps: OBJ 1, 4, 5), props and effects, the kit's digits, glyphs and medals | |

## Screens (the house flow: no "get ready", the title is the only menu)
title (the logo, the bobbing hero, the player slots, PRESS A with the A glyph, BEST, the join line, the copyright
line) -> P1 presses A (or an extra start input in main.c START_INPUTS; the press is
also the first jump) -> play (the score at the top, Select pauses) -> the hit (hit-stop, a shake) -> game over
(the banner, the panel sliding up: score, best / new best, the medal; 2-4 players: the ranking with medals)
-> one press: back to the title (hu_over_back; the same players start again with A).

## Up to four players
Pads 2-4 join with A on the title (B leaves; the house kit's lobby, docs/art-direction.md "Title and players");
everyone plays at the same time on the same course, each a few steps behind the one before (P_OFFSET_X).
Players 2-4 are the hero with its palette swapped (tools/build_assets.py: house_style.player_palettes, from
P2_HUE); the title shows each joined player's icon (the hero_icon sprite); 3-4 players get score chips in the
corners; the results rank everyone by score with medals.

## Art
Code-drawn only (tools/make_art.py with games/common/tools/house_style.py): no image generation.
TODO: the hero's ramp (house_style ACCENTS), the props, the backgrounds.

## Sound and music
The house synthesiser (games/common/src/house_audio.c) for the effects, the house instrument set for the MOD
(tools/make_music.py). TODO: the game's own sounds (main.c sound_init) and the tune's key, tempo and mood.

## Tests
`make @ID@-check`: the libretro loader, the smoke tests (flow, start inputs, retry, pause, save RAM, joining and
leaving, 4 players, the bot hook, determinism with 1 and 4 players), the UI screens (tests/ui_test.sh: the title
with 0-3 players joined, play, 4 players, the results), the save states (the title's lobby, 4 players) and their
audit.
