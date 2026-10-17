#!/usr/bin/env python3
"""Draw and Guess's words in 22 languages: tools/drawguess_words.tsv (a row a word, a column a language; simple things
to draw) into help/<lang>/drawguess_words.txt, as tools/spy_places.py does. Run it after changing the table;
tests/drawguess_test.c checks that every language has every word, each once.

    python3 apps/boardgames/tools/drawguess_words.py
MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
"""
import spy_places

if __name__ == "__main__":
    spy_places.write("drawguess_words.tsv", "drawguess_words",
                     "Draw and Guess: the words (a line each, in tools/drawguess_words.tsv's order)")
