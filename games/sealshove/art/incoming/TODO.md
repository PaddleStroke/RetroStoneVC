# Seal Shove art TODO

> **Code-drawn art, no image generation.** Every sprite and tile of Seal Shove is drawn by
> `games/sealshove/tools/make_art.py` with the 8BCraft house style kit (`games/common/tools/house_style.py`,
> rules in `docs/art-direction.md`); the ice floe itself is painted at run time by `src/draw.c` from the rules' grid.
> Image agents: there is nothing to generate here. Do not add AI images to this folder.

To change the art: edit `tools/make_art.py` (or the floe's painter in `src/draw.c`), run `make sealshove-art` (it
rewrites `art/`), look at the game (`make sealshove-screenshots`), commit `art/` with the script.

## Checklist (house style)
- [x] 1-px outline on every sprite (the seals navy violet #221a3e, the props and UI navy, the penguins navy)
- [x] 3-4 shades per material, light from the top-left (the seals' bodies, the caps, the fish, the icebergs)
- [x] the seals read at 1x in the 3/4 view: front (walking down), back (up), side (left and right, mirrored); a
      waddle in 2 frames, the belly slide stretched flat
- [x] players 2-4 = the cap and scarf turned (blue, green, purple); the bots take the free colours
- [x] the floe: white ice lit from the top-left, its side and a submerged skirt below, the dark crack and the blink of
      a cracking chunk; the sea darker and calmer than the floe, its ripples shimmering (palette cycling)
- [x] the title logo colours: two ramps in src/draw.c (title_ramps)
