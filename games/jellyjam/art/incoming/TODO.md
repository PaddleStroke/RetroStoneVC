# Jelly Jam art TODO

> **Code-drawn art, no image generation.** Every sprite and tile of Jelly Jam is drawn by
> `games/jellyjam/tools/make_art.py` with the 8BCraft house style kit (`games/common/tools/house_style.py`,
> rules in `docs/art-direction.md`). Image agents: there is nothing to generate here. Do not add AI
> images to this folder.

To change the art: edit `tools/make_art.py`, run `make jellyjam-art` (it rewrites `art/`), look at the
game (`make jellyjam-screenshots`), commit `art/` with the script.

## Checklist (house style)
- [x] 1-px outline on every sprite and jelly (dark violet `#281030`), open where two jellies of a colour join
- [x] 3 shades per jelly colour, light from the top-left, a white glint; one look per colour's face
- [x] the jellies read at 12 px (3-4 players) and 16 px (1-2 players)
- [x] players 2-4 = the hero Jam's pinks turned (blue, green, purple); the toque stays white
- [x] the chefs: our own animal cooks (mouse, duckling, bunny, cat, piglet, fox, panda, bear), 3 moods
- [x] backgrounds: the wall a desaturated mid teal, the jars' insides dark plum: the jellies are the brightest things
- [x] the title logo colours: two ramps in src/draw.c (title_ramps)
