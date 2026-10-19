#!/usr/bin/env python3
"""Prickle Patch music: 4-channel ProTracker MODs with the house kit (games/common/tools/house_music.py;
docs/art-direction.md "Audio"): channel 0 a plucked arpeggio, 1 the bass, 2 the drums (kick, snare, hats), 3 a square
lead singing the tune. Written note by note from small motifs (no randomness: the same files every build).

    make_music.py OUTDIR       -> OUTDIR/garden.mod (the title and the run), OUTDIR/boss.mod (a boss in the garden)

  "Garden Patrol"   G mixolydian, speed 5: a bouncy march for a gardener on the move
  "Big Pest"        A minor, speed 4: a driving bass, the hats doubled, a nervous lead, while a boss is out

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pricklepatch/LICENSE. The tunes are original (8BCraft).
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "common", "tools"))
import house_music as hm  # noqa: E402
from house_music import cell, note  # noqa: E402

S_LEAD, S_BASS, S_PLUCK, S_KICK, S_SNARE, S_HAT = 1, 2, 3, 4, 5, 6


def samples():
    return [("lead", hm.square_lead(0.25), 24, 0, 32), ("bass", hm.bass(), 40, 0, 64), ("pluck", hm.drop(), 30, 0, 0),
            ("kick", hm.kick(), 40, 0, 0), ("snare", hm.snare(), 28, 0, 0), ("hat", hm.hat(), 16, 0, 0)]


# key (semitone of the scale's root, C-1 = 0), scale, speed, the 4 patterns' chords (scale degrees, one per bar) and
# the lead's motifs (per bar: (row, degree offset from the chord's root, length in rows))
SONGS = {
    "garden": dict(key=7, scale=hm.MIXOLYDIAN, speed=5, busy=False,
                   chords=[[0, 3, 0, 4], [0, 3, 6, 4], [3, 4, 0, 5], [3, 6, 4, 0]],
                   motifs=[[(0, 4, 2), (2, 4, 2), (4, 2, 2), (6, 4, 2), (8, 7, 4), (12, 4, 4)],
                           [(0, 2, 2), (2, 4, 2), (4, 5, 2), (6, 4, 2), (8, 2, 6), (14, 0, 2)],
                           [(0, 7, 2), (2, 6, 2), (4, 4, 2), (6, 2, 2), (8, 4, 4), (12, 0, 4)],
                           [(0, 0, 2), (2, 2, 2), (4, 4, 4), (8, 7, 8)]],
                   order=[0, 1, 0, 2, 3]),
    "boss": dict(key=9, scale=hm.MINOR, speed=4, busy=True,
                 chords=[[0, 0, 5, 4], [0, 0, 3, 4], [5, 3, 0, 4], [5, 6, 4, 4]],
                 motifs=[[(0, 0, 1), (2, 2, 1), (4, 0, 1), (6, 4, 2), (8, 3, 2), (10, 2, 2), (12, 0, 4)],
                         [(0, 7, 2), (2, 6, 2), (4, 4, 2), (6, 3, 2), (8, 4, 4), (12, 2, 4)],
                         [(0, 4, 1), (1, 4, 1), (2, 7, 2), (4, 4, 2), (8, 3, 2), (10, 2, 2), (12, -1, 4)],
                         [(0, 0, 4), (4, 2, 4), (8, 4, 4), (12, 6, 4)]],
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
                step = [0, 2, 4, 7, 4, 2, 0, 2][(r // 2) % 8]
                c[0] = cell(S_PLUCK, note(deg(ch + step, 2)), 0xC, 28 if r % 4 else 36)
            if sd["busy"]:                                               # the boss: a driving eighth-note bass
                if beat % 2 == 0:
                    c[1] = cell(S_BASS, note(deg(ch + (4 if beat % 8 == 6 else 0), 0)))
            elif beat in (0, 6, 8, 14):                                  # the march: root, fifth, octave hops
                d = {0: 0, 6: 4, 8: 7 if p % 2 else 0, 14: 4}[beat]
                c[1] = cell(S_BASS, note(deg(ch + d, 0)))
            kicks = (0, 6, 8, 14) if sd["busy"] else (0, 8)
            if beat in kicks or (beat == 10 and p == 3):                 # the drums on one channel
                c[2] = cell(S_KICK, note(24), 0xC, 44)
            elif beat in (4, 12):
                c[2] = cell(S_SNARE, note(24), 0xC, 34)
            elif beat % 2 == 0 or sd["busy"]:
                c[2] = cell(S_HAT, note(30), 0xC, 20 if beat % 4 else 26)
            motif = sd["motifs"][3] if bar == 3 and p in (1, 3) else sd["motifs"][(p + bar) % 3]
            for (row, d, ln) in motif:
                if row == beat:
                    c[3] = cell(S_LEAD, note(deg(ch + d, 1)), 0xC, 34)
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
            f.write(build_song(sd, "pricklepatch " + name))


if __name__ == "__main__":
    main()
