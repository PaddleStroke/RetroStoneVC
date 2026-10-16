# Caterpillar Crawl art TODO

> **Code-drawn art, no image generation.** Every sprite, tile and panorama of Caterpillar Crawl is drawn by
> `games/caterpillarcrawl/tools/make_art.py` with the 8BCraft house style kit (`games/common/tools/house_style.py`,
> rules in `docs/art-direction.md`). Image agents: there is nothing to generate here. Do not add AI
> images to this folder.

To change the art: edit `tools/make_art.py`, run `make caterpillarcrawl-art` (it rewrites `art/`), look at the
game (`make caterpillarcrawl-screenshots`), commit `art/` with the script.

## Checklist (house style)
- [x] 1-px outline on every sprite (the caterpillar #1e2238 navy, the leaf its darkest green, the flower #782850,
      the berry and the UI pips #161228, the obstacles #281e2c)
- [x] 3-4 shades per material, light from the top-left
- [x] the hero reads at 1x on the busiest background (the soil checker, a long body); the head chomps, bonks, beams
- [x] players 2-4 = the greens turned (build_assets.py P2_HUE: pink, then blue and orange), heads and bodies alike
- [x] backgrounds: the lawn and the soil lower in contrast than the caterpillars, no outline on the garden
- [x] the title logo colours: two ramps in src/draw.c (title_ramps)
