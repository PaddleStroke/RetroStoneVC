#!/usr/bin/env python3
"""Falling Notes' songs: src/games/pianofall_levels.h, from Play by Ear's tune library tools/melody_tunes.txt (every
tune in the public domain; their titles: help/<lang>/melody_titles.txt).

Each tune is one song, played whole with its rhythm (its rests kept). The songs are graded (notes a second, leaps,
black keys, range, length) and laid out in six chapters of ten, of rising difficulty:
  1  white keys only, a little slower than written (x0.85)
  2  white keys only, as written
  3  black keys come in (the easiest tunes that use them)
  4  faster (x1.15)
  5  two notes at once: a second voice below the tune on its strong beats (a third or a sixth in the tune's own
     scale, else a fourth)
  6  the hardest tunes, the second voice, faster (x1.1)
A chapter's songs are ordered by grade. A short tune is played again (up to four times, a half note's rest between)
until the song lasts about 20 seconds. The keyboard of a song: its white keys from the one at or below its lowest
note to the one at or above its highest, at least 8 (an octave), at most 15; the second voice never goes below G3 nor
widens the keyboard past 15 white keys. Same-key notes never overlap; a note's bar is its length (to the next note
on that key at most).

    python3 apps/boardgames/tools/pianofall_gen.py > apps/boardgames/src/games/pianofall_levels.h

Deterministic (no chance). --list prints the songs and their grades.
MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import melody_gen  # noqa: E402  (the tune library's reader: ids, titles, composers, bpm)

CHAPTERS, PER = 6, 10
NAMES = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}
BLACK = {1, 3, 6, 8, 10}
LOWEST = 55                                     # G3: the second voice's floor
MAX_WHITES, MIN_WHITES = 15, 8
TEMPO = [0.85, 1.0, 1.0, 1.15, 1.0, 1.1]
CHORDS = [0, 0, 0, 0, 1, 1]
SONG_SECS, MAX_NOTES = 20.0, 200


def die(msg):
    sys.stderr.write("pianofall_gen: %s\n" % msg)
    sys.exit(1)


def is_white(p):
    return p % 12 not in BLACK


def parse(tid, s):
    """the notes with their rhythm: [start, length, pitch] in sixteenths, the rests kept as gaps"""
    out, t, octave = [], 0, 4
    for tok in s.split():
        if tok == "|":
            continue
        p, _, d = tok.partition(",")
        dur = int(d) if d else 4
        if p == "r":
            t += dur
            continue
        m, rest = NAMES[p[0]], p[1:]
        if rest[:1] == "#":
            m, rest = m + 1, rest[1:]
        elif rest[:1] == "b":
            m, rest = m - 1, rest[1:]
        if rest:
            octave = int(rest)
        out.append([t, dur, 12 * (octave + 1) + m])
        t += dur
    if not out:
        die("%s: no notes" % tid)
    return out


def whites_between(lo, hi):
    return sum(1 for p in range(lo, hi + 1) if is_white(p))


def white_down(p):
    return p if is_white(p) else p - 1


def white_up(p):
    return p if is_white(p) else p + 1


def keyboard(notes):
    """the white keys shown: (lowest, highest) pitches, both white"""
    lo, hi = white_down(min(n[2] for n in notes)), white_up(max(n[2] for n in notes))
    while whites_between(lo, hi) < MIN_WHITES:     # an octave at least: grown upwards, then downwards
        if hi < 84:
            hi = white_up(hi + 1)
        else:
            lo = white_down(lo - 1)
    if whites_between(lo, hi) > MAX_WHITES:
        die("a song wider than %d white keys" % MAX_WHITES)
    return lo, hi


def second_voice(notes):
    """a note below the tune on its strong beats (each half bar of 4/4, notes a quarter long or more): a third, else a
    sixth, else a fourth below, in the tune's own scale, on a key no other note holds then"""
    scale = {n[2] % 12 for n in notes}
    top = max(n[2] for n in notes)
    floor = LOWEST
    while whites_between(white_down(floor), white_up(top)) > MAX_WHITES:
        floor += 1
    def harmony(shortest):
        added, last = [], -99
        for t, d, p in notes:
            if t % 4 or d < shortest or t - last < 8:
                continue
            for iv in (3, 4, 8, 9, 5):
                q = p - iv
                if q < floor or q % 12 not in scale:
                    continue
                if any(o[2] == q and o[0] < t + d and t < o[0] + o[1] for o in notes + added):
                    continue
                added.append([t, d, q])
                last = t
                break
        return added
    # on the beats, half a bar apart at least; a tune of quick notes: its eighths too
    added = harmony(4)
    if len(added) < len(notes) // 8:
        added = harmony(2)
    if not added:
        die("no second voice")
    return sorted(notes + added, key=lambda n: (n[0], -n[2]))


def clip(notes):
    """a note's bar ends where the next note on its key starts"""
    for i, n in enumerate(notes):
        for o in notes[i + 1:]:
            if o[2] == n[2]:
                if o[0] <= n[0]:
                    die("two notes at once on one key")
                n[1] = min(n[1], o[0] - n[0])
                break
    return notes


def grade(notes, bpm):
    ps = [n[2] for n in notes]
    secs = (notes[-1][0] + notes[-1][1]) * 15.0 / bpm
    starts = sorted({n[0] for n in notes})
    rate = len(starts) / secs
    steps = [abs(b - a) for a, b in zip(ps, ps[1:])]
    leaps = sum(1 for s in steps if s >= 5) / max(1, len(steps))
    blacks = sum(1 for p in ps if p % 12 in BLACK) / len(ps)
    short = min(b - a for a, b in zip(starts, starts[1:])) * 15.0 / bpm if len(starts) > 1 else 1
    span = whites_between(white_down(min(ps)), white_up(max(ps)))
    g = 3.0 * rate + 6.0 * leaps + 10.0 * blacks + 0.25 * span + len(starts) / 25.0 + 0.6 / max(0.08, short)
    return round(g, 2)


