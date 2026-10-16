#!/usr/bin/env python3
"""Gecko Golf alternative music: two 4-channel ProTracker MODs written with the house music kit
(games/common/tools/house_music.py: the MOD writer, periods and drums; docs/art-direction.md "Audio"), at the house
mix (sample volumes <= 40). The owner picks; the game plays tune.mod (make_music.py).

  alt1  "Siesta Green": a sleepy afternoon sway in D dorian, speed 9 (83 BPM). The pad breathing long chords, the
        pluck dripping slowly down, a bass on the bar and its fifth, the lead humming long notes with a lazy turn.
        A (Dm7 G Dm7 G), A' (the lead an octave up), B (Bbmaj7 C Am Dm), then again.
  alt2  "Tee Time Shuffle": a light shuffle in F major, speed 6 (125 BPM), soft brushes (a kick, a quiet snare,
        hats on the swung off-beats), a hopping bass, the soft square lead whistling a jaunty call, the pluck
        answering. A (F Dm Gm C), A' (the lead an octave up), B (Bb C Am Dm, a snare fill), then again.

    make_music_alt.py              -> ../music-alt/alt1.mod, alt2.mod (previews; the game is unchanged)
    make_music_alt.py OUTDIR N     -> OUTDIR/tune.mod: alternative N (1 or 2), for tools/build_assets.py

To switch, in tools/build_assets.py call make_music_alt.py with OUTDIR and N instead of make_music.py.

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/geckogolf/LICENSE.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "common", "tools"))
import house_music as hm  # noqa: E402
from house_music import cell, note  # noqa: E402

_ = None


# ---- alt1: Siesta Green -------------------------------------------------------------------------------------------------
def siesta():
    KEY, SC = 2, hm.DORIAN                           # D dorian
    PAD, BASS, DROP, LEAD = 1, 2, 3, 4

    def deg(d, o):
        return hm.deg(d, o, KEY, SC)
    tune_a = [[4, _, _, _, 2, _, 3, _], [4, _, _, _, _, _, _, _], [6, _, 5, _, 4, _, 2, _], [3, _, _, _, 1, _, _, _]]
    tune_b = [[5, _, _, 4, 2, _, _, _], [6, _, _, 5, 4, _, _, _], [4, _, 2, _, 0, _, 1, _], [0, _, _, _, _, _, _, _]]
    ch_a, ch_b = [0, 3, 0, 3], [5, 6, 4, 0]

    def pat(tune, chords, up=0):
        rows = []
        for r in range(64):
            bar, st = r // 16, r % 16
            ch = chords[bar]
            c = [cell(), cell(), cell(), cell()]
            if st % 4 == 0:                          # the pluck drips slowly down the chord
                a = ch + [9, 7, 4, 2][st // 4]
                c[0] = cell(DROP, note(deg(a, 1)), 0xC, 18)
            if st in (0, 8):                         # the bass: the bar and its fifth
                c[1] = cell(BASS, note(deg(ch + (4 if st else 0), 0)), 0xC, 34 if st == 0 else 24)
            if st == 0:
                c[2] = cell(PAD, note(deg(ch + 2, 1)), 0xC, 24)
            elif st == 8:
                c[2] = cell(PAD, note(deg(ch + 4, 1)), 0xC, 18)
            if st % 2 == 0:
                d = tune[bar][st // 2]
                if d is not None:
                    c[3] = cell(LEAD, note(deg(d + up, 1)), 0xC, 38)
            rows.append(b"".join(c))
        return b"".join(rows)
    smp = hm.instruments(seed=23)
    pats = [pat(tune_a, ch_a), pat(tune_a, ch_a, up=7), pat(tune_b, ch_b)]
    return hm.build_mod("geckogolf alt1", smp, pats, [0, 1, 2], speed=9)


# ---- alt2: Tee Time Shuffle ---------------------------------------------------------------------------------------------
def shuffle():
    KEY, SC = 5, hm.MAJOR                            # F major
    SQ, BAS, KICK, HAT, SNR, DROP = 1, 2, 3, 4, 5, 6

    def deg(d, o):
        return hm.deg(d, o, KEY, SC)
    tune_a = [[4, _, 5, 4, 2, _, _, 4], [5, _, 4, 2, 0, _, _, _], [3, _, 4, 3, 1, _, _, 3], [4, _, 2, _, 4, _, _, _]]
    tune_b = [[3, _, _, 5, 7, _, 5, _], [4, _, _, 6, 8, _, 7, _], [6, 5, 4, _, 2, _, 4, _], [5, _, _, _, 4, _, _, _]]
    ch_a, ch_b = [0, 5, 1, 4], [3, 4, 2, 5]

    def pat(tune, chords, up=0, fill=False):
        rows = []
        for r in range(64):
            bar, st = r // 16, r % 16
            ch = chords[bar]
            c = [cell(), cell(), cell(), cell()]
            if st % 4 == 0 or st % 4 == 3:           # a hopping bass (the swung pickup)
                c[1] = cell(BAS, note(deg(ch, 0 if st % 8 == 0 else 1)), 0xC, 36 if st % 8 == 0 else 22)
            if st in (0, 8):
                c[2] = cell(KICK, note(12), 0xC, 30)
            elif st in (4, 12):
                c[2] = cell(SNR, note(24), 0xC, 14)
            elif st % 4 == 3:
                c[2] = cell(HAT, note(30), 0xC, 10)
            if fill and bar == 3 and st >= 10 and st % 2:
                c[2] = cell(SNR, note(24), 0xC, 8 + st)
            if st % 4 == 2:                          # the pluck answers on the off-beats
                c[0] = cell(DROP, note(deg(ch + 4, 1)), 0xC, 16)
            if st % 2 == 0 or st % 4 == 3:
                d = tune[bar][st // 2] if st % 2 == 0 else None
                if d is not None:
                    c[3] = cell(SQ, note(deg(d + up, 1)), 0xC, 26)
            rows.append(b"".join(c))
        return b"".join(rows)
    smp = [("square", hm.square_lead(0.25), 26, 0, 32), ("bass", hm.bass(), 36, 0, 64), ("kick", hm.kick(), 30, 0, 0),
           ("hat", hm.hat(), 10, 0, 0), ("snare", hm.snare(), 14, 0, 0), ("drop", hm.drop(), 28, 0, 0)]
    pats = [pat(tune_a, ch_a), pat(tune_a, ch_a, up=7), pat(tune_b, ch_b, fill=True)]
    return hm.build_mod("geckogolf alt2", smp, pats, [0, 1, 2], speed=6)


def main():
    if len(sys.argv) >= 3:
        os.makedirs(sys.argv[1], exist_ok=True)
        with open(os.path.join(sys.argv[1], "tune.mod"), "wb") as f:
            f.write(siesta() if sys.argv[2] == "1" else shuffle())
        return
    out = os.path.join(HERE, "..", "music-alt")
    os.makedirs(out, exist_ok=True)
    for name, fn in (("alt1.mod", siesta), ("alt2.mod", shuffle)):
        with open(os.path.join(out, name), "wb") as f:
            f.write(fn())
        print("wrote", os.path.normpath(os.path.join(out, name)))


if __name__ == "__main__":
    main()
