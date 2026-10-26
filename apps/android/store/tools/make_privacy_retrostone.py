#!/usr/bin/env python3
"""The web page of RetroStone's privacy policy (the Play Console's "Privacy policy" URL), written from the app's own
policy so the two never differ: apps/boardgames/help/<en|fr>/_privacy.txt (the Settings page's System and legal:
a title line, "# " headings, "- " list lines, paragraphs) into apps/android/store/privacy/privacy-retrostone.html,
English then French, in the look of privacy-board.html.

    python3 apps/android/store/tools/make_privacy_retrostone.py

Host the page (e.g. https://www.8bcraft.com/privacy-policy-retrostone/) and put its address in app-retrostone.conf's
PRIVACY_URL. MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
"""
import html
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
STORE = os.path.dirname(HERE)
ROOT = os.path.abspath(os.path.join(STORE, "..", "..", ".."))
LANGS = (("en", "Version française", "#fr"), ("fr", None, None))

STYLE = """body{margin:0;background:#f6f1e4;color:#1a2140;font:17px/1.55 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif}
main{max-width:720px;margin:0 auto;padding:24px 16px 48px}
h1{font-size:1.5em;line-height:1.25;color:#101838;border-bottom:4px solid #fcd848;padding-bottom:8px}
h2{font-size:1.15em;margin-top:1.8em;color:#2a4476}
li{margin:.3em 0}
a{color:#2a4476}
nav{font-size:.95em}
hr{border:0;border-top:2px solid #d9cfb4;margin:48px 0}
@media (prefers-color-scheme:dark){body{background:#101838;color:#eef0f6}h1{color:#fff6dc}h2{color:#fcd848}a{color:#fcd848}hr{border-top-color:#2a3458}}"""


def inline(s):
    s = html.escape(s, quote=False)
    return re.sub(r"[\w.+-]+@[\w-]+(?:\.[\w-]+)+", lambda m: f'<a href="mailto:{m.group(0)}">{m.group(0)}</a>', s)


def section(lang):
    lines = [l.rstrip() for l in open(os.path.join(ROOT, "apps", "boardgames", "help", lang, "_privacy.txt"),
                                      encoding="utf-8-sig")]
    out = [f"<h1>{inline(lines[0])}: RetroStone</h1>"]
    in_list = False
    for l in lines[1:]:
        if not l:
            continue
        if l.startswith("- ") != in_list:
            out.append("<ul>" if not in_list else "</ul>")
            in_list = not in_list
        if l.startswith("# "):
            out.append(f"<h2>{inline(l[2:])}</h2>")
        elif l.startswith("- "):
            out.append(f"<li>{inline(l[2:])}</li>")
        else:
            out.append(f"<p>{inline(l)}</p>")
    if in_list:
        out.append("</ul>")
    return "\n".join(out)


def main():
    body = []
    for lang, nav, href in LANGS:
        if nav:
            body.append(f'<nav><a href="{href}">{nav}</a></nav>\n')
            body.append(section(lang))
        else:
            body.append(f'\n<hr>\n\n<section id="{lang}" lang="{lang}">\n{section(lang)}\n</section>')
    title = open(os.path.join(ROOT, "apps", "boardgames", "help", "en", "_privacy.txt"),
                 encoding="utf-8-sig").readline().strip()
    page = (f'<!doctype html>\n<html lang="en">\n<head>\n<meta charset="utf-8">\n'
            f'<meta name="viewport" content="width=device-width, initial-scale=1">\n'
            f"<title>{html.escape(title)}: RetroStone</title>\n<style>\n{STYLE}\n</style>\n</head>\n<body>\n<main>\n"
            + "\n".join(body) + "\n</main>\n</body>\n</html>\n")
    path = os.path.join(STORE, "privacy", "privacy-retrostone.html")
    with open(path, "w", encoding="utf-8") as fh:
        fh.write(page)
    print(path)


if __name__ == "__main__":
    main()