def build():
    tunes = melody_gen.read_tunes(os.path.join(HERE, "melody_tunes.txt"))
    raw, cur = {}, None                         # melody_gen folds the rests: the notes read again, with their rhythm
    for line in open(os.path.join(HERE, "melody_tunes.txt"), encoding="utf-8"):
        line = line.rstrip("\n")
        if not line.strip() or line.startswith("#"):
            continue
        key, _, val = line.partition(" ")
        if key == "tune":
            cur = val.strip()
        elif key == "notes":
            raw[cur] = parse(cur, val)
    songs = []
    for t in tunes:
        notes = raw[t["id"]]
        songs.append({"tune": t, "notes": notes, "white": all(is_white(n[2]) for n in notes),
                      "grade": grade(notes, t["bpm"])})
    if len(songs) < CHAPTERS * PER:
        die("only %d tunes" % len(songs))
    by = sorted(songs, key=lambda s: (s["grade"], s["tune"]["index"]))
    whites = [s for s in by if s["white"]]
    if len(whites) < 2 * PER:
        die("only %d tunes on white keys" % len(whites))
    ch = [whites[:PER], whites[PER:2 * PER]]
    left = [s for s in by if s not in ch[0] and s not in ch[1]]
    blacks = [s for s in left if not s["white"]]
    ch.append(blacks[:PER])
    left = [s for s in left if s not in ch[2]]
    # the rest: graded with what each chapter adds (the hardest last)
    ch.append(left[:PER])
    ch.append(left[PER:2 * PER])
    ch.append(left[2 * PER:3 * PER])
    levels = []
    for k in range(CHAPTERS):
        made = []
        for s in ch[k]:
            notes = [list(n) for n in s["notes"]]
            if CHORDS[k]:
                notes = second_voice(notes)
            bpm = int(round(s["tune"]["bpm"] * TEMPO[k]))
            # a short tune is played again (after a half note's rest) until the song lasts SONG_SECS or so
            once = [list(n) for n in notes]
            play = max(n[0] + n[1] for n in once) + 8
            reps = 1
            while reps < 4 and (reps * play - 8) * 15.0 / bpm < SONG_SECS and len(once) * (reps + 1) <= MAX_NOTES:
                reps += 1
            notes = [[t + r * play, d, p] for r in range(reps) for t, d, p in once]
            notes = clip(sorted(notes, key=lambda n: (n[0], -n[2])))
            lo, hi = keyboard(notes)
            made.append({"tune": s["tune"], "notes": notes, "bpm": bpm, "lo": lo, "hi": hi,
                         "grade": grade(notes, bpm), "chords": CHORDS[k]})
        made.sort(key=lambda m: (m["grade"], m["tune"]["index"]))
        levels += made
    return tunes, levels


def header(tunes, levels):
    w = sys.stdout.write
    total = sum(len(m["notes"]) for m in levels)
    w("/* Falling Notes' songs, made by tools/pianofall_gen.py from tools/melody_tunes.txt (do not edit; run it again):\n"
      " * %d songs in %d chapters of %d (games/pianofall.h: pfl_song). Every tune is in the public domain: its source\n"
      " * and its composer's death year are in tools/melody_tunes.txt; its title: help/<lang>/melody_titles.txt. */\n"
      % (len(levels), CHAPTERS, PER))
    w("#ifndef PIANOFALL_LEVELS_H\n#define PIANOFALL_LEVELS_H\n\n#include \"games/pianofall.h\"\n\n")
    w("#define PFL_LEVELS %d\n#define PFL_CHAPTERS %d\n#define PFL_NOTES_ALL %d\n\n" % (len(levels), CHAPTERS, total))
    w("/* every song's notes one after the other: the start, the length (sixteenths), the pitch (MIDI: 60 is C4) */\n")
    w("static const pfl_src pfl_notes[PFL_NOTES_ALL] = {\n")
    for i, m in enumerate(levels):
        w("    /* %d %s */\n" % (i + 1, m["tune"]["id"]))
        row = ["{%d,%d,%d}" % (t, d, p) for t, d, p in m["notes"]]
        for k in range(0, len(row), 10):
            w("    %s,\n" % ",".join(row[k:k + 10]))
    w("};\n\n")
    w("/* tune (melody_titles), bpm, the keyboard's lowest and highest white keys, two voices, notes, the first */\n")
    w("static const pfl_song pfl_songs[PFL_LEVELS] = {\n")
    first = 0
    for i, m in enumerate(levels):
        if i % PER == 0:
            w("    /* chapter %d */\n" % (i // PER + 1))
        w("    {%d, %d, %d, %d, %d, %d, %d},  /* %d %s, grade %.1f */\n"
          % (m["tune"]["index"], m["bpm"], m["lo"], m["hi"], m["chords"], len(m["notes"]), first, i + 1,
             m["tune"]["id"], m["grade"]))
        first += len(m["notes"])
    w("};\n\n#endif\n")


def main():
    tunes, levels = build()
    if "--list" in sys.argv:
        for i, m in enumerate(levels):
            secs = (m["notes"][-1][0] + m["notes"][-1][1]) * 15.0 / m["bpm"]
            print("%2d ch%d g%5.2f %-12s bpm%3d n%3d keys %d..%d (%2d whites) %4.0fs" % (
                i + 1, i // PER + 1, m["grade"], m["tune"]["id"], m["bpm"], len(m["notes"]), m["lo"], m["hi"],
                whites_between(m["lo"], m["hi"]), secs))
        return
    header(tunes, levels)


if __name__ == "__main__":
    main()
