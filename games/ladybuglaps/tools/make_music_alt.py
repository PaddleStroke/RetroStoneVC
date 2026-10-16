#!/usr/bin/env python3
"""Ladybug Laps alternative music: two 4-channel ProTracker MODs written with the house music kit
(games/common/tools/house_music.py: the MOD writer, periods and drums; docs/art-direction.md "Audio"), at the house
mix (sample volumes <= 40). The owner picks; the game plays tune.mod (make_music.py).

  alt1  "Dewdrop Dash": a light, skipping garden tune in D mixolydian, speed 6 (125 BPM). The water-drop pluck in
        quick arpeggios, a bouncing bass, the soft pad on each chord, the breathy lead with a sing-song call.
        A (D C G D), A' (the lead an octave up), B (G A Bm A), then again. No drums: the gentlest of the three.
  alt2  "Sugar Rush": a fast, punchy chase in A minor, speed 4 (188 BPM). Kick and snare on every beat pair, hats,
        a galloping bass, the square lead with a tight riff, pluck stabs on the off-beats.
        A (Am F C G), A' (the lead an octave up), B (F G Em Am, a snare roll), then again. The fastest.

    make_music_alt.py              -> ../music-alt/alt1.mod, alt2.mod (previews; the game is unchanged)
    make_music_alt.py OUTDIR N     -> OUTDIR/tune.mod: alternative N (1 or 2), for tools/build_assets.py

To switch, in tools/build_assets.py call make_music_alt.py with OUTDIR and N instead of make_music.py.

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/ladybuglaps/LICENSE.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "common", "tools"))
import house_music as hm  # noqa: E402
from house_music import cell, note  # noqa: E402

_ = None


# ---- alt1: Dewdrop Dash ------------------------------------------------------------------------------------------------
def dewdrop():
    KEY, SC = 2, hm.MIXOLYDIAN                       # D mixolydian
    PAD, BASS, DROP, LEAD = 1, 2, 3, 4

    def deg(d, o):
        return hm.deg(d, o, KEY, SC)
    tune_a = [[4, _, 2, 4, 5, _, 4, _], [6, _, 5, _, 4, _, _, _], [3, _, 4, 3, 2, _, 0, _], [1, _, 2, _, 0, _, _, _]]
    tune_b = [[3, _, 4, _, 5, _, 7, _], [4, _, 5, _, 6, _, 4, _], [5, _, 4, _, 2, _, 4, _], [4, _, _, _, _, _, _, _]]
    ch_a, ch_b = [0, 6, 3, 0], [3, 4, 5, 4]

    def pat(tune, chords, up=0):
        rows = []
        for r in range(64):
            bar, st = r // 16, r % 16
            ch = chords[bar]
            c = [cell(), cell(), cell(), cell()]
            if st % 2 == 0:                          # quick arpeggios
                c[0] = cell(DROP, note(deg(ch + [0, 2, 4, 7][(st // 2) % 4], 2)), 0xC, 16 if st % 4 else 22)
            if st in (0, 6, 8, 14):                  # a bouncing bass
                c[1] = cell(BASS, note(deg(ch + (4 if st in (6, 14) else 0), 0)), 0xC, 36 if st == 0 else 26)
            if st == 0:
                c[2] = cell(PAD, note(deg(ch + 2, 1)), 0xC, 22)
            if st % 2 == 0:
                d = tune[bar][st // 2]
                if d is not None:
                    c[3] = cell(LEAD, note(deg(d + up, 1)), 0xC, 40)
            rows.append(b"".join(c))
        return b"".join(rows)
    smp = hm.instruments(seed=31)
    pats = [pat(tune_a, ch_a), pat(tune_a, ch_a, up=7), pat(tune_b, ch_b)]
    return hm.build_mod("ladybuglaps alt1", smp, pats, [0, 1, 2], speed=6)


# ---- alt2: Sugar Rush ------------------------------------------------------------------------------------------------------
def sugar():
    KEY, SC = 9, hm.MINOR                            # A minor
    SQ, BAS, KICK, HAT, SNR, DROP = 1, 2, 3, 4, 5, 6

    def deg(d, o):
        return hm.deg(d, o, KEY, SC)
    tune_a = [[4, 4, _, 2, 4, _, 6, _], [5, _, 4, _, 2, _, _, _], [2, 2, _, 4, 6, _, 7, _], [6, _, 4, _, 4, _, _, _]]
    tune_b = [[5, _, 7, _, 9, _, 7, 5], [6, _, 8, _, 9, _, 8, 6], [4, _, 6, _, 7, _, 6, 4], [4, 5, 6, _, 7, _, _, _]]
    ch_a, ch_b = [0, 5, 2, 6], [5, 6, 4, 0]

    def pat(tune, chords, up=0, roll=False):
        rows = []
        for r in range(64):
            bar, st = r // 16, r % 16
            ch = chords[bar]
            c = [cell(), cell(), cell(), cell()]
            if st % 4 in (0, 3):                     # a galloping bass
                c[1] = cell(BAS, note(deg(ch, 0 if st % 8 == 0 else 1)), 0xC, 38 if st % 8 == 0 else 26)
            if st % 8 == 0:
                c[2] = cell(KICK, note(12), 0xC, 34)
            elif st % 8 == 4:
                c[2] = cell(SNR, note(24), 0xC, 22)
            elif st % 2 == 0:
                c[2] = cell(HAT, note(30), 0xC, 12)
            if roll and bar == 3 and st >= 8:
                c[2] = cell(SNR, note(24), 0xC, 8 + st)
            if st % 4 == 2:                          # pluck stabs
                c[0] = cell(DROP, note(deg(ch + 4, 1)), 0xC, 18)
            if st % 2 == 0:
                d = tune[bar][st // 2]
                if d is not None:
                    c[3] = cell(SQ, note(deg(d + up, 1)), 0xC, 30)
            rows.append(b"".join(c))
        return b"".join(rows)
    smp = [("square", hm.square_lead(0.25), 30, 0, 32), ("bass", hm.bass(), 38, 0, 64), ("kick", hm.kick(), 34, 0, 0),
           ("hat", hm.hat(), 12, 0, 0), ("snare", hm.snare(), 22, 0, 0), ("drop", hm.drop(), 30, 0, 0)]
    pats = [pat(tune_a, ch_a), pat(tune_a, ch_a, up=7), pat(tune_b, ch_b, roll=True)]
    return hm.build_mod("ladybuglaps alt2", smp, pats, [0, 1, 2], speed=4)


def main():
    if len(sys.argv) >= 3:
        os.makedirs(sys.argv[1], exist_ok=True)
        with open(os.path.join(sys.argv[1], "tune.mod"), "wb") as f:
            f.write(dewdrop() if sys.argv[2] == "1" else sugar())
        return
    out = os.path.join(HERE, "..", "music-alt")
    os.makedirs(out, exist_ok=True)
    for name, fn in (("alt1.mod", dewdrop), ("alt2.mod", sugar)):
        with open(os.path.join(out, name), "wb") as f:
            f.write(fn())
        print("wrote", os.path.normpath(os.path.join(out, name)))


if __name__ == "__main__":
    main()
