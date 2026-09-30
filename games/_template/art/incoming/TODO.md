# @NAME@ art TODO

> **Code-drawn art, no image generation.** Every sprite, tile and panorama of @NAME@ is drawn by
> `games/@ID@/tools/make_art.py` with the 8BCraft house style kit (`games/common/tools/house_style.py`,
> rules in `docs/art-direction.md`). Image agents: there is nothing to generate here. Do not add AI
> images to this folder.

To change the art: edit `tools/make_art.py`, run `make @ID@-art` (it rewrites `art/`), look at the
game (`make @ID@-screenshots`), commit `art/` with the script.

## Checklist (house style)
- [ ] 1-px outline on every sprite (the material's darkest shade towards violet or navy; UI: #161228)
- [ ] 3-4 shades per material, light from the top-left
- [ ] the hero reads at 1x on the busiest background; squash on take-off and landing, stretch while rising
- [ ] player 2 = the hero's palette swap (build_assets.py P2_HUE), readable next to player 1
- [ ] backgrounds: lower contrast than the playfield, no outline on the far layer
- [ ] the title logo colours: two ramps in src/draw.c (title_ramps)
