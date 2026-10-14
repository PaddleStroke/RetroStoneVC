#!/usr/bin/env python3
"""Fishy Feast alternative music: two 4-channel ProTracker MODs written with the house music kit
(games/common/tools/house_music.py: the MOD writer, periods and drums; docs/art-direction.md "Audio"), at the house
mix (sample volumes <= 40). The owner picks; the game plays tune.mod (make_music.py).

  alt1  "Lazy Lagoon": a dreamy, slow sway in D dorian, speed 8 (94 BPM). The pad breathing long chords, the pluck
        rippling down and up, a slow walking bass, the lead singing long notes with a little turn.
        A (Dm7 G Dm7 G), A' (the lead an octave up), B (Bbmaj7 C Am Dm), then again.
  alt2  "Reef Bounce": a bouncy reef tune in C major, speed 5 (150 BPM). Kick, snare and hats, a hopping octave bass,
        the soft square lead with a skipping call, the pluck sparkling on the off-beats.
        A (C Am F G), A' (the lead an octave up), B (F G Em Am, a snare fill), then again.

    make_music_alt.py              -> ../music-alt/alt1.mod, alt2.mod (previews; the game is unchanged)
    make_music_alt.py OUTDIR N     -> OUTDIR/tune.mod: alternative N (1 or 2), for tools/build_assets.py

To switch, in tools/build_assets.py call make_music_alt.py with OUTDIR and N instead of make_music.py.

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/fishyfeast/LICENSE.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "common", "tools"))
import house_music as hm  # noqa: E402
from house_music import cell, note  # noqa: E402

_ = None


# ---- alt1: Lazy Lagoon --------------------------------------------------------------------------------------------------
def lagoon():
    KEY, SC = 2, hm.DORIAN                           # D dorian
    PAD, BASS, DROP, LEAD = 1, 2, 3, 4

    def deg(d, o):
        return hm.deg(d, o, KEY, SC)
    tune_a = [[4, _, _, _, 3, _, 2, _], [3, _, _, _, _, _, _, _], [4, _, 6, _, 7, _, 6, 4], [3, _, _, _, 1, _, _, _]]
    tune_b = [[5, _, _, 4, 2, _, _, _], [6, _, _, 5, 4, _, _, _], [4, _, 2, _, 0, _, 2, _], [0, _, _, _, _, _, _, _]]
    ch_a, ch_b = [0, 3, 0, 3], [5, 6, 4, 0]

    def pat(tune, chords, up=0):
        rows = []
        for r in range(64):
            bar, st = r // 16, r % 16
            ch = chords[bar]
            c = [cell(), cell(), cell(), cell()]
            if st % 2 == 0:                          # the pluck ripples down then up
                a = ch + [9, 7, 4, 2, 0, 2, 4, 7][st // 2]
                c[0] = cell(DROP, note(deg(a, 2)), 0xC, 16 if st % 4 else 22)
            if st in (0, 6, 12):                     # a slow walking bass
                c[1] = cell(BASS, note(deg(ch + {0: 0, 6: 4, 12: 2}[st], 0)), 0xC, 36 if st == 0 else 26)
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
    smp = hm.instruments(seed=21)
    pats = [pat(tune_a, ch_a), pat(tune_a, ch_a, up=7), pat(tune_b, ch_b)]
    return hm.build_mod("fishyfeast alt1", smp, pats, [0, 1, 2], speed=8)


# ---- alt2: Reef Bounce ---------------------------------------------------------------------------------------------------
def reef():
    KEY, SC = 0, hm.MAJOR                            # C major
    SQ, BAS, KICK, HAT, SNR, DROP = 1, 2, 3, 4, 5, 6

    def deg(d, o):
        return hm.deg(d, o, KEY, SC)
    tune_a = [[4, _, 4, 5, 7, _, 4, _], [5, _, 4, 2, 0, _, _, _], [3, _, 5, 3, 1, _, 3, _], [4, 3, 2, _, 4, _, _, _]]
    tune_b = [[3, _, 5, _, 7, 5, 3, _], [4, _, 6, _, 8, 6, 4, _], [2, _, 4, 2, 0, _, 2, _], [5, _, 6, _, 7, _, _, _]]
    ch_a, ch_b = [0, 5, 3, 4], [3, 4, 2, 5]

    def pat(tune, chords, up=0, fill=False):
        rows = []
        for r in range(64):
            bar, st = r // 16, r % 16
            ch = chords[bar]
            c = [cell(), cell(), cell(), cell()]
            if st % 4 == 0 or st % 4 == 3:           # a hopping octave bass
                c[1] = cell(BAS, note(deg(ch, 0 if st % 8 == 0 else 1)), 0xC, 38 if st % 8 == 0 else 26)
            if st in (0, 8):
                c[2] = cell(KICK, note(12), 0xC, 34)
            elif st in (4, 12):
                c[2] = cell(SNR, note(24), 0xC, 22)
            elif st % 2 == 0:
                c[2] = cell(HAT, note(30), 0xC, 12)
            if fill and bar == 3 and st >= 10 and st % 2:
                c[2] = cell(SNR, note(24), 0xC, 12 + st)
            if st % 4 == 2:                          # the pluck sparkles on the off-beats
                c[0] = cell(DROP, note(deg(ch + 7, 1)), 0xC, 18)
            if st % 2 == 0:
                d = tune[bar][st // 2]
                if d is not None:
                    c[3] = cell(SQ, note(deg(d + up, 1)), 0xC, 30)
            rows.append(b"".join(c))
        return b"".join(rows)
    smp = [("square", hm.square_lead(0.25), 30, 0, 32), ("bass", hm.bass(), 38, 0, 64), ("kick", hm.kick(), 34, 0, 0),
           ("hat", hm.hat(), 12, 0, 0), ("snare", hm.snare(), 22, 0, 0), ("drop", hm.drop(), 30, 0, 0)]
    pats = [pat(tune_a, ch_a), pat(tune_a, ch_a, up=7), pat(tune_b, ch_b, fill=True)]
    return hm.build_mod("fishyfeast alt2", smp, pats, [0, 1, 2], speed=5)


def main():
    if len(sys.argv) >= 3:
        os.makedirs(sys.argv[1], exist_ok=True)
        with open(os.path.join(sys.argv[1], "tune.mod"), "wb") as f:
            f.write(lagoon() if sys.argv[2] == "1" else reef())
        return
    out = os.path.join(HERE, "..", "music-alt")
    os.makedirs(out, exist_ok=True)
    for name, fn in (("alt1.mod", lagoon), ("alt2.mod", reef)):
        with open(os.path.join(out, name), "wb") as f:
            f.write(fn())
        print("wrote", os.path.normpath(os.path.join(out, name)))


if __name__ == "__main__":
    main()
