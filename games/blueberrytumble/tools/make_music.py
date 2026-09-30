#!/usr/bin/env python3
"""Blueberry Tumble music: one 4-channel ProTracker MOD per biome (and a night variant of each) with the house
instrument set and the house drums (games/common/tools/house_music.py; docs/art-direction.md "Audio").

Every track is a 24-bar song (6 patterns of 4 bars, 4 rows per beat): intro, A, B, A', bridge, build. A biome
lasts exactly 24 bars, so each biome's music starts on its gate's first beat and ends with it: the music evolves
down the mountain. The tempo is the biome's tier (src/tuning.h TIER_MOD_TEMPO / TIER_MOD_SPEED, BPM = 6 T / S):
the scroll covers one beat (4 cells) per beat, so the obstacles fall on the music's grid.

Channels: 0 the plucked arpeggio, 1 the bass (every beat), 2 the drums (a hit on every beat: the beat is always
clear), 3 the lead tune (the pad in the intro and the bridge).

    make_music.py OUTDIR       -> OUTDIR/summit.mod forest.mod meadow.mod village.mod night_*.mod

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/blueberrytumble/LICENSE.
"""
import os
import random
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "common", "tools"))
import house_music as hm  # noqa: E402
from house_music import cell, note  # noqa: E402


def tuning_list(name):
    """The tier table from src/tuning.h (one place for the tempos)."""
    src = open(os.path.join(HERE, "..", "src", "tuning.h")).read()
    m = re.search(r"#define\s+%s\s+\{([^}]*)\}" % name, src)
    return [int(v) for v in m.group(1).split(",")]


TEMPO = tuning_list("TIER_MOD_TEMPO")
SPEED = tuning_list("TIER_MOD_SPEED")

# samples: 1 pad, 2 bass, 3 drop (pluck), 4 lead, 5 kick, 6 snare, 7 hat, 8 square lead
S_PAD, S_BASS, S_DROP, S_LEAD, S_KICK, S_SNARE, S_HAT, S_SQUARE = 1, 2, 3, 4, 5, 6, 7, 8

# per biome: key, scale, chords (scale degrees, one per bar), the lead instrument, a seed, the name
TRACKS = [
    dict(name="summit", key=2, scale=hm.MAJOR, chords=[0, 4, 5, 3], lead=S_LEAD, seed=11),         # D major, airy
    dict(name="forest", key=9, scale=hm.DORIAN, chords=[0, 6, 3, 4], lead=S_DROP, seed=23),        # A dorian, woody
    dict(name="meadow", key=7, scale=hm.MAJOR, chords=[0, 3, 4, 0], lead=S_SQUARE, seed=37),       # G major, bouncy
    dict(name="village", key=0, scale=hm.MIXOLYDIAN, chords=[0, 6, 3, 4], lead=S_SQUARE, seed=41), # C mixolydian
]
NIGHT = [
    dict(name="night_summit", key=9, scale=hm.MINOR, chords=[0, 5, 2, 6], lead=S_LEAD, seed=53),
    dict(name="night_forest", key=4, scale=hm.MINOR, chords=[0, 3, 6, 4], lead=S_DROP, seed=61),
    dict(name="night_meadow", key=2, scale=hm.DORIAN, chords=[0, 3, 4, 6], lead=S_SQUARE, seed=67),
    dict(name="night_village", key=5, scale=hm.MINOR, chords=[0, 5, 3, 4], lead=S_SQUARE, seed=71),
]


def samples():
    s = hm.instruments(seed=5)                              # pad 22, bass 40, drop 34, lead 20 (the house mix)
    return s + [("kick", hm.kick(), 40, 0, 0), ("snare", hm.snare(), 28, 0, 0), ("hat", hm.hat(), 16, 0, 0),
                ("square", hm.square_lead(0.25), 18, 0, 32)]


def motif(rng):
    """A one-bar motif: (row in the bar, scale degree offset, accent) on 8th and 16th notes."""
    rows = sorted(rng.sample([0, 2, 4, 6, 8, 10, 12, 14], rng.choice([4, 5, 6])))
    if 0 not in rows:
        rows = [0] + rows[1:]
    out, d = [], 0
    for r in rows:
        d = max(-2, min(5, d + rng.choice([-2, -1, 1, 2, 0])))
        out.append((r, d, 44 if r % 4 == 0 else 34))
    if rng.random() < 0.5:
        out.append((15, d + 1, 26))
    return out


