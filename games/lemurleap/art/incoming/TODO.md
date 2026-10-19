# Lemur Leap art TODO

> **Code-drawn art, no image generation.** Every sprite, tile and panorama of Lemur Leap is drawn by
> `games/lemurleap/tools/make_art.py` with the 8BCraft house style kit (`games/common/tools/house_style.py`,
> rules in `docs/art-direction.md`). Image agents: there is nothing to generate here. Do not add AI
> images to this folder.

To change the art: edit `tools/make_art.py`, run `make lemurleap-art` (it rewrites `art/`), look at the
game (`make lemurleap-screenshots`), commit `art/` with the script.

## Checklist (house style)
- [x] 1-px outline on every sprite (the material's darkest shade towards violet or navy)
- [x] 3-4 shades per material, light from the top-left
- [x] the lemur reads at 1x on every sky: a grey ring-tailed lemur, big amber eyes, a red scarf; squashed on
      landing, stretched (arms up) while rising, curled for a spring's somersault
- [x] every leaf type reads at a glance: green leaf, winged leaf (moving), brown cracked leaf (breaks), dandelion
      puff (one bounce), red mushroom, white cloud
- [x] backgrounds: the far clouds and the stars have no outline; the side panels are darker than the column
- [x] the title logo colours: two ramps in src/draw.c (title_ramps: the amber eyes, the leaves)
- [ ] (later) more frames for the pests; a hat for each fur
