# Fishy Feast art TODO

> **Code-drawn art, no image generation.** Every sprite and tile of Fishy Feast is drawn by
> `games/fishyfeast/tools/make_art.py` with the 8BCraft house style kit (`games/common/tools/house_style.py`,
> rules in `docs/art-direction.md`). Image agents: there is nothing to generate here. Do not add AI
> images to this folder.

To change the art: edit `tools/make_art.py`, run `make fishyfeast-art` (it rewrites `art/`), look at the
game (`make fishyfeast-screenshots`), commit `art/` with the script.

## Checklist (house style)
- [x] 1-px outline on every sprite (the fish navy #281840, the players' fish their colour ring; the urchins and
      props navy); the bubbles are rims only
- [x] 3-4 shades per material, light from the top-left (the big fish: a round cushion, the highlight top-left)
- [x] every fish reads at its smallest tier (3 px: a dot with an eye) and its biggest (40 px: 7 parts)
- [x] players 2-4 = the goldfish's orange and ring turned (blue, green, purple); the bots' eight colours stay
- [x] backgrounds: the floor muted and darker than the fish, no outline; the rays added light only
- [x] the plankton specks are the brightest small things on the floor (lime, pink, aqua)
- [x] the title logo colours: two ramps in src/draw.c (title_ramps)