def pattern(t, section, rng, motifs, tier):
    """64 rows: section 0 intro, 1 A, 2 B, 3 A', 4 bridge, 5 build."""
    key, scale, chords = t["key"], t["scale"], t["chords"]

    def deg(d, octave):
        return hm.deg(d, octave, key, scale)

    rows = []
    for r in range(64):
        bar, rb = r // 16, r % 16
        ch = chords[bar] if section != 4 else chords[(bar + 2) % 4]
        c = [cell(), cell(), cell(), cell()]
        # drums: a hit on every beat, kick / snare, hats on the offbeats (the intro: kick on every beat)
        if rb % 4 == 0:
            if section == 0 or section == 4:
                c[2] = cell(S_KICK, note(24), 0xC, 44 if rb == 0 else 34)
            else:
                c[2] = cell(S_SNARE if rb in (4, 12) else S_KICK, note(24), 0xC, 40)
        elif rb % 2 == 0 and section != 4:
            c[2] = cell(S_HAT, note(31), 0xC, 30 if rb % 4 == 2 else 20)
        elif section == 5 and bar == 3 and rb % 2 == 1:
            c[2] = cell(S_SNARE, note(24), 0xC, 16 + rb * 2)     # the build: a snare roll into the next biome
        # bass: the root on the beat, the octave or the fifth between
        if section != 0 or bar >= 2:
            if rb % 4 == 0:
                c[1] = cell(S_BASS, note(deg(ch, 0)), 0xC, 40)
            elif rb % 4 == 2 and section in (1, 2, 3, 5):
                c[1] = cell(S_BASS, note(deg(ch + (4 if rb == 10 else 7), 0)), 0xC, 26)
        # the plucked arpeggio: chord tones on the 8ths (16ths in the build)
        step = 1 if section == 5 else 2
        if section != 0 and rb % step == 0:
            tone = [0, 2, 4, 2, 7, 4, 2, 4][(rb // step) % 8]
            c[0] = cell(S_DROP, note(deg(ch + tone, 2)), 0xC, 30 if rb % 4 else 38)
        # the lead: the motif of the section (a variation in A', a cadence at the end of the phrase); the pad in the
        # intro and the bridge
        if section in (0, 4):
            if rb == 0:
                c[3] = cell(S_PAD, note(deg(ch + 2, 1)), 0xC, 30)
            elif rb == 8 and section == 4:
                c[3] = cell(S_PAD, note(deg(ch + 4, 1)), 0xC, 24)
        else:
            m = motifs[0 if section in (1, 3) else 1]
            for (mr, md, acc) in m:
                if mr == rb:
                    d = ch + md + (1 if section == 3 and bar == 1 else 0)
                    if bar == 3 and rb >= 12:
                        d = 0 + (7 if section != 5 else 4)                 # land on the tonic (or the fifth)
                    c[3] = cell(t["lead"], note(deg(d, 1 if t["lead"] != S_DROP else 2)), 0xC, acc)
        if r == 0 and section == 0:
            # the tempo (channel 1) and the speed (channel 3) of the tier, on the song's first row
            c[1] = bytes(c[1][:2]) + bytes([(c[1][2] & 0xF0) | 0xF, TEMPO[tier]])
            c[3] = bytes(c[3][:2]) + bytes([(c[3][2] & 0xF0) | 0xF, SPEED[tier]])
        rows.append(b"".join(c))
    return b"".join(rows)


def build(t, tier):
    rng = random.Random(t["seed"])
    motifs = [motif(rng), motif(rng)]
    pats = [pattern(t, s, rng, motifs, tier) for s in range(6)]
    return hm.build_mod(t["name"], samples(), pats, [0, 1, 2, 3, 4, 5])


def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else "."
    os.makedirs(outdir, exist_ok=True)
    for b, t in enumerate(TRACKS):
        with open(os.path.join(outdir, t["name"] + ".mod"), "wb") as f:
            f.write(build(t, b))
    for t in NIGHT:
        with open(os.path.join(outdir, t["name"] + ".mod"), "wb") as f:
            f.write(build(t, len(TEMPO) - 1))


if __name__ == "__main__":
    main()
