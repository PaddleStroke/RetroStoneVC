#!/usr/bin/env python3
"""Goat Gorge music: a cheerful chiptune per chapter, 4-channel ProTracker MODs with the house kit
(games/common/tools/house_music.py; docs/art-direction.md "Audio"): channel 0 a plucked arpeggio, 1 the bass, 2 the
drums (kick, snare, hats), 3 a square lead singing the chapter's tune. Written note by note from small motifs (no
randomness: the same files every build).

    make_music.py OUTDIR       -> OUTDIR/ch1.mod .. ch5.mod (the title plays ch1.mod)

  1 "Meadow Hop"      C major, speed 5: a skipping tune over I V vi IV
  2 "Pine Path"       G major, speed 6: a walking tune, the lead an octave lower in the middle
  3 "Crystal Drip"    A dorian, speed 6: glassy plucks, a curious lead
  4 "Gusty Ridge"     D mixolydian, speed 5: brisk, the bass hopping octaves
  5 "Summit Song"     F major, speed 5: bright, the tune climbing to the top

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/goatgorge/LICENSE. The tunes are original (8BCraft).
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "common", "tools"))
import house_music as hm  # noqa: E402
from house_music import cell, note  # noqa: E402

S_LEAD, S_BASS, S_PLUCK, S_KICK, S_SNARE, S_HAT = 1, 2, 3, 4, 5, 6


def samples():
    return [("lead", hm.square_lead(0.25), 26, 0, 32), ("bass", hm.bass(), 40, 0, 64), ("pluck", hm.drop(), 30, 0, 0),
            ("kick", hm.kick(), 40, 0, 0), ("snare", hm.snare(), 28, 0, 0), ("hat", hm.hat(), 16, 0, 0)]


# each chapter: key (semitone of the scale's root, C-1 = 0), scale, speed, the 4 patterns' chords (scale degrees, one
# per bar) and the lead's motifs (per bar: (row, degree offset from the chord's root, length in rows); None: rest)
SONGS = [
    dict(key=0, scale=hm.MAJOR, speed=5, chords=[[0, 4, 5, 3], [0, 4, 5, 3], [3, 4, 2, 5], [3, 4, 0, 0]],
         motifs=[[(0, 4, 2), (2, 2, 2), (4, 0, 2), (6, 2, 2), (8, 4, 4), (12, 7, 4)],
                 [(0, 7, 2), (2, 6, 2), (4, 4, 4), (8, 2, 2), (10, 4, 2), (12, 2, 4)],
                 [(0, 0, 4), (4, 2, 2), (6, 4, 2), (8, 7, 6), (14, 6, 2)],
                 [(0, 4, 3), (3, 2, 1), (4, 0, 4), (8, -1, 4), (12, 0, 4)]],
         order=[0, 1, 0, 2, 3]),
    dict(key=7, scale=hm.MAJOR, speed=6, chords=[[0, 3, 4, 0], [5, 3, 4, 4], [0, 3, 4, 0], [5, 1, 4, 0]],
         motifs=[[(0, 2, 4), (4, 4, 2), (6, 2, 2), (8, 0, 6), (14, 1, 2)],
                 [(0, 4, 2), (2, 4, 2), (4, 5, 4), (8, 4, 4), (12, 2, 4)],
                 [(0, 0, 2), (2, 2, 2), (4, 4, 2), (6, 7, 2), (8, 6, 4), (12, 4, 4)],
                 [(0, 2, 6), (6, 1, 2), (8, 0, 8)]],
         order=[0, 1, 2, 3]),
    dict(key=9, scale=hm.DORIAN, speed=6, chords=[[0, 3, 0, 4], [0, 3, 5, 4], [2, 3, 0, 4], [5, 4, 0, 0]],
         motifs=[[(0, 4, 2), (3, 2, 1), (4, 0, 2), (8, 4, 2), (10, 5, 2), (12, 4, 4)],
                 [(0, 7, 4), (6, 6, 2), (8, 4, 2), (10, 2, 2), (12, 0, 4)],
                 [(2, 2, 2), (4, 4, 2), (6, 2, 2), (8, 0, 4), (14, -1, 2)],
                 [(0, 0, 4), (4, 4, 4), (8, 7, 4), (12, 4, 4)]],
         order=[0, 1, 0, 2, 3]),
    dict(key=2, scale=hm.MIXOLYDIAN, speed=5, chords=[[0, 6, 3, 0], [0, 6, 3, 4], [3, 3, 0, 0], [6, 6, 4, 0]],
         motifs=[[(0, 0, 2), (2, 0, 2), (4, 4, 2), (6, 4, 2), (8, 6, 2), (10, 7, 2), (12, 4, 4)],
                 [(0, 7, 2), (2, 9, 2), (4, 7, 2), (6, 4, 2), (8, 2, 4), (12, 0, 4)],
                 [(0, 4, 6), (6, 2, 2), (8, 4, 2), (10, 6, 2), (12, 7, 4)],
                 [(0, 2, 2), (2, 0, 2), (4, 2, 4), (8, 4, 8)]],
         order=[0, 1, 0, 2, 3]),
    dict(key=5, scale=hm.MAJOR, speed=5, chords=[[0, 3, 4, 0], [0, 5, 3, 4], [3, 4, 5, 3], [3, 4, 0, 0]],
         motifs=[[(0, 0, 2), (2, 2, 2), (4, 4, 2), (6, 7, 6), (12, 6, 2), (14, 7, 2)],
                 [(0, 9, 4), (4, 7, 2), (6, 4, 2), (8, 7, 4), (12, 4, 4)],
                 [(0, 2, 2), (2, 4, 2), (4, 7, 4), (8, 9, 2), (10, 7, 2), (12, 11, 4)],
                 [(0, 7, 4), (4, 9, 4), (8, 14, 8)]],
         order=[0, 1, 2, 0, 3]),
]


def build_song(sd, title):
    key, scale = sd["key"], sd["scale"]

    def deg(d, octave):
        return hm.deg(d, octave, key, scale)

    pats = []
    for p in range(4):
        rows = []
        lead_off = {}                                                    # row -> a note-off (volume 0) on the lead
        for r in range(64):
            bar, beat = r // 16, r % 16
            ch = sd["chords"][p][bar]
            c = [cell(), cell(), cell(), cell()]
            if r % 2 == 0:                                               # the plucked arpeggio (up and down)
                step = [0, 2, 4, 7, 4, 2, 0, 2][(r // 2) % 8]
                c[0] = cell(S_PLUCK, note(deg(ch + step, 2)), 0xC, 30 if r % 4 else 38)
            if beat in (0, 6, 8, 14):                                    # the bass: root, fifth, octave hops
                d = {0: 0, 6: 4, 8: 7 if p % 2 else 0, 14: 4}[beat]
                c[1] = cell(S_BASS, note(deg(ch + d, 0)))
            if beat in (0, 8) or (beat == 10 and p == 3):                # the drums on one channel
                c[2] = cell(S_KICK, note(24), 0xC, 44)
            elif beat in (4, 12):
                c[2] = cell(S_SNARE, note(24), 0xC, 34)
            elif beat % 2 == 0:
                c[2] = cell(S_HAT, note(30), 0xC, 20 if beat % 4 else 26)
            motif = sd["motifs"][3] if (p == 3 and bar == 3) or (bar == 3 and p == 1) else sd["motifs"][(p + bar) % 3]
            for (row, d, ln) in motif:
                if row == beat:
                    c[3] = cell(S_LEAD, note(deg(sd["chords"][p][bar] + d, 1)), 0xC, 34)
                    lead_off[r + ln] = 1
            if r in lead_off and c[3] == cell():
                c[3] = cell(0, 0, 0xC, 0)
            rows.append(b"".join(c))
        pats.append(b"".join(rows))
    return hm.build_mod(title, samples(), pats, sd["order"], speed=sd["speed"])


def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else "."
    os.makedirs(outdir, exist_ok=True)
    for i, sd in enumerate(SONGS):
        with open(os.path.join(outdir, "ch%d.mod" % (i + 1)), "wb") as f:
            f.write(build_song(sd, "goatgorge %d" % (i + 1)))


if __name__ == "__main__":
    main()
