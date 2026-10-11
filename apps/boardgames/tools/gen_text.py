#!/usr/bin/env python3
"""The board games' texts and the fonts to draw them: build/<...>/gen/text_data.c.

    python3 apps/boardgames/tools/gen_text.py --out build/boardgames/gen

The texts: apps/boardgames/help/<lang>/<game>.txt, the rules of each game in 22 languages (English is the source;
a missing translation falls back to English), and help/<lang>/_app.txt, what the shell's buttons do (shown after
every game's rules). The file's format: the first line is the game's name (the menu card, the page's title); then
paragraphs separated by blank lines; "# " starts a heading, "- " a list item. help/glyphs.txt: the characters
the boards themselves draw (xiangqi's, shogi's pieces...): a font of their own, the same in every language
("#" lines: comments).

The fonts, cut down (fontTools) to the characters the texts use, Regular and Bold, no layout tables (the app draws
glyph by glyph with stb_truetype):
- Noto Sans CJK (SIL Open Font License 1.1, the system package fonts-noto-cjk): Japanese, Korean and Chinese (their
  own faces: their own glyph shapes), the languages in Latin, Greek or Cyrillic letters it covers (the Japanese
  face's letters), and the boards' characters;
- DejaVu Sans (the Bitstream Vera licence and public domain, the system package fonts-dejavu-core) for a language
  using a letter Noto Sans CJK lacks (Greek's accented vowels, Ukrainian's є і ї ґ...): that language's whole text,
  so its letters match.
MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
"""
import argparse
import io
import os
import sys

from fontTools import subset
from fontTools.ttLib import TTFont

HERE = os.path.dirname(os.path.abspath(__file__))
APP = os.path.dirname(HERE)
LANGS = ["en", "cs", "da", "de", "el", "es", "fi", "fr", "id", "it", "ja", "ko", "nb", "nl", "pl", "pt_BR",
         "pt_PT", "ru", "sv", "tr", "uk", "zh_CN"]
NOTO = "/usr/share/fonts/opentype/noto/NotoSansCJK-%s.ttc"
DEJAVU = "/usr/share/fonts/truetype/dejavu/DejaVuSans%s.ttf"
# a font group: where its two fonts come from (a path pattern, Regular / Bold, the face in a collection)
SOURCES = {"base": (NOTO, ("Regular", "Bold"), 0), "ja": (NOTO, ("Regular", "Bold"), 0),
           "ko": (NOTO, ("Regular", "Bold"), 1), "zh_CN": (NOTO, ("Regular", "Bold"), 2),
           "dejavu": (DEJAVU, ("", "-Bold"), None), "glyphs": (NOTO, ("Regular", "Bold"), 0)}
ALWAYS = "".join(chr(c) for c in range(32, 127)) + "•…–—‘’“”«»×"


def load(group, bold):
    pattern, styles, face = SOURCES[group]
    path = pattern % styles[bold]
    return TTFont(path, fontNumber=face, lazy=False) if face is not None else TTFont(path, lazy=False)


def read_texts():
    """{lang: {game: text}} (a text: the file as it is, trailing spaces removed)"""
    texts = {}
    for lang in LANGS:
        d = os.path.join(APP, "help", lang)
        texts[lang] = {}
        if not os.path.isdir(d):
            continue
        for f in sorted(os.listdir(d)):
            if f.endswith(".txt"):
                with open(os.path.join(d, f), encoding="utf-8") as fh:
                    lines = [l.rstrip() for l in fh.read().replace("\r", "").split("\n")]
                texts[lang][f[:-4]] = "\n".join(lines).strip() + "\n"
    return texts


def groups_of(texts):
    """{lang: font group}: the CJK languages their own; the others Noto when it has all their letters, else DejaVu"""
    noto = load("base", 0).getBestCmap()
    out = {}
    for lang in LANGS:
        if lang in ("ja", "ko", "zh_CN"):
            out[lang] = lang
            continue
        used = set(ALWAYS).union(*texts[lang].values()) if texts[lang] else set(ALWAYS)
        out[lang] = "base" if all(ord(c) in noto for c in used if c != "\n") else "dejavu"
    return out


