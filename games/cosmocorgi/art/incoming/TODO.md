# Cosmo Corgi art TODO

> **Code-drawn art, no image generation.** Every sprite, tile and panorama of Cosmo Corgi is drawn by
> `games/cosmocorgi/tools/make_art.py` with the 8BCraft house style kit (`games/common/tools/house_style.py`,
> rules in `docs/art-direction.md`). Image agents: there is nothing to generate here. Do not add AI
> images to this folder.

To change the art: edit `tools/make_art.py`, run `make cosmocorgi-art` (it rewrites `art/`), look at the
game (`make cosmocorgi-screenshots`), commit `art/` with the script.

## Checklist (house style)
- [x] 1-px outline on every sprite (the rocket and the corgi #281438, the robots and bosses #221436, the props
      #241434); the shield bubble and the pops are rims and sparkles only
- [x] 3-4 shades per material, light from the top-left
- [x] the hero reads at 1x on the busiest background (clouds, walls, a gold group); its nose tilts as it climbs or
      dives, its flame flickers
- [x] players 2-4 = the rocket's paint and the fur recoloured (make_art.py ACC_P, FUR_P: blue and sable, green and
      black, purple and fawn), readable next to player 1 (red and red-tan); their shots take their paint too
- [x] backgrounds: lower contrast than the playfield, no outline on the clouds, the planets and the stars
- [x] the robots' shots are the brightest pink on screen, drawn in front of everything but the UI
- [x] the title logo colours: two ramps in src/draw.c (title_ramps)
