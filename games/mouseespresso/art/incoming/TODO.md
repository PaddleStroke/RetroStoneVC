# Mouse Espresso art

> **Code-drawn art, no image generation.** Every sprite, tile and panorama of Mouse Espresso is drawn by
> `games/mouseespresso/tools/make_art.py` with the 8BCraft house style kit (`games/common/tools/house_style.py`,
> rules in `docs/art-direction.md`). Image agents: there is nothing to generate here. Do not add AI
> images to this folder.

To change the art: edit `tools/make_art.py`, run `make mouseespresso-art` (it rewrites `art/`), look at the
game (`make mouseespresso-screenshots`), commit `art/` with the script.

## Checklist (house style)
- [x] Violet/navy sprite outlines; biscuits use their darkest brown
- [x] 3-4 material shades, lit from the top-left
- [x] Large ears, muzzle and enamel espresso pack; running/thrust/coast/hit poses
- [x] Separate compact 16 px mouse and props for small player views
- [x] Four distinct pack/scarf colours; ears, fur and chrome stay unchanged
- [x] Desaturated café windows, cups and wood panelling; parallax background
- [x] Coral/seafoam title logo ramps
