#!/usr/bin/env python3
"""Gopher Dig alternative music: two 4-channel ProTracker MODs written with the house music kit
(games/common/tools/house_music.py: the MOD writer, periods and drums; docs/art-direction.md "Audio"), at the house
mix (sample volumes <= 40). The owner picks; the game plays tune.mod (make_music.py).

  alt1  "Tunnel Stroll": a calm stroll through the burrows in D dorian, speed 7 (107 BPM), no drums. The pad holds the
        chords, the pluck drips down the walls, a walking bass, the breathy lead hums a lazy tune.
        A (Dm G Dm G), A' (the lead an octave up), B (Bb C Am Dm), then again.
  alt2  "Lava Shuffle": a warm, swinging tune in E minor, speed 6 (125 BPM). Kick and snare in a shuffle, a walking
        bass, the square lead with a bluesy hook, the pluck bubbling like hot rocks.
        A (Em Am Em B), A' (the hook up), B (C D Em Em, a snare fill), then again.

    make_music_alt.py              -> ../music-alt/alt1.mod, alt2.mod (previews; the game is unchanged)
    make_music_alt.py OUTDIR N     -> OUTDIR/tune.mod: alternative N (1 or 2), for tools/build_assets.py

To switch, in tools/build_assets.py call make_music_alt.py with OUTDIR and N instead of make_music.py.

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/gopherdig/LICENSE.
"""
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "common", "tools"))
import house_music as hm  # noqa: E402
from house_music import cell, note  # noqa: E402

_ = None


# ---- alt1: Tunnel Stroll -------------------------------------------------------------------------------------------------
def stroll():
    KEY, SC = 2, hm.DORIAN
    PAD, BASS, DROP, LEAD = 1, 2, 3, 4

    def deg(d, o):
        return hm.deg(d, o, KEY, SC)
    tune_a = [[4, _, 3, _, 2, _, _, _], [3, _, 4, _, 6, _, _, _], [4, _, 2, _, 0, _, 2, _], [1, _, _, _, _, _, _, _]]
    tune_b = [[5, _, _, 4, 2, _, 4, _], [6, _, _, 5, 4, _, _, _], [4, _, 2, _, 4, _, 5, _], [0, _, _, _, _, _, _, _]]
    ch_a, ch_b = [0, 3, 0, 3], [5, 6, 4, 0]

    def pat(tune, chords, up=0):
        rows = []
        for r in range(64):
            bar, st = r // 16, r % 16
            ch = chords[bar]
            c = [cell(), cell(), cell(), cell()]
            if st % 4 == 1:                          # drips down the tunnel walls
                c[0] = cell(DROP, note(deg(ch + [9, 7, 4, 2][st // 4], 2)), 0xC, 18)
            if st in (0, 4, 8, 12):                  # a walking bass
                c[1] = cell(BASS, note(deg(ch + [0, 2, 4, 2][st // 4], 0)), 0xC, 34 if st == 0 else 24)
            if st == 0:
                c[2] = cell(PAD, note(deg(ch + 2, 1)), 0xC, 24)
            if st % 2 == 0:
                d = tune[bar][st // 2]
                if d is not None:
                    c[3] = cell(LEAD, note(deg(d + up, 1)), 0xC, 40)
            rows.append(b"".join(c))
        return b"".join(rows)
    smp = hm.instruments(seed=31)
    pats = [pat(tune_a, ch_a), pat(tune_a, ch_a, up=7), pat(tune_b, ch_b)]
    return hm.build_mod("gopherdig alt1", smp, pats, [0, 1, 2], speed=7)


# ---- alt2: Lava Shuffle ---------------------------------------------------------------------------------------------------
def shuffle():
    KEY, SC = 4, hm.MINOR
    SQ, BAS, KICK, HAT, SNR, DROP = 1, 2, 3, 4, 5, 6

    def deg(d, o):
        return hm.deg(d, o, KEY, SC)
    tune_a = [[0, _, 2, 3, 4, _, 3, _], [3, _, 2, _, 0, _, _, _], [0, _, 2, 3, 4, _, 6, _], [4, 3, 1, _, _, _, _, _]]
    tune_b = [[2, _, 4, _, 5, 4, 2, _], [3, _, 5, _, 6, 5, 3, _], [4, _, 2, 0, 2, _, 4, _], [0, _, _, _, _, _, _, _]]
    ch_a, ch_b = [0, 3, 0, 4], [5, 6, 0, 0]
    rng = random.Random(4)

    def pat(tune, chords, up=0, fill=False):
        rows = []
        for r in range(64):
            bar, st = r // 16, r % 16
            ch = chords[bar]
            c = [cell(), cell(), cell(), cell()]
            if st in (0, 3, 6, 8, 11, 14):           # a walking, swinging bass
                c[1] = cell(BAS, note(deg(ch + [0, 2, 4, 0, 4, 2][[0, 3, 6, 8, 11, 14].index(st)], 0)), 0xC,
                            36 if st in (0, 8) else 24)
            if st in (0, 6, 8):
                c[2] = cell(KICK, note(12), 0xC, 32)
            elif st in (4, 12):
                c[2] = cell(SNR, note(24), 0xC, 22)
            elif st in (3, 7, 11, 15):
                c[2] = cell(HAT, note(30), 0xC, 12)
            if fill and bar == 3 and st >= 8 and st % 2:
                c[2] = cell(SNR, note(24), 0xC, 10 + st)
            if st % 8 == 5 and rng.random() < 0.8:   # hot rocks bubbling
                c[0] = cell(DROP, note(deg(ch + 7 + rng.choice([0, 2, 4]), 1)), 0xC, 16)
            if st % 2 == 0:
                d = tune[bar][st // 2]
                if d is not None:
                    c[3] = cell(SQ, note(deg(d + up, 1)), 0xC, 28)
            rows.append(b"".join(c))
        return b"".join(rows)
    smp = [("square", hm.square_lead(0.5), 26, 0, 32), ("bass", hm.bass(), 38, 0, 64), ("kick", hm.kick(), 34, 0, 0),
           ("hat", hm.hat(), 12, 0, 0), ("snare", hm.snare(), 22, 0, 0), ("drop", hm.drop(), 28, 0, 0)]
    pats = [pat(tune_a, ch_a), pat(tune_a, ch_a, up=7), pat(tune_b, ch_b, fill=True)]
    return hm.build_mod("gopherdig alt2", smp, pats, [0, 1, 2], speed=6)


def main():
    if len(sys.argv) >= 3:
        os.makedirs(sys.argv[1], exist_ok=True)
        with open(os.path.join(sys.argv[1], "tune.mod"), "wb") as f:
            f.write(stroll() if sys.argv[2] == "1" else shuffle())
        return
    out = os.path.join(HERE, "..", "music-alt")
    os.makedirs(out, exist_ok=True)
    for name, fn in (("alt1.mod", stroll), ("alt2.mod", shuffle)):
        with open(os.path.join(out, name), "wb") as f:
            f.write(fn())
        print("wrote", os.path.normpath(os.path.join(out, name)))


if __name__ == "__main__":
    main()
