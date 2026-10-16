# Llama Drama art TODO

> **Code-drawn art, no image generation.** Every sprite, tile and panorama of Llama Drama is drawn by
> `games/llamadrama/tools/make_art.py` with the 8BCraft house style kit (`games/common/tools/house_style.py`,
> rules in `docs/art-direction.md`). Image agents: there is nothing to generate here. Do not add AI
> images to this folder.

To change the art: edit `tools/make_art.py`, run `make llamadrama-art` (it rewrites `art/`), look at the
game (`make llamadrama-screenshots`), commit `art/` with the script.

## Checklist (house style)
- [x] 1-px outline on every sprite (the llama #3e2030, the hornets #28161a, the spit #245c4c, the props #161228);
      the far scenery, the clouds, the sun and the moon have none
- [x] 3-4 shades per material, light from the top-left (the head is drawn in head units and turned to each aim; its
      shading stays lit from the top-left of the screen)
- [x] the hero reads at 1x on the busiest background (the night, a swarm): a cream llama, big dark eyes, a red pompom
      and blanket; its face puffs as it spits and turns scared when a hornet is close
- [x] players 2-4 = the wool and the tassels recoloured (make_art.py FUR_P, ACC_P: brown and blue, grey and green,
      caramel and purple), readable next to player 1 (cream and red)
- [x] backgrounds: lower contrast than the playfield, no outline on the mountains; the scenery dims with the time of day,
      the llamas and the hornets never do
- [x] the title logo colours: two ramps in src/draw.c (title_ramps)