def cut(group, bold, chars):
    font = load(group, bold)
    cmap = font.getBestCmap()
    missing = sorted(c for c in chars if ord(c) not in cmap and c != "\n")
    opts = subset.Options()
    opts.layout_features = []
    opts.hinting = False
    opts.desubroutinize = True
    opts.notdef_outline = True
    opts.name_IDs = [0, 1, 2, 3, 4, 5, 6, 13, 14]        # the copyright and the licence stay in the font
    sub = subset.Subsetter(opts)
    sub.populate(unicodes=[ord(c) for c in chars if c != "\n"])
    sub.subset(font)
    buf = io.BytesIO()
    font.save(buf)
    return buf.getvalue(), missing


def c_bytes(name, data):
    out = [f"static const unsigned char {name}[{len(data)}] = {{"]
    for i in range(0, len(data), 24):
        out.append("    " + ",".join(str(b) for b in data[i:i + 24]) + ",")
    out.append("};")
    return "\n".join(out)


def c_str(s):
    s = s.replace("\\", "\\\\").replace('"', '\\"')
    parts = s.split("\n")
    return "\n        ".join('"' + p + ('\\n"' if i < len(parts) - 1 else '"') for i, p in enumerate(parts))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    texts = read_texts()
    if "en" not in texts or not texts["en"]:
        sys.exit("gen_text: no English texts in apps/boardgames/help/en")
    group = groups_of(texts)
    chars = {g: set() for g in SOURCES}
    for lang in LANGS:
        chars[group[lang]].update(ALWAYS)
        for t in texts[lang].values():
            chars[group[lang]].update(t)
    glyphs = os.path.join(APP, "help", "glyphs.txt")
    if os.path.exists(glyphs):
        with open(glyphs, encoding="utf-8") as fh:
            chars["glyphs"] = set(c for l in fh if not l.startswith("#") for c in l if c not in " \t\r\n")
    chars["glyphs"].add(" ")
    c = ["/* Generated by apps/boardgames/tools/gen_text.py: do not edit. The fonts: Noto Sans CJK (SIL OFL 1.1),",
         " * DejaVu Sans (Bitstream Vera licence, public domain). */", '#include "text.h"', ""]
    problems = 0
    for g in SOURCES:
        if not chars[g]:
            continue
        for bold in (0, 1):
            data, missing = cut(g, bold, chars[g])
            if missing:
                print(f"gen_text: {g}{' bold' if bold else ''}: no glyph for {''.join(missing)!r}", file=sys.stderr)
                problems += 1
            c.append(c_bytes(f"font_{g}_{bold}", data))
            print(f"gen_text: font {g}{' bold' if bold else ''}: {len(chars[g])} characters, {len(data)} bytes")
    c.append("")
    c.append("const tx_lang tx_langs[] = {")
    for lang in LANGS:
        g = group[lang]
        c.append(f'    {{"{lang}", font_{g}_0, sizeof font_{g}_0, font_{g}_1, sizeof font_{g}_1}},')
    c.append("};")
    c.append(f"const int tx_nlangs = {len(LANGS)};")
    c.append("const unsigned char *const tx_glyph_font[2] = {font_glyphs_0, font_glyphs_1};")
    c.append("")
    c.append("const tx_text tx_texts[] = {")
    for lang in LANGS:
        for game, t in texts[lang].items():
            c.append(f'    {{"{lang}", "{game}",\n        {c_str(t)}}},')
    c.append("};")
    c.append("const int tx_ntexts = sizeof tx_texts / sizeof tx_texts[0];")
    c.append("")
    os.makedirs(a.out, exist_ok=True)
    path = os.path.join(a.out, "text_data.c")
    body = "\n".join(c)
    if not os.path.exists(path) or open(path, encoding="utf-8").read() != body:
        with open(path, "w", encoding="utf-8") as f:
            f.write(body)
    n = sum(len(v) for v in texts.values())
    dv = [l for l in LANGS if group[l] == "dejavu"]
    print(f"gen_text: {path}: {n} texts in {sum(1 for v in texts.values() if v)} languages"
          + (f"; DejaVu Sans for {' '.join(dv)}" if dv else ""))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
