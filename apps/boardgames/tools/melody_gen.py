#!/usr/bin/env python3
"""Play by Ear's levels: src/games/melody_levels.h, from tools/melody_tunes.txt (public-domain tunes only).

Each tune gives a level for each "|" it marks: its first bar, a longer phrase, the whole phrase. The candidates are
checked (C4..C6, the keyboard shown holds them, lengths, durations), graded (the notes, the leaps, the black keys,
the range, the speed), the near-duplicates of one tune dropped (a phrase only a note or two longer than another),
150 kept (the tunes with the most phrases give up their middle ones first), ordered by grade and cut into six
chapters of 25. Two levels of the same tune never follow each other (a swap with a level of about the same grade).
Each chapter ends on a showpiece where the curve allows: a whole tune close in grade to its hardest moves last.
The keyboard of a level: C4..C5 (8 white keys), C4..G5 (12) or C4..C6 (15), the smallest that holds it.

    python3 apps/boardgames/tools/melody_gen.py > apps/boardgames/src/games/melody_levels.h

Deterministic: no chance at all. --list prints the levels, their notes and intervals (to check them by ear and eye).
MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
LEVELS, CHAPTERS, MAX_NOTES = 150, 6, 56
LOW, HIGH = 60, 84                              # C4, C6
KEYBOARDS = [(8, 72), (12, 79), (15, 84)]       # white keys from C4, the highest key
NAMES = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}
SHARP = ["C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"]
BLACK = {1, 3, 6, 8, 10}


def die(msg):
    sys.stderr.write("melody_gen: %s\n" % msg)
    sys.exit(1)


def name(m):
    return "%s%d" % (SHARP[m % 12], m // 12 - 1)


def parse_notes(tid, s):
    """the notes (pitch, gap in sixteenths to the next note, rests folded in) and the cut points"""
    notes, cuts, octave = [], [], 4
    for tok in s.split():
        if tok == "|":
            if notes and (not cuts or cuts[-1] != len(notes)):
                cuts.append(len(notes))
            continue
        p, _, d = tok.partition(",")
        dur = int(d) if d else 4
        if dur <= 0 or dur > 24:
            die("%s: bad length %r" % (tid, tok))
        if p == "r":
            if not notes:
                die("%s: a rest first" % tid)
            notes[-1][1] += dur
            continue
        if p[0] not in NAMES:
            die("%s: bad note %r" % (tid, tok))
        m, rest = NAMES[p[0]], p[1:]
        if rest[:1] == "#":
            m, rest = m + 1, rest[1:]
        elif rest[:1] == "b":
            m, rest = m - 1, rest[1:]
        if rest:
            if not rest.isdigit():
                die("%s: bad octave %r" % (tid, tok))
            octave = int(rest)
        pitch = 12 * (octave + 1) + m
        if pitch < LOW or pitch > HIGH:
            die("%s: %s is outside C4..C6" % (tid, name(pitch)))
        notes.append([pitch, dur])
    if not cuts or cuts[-1] != len(notes):
        cuts.append(len(notes))
    return [(p, d) for p, d in notes], cuts


def read_tunes(path):
    tunes, cur = [], None
    for line in open(path, encoding="utf-8"):
        line = line.rstrip("\n")
        if not line.strip() or line.startswith("#"):
            continue
        key, _, val = line.partition(" ")
        val = val.strip()
        if key == "tune":
            cur = {"id": val}
            tunes.append(cur)
        elif cur is None:
            die("a line before the first tune: %r" % line)
        elif key in ("title", "by", "bpm", "notes"):
            cur[key] = val
        else:
            die("%s: unknown line %r" % (cur["id"], line))
    for i, t in enumerate(tunes):
        for k in ("title", "by", "bpm", "notes"):
            if k not in t:
                die("%s: no %s" % (t["id"], k))
        by = [x.strip() for x in t["by"].split(";")]
        if len(by) != 3:
            die("%s: 'by' wants composer; death year; source" % t["id"])
        t["composer"], t["died"], t["source"] = by
        if t["composer"] != "traditional":
            if not t["died"].isdigit() or int(t["died"]) > 1955:
                die("%s: %s died in %s: not yet in the public domain" % (t["id"], t["composer"], t["died"]))
        elif t["died"] != "-":
            die("%s: a traditional tune has no death year" % t["id"])
        t["bpm"] = int(t["bpm"])
        t["index"] = i
        t["notes"], t["cuts"] = parse_notes(t["id"], t["notes"])
    ids = [t["id"] for t in tunes]
    if len(set(ids)) != len(ids):
        die("two tunes with the same id")
    return tunes


def keyboard(notes):
    top = max(p for p, _ in notes)
    for whites, high in KEYBOARDS:
        if top <= high:
            return whites
    die("no keyboard holds it")


def grade(notes, bpm):
    """how hard it is to hear and play back: its length first, then its leaps, black keys, range and speed"""
    n = len(notes)
    ps = [p for p, _ in notes]
    steps = [abs(b - a) for a, b in zip(ps, ps[1:])]
    leaps = sum(1 for s in steps if s >= 3)
    big = sum(1 for s in steps if s >= 7)
    blacks = sum(1 for p in ps if p % 12 in BLACK)
    black_keys = len({p for p in ps if p % 12 in BLACK})
    span = max(ps) - min(ps)
    secs = sum(d for _, d in notes[:-1]) * 15.0 / bpm  # a sixteenth is 15 / bpm seconds
    speed = (n - 1) / secs if secs > 0 else 1
    g = n + 0.45 * leaps + 0.9 * big + 0.35 * blacks + 1.2 * black_keys + span / 4.0 + 2.0 * max(0.0, speed - 2.5)
    return round(g, 1)


def candidates(tunes):
    out = []
    for t in tunes:
        last = 0
        for c in t["cuts"]:
            # a phrase only a little longer than the tune's shorter one is a near-duplicate: dropped
            if last and (c - last < 3 or c < 1.25 * last):
                continue
            if c < 3:
                continue
            if c > MAX_NOTES:
                die("%s: %d notes, at most %d" % (t["id"], c, MAX_NOTES))
            notes = t["notes"][:c]
            out.append({"tune": t, "n": c, "notes": notes, "whites": keyboard(notes), "grade": grade(notes, t["bpm"])})
            last = c
    return out


def choose(cands):
    """150 of them: while too many, the tune with the most levels gives up its middle one (its first bar and its whole
    phrase stay)"""
    while len(cands) > LEVELS:
        by = {}
        for c in cands:
            by.setdefault(c["tune"]["id"], []).append(c)
        tid = max(sorted(by), key=lambda k: (len(by[k]), k))
        mine = sorted(by[tid], key=lambda c: c["n"])
        if len(mine) < 3:
            die("too many tunes: %d levels, %d wanted" % (len(cands), LEVELS))
        drop = mine[len(mine) // 2]
        cands = [c for c in cands if c is not drop]
    if len(cands) < LEVELS:
        die("only %d levels, %d wanted: mark more phrases" % (len(cands), LEVELS))
    cands.sort(key=lambda c: (c["grade"], c["n"], c["tune"]["index"]))
    # no tune twice in a row: the second swapped with the next level of another tune close in grade
    for i in range(1, len(cands)):
        if cands[i]["tune"] is cands[i - 1]["tune"]:
            for j in range(i + 1, min(i + 6, len(cands))):
                if (cands[j]["tune"] is not cands[i - 1]["tune"] and cands[j]["grade"] - cands[i]["grade"] <= 1.0
                        and (j + 1 >= len(cands) or cands[j + 1]["tune"] is not cands[i]["tune"])):
                    cands[i], cands[j] = cands[j], cands[i]
                    break
    # each chapter ends on a showpiece: a whole tune among its last four, if one is within a grade point of its
    # hardest, moves to the end (the ones after it move up one)
    per = LEVELS // CHAPTERS
    for k in range(CHAPTERS):
        end = (k + 1) * per - 1
        for i in range(end - 1, end - 4, -1):
            c = cands[i]
            if (c["n"] == len(c["tune"]["notes"]) and cands[end]["grade"] - c["grade"] <= 1.0
                    and cands[i - 1]["tune"] is not cands[i + 1]["tune"] and cands[end]["tune"] is not c["tune"]
                    and (end + 1 >= LEVELS or cands[end + 1]["tune"] is not c["tune"])):
                cands[i:end + 1] = cands[i + 1:end + 1] + [c]
                break
    return cands


def header(tunes, levels):
    w = sys.stdout.write
    w("/* Play by Ear's levels, made by tools/melody_gen.py from tools/melody_tunes.txt (do not edit; run it again):\n"
      " * %d levels in %d chapters of %d, ordered by grade (games/melody.h: mel_level). Every tune is in the public\n"
      " * domain: its source and its composer's death year are in tools/melody_tunes.txt. */\n"
      % (LEVELS, CHAPTERS, LEVELS // CHAPTERS))
    w("#ifndef MELODY_LEVELS_H\n#define MELODY_LEVELS_H\n\n#include \"games/melody.h\"\n\n")
    w("#define MEL_LEVELS %d\n#define MEL_CHAPTERS %d\n#define MEL_TUNES %d\n\n" % (LEVELS, CHAPTERS, len(tunes)))
    w("/* the tunes, in the order of help/<lang>/melody_titles.txt: their ids, bpm, the composer's death year (0:\n"
      " * traditional) */\n")
    w("static const mel_tune mel_tunes[MEL_TUNES] = {\n")
    for t in tunes:
        w("    {\"%s\", %d, %d},\n" % (t["id"], t["bpm"], int(t["died"]) if t["died"].isdigit() else 0))
    w("};\n\n")
    w("/* tune, notes, white keys, grade x 10; the pitches (MIDI: 60 is C4), the gaps to the next note (sixteenths) */\n")
    w("static const mel_level mel_levels[MEL_LEVELS] = {\n")
    for i, c in enumerate(levels):
        if i % (LEVELS // CHAPTERS) == 0:
            w("    /* chapter %d */\n" % (i // (LEVELS // CHAPTERS) + 1))
        ps = ",".join(str(p) for p, _ in c["notes"])
        ds = ",".join(str(d) for _, d in c["notes"])
        w("    {%d, %d, %d, %d, {%s}, {%s}},  /* %d %s */\n"
          % (c["tune"]["index"], c["n"], c["whites"], int(round(c["grade"] * 10)), ps, ds, i + 1, c["tune"]["id"]))
    w("};\n\n#endif\n")


def listing(levels):
    for i, c in enumerate(levels):
        ps = [p for p, _ in c["notes"]]
        iv = " ".join("%+d" % (b - a) for a, b in zip(ps, ps[1:]))
        print("%3d ch%d g%5.1f %-12s n%2d k%2d  %s\n%38s%s" % (
            i + 1, i // (LEVELS // CHAPTERS) + 1, c["grade"], c["tune"]["id"], c["n"], c["whites"],
            " ".join(name(p) for p in ps), "", iv))


def main():
    tunes = read_tunes(os.path.join(HERE, "melody_tunes.txt"))
    levels = choose(candidates(tunes))
    if "--list" in sys.argv:
        listing(levels)
        per = LEVELS // CHAPTERS
        for k in range(CHAPTERS):
            ch = levels[k * per:(k + 1) * per]
            print("chapter %d: %d..%d notes, grades %.1f..%.1f, %d tunes" % (
                k + 1, min(c["n"] for c in ch), max(c["n"] for c in ch), ch[0]["grade"], ch[-1]["grade"],
                len({c["tune"]["id"] for c in ch})))
        return
    header(tunes, levels)


if __name__ == "__main__":
    main()
