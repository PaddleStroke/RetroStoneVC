# Snowball Siege art TODO

> **Code-drawn art, no image generation.** Every sprite, tile and panorama of Snowball Siege is drawn by
> `games/snowballsiege/tools/make_art.py` with the 8BCraft house style kit (`games/common/tools/house_style.py`,
> rules in `docs/art-direction.md`); the snowy battlefield itself is drawn at run time from the terrain bitmap
> (`src/draw.c`, the palette `TERRAIN` in make_art.py). Image agents: there is nothing to generate here. Do not add
> AI images to this folder.

To change the art: edit `tools/make_art.py`, run `make snowballsiege-art` (it rewrites `art/`), look at the
game (`make snowballsiege-screenshots`), commit `art/` with the script.

## Checklist (house style)
- [x] 1-px outline on every sprite (each team its fur's darkest shade towards violet or navy; props navy)
- [x] 3-4 shades per material, light from the top-left (fur, scarves, snowballs, pines)
- [x] the four teams read at 1x on the snow: bead eyes, a scarf in the player's colour (red, blue, green, purple)
- [x] four species, not palette swaps (approved design): bear cubs, penguins, foxes, hares; poses: idle (2), walk
      (2), throw, hurt (a snow clump on the head), air (stretched), the snow mound when out, the icon
- [x] backgrounds: the far mountains and pines pale and without outline; the field's dark top line, light top rows
- [x] the title logo colours: two ramps in src/draw.c (title_ramps): SNOWBALL icy, SIEGE scarf red
