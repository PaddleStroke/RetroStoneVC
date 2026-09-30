# Blueberry Tumble art TODO

> **Code-drawn art, no image generation.** Every sprite, tile and panorama of Blueberry Tumble is drawn by
> `games/blueberrytumble/tools/make_art.py` with the 8BCraft house style kit (`games/common/tools/house_style.py`,
> rules in `docs/art-direction.md`). Image agents: there is nothing to generate here. Do not add AI
> images to this folder.

To change the art: edit `tools/make_art.py`, run `make blueberrytumble-art` (it rewrites `art/`), look at the
game (`make blueberrytumble-screenshots`), commit `art/` with the script.

## Checklist (house style)
- [ ] 1-px outline on every sprite (the material's darkest shade towards violet or navy; UI: #161228)
- [ ] 3-4 shades per material, light from the top-left
- [ ] the hero reads at 1x on the busiest background; squash on take-off and landing, stretch while rising
- [ ] player 2 = the hero's palette swap (build_assets.py P2_HUE), readable next to player 1
- [ ] backgrounds: lower contrast than the playfield, no outline on the far layer
- [ ] the title logo colours: two ramps in src/draw.c (title_ramps)

## What make_art.py draws
- the berry: 16 roll angles (the star calyx and leaf stem turn, the light stays top-left), squash and stretch at 4
  angles, X eyes; the snowberry (4 angles); juice drops, the splat, snow crumbs
- props: the dew drop and its ring, the mushroom (and squashed), the golden blueberry, sparkles, the pine cone
  (4 angles), the maple leaf (3 tilts), gust leaves
- the playfield: 29 metatiles drawn with 15 colour roles, one palette per biome (tiles/playfield.png, palettes.json)
- the mid-ground bands (summit hills and firs, the pine wood, the meadows with the berry family, the village) and the
  far mountains
