# Hamster Havoc art TODO

> **Code-drawn art, no image generation.** Every sprite, tile and panorama of Hamster Havoc is drawn by
> `games/hamsterhavoc/tools/make_art.py` with the 8BCraft house style kit (`games/common/tools/house_style.py`,
> rules in `docs/art-direction.md`). Image agents: there is nothing to generate here. Do not add AI
> images to this folder.

To change the art: edit `tools/make_art.py`, run `make hamsterhavoc-art` (it rewrites `art/`), look at the
game (`make hamsterhavoc-screenshots`), commit `art/` with the script.

## Checklist (house style)
- [x] 1-px outline on every sprite (the ship #281634, the mice #241438, the crumbs and saucer #281828, the mothership
      #1e102e); the shelters' rind is drawn inside their shape (the game erodes those pixels)
- [x] 3-4 shades per material, light from the top-left
- [x] the hero reads at 1x on the busiest background (a full formation, the moon); the ship recoils as it fires
- [x] players 2-4 = the fur recoloured (make_art.py FUR_P: white, grey, ginger), readable next to player 1 (golden)
- [x] backgrounds: lower contrast than the playfield, no outline on the decor and the stars
- [x] the title logo colours: two ramps in src/draw.c (title_ramps)
