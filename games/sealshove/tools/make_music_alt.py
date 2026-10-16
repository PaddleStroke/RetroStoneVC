#!/usr/bin/env python3
"""Seal Shove alternative music: two 4-channel ProTracker MODs written with the house music kit
(games/common/tools/house_music.py: the MOD writer, periods and drums; docs/art-direction.md "Audio"), at the house
mix (sample volumes <= 40). The owner picks; the game plays tune.mod (make_music.py).

  alt1  "Snowdrift Lullaby": a gentle, floating tune in C major, speed 7 (107 BPM). The pad holding soft chords, the
        pluck falling like snowflakes, a slow bass, the breathy lead singing long notes. A (C Am F G), A' (the lead an
        octave up), B (F G Em Am), then again.
  alt2  "Penguin Parade": a jaunty little march in F major, speed 6 (125 BPM). A snare roll, a marching bass on the
        beats, the pluck on the off-beats, the square lead whistling a strutting tune. A (F Dm Bb C), A' (an octave
        up), B (Bb C Am Dm, a drum fill), then again.

    make_music_alt.py              -> ../music-alt/alt1.mod, alt2.mod (previews; the game is unchanged)
    make_music_alt.py OUTDIR N     -> OUTDIR/tune.mod: alternative N (1 or 2), for tools/build_assets.py

To switch, in tools/build_assets.py call make_music_alt.py with OUTDIR and N instead of make_music.py.

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/sealshove/LICENSE.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "common", "tools"))
import house_music as hm  # noqa: E402
from house_music import cell, note  # noqa: E402

_ = None


# ---- alt1: Snowdrift Lullaby ----------------------------------------------------------------------------------------------
def lullaby():
    KEY, SC = 0, hm.MAJOR                            # C major
    PAD, BASS, DROP, LEAD = 1, 2, 3, 4

    def deg(d, o):
        return hm.deg(d, o, KEY, SC)
    tune_a = [[4, _, _, _, 2, _, 4, _], [5, _, _, _, _, _, 4, _], [3, _, _, 5, 4, _, 3, _], [1, _, _, _, _, _, _, _]]
    tune_b = [[5, _, _, 4, 3, _, _, _], [6, _, _, 5, 4, _, _, _], [4, _, 2, _, 1, _, 2, _], [0, _, _, _, _, _, _, _]]
    ch_a, ch_b = [0, 5, 3, 4], [3, 4, 2, 5]

    def pat(tune, chords, up=0):
        rows = []
        for r in range(64):
            bar, st = r // 16, r % 16
            ch = chords[bar]
            c = [cell(), cell(), cell(), cell()]
            if st % 4 == 0:                          # snowflakes: the pluck falls down the chord
                c[0] = cell(DROP, note(deg(ch + [9, 7, 4, 2][st // 4], 2)), 0xC, 18 if st else 24)
            if st in (0, 8):
                c[1] = cell(BASS, note(deg(ch + (0 if st == 0 else 4), 0)), 0xC, 34 if st == 0 else 24)
            if st == 0:
                c[2] = cell(PAD, note(deg(ch + 2, 1)), 0xC, 26)
            elif st == 8:
                c[2] = cell(PAD, note(deg(ch + 4, 1)), 0xC, 20)
            if st % 2 == 0:
                d = tune[bar][st // 2]
                if d is not None:
                    c[3] = cell(LEAD, note(deg(d + up, 1)), 0xC, 40)
            rows.append(b"".join(c))
        return b"".join(rows)
    smp = hm.instruments(seed=31)
    pats = [pat(tune_a, ch_a), pat(tune_a, ch_a, up=7), pat(tune_b, ch_b)]
    return hm.build_mod("sealshove alt1", smp, pats, [0, 1, 2], speed=7)


# ---- alt2: Penguin Parade --------------------------------------------------------------------------------------------------
def parade():
    KEY, SC = 5, hm.MAJOR                            # F major
    SQ, BAS, KICK, HAT, SNR, DROP = 1, 2, 3, 4, 5, 6

    def deg(d, o):
        return hm.deg(d, o, KEY, SC)
    tune_a = [[0, _, 0, 2, 4, _, 2, 0], [5, _, 4, _, 2, _, _, _], [3, _, 3, 5, 6, _, 5, 3], [4, _, 1, _, 4, _, _, _]]
    tune_b = [[3, _, 5, 3, 1, _, 3, _], [4, _, 6, 4, 2, _, 4, _], [2, _, 4, 2, 0, _, 2, _], [5, 4, 3, _, 1, _, _, _]]
    ch_a, ch_b = [0, 5, 3, 4], [3, 4, 2, 5]

    def pat(tune, chords, up=0, fill=False):
        rows = []
        for r in range(64):
            bar, st = r // 16, r % 16
            ch = chords[bar]
            c = [cell(), cell(), cell(), cell()]
            if st % 4 == 0:                          # the marching bass
                c[1] = cell(BAS, note(deg(ch + (0 if st % 8 == 0 else 4), 0)), 0xC, 38 if st % 8 == 0 else 28)
            if st in (0, 8):
                c[2] = cell(KICK, note(12), 0xC, 30)
            elif st in (4, 12):
                c[2] = cell(SNR, note(24), 0xC, 22)
            elif st % 2:
                c[2] = cell(SNR, note(24), 0xC, 8)  # the roll
            if fill and bar == 3 and st >= 8:
                c[2] = cell(SNR, note(24), 0xC, 8 + st * 2)
            if st % 4 == 2:
                c[0] = cell(DROP, note(deg(ch + 2, 2)), 0xC, 18)
            if st % 2 == 0:
                d = tune[bar][st // 2]
                if d is not None:
                    c[3] = cell(SQ, note(deg(d + up, 1)), 0xC, 30)
            rows.append(b"".join(c))
        return b"".join(rows)
    smp = [("square", hm.square_lead(0.125), 28, 0, 32), ("bass", hm.bass(), 38, 0, 64), ("kick", hm.kick(), 30, 0, 0),
           ("hat", hm.hat(), 12, 0, 0), ("snare", hm.snare(), 22, 0, 0), ("drop", hm.drop(), 30, 0, 0)]
    pats = [pat(tune_a, ch_a), pat(tune_a, ch_a, up=7), pat(tune_b, ch_b, fill=True)]
    return hm.build_mod("sealshove alt2", smp, pats, [0, 1, 2], speed=6)


def main():
    if len(sys.argv) >= 3:
        os.makedirs(sys.argv[1], exist_ok=True)
        with open(os.path.join(sys.argv[1], "tune.mod"), "wb") as f:
            f.write(lullaby() if sys.argv[2] == "1" else parade())
        return
    out = os.path.join(HERE, "..", "music-alt")
    os.makedirs(out, exist_ok=True)
    for name, fn in (("alt1.mod", lullaby), ("alt2.mod", parade)):
        with open(os.path.join(out, name), "wb") as f:
            f.write(fn())
        print("wrote", os.path.normpath(os.path.join(out, name)))


if __name__ == "__main__":
    main()
