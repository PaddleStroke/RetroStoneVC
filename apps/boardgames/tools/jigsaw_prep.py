#!/usr/bin/env python3
"""Jigsaw's paintings: apps/boardgames/art/jigsaw/<id>.jpg from the files fetched from Wikimedia Commons.

    python3 -I apps/boardgames/tools/jigsaw_prep.py SRC_DIR

SRC_DIR holds <id>.src.jpg (each painting's 1280 px thumbnail from Commons) and meta.json (per id: title, artist,
death year, the Commons file and its page, the licence and its public-domain templates, checked when fetched). Each
picture: RGB, its long side 1024 px (Lanczos), baseline JPEG at the highest quality from 92 down that stays within
200 KB (the APK's budget: about 150-200 KB each, 8 MB in all); its EXIF and other metadata dropped. Then
apps/boardgames/tools/jigsaw_paintings.txt (the record: title, artist, death year, Commons page, licence), and on
stdout the lines of src/games/jigsaw_pictures.h (the picture's name and size). Deterministic: re-running gives the same
files. Needs Pillow (the system's python3-pil).
MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
"""
import io
import json
import os
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
APP = os.path.dirname(HERE)
OUT = os.path.join(APP, "art", "jigsaw")
LONG = 1024
MAX_BYTES = 200 * 1024
SETS = {"japan": "JS_JAPAN", "impression": "JS_IMPRESSION", "post": "JS_POST", "masters": "JS_MASTERS",
        "romantic": "JS_ROMANTIC"}


def encode(im):
    for q in range(92, 59, -2):
        b = io.BytesIO()
        im.save(b, "JPEG", quality=q, optimize=True, progressive=False, subsampling="4:2:0")
        if b.tell() <= MAX_BYTES:
            return b.getvalue(), q
    return b.getvalue(), q


def main():
    src = sys.argv[1]
    meta = json.load(open(os.path.join(src, "meta.json"), encoding="utf-8"))
    os.makedirs(OUT, exist_ok=True)
    rec = ["# Jigsaw's public-domain paintings (art/jigsaw/<id>.jpg, tools/jigsaw_prep.py), from Wikimedia Commons:",
           "# a painting whose artist died more than 70 years ago, public domain on Commons (its PD-Art / PD-old tags",
           "# checked when fetched). Public domain needs no credit; this is the record. One painting a line, tabs:",
           "# id, set, title, artist, death year, Commons file page, licence on Commons", ""]
    lines = []
    total = 0
    for pid, m in meta.items():
        if not m.get("ok"):
            continue
        im = Image.open(os.path.join(src, pid + ".src.jpg")).convert("RGB")
        w, h = im.size
        k = LONG / max(w, h)
        if k < 1:
            im = im.resize((round(w * k), round(h * k)), Image.LANCZOS)
        data, q = encode(im)
        with open(os.path.join(OUT, pid + ".jpg"), "wb") as f:
            f.write(data)
        total += len(data)
        tpl = ",".join(t[9:] for t in m["pd_templates"] if "/" not in t and not t.endswith(("-text", "-category")))
        rec.append("\t".join([pid, m["group"], m["title"], m["artist"], str(m["died"]), m["page"].replace("%3A", ":"),
                              m["licence"] + " (" + tpl + ")"]))
        lines.append('    {-1, "%s", %d, %d, %s, %d},%s/* %s, %s */' % (
            pid, im.size[0], im.size[1], SETS[m["group"]], m["died"], " ", m["title"], m["artist"]))
        print("%-26s %4dx%-4d q%d %6d bytes" % (pid, im.size[0], im.size[1], q, len(data)), file=sys.stderr)
    with open(os.path.join(HERE, "jigsaw_paintings.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(rec) + "\n")
    print("\n".join(lines))
    print("total %d bytes, %d paintings" % (total, len(lines)), file=sys.stderr)


if __name__ == "__main__":
    main()
