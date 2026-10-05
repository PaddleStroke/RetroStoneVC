# Moth Around art

> **Code-drawn art, no image generation.** Every sprite, tile and panorama of Moth Around is drawn by
> `games/motharound/tools/make_art.py` with the 8BCraft house style kit (`games/common/tools/house_style.py`,
> rules in `docs/art-direction.md`). Image agents: there is nothing to generate here. Do not add AI
> images to this folder.

To change the art: edit `tools/make_art.py`, run `make motharound-art` (it rewrites `art/`), look at the
game (`make motharound-screenshots`), commit `art/` with the script.

## Checklist (house style)
- [x] 1-px violet/navy outlines on characters and props
- [x] 3-4 shades per material, light from the top-left
- [x] 32 px animated wings and separate 16 px flapping frames for small views
- [x] Four distinct wing palettes, with brass eyespots unchanged
- [x] Lower-contrast garden and fence, with no far-layer outline
- [x] Gold/lavender title logo ramps
