#!/usr/bin/env python3
"""Puffin Plunge music: 4-channel ProTracker MODs with the house kit (games/common/tools/house_music.py;
docs/art-direction.md "Audio"): channel 0 a plucked arpeggio, 1 the bass, 2 the drums (kick, snare, hats), 3 a square
lead singing the tune. Written note by note from small motifs (no randomness: the same files every build).

    make_music.py OUTDIR    -> OUTDIR/zone1.mod .. zone4.mod (the title plays zone1), OUTDIR/boss.mod

  "Crystal Grotto"   E dorian, speed 5: a bright, curious dive
  "Coral Trench"     A mixolydian, speed 5: a bubbly swing
  "Mushroom Cave"    D minor, speed 5: a sneaky walk in the dark
  "Frozen Deep"      B minor, speed 4: a cold, driving descent
  "Grumpy Walrus"    G minor, speed 4: the boss at the bottom of the well

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/puffinplunge/LICENSE. The tunes are original (8BCraft).
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
            ("kick", hm.kick(), 40, 0, 0), ("snare", hm.snare(), 26, 0, 0), ("hat", hm.hat(), 14, 0, 0)]


# key (C-1 = 0), scale, speed, the 4 patterns' chords (scale degrees, one a bar), the lead's motifs (per bar: (row,
# degree from the chord's root, rows held)), the arpeggio's shape, the order
SONGS = {
    "zone1": dict(key=4, scale=hm.DORIAN, speed=5, busy=False, arp=[0, 2, 4, 7, 9, 7, 4, 2],
                  chords=[[0, 3, 0, 4], [0, 3, 5, 4], [3, 4, 0, 5], [3, 1, 4, 0]],
                  motifs=[[(0, 4, 3), (3, 2, 1), (4, 4, 2), (6, 7, 2), (8, 6, 4), (12, 4, 4)],
                          [(0, 2, 2), (2, 4, 2), (4, 2, 2), (6, 0, 2), (8, 1, 6), (14, 2, 2)],
                          [(0, 7, 2), (2, 9, 2), (4, 7, 2), (6, 4, 2), (8, 6, 6), (14, 4, 2)],
                          [(0, 4, 4), (4, 2, 4), (8, 0, 8)]],
                  order=[0, 1, 0, 2, 3]),
    "zone2": dict(key=9, scale=hm.MIXOLYDIAN, speed=5, busy=False, arp=[0, 4, 2, 4, 7, 4, 2, 4],
                  chords=[[0, 0, 6, 6], [3, 3, 0, 4], [0, 6, 3, 4], [5, 3, 4, 0]],
                  motifs=[[(0, 2, 1), (2, 4, 1), (3, 4, 1), (4, 6, 2), (8, 4, 2), (10, 2, 2), (12, 0, 4)],
                          [(0, 7, 2), (3, 6, 1), (4, 4, 4), (10, 2, 2), (12, 4, 4)],
                          [(0, 0, 2), (2, 2, 2), (4, 4, 2), (6, 7, 2), (8, 8, 4), (12, 7, 4)],
                          [(0, 4, 2), (2, 2, 2), (4, 0, 8), (12, 0, 4)]],
                  order=[0, 1, 2, 3]),
    "zone3": dict(key=2, scale=hm.MINOR, speed=5, busy=False, arp=[0, 2, 4, 2, 7, 4, 2, 0],
                  chords=[[0, 5, 3, 4], [0, 5, 6, 4], [5, 3, 0, 4], [5, 6, 4, 4]],
                  motifs=[[(0, 0, 2), (3, 2, 1), (4, 4, 2), (8, 3, 2), (11, 2, 1), (12, 0, 4)],
                          [(0, 4, 1), (2, 4, 1), (4, 6, 2), (6, 7, 2), (8, 4, 6), (14, 2, 2)],
                          [(0, 7, 3), (3, 6, 1), (4, 4, 2), (6, 2, 2), (8, 3, 4), (12, 1, 4)],
                          [(0, 0, 2), (2, 1, 2), (4, 2, 4), (8, 4, 8)]],
                  order=[0, 1, 0, 2, 3]),
    "zone4": dict(key=11, scale=hm.MINOR, speed=4, busy=True, arp=[0, 4, 7, 4, 0, 4, 7, 9],
                  chords=[[0, 0, 5, 6], [0, 0, 3, 4], [5, 6, 0, 4], [3, 4, 5, 4]],
                  motifs=[[(0, 4, 2), (2, 4, 2), (4, 7, 4), (8, 6, 2), (10, 4, 2), (12, 2, 4)],
                          [(0, 0, 2), (2, 2, 2), (4, 4, 2), (6, 2, 2), (8, 4, 8)],
                          [(0, 9, 2), (2, 7, 2), (4, 6, 2), (6, 4, 2), (8, 6, 4), (12, 7, 4)],
                          [(0, 4, 4), (4, 6, 4), (8, 7, 4), (12, 8, 4)]],
                  order=[0, 1, 2, 1, 3]),
    "boss": dict(key=7, scale=hm.MINOR, speed=4, busy=True, arp=[0, 0, 7, 0, 3, 0, 7, 5],
                 chords=[[0, 0, 5, 4], [0, 0, 6, 4], [5, 3, 0, 4], [5, 6, 4, 4]],
                 motifs=[[(0, 0, 1), (2, 0, 1), (4, 2, 2), (6, 3, 2), (8, 4, 2), (10, 3, 2), (12, 2, 4)],
                         [(0, 7, 2), (2, 6, 2), (4, 4, 2), (6, 3, 2), (8, 4, 4), (12, 0, 4)],
                         [(0, 4, 1), (1, 4, 1), (2, 7, 2), (4, 6, 2), (8, 4, 2), (10, 3, 2), (12, 1, 4)],
                         [(0, 0, 4), (4, 2, 4), (8, 3, 4), (12, 4, 4)]],
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
            if r % 2 == 0:                                               # the plucked arpeggio
                step = sd["arp"][(r // 2) % 8]
                c[0] = cell(S_PLUCK, note(deg(ch + step, 2)), 0xC, 26 if r % 4 else 34)
            if sd["busy"]:                                               # a driving eighth-note bass
                if beat % 2 == 0:
                    c[1] = cell(S_BASS, note(deg(ch + (4 if beat % 8 == 6 else 0), 0)))
            elif beat in (0, 3, 8, 11, 14):                              # a bouncy bass
                d = {0: 0, 3: 0, 8: 4, 11: 4, 14: 7}[beat]
                c[1] = cell(S_BASS, note(deg(ch + d, 0)))
            kicks = (0, 6, 8, 14) if sd["busy"] else (0, 10)
            if beat in kicks:
                c[2] = cell(S_KICK, note(24), 0xC, 44)
            elif beat in (4, 12):
                c[2] = cell(S_SNARE, note(24), 0xC, 32)
            elif beat % 2 == 0 or sd["busy"]:
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
            f.write(build_song(sd, "puffinplunge " + name))


if __name__ == "__main__":
    main()
