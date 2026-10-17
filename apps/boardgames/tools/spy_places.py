#!/usr/bin/env python3
"""The Spy's places in 22 languages: tools/spy_places.tsv (a row a place, a column a language) into
help/<lang>/spy_places.txt (a title line for the translators, then a place a line, in the table's order). Run it after
changing the table; tests/spy_test.c checks that every language has every place.

    python3 apps/boardgames/tools/spy_places.py
MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
HELP = os.path.join(os.path.dirname(HERE), "help")


def write(tsv, key, title):
    with open(os.path.join(HERE, tsv), encoding="utf-8") as f:
        rows = [l.rstrip("\n").split("\t") for l in f if l.strip()]
    langs, rows = rows[0], rows[1:]
    for r in rows:
        if len(r) != len(langs) or not all(c.strip() for c in r):
            sys.exit(f"{tsv}: a row without every language: {r[0]}")
    for i, lang in enumerate(langs):
        with open(os.path.join(HELP, lang, key + ".txt"), "w", encoding="utf-8") as f:
            f.write(title + "\n" + "".join(r[i].strip() + "\n" for r in rows))
    print(f"{key}: {len(rows)} lines in {len(langs)} languages")


if __name__ == "__main__":
    write("spy_places.tsv", "spy_places", "The Spy: the places (a line each, in tools/spy_places.tsv's order)")
