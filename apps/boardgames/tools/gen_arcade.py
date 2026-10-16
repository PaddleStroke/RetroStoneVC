#!/usr/bin/env python3
"""The RetroStone app's Arcade tab: the RetroStone VC games of the Arcade's catalogue, played inside the app.

    python3 apps/boardgames/tools/gen_arcade.py --conf apps/android/app-retrostone.conf --out build/boardgames/arcade/gen

The edition's .conf names the Arcade's own with ARCADE=<its .conf> (apps/android/app-full.conf): its catalogue, as
apps/launcher/tools/gen_catalog.py makes it (every game of games/ with a docs/screenshots/title.png, by name), its
HIDDEN_GAMES left out. Written in --out (each file only when it changes):
  - arcade.conf: the edition's .conf with FREE_GAMES=all and the Arcade's HIDDEN_GAMES, for apps/launcher/bundle.mk
    (board-lib: the runtime, the games as modules, the SDK) and its catalogue: the same games in the same order;
  - arcade_data.c: a menu card for each game (src/arcade.h, arcade_card): its picture, 96 x 96 pixels at 1x around
    the Arcade's card (apps/launcher/cards: cards.cfg and the screen cards/<id>.png; else the middle of its title
    screen), 256 colours at most; and the shell's game for it (the Arcade section, src/arcade.c);
  - text/<lang>/arcade_<id>.txt: its name (tools/gen_text.py --extra), the Arcade's (the game's own, untranslated:
    its logo's), or NAME = in the game's strings/<lang>.txt when it has one;
  - text/en/_licences.txt: help/en/_licences.txt and the Arcade's components (apps/launcher/tools/gen_credits.py).
MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
"""
import argparse
import os
import re
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
APP = os.path.dirname(HERE)
ROOT = os.path.abspath(os.path.join(APP, "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "apps", "launcher", "tools"))
import cards  # noqa: E402  (the Arcade's cards: the table, the screens)
import gen_catalog  # noqa: E402  (the Arcade's catalogue: its games, their names)
import gen_credits  # noqa: E402  (the Arcade's components and their licences)

SIDE = 96                                       # the picture: 96 x 96 pixels of the game's screen at 1x
LANGS = ["en", "cs", "da", "de", "el", "es", "fi", "fr", "id", "it", "ja", "ko", "nb", "nl", "pl", "pt_BR",
         "pt_PT", "ru", "sv", "tr", "uk", "zh_CN"]


def write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    try:
        with open(path, encoding="utf-8") as f:
            if f.read() == text:
                return
    except OSError:
        pass
    with open(path, "w", encoding="utf-8") as f:
        f.write(text)


def picture(gid, table):
    """SIDE x SIDE x 3 (RGB555 colours): around the Arcade's card (its 96 x 72 and 12 rows above and below), else the
    middle of the title screen"""
    e = table.get(gid)
    if e and os.path.exists(os.path.join(cards.CARDS, gid + ".png")):
        y = min(max(e["y"] + cards.THUMB_H // 2 - SIDE // 2, 0), cards.SCREEN_H - SIDE)
        x = min(max(e["x"] + cards.CARD_W // 2 - SIDE // 2, 0), cards.SCREEN_W - SIDE)
        return cards._screen(gid, e, x, y, SIDE, SIDE)
    src = Image.open(os.path.join(ROOT, "games", gid, "docs", "screenshots", "title.png")).convert("RGB")
    k = max(1, src.size[1] // cards.SCREEN_H)
    a = np.array(src)[::k, ::k]
    h, w = a.shape[:2]
    x, y = (w - SIDE) // 2, (h - SIDE) // 2
    return cards.to555(a[y:y + SIDE, x:x + SIDE])


def indexed(a):
    """(indices SIDE*SIDE bytes, palette [(r, g, b)]): every colour when there are 256 at most, else PIL's 256"""
    cols, inv = np.unique(a.reshape(-1, 3), axis=0, return_inverse=True)
    if len(cols) <= 256:
        return bytes(int(i) for i in inv.reshape(-1)), [tuple(int(v) for v in c) for c in cols]
    q = Image.fromarray(a, "RGB").quantize(256, method=Image.Quantize.MEDIANCUT)
    pal = q.getpalette()[:256 * 3]
    return bytes(q.tobytes()), [tuple(pal[i:i + 3]) for i in range(0, len(pal), 3)]


def strings_name(gid, lang):
    """NAME = ... in the game's strings/<lang>.txt (none yet: the games keep their logo's name)"""
    p = os.path.join(ROOT, "games", gid, "strings", lang + ".txt")
    if not os.path.exists(p):
        return None
    for line in open(p, encoding="utf-8"):
        m = re.match(r"\s*NAME\s*=\s*(.+?)\s*$", line)
        if m:
            return m.group(1)
    return None


def c_bytes(name, data):
    out = [f"static const uint8_t {name}[{len(data)}] = {{"]
    for i in range(0, len(data), 32):
        out.append("    " + ",".join(str(b) for b in data[i:i + 32]) + ",")
    out.append("};")
    return "\n".join(out)


def licences():
    """help/en/_licences.txt, then the Arcade's components (the CC BY-NC-SA's legal code: its address)"""
    with open(os.path.join(APP, "help", "en", "_licences.txt"), encoding="utf-8") as f:
        text = f.read().rstrip("\n") + "\n"
    text += "# The arcade games (RetroStone VC)\n"
    text += ("The Arcade tab's games run on the RetroStone VC runtime, built in. Their components and licences "
             "follow.\n")
    for name, role, lic, title, body in gen_credits.credits():
        text += "# " + title + "\n"
        paras = gen_credits.paragraphs(body, stanzas=name.startswith("STB"), headings=name.startswith("UNIFONT"))
        paras = [p.strip() for p in paras.split("\n\n") if p.strip()]
        if lic.startswith("CC BY"):             # the legal code is long: its first words and its address
            paras = paras[:1] + ["The licence's full legal code: "
                                 "https://creativecommons.org/licenses/by-nc-sa/4.0/legalcode"]
        text += "\n".join(paras) + "\n"
    return text


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--conf", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    conf_lines = open(a.conf, encoding="utf-8").read().splitlines()
    conf = gen_catalog.read_conf(a.conf)
    arcade = conf.get("ARCADE")
    if not arcade:
        sys.exit(f"{a.conf}: no ARCADE=<the Arcade's .conf>")
    aconf = gen_catalog.read_conf(os.path.join(ROOT, arcade))
    hidden = aconf.get("HIDDEN_GAMES", "")
    # the bundle's .conf: this app's identity, every game of the Arcade's catalogue but its hidden ones
    keep = [l for l in conf_lines if not re.match(r"\s*(FREE_GAMES|HIDDEN_GAMES|IAP_PRODUCT)\s*=", l)]
    bconf = "\n".join(["# generated by apps/boardgames/tools/gen_arcade.py from " + os.path.basename(a.conf) +
                       " and " + arcade + ": do not edit"] + keep +
                      ["FREE_GAMES=all", "HIDDEN_GAMES=" + hidden, ""])
    write(os.path.join(a.out, "arcade.conf"), bconf)
    games = [g for g, locked in gen_catalog.list_games({"FREE_GAMES": "all", "HIDDEN_GAMES": hidden})]
    table = cards.read_table()
    c = ["/* generated by apps/boardgames/tools/gen_arcade.py: do not edit. The Arcade tab's cards (src/arcade.h). */",
         '#include "arcade.h"', ""]
    rows = []
    for i, gid in enumerate(games):
        pix, pal = indexed(picture(gid, table))
        c.append(c_bytes(f"pix_{i}", pix))
        c.append(c_bytes(f"pal_{i}", bytes(v for col in pal for v in col)))
        c.append(f"static void thumb_{i}(int v, int size, uint8_t *rgba) {{ (void)v; arcade_thumb({i}, size, rgba); }}")
        c.append(f'static const game card_{i} = ARCADE_CARD("arcade_{gid}", {i}, thumb_{i});')
        c.append(f"REGISTER_GAME(card_{i})")
        c.append("")
        rows.append(f'    {{"{gid}", &card_{i}, pix_{i}, pal_{i}, {len(pal)}}},')
        name = gen_catalog.game_name(gid)
        for lang in LANGS:
            own = strings_name(gid, lang)
            if lang == "en" or own:
                write(os.path.join(a.out, "text", lang, f"arcade_{gid}.txt"), (own or name) + "\n")
    c.append(f"const arcade_card arcade_cards[{max(1, len(games))}] = {{")
    c.extend(rows)
    c.append("};")
    c.append(f"const int arcade_ncards = {len(games)};")
    c.append("")
    write(os.path.join(a.out, "arcade_data.c"), "\n".join(c))
    write(os.path.join(a.out, "text", "en", "_licences.txt"), licences())
    print(f"gen_arcade: {len(games)} arcade games ({' '.join(games)}); hidden: {hidden or 'none'}")


if __name__ == "__main__":
    main()
