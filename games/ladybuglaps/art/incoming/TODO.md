# Ladybug Laps art TODO

> **Code-drawn art, no image generation.** Every sprite and every track of Ladybug Laps is drawn by
> `games/ladybuglaps/tools/make_art.py` with the 8BCraft house style kit (`games/common/tools/house_style.py`,
> rules in `docs/art-direction.md`); the tracks' layouts come from `src/tracks.c`. Image agents: there is nothing to
> generate here. Do not add AI images to this folder.

To change the art: edit `tools/make_art.py` (or a track in `src/tracks.c`), run `make ladybuglaps-art` (it rewrites
`art/`), look at the game (`make ladybuglaps-screenshots`), commit `art/` with the script.

## Checklist (house style)
- [x] 1-px outline on every sprite (each ladybug the darkest shade of its shell towards violet; props navy)
- [x] 3-4 shades per material, light from the top-left (the shells, the porcelain, the fruit, the pots)
- [x] the four ladybugs read at 1x on every track: red, yellow, blue and green shells with black spots
- [x] objects cast a shadow down-right; the far floor below the table has no outline and is darker
- [x] transparency only by the house checker (the shadows, the see-through middle of the leaves, the touch buttons)
- [x] the title logo colours: two ramps in src/draw.c (title_ramps)
- [ ] (idea) more scenery per track: each track uses 400-550 of its 600 tiles
