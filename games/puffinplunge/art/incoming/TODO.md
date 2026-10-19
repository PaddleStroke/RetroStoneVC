# Puffin Plunge art TODO

> **Code-drawn art, no image generation.** Every sprite and tile of Puffin Plunge is drawn by
> `games/puffinplunge/tools/make_art.py` with the 8BCraft house style kit (`games/common/tools/house_style.py`,
> rules in `docs/art-direction.md`). Image agents: there is nothing to generate here. Do not add AI
> images to this folder.

To change the art: edit `tools/make_art.py`, run `make puffinplunge-art` (it rewrites `art/`), look at the
game (`make puffinplunge-screenshots`), commit `art/` with the script.

## Checklist (house style)
- [x] 1-px outline on every sprite (the material's darkest shade towards violet or navy)
- [x] 3-4 shades per material, light from the top-left
- [x] the puffin reads at 1x on every zone's rock (black back, white face, the striped beak, blue bubble boots)
- [x] the critters share one 15-colour palette per zone (four symbolic ramps: walker, flyer, spiky, special); the
      spiky ones (urchins, jellyfish, icicles, wisps) look spiky or stingy: they are not stomped
- [x] gems are cyan diamonds (big ones gold); hearts red; the bubbles' pips blue
- [x] the title logo colours: two ramps in src/draw.c (title_ramps: the beak, the bubbles)
- [ ] (later) more frames for the walrus (a slide's dust, a throw); a fifth decoration per zone
