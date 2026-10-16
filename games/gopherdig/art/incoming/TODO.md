# Gopher Dig art TODO

> **Code-drawn art, no image generation.** Every sprite and tile of Gopher Dig is drawn by
> `games/gopherdig/tools/make_art.py` with the 8BCraft house style kit (`games/common/tools/house_style.py`,
> rules in `docs/art-direction.md`). Image agents: there is nothing to generate here. Do not add AI
> images to this folder.

To change the art: edit `tools/make_art.py`, run `make gopherdig-art` (it rewrites `art/`), look at the
game (`make gopherdig-screenshots`), commit `art/` with the script.

## Checklist (house style)
- [x] outline on every sprite (the gopher #341a24, the props navy #1e182c, the crumbs the blocks' #22122e)
- [x] 3-4 shades per material, light from the top-left (the blocks: a light top-left rim, a dark bottom-right one)
- [x] the gopher reads at 1x on the busiest well: it always stands in a dug-out (dark) cell
- [x] players 2-4 = the hard hat's yellow turned (blue, then the two farthest hues); the fur never changes
- [x] backgrounds: the earth and the back wall muted and darker than the blocks, no outline
- [x] the title logo colours: two ramps in src/draw.c (title_ramps): GOPHER fur brown, DIG hard-hat yellow
