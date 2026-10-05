#!/usr/bin/env python3
"""The "Credits & licences" screen's data: build/bundle/gen/credits.c (apps/launcher/src/rsb.h, rsb_credit).

    gen_credits.py --out build/bundle/gen

Every component the apps are made of, with its licence's FULL text (English, as published; the screen's own words
are translated in apps/launcher/strings). The list follows THIRD_PARTY.md: the RetroStone VC games and this app
(code MIT, assets CC BY-NC-SA 4.0, (c) 8BCraft), SDL2 (zlib), libxmp-lite (MIT), GNU Unifont (SIL OFL 1.1),
stb_image_write (MIT, or public domain). libretro.h, also in THIRD_PARTY.md, is not part of the apps (it is the
libretro cores' header). The texts are read from the files that come with each component; they are made ASCII
(the house font) and their hard-wrapped lines joined into paragraphs (the launcher wraps them to the screen).
MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
"""
import argparse
import glob
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
OWNER = "RETROSTONE VC  (C) 2026 8BCRAFT"

ASCII = {"\u2018": "'", "\u2019": "'", "\u201c": '"', "\u201d": '"', "\u2013": "-", "\u2014": "--", "\u00a9": "(c)",
         "\u2026": "...", "\u00a0": " ", "\u00e9": "e", "\u00e8": "e", "\u00f6": "o", "\u00e4": "a", "\u00fc": "u",
         "\u2022": "-", "\u00b7": "-"}


def read(path):
    return open(os.path.join(ROOT, path), encoding="utf-8", errors="replace").read()


def section(text, start, end=None):
    """the part of text from the line matching start (a regex) to the line before end (or the end)"""
    lines = text.splitlines()
    i = next(k for k, l in enumerate(lines) if re.search(start, l))
    j = len(lines)
    if end:
        j = next((k for k in range(i + 1, len(lines)) if re.search(end, lines[k])), len(lines))
    return "\n".join(lines[i:j])


def paragraphs(text, stanzas=False, headings=False):
    """ASCII; the hard-wrapped lines of a paragraph joined; a paragraph keeps its first line's indent (2 at most).
    Lines of dashes separate paragraphs. stanzas: the text has no blank lines (stb): a line ending a sentence
    (. or :) ends its paragraph, and the headings (ALTERNATIVE, Copyright) are paragraphs of their own. headings:
    a line in capitals alone (OFL: PREAMBLE, DEFINITIONS) is a paragraph of its own."""
    for k, v in ASCII.items():
        text = text.replace(k, v)
    text = text.encode("ascii", "replace").decode("ascii").replace("\t", "    ").replace("\r", "")
    text = re.sub(r"(?m)^\s*[-=]{8,}\s*$", "", text)
    if headings:
        text = re.sub(r"(?m)^(\s*[A-Z][A-Z ]{3,38})$", r"\n\1\n", text)
    if stanzas:
        text = re.sub(r"(?m)^((?:ALTERNATIVE|Copyright).*|.*[.:])$", r"\1\n", text)
    out = []
    for block in re.split(r"\n\s*\n", text.strip("\n")):
        lines = [l.rstrip() for l in block.splitlines() if l.strip()]
        if not lines:
            continue
        # list items written one per line (a, b, 1., -): each its own paragraph
        items, cur = [], []
        for l in lines:
            if cur and re.match(r"\s*([a-z0-9ivx]{1,4}[.)]|-|\*)\s", l):
                items.append(cur)
                cur = []
            cur.append(l)
        items.append(cur)
        for it in items:
            indent = min(2, (len(it[0]) - len(it[0].lstrip())) // 2)
            out.append(" " * indent + " ".join(l.strip() for l in it))
    return "\n\n".join(out)


def c_str(s):
    def esc(p):
        return p.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n")
    parts = [s[i:i + 96] for i in range(0, len(s), 96)] or [""]      # cut before escaping: no split escape
    return "\n        ".join('"' + esc(p) + '"' for p in parts)


def credits():
    sdl = sorted(glob.glob(os.path.join(ROOT, "build", "deps", "src", "SDL2-*", "LICENSE.txt")))
    if not sdl:
        raise SystemExit("gen_credits: SDL2's LICENSE.txt is missing (run tools/fetch_sdl2_src.sh build/deps)")
    sdl_v = re.search(r"SDL2-([\d.]+)", sdl[-1]).group(1)
    xmp_readme = read("sdk/third_party/libxmp-lite/README")
    xmp_v = re.search(r"libxmp-lite[^|]*\|\s*([\d.]+)", read("THIRD_PARTY.md"))
    stb = read("sdk/third_party/stb/stb_image_write.h")
    stb_v = re.search(r"stb_image_write - v([\d.]+)", stb)
    return [
        ("RETROSTONE VC", "RSB_ROLE_CODE", "MIT", "RetroStone VC: the games and this app (code)",
         "The code of the RetroStone VC games, of their SDK and of this app.\n\n" + read("LICENSE-MIT")),
        ("RETROSTONE VC", "RSB_ROLE_ART", "CC BY-NC-SA 4.0", "RetroStone VC: the games' art, music and sound",
         "Copyright (c) 2026 Pierre-Louis Boyer (8BCraft). The games' art, music, sound, levels and design documents "
         "are licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International licence. "
         "The licensor's own use (this app) is not limited by it.\n\n" + read("LICENSE-CC-BY-NC-SA-4.0.txt")),
        (f"SDL {sdl_v}", "RSB_ROLE_NONE", "ZLIB", f"Simple DirectMedia Layer {sdl_v} (zlib licence)",
         open(sdl[-1], encoding="utf-8").read()),
        ("LIBXMP-LITE " + (xmp_v.group(1) if xmp_v else ""), "RSB_ROLE_NONE", "MIT",
         "libxmp-lite (the music player, MIT licence)", section(xmp_readme, r"^Extended Module Player Lite",
                                                                  r"^\s*THE SOFTWARE\.\s*$") + "\nTHE SOFTWARE."),
        ("UNIFONT 13.0.06", "RSB_ROLE_NONE", "SIL OFL 1.1",
         "GNU Unifont 13.0.06 (the Japanese, Korean and Chinese glyphs; SIL Open Font License 1.1)",
         read("games/common/fonts/LICENSE-unifont-OFL.txt")),
        ("STB IMAGE WRITE " + (stb_v.group(1) if stb_v else ""), "RSB_ROLE_NONE", "MIT",
         "stb_image_write by Sean Barrett (MIT licence, or public domain)",
         section(stb, r"This software is available under 2 licenses", r"^\*/")),
    ]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    c = ["/* Generated by apps/launcher/tools/gen_credits.py: do not edit. */", '#include "rsb.h"', "",
         f"const char rsb_credits_owner[] = {c_str(OWNER)};", "const rsb_credit rsb_credits[] = {"]
    items = credits()
    for name, role, lic, title, text in items:
        body = paragraphs(text, stanzas=name.startswith("STB"), headings=name.startswith("UNIFONT"))
        c.append(f"    {{{c_str(name.strip())}, {role}, {c_str(lic)}, {c_str(title)},\n        {c_str(body)}}},")
    c += ["};", f"const int rsb_ncredits = {len(items)};", ""]
    os.makedirs(a.out, exist_ok=True)
    path = os.path.join(a.out, "credits.c")
    text = "\n".join(c)
    if not os.path.exists(path) or open(path, encoding="utf-8").read() != text:
        with open(path, "w", encoding="utf-8") as f:
            f.write(text)
    print(f"{path}: {len(items)} credits, {sum(len(t) for *_, t in items)} characters of licence text")


if __name__ == "__main__":
    main()
