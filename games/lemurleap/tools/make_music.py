#!/usr/bin/env python3
"""Lemur Leap music: 4-channel ProTracker MODs with the house kit (games/common/tools/house_music.py;
docs/art-direction.md "Audio"): channel 0 a plucked arpeggio, 1 the bass, 2 the drums (kick, snare, hats), 3 a square
lead singing the tune. Written note by note from small motifs (no randomness: the same files every build).

    make_music.py OUTDIR       -> OUTDIR/climb.mod (the title, the garden and the clouds), OUTDIR/stars.mod (the sky
                                  and space)

  "Beanstalk Hop"   C major, speed 5: a springy tune, the bass hopping octaves like the lemur on the leaves
  "Starry Leaves"   A dorian, speed 6: lighter drums, a floating lead, high above the clouds

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/lemurleap/LICENSE. The tunes are original (8BCraft).
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "common", "tools"))
import house_music as hm  # noqa: E402
from house_music import cell, note  # noqa: E402

S_LEAD, S_BASS, S_PLUCK, S_KICK, S_SNARE, S_HAT = 1, 2, 3, 4, 5, 6


def samples():
    return [("lead", hm.square_lead(0.25), 22, 0, 32), ("bass", hm.bass(), 40, 0, 64), ("pluck", hm.drop(), 30, 0, 0),
            ("kick", hm.kick(), 40, 0, 0), ("snare", hm.snare(), 26, 0, 0), ("hat", hm.hat(), 15, 0, 0)]


# key (C-1 = 0), scale, speed, the 4 patterns' chords (scale degrees, one per bar), the lead's motifs (per bar:
# (row, degree offset from the chord's root, length in rows)), the order
SONGS = {
    "climb": dict(key=0, scale=hm.MAJOR, speed=5, light=False,
                  chords=[[0, 4, 5, 3], [0, 4, 3, 4], [5, 3, 0, 4], [3, 4, 0, 0]],
                  motifs=[[(0, 0, 2), (2, 2, 2), (4, 4, 2), (6, 7, 2), (8, 4, 2), (10, 2, 2), (12, 4, 4)],
                          [(0, 7, 3), (3, 6, 1), (4, 4, 2), (6, 2, 2), (8, 4, 6), (14, 2, 2)],
                          [(0, 2, 2), (2, 4, 2), (4, 2, 2), (6, 0, 2), (8, 2, 4), (12, -1, 4)],
                          [(0, 4, 2), (2, 4, 2), (4, 7, 4), (8, 9, 4), (12, 7, 4)]],
                  order=[0, 1, 0, 2, 3]),
    "stars": dict(key=9, scale=hm.DORIAN, speed=6, light=True,
                  chords=[[0, 3, 4, 0], [5, 3, 4, 4], [0, 6, 3, 4], [5, 6, 0, 0]],
                  motifs=[[(0, 4, 4), (4, 6, 2), (6, 7, 2), (8, 6, 6), (14, 4, 2)],
                          [(0, 2, 2), (2, 4, 2), (4, 7, 4), (8, 4, 8)],
                          [(0, 9, 4), (4, 7, 4), (8, 6, 4), (12, 4, 4)],
                          [(0, 4, 2), (2, 2, 2), (4, 0, 8), (12, 2, 4)]],
                  order=[0, 1, 2, 3]),
}


def build_song(sd, title):
    key, scale = sd["key"], sd["scale"]

    def deg(d, octave):
        return hm.deg(d, octave, key, scale)

    pats = []
    for p in range(4):
        rows, lead_off = [], {}
        for r in range(64):
            bar, beat = r // 16, r % 16
            ch = sd["chords"][p][bar]
            c = [cell(), cell(), cell(), cell()]
            if r % 2 == 0:                                               # the plucked arpeggio, up and down
                step = [0, 2, 4, 7, 9, 7, 4, 2][(r // 2) % 8]
                c[0] = cell(S_PLUCK, note(deg(ch + step, 2)), 0xC, 26 if r % 4 else 34)
            if sd["light"]:                                              # long low notes
                if beat in (0, 8):
                    c[1] = cell(S_BASS, note(deg(ch, 0)))
            elif beat in (0, 4, 8, 12):                                  # the hop: root, octave, root, fifth
                d = {0: 0, 4: 7, 8: 0, 12: 4}[beat]
                c[1] = cell(S_BASS, note(deg(ch + d, 0)))
            if sd["light"]:
                if beat in (0, 10):
                    c[2] = cell(S_KICK, note(24), 0xC, 34)
                elif beat == 8:
                    c[2] = cell(S_SNARE, note(24), 0xC, 24)
                elif beat % 4 == 2:
                    c[2] = cell(S_HAT, note(30), 0xC, 16)
            else:
                if beat in (0, 8) or (beat == 14 and p % 2):
                    c[2] = cell(S_KICK, note(24), 0xC, 42)
                elif beat in (4, 12):
                    c[2] = cell(S_SNARE, note(24), 0xC, 32)
                elif beat % 2 == 0:
                    c[2] = cell(S_HAT, note(30), 0xC, 18 if beat % 4 else 24)
            motif = sd["motifs"][3] if bar == 3 and p in (1, 3) else sd["motifs"][(p + bar) % 3]
            for (row, d, ln) in motif:
                if row == beat:
                    c[3] = cell(S_LEAD, note(deg(ch + d, 1)), 0xC, 32)
                    lead_off[r + ln] = 1
            if r in lead_off and c[3] == cell():
                c[3] = cell(0, 0, 0xC, 0)
            rows.append(b"".join(c))
        pats.append(b"".join(rows))
    return hm.build_mod(title, samples(), pats, sd["order"], speed=sd["speed"])


def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else "."
    os.makedirs(outdir, exist_ok=True)
    for name, sd in SONGS.items():
        with open(os.path.join(outdir, name + ".mod"), "wb") as f:
            f.write(build_song(sd, "lemurleap " + name))


if __name__ == "__main__":
    main()
