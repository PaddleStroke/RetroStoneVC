# @NAME@: design

(c) 2026 Pierre-Louis Boyer (8BCraft), CC BY-NC-SA 4.0 (games/@ID@/LICENSE). A game for the RetroStone
virtual console (docs/spec.md), in the 8BCraft house style (docs/art-direction.md).

> Made from games/_template by `tools/new_game.py`. Fill in every TODO, delete this note.

## Pitch
TODO: three sentences. Who the hero is, the one verb the button does, why one more try.
One button, instant retry, a best score to beat.

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
| BG1 | the house UI kit: the logo, GET READY, the panels, PAUSED | fixed |
| OBJ | the heroes (P2 = palette swap), props and effects, the kit's digits, glyphs and medals | |

## Screens (the house flow)
title (the logo, the bobbing hero, PRESS A with the A glyph, BEST, the copyright line)
-> get ready (the 2P join line) -> play (the score at the top, Select pauses) -> the hit (hit-stop, a shake)
-> game over (the banner, the panel sliding up: score, best / new best, the medal) -> A: get ready again.

## Two players
Player 2 joins with A on pad 2 on the title or "get ready"; they play at the same time, player 2 is the hero
with its palette swapped (tools/build_assets.py P2_HUE); the panel shows both scores and the winner.

## Art
Code-drawn only (tools/make_art.py with games/common/tools/house_style.py): no image generation.
TODO: the hero's ramp (house_style ACCENTS), the props, the backgrounds.

## Sound and music
The house synthesiser (games/common/src/house_audio.c) for the effects, the house instrument set for the MOD
(tools/make_music.py). TODO: the game's own sounds (main.c sound_init) and the tune's key, tempo and mood.

## Tests
`make @ID@-check`: the libretro loader, the smoke tests (flow, pause, save RAM, player 2, the bot hook,
determinism), the UI screens (tests/ui_test.sh), the save states and their audit.
