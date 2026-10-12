# Unicorn Hop art TODO

> **Code-drawn art, no image generation.** Every sprite, tile and panorama of Unicorn Hop is drawn by
> `games/unicornhop/tools/make_art.py` with the 8BCraft house style kit (`games/common/tools/house_style.py`,
> rules in `docs/art-direction.md`). Image agents: there is nothing to generate here. Do not add AI
> images to this folder.

To change the art: edit `tools/make_art.py`, run `make unicornhop-art` (it rewrites `art/`), look at the
game (`make unicornhop-screenshots`), commit `art/` with the script.

## Checklist (house style)
- [x] 1-px outline on every sprite (the unicorn violet-navy #36245a, the clouds #584c96, the thunder cloud #241c3c)
- [x] 3-4 shades per material, light from the top-left
- [x] the hero reads at 1x on the busiest background (the aurora); squash on take-off and landing, stretch while rising
- [x] players 2-4 = the body recoloured (build_assets.py BODY_P: lilac, pink, mint), readable next to player 1
- [x] backgrounds: lower contrast than the playfield, no outline on the far layers (recoloured with the sky)
- [x] the title logo colours: two ramps in src/draw.c (title_ramps)
