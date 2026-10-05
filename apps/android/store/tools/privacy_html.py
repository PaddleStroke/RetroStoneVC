#!/usr/bin/env python3
"""The privacy policy as web pages, ready to host: apps/android/store/privacy/privacy-policy-<en|fr>.html from the
Markdown files next to them (headings, paragraphs, lists, **bold**, *italic*: all they use). One self-contained
page each (no script, no font or file from elsewhere), readable on a phone.

    python3 apps/android/store/tools/privacy_html.py
MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
"""
import html
import os
import re

DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "privacy")
CSS = """body{margin:0;background:#f6f1e4;color:#1a2140;font:17px/1.55 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif}
main{max-width:720px;margin:0 auto;padding:24px 16px 48px}
h1{font-size:1.5em;line-height:1.25;color:#101838;border-bottom:4px solid #fcd848;padding-bottom:8px}
h2{font-size:1.15em;margin-top:1.8em;color:#2a4476}
li{margin:.3em 0}
@media (prefers-color-scheme:dark){body{background:#101838;color:#eef0f6}h1{color:#fff6dc}h2{color:#fcd848}}"""


def inline(s):
    s = html.escape(s, quote=False)
    s = re.sub(r"\*\*(.+?)\*\*", r"<strong>\1</strong>", s)
    return re.sub(r"\*(.+?)\*", r"<em>\1</em>", s)


def convert(md, lang):
    out, para, items = [], [], []

    def flush():
        if para:
            out.append("<p>" + inline(" ".join(para)) + "</p>")
            para.clear()
        if items:
            out.append("<ul>" + "".join(f"<li>{inline(i)}</li>" for i in items) + "</ul>")
            items.clear()
    title = ""
    for line in md.splitlines():
        if line.startswith("# "):
            flush()
            title = line[2:]
            out.append(f"<h1>{inline(title)}</h1>")
        elif line.startswith("## "):
            flush()
            out.append(f"<h2>{inline(line[3:])}</h2>")
        elif line.startswith("- "):
            if para:
                flush()
            items.append(line[2:])
        elif line.startswith("  ") and items:
            items[-1] += " " + line.strip()
        elif not line.strip():
            flush()
        else:
            para.append(line.strip())
    flush()
    return (f'<!doctype html>\n<html lang="{lang}">\n<head>\n<meta charset="utf-8">\n'
            f'<meta name="viewport" content="width=device-width, initial-scale=1">\n<title>{html.escape(title)}</title>\n'
            f"<style>\n{CSS}\n</style>\n</head>\n<body>\n<main>\n" + "\n".join(out) + "\n</main>\n</body>\n</html>\n")


for lang in ("en", "fr"):
    src = os.path.join(DIR, f"privacy-policy-{lang}.md")
    with open(os.path.join(DIR, f"privacy-policy-{lang}.html"), "w", encoding="utf-8") as f:
        f.write(convert(open(src, encoding="utf-8").read(), lang))
    print(f"{DIR}/privacy-policy-{lang}.html")
