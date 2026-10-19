# Prickle Patch art TODO

> **Code-drawn art, no image generation.** Every sprite and tile of Prickle Patch is drawn by
> `games/pricklepatch/tools/make_art.py` with the 8BCraft house style kit (`games/common/tools/house_style.py`,
> rules in `docs/art-direction.md`). Image agents: there is nothing to generate here. Do not add AI
> images to this folder.

To change the art: edit `tools/make_art.py`, run `make pricklepatch-art` (it rewrites `art/`), look at the
game (`make pricklepatch-screenshots`), commit `art/` with the script.

## Checklist (house style)
- [x] 1-px outline on every sprite (the material's darkest shade towards violet or navy)
- [x] 3-4 shades per material, light from the top-left
- [x] the gardener reads at 1x on the busiest garden (a straw hat, a white-faced hedgehog on green)
- [x] the swarm shares one 15-colour palette (the soft layer, `SOFT_PAL`): every pest keeps its own family of colours
      (slugs orange, beetles violet, ants red, wasps yellow, crows grey, thistles green and violet)
- [x] the gems are violet, red and gold diamonds; no violet flower in the grass
- [x] the title logo colours: two ramps in src/draw.c (title_ramps: the spines, the leaves)
- [ ] (later) more frames for the bosses; a second garden's own tiles (today: the night palette)
