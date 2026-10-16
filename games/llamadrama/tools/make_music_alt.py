#!/usr/bin/env python3
"""Llama Drama alternative music: two 4-channel ProTracker MODs written with the house music kit
(games/common/tools/house_music.py: the MOD writer, periods and drums; docs/art-direction.md "Audio"), at the house
mix (sample volumes <= 40). The owner picks; the game plays tune.mod (make_music.py).

  alt1  "Condor Glide": a slow, airy Andean air in D dorian, speed 5 (125 BPM, half-time feel). The quena (the breathy
        house lead) sings long notes and sighs down the scale, the house pad holds the chords, the pluck rolls gentle
        broken chords like a harp, the bombo only on the first beat. A (Dm C Dm Am), B (F C G Am), A, C (Bb F G A).
  alt2  "Altiplano Hop": a quick, bouncy sanjuanito-like dance in E minor, speed 3 (125 BPM). An oom-pah bass, the
        bombo on every beat and a snare on the off-beats, a soft square lead hopping in short notes, the pluck strumming
        chords (the arpeggio effect). A (Em D G Em), A' (the lead an octave up), B (C G D Em), then again.

    make_music_alt.py              -> ../music-alt/alt1.mod, alt2.mod (previews; the game is unchanged)
    make_music_alt.py OUTDIR N     -> OUTDIR/tune.mod: alternative N (1 or 2), for tools/build_assets.py

To switch, in tools/build_assets.py call make_music_alt.py with OUTDIR and N instead of make_music.py.

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/llamadrama/LICENSE.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "common", "tools"))
import house_music as hm  # noqa: E402
from house_music import cell, note  # noqa: E402

_ = None


def rows_to_pattern(chans):
    return b"".join(b"".join(chans[k].get(r, cell()) for k in range(4)) for r in range(64))


# ---- alt1: Condor Glide ----------------------------------------------------------------------------------------------
def condor():
    KEY, SC = 2, hm.DORIAN                           # D dorian
    PAD, BASS, DROP, LEAD, KICK = 1, 2, 3, 4, 5
    smp = hm.instruments(seed=3) + [("bombo", hm.kick(), 30, 0, 0)]

    def dg(d, o):
        return hm.deg(d, o, KEY, SC)
    # the quena: four notes a bar (16 rows), None holds
    A = [[4, _, 3, 2], [1, _, _, 3], [4, 6, 5, 4], [2, _, _, _]]
    B = [[5, _, 4, 3], [4, _, 2, _], [6, _, 5, 4], [4, _, _, _]]
    C = [[7, _, 6, 5], [4, _, 5, _], [6, 5, 4, 3], [4, _, _, _]]

    def pat(tune, chords):
        ch = [{}, {}, {}, {}]
        for bar in range(4):
            r0, c0 = bar * 16, chords[bar]
            ch[2][r0] = cell(PAD, note(dg(c0, 1)), 0xC, 22)
            ch[1][r0] = cell(BASS, note(dg(c0, 0)), 0xC, 36)
            ch[1][r0 + 10] = cell(BASS, note(dg(c0 + 4, 0)), 0xC, 24)
            for k in range(8):                        # the harp-like broken chord
                ch[0][r0 + k * 2] = cell(DROP, note(dg(c0 + [0, 2, 4, 7, 4, 2, 4, 7][k], 2)), 0xC, 18 if k else 24)
            for k, d in enumerate(tune[bar]):
                if d is not None:
                    ch[3][r0 + k * 4] = cell(LEAD, note(dg(d, 1)), 0xC, 44 if k == 0 else 36)
            if bar % 2 == 0:
                ch[2][r0 + 8] = cell(KICK, note(12), 0xC, 26)
        return rows_to_pattern(ch)

    pats = [pat(A, [0, 6, 0, 4]), pat(B, [2, 6, 3, 4]), pat(C, [5, 2, 3, 4])]
    return hm.build_mod("llamadrama condor", smp, pats, [0, 1, 0, 2], speed=5)


# ---- alt2: Altiplano Hop --------------------------------------------------------------------------------------------
def hop():
    KEY, SC = 4, hm.MINOR                            # E minor
    SQ, BAS, PLK, KICK, SNR = 1, 2, 3, 4, 5
    smp = [("square", hm.square_lead(0.25), 18, 0, 32), ("bass", hm.bass(), 38, 0, 64), ("pluck", hm.drop(), 30, 0, 0),
           ("kick", hm.kick(), 34, 0, 0), ("snare", hm.snare(), 20, 0, 0)]

    def dg(d, o):
        return hm.deg(d, o, KEY, SC)
    # eight 8th notes a bar
    A = [[4, 4, 3, 2, 0, _, 2, _], [1, 1, 2, 3, 4, _, _, _], [6, 6, 4, 3, 2, 3, 4, _], [0, _, 4, _, 0, _, _, _]]
    B = [[2, 2, 4, 6, 7, _, 6, _], [6, 4, 2, 4, 6, _, _, _], [5, 5, 4, 3, 1, 3, 4, _], [4, _, 2, 1, 0, _, _, _]]
    CHORD = {0: 0x37, 6: 0x47, 2: 0x47, 5: 0x47, 1: 0x47}

    def pat(tune, chords, up=0):
        ch = [{}, {}, {}, {}]
        for bar in range(4):
            r0, c0 = bar * 16, chords[bar]
            for b in range(4):                        # oom-pah: the root, then the fifth up
                ch[1][r0 + b * 4] = cell(BAS, note(dg(c0 + (4 if b % 2 else 0), 0)), 0xC, 38 if b % 2 == 0 else 28)
                ch[2][r0 + b * 4] = cell(KICK, note(12), 0xC, 30) if b % 2 == 0 else cell(SNR, note(24), 0xC, 18)
                ch[3][r0 + b * 4 + 2] = cell(PLK, note(dg(c0, 2)), 0x0, CHORD.get(c0 % 7, 0x37))
            for k, d in enumerate(tune[bar]):
                if d is not None:
                    ch[0][r0 + k * 2] = cell(SQ, note(dg(d, 1 + up)), 0xC, 34 if k % 2 == 0 else 26)
        return rows_to_pattern(ch)

    pats = [pat(A, [0, 6, 2, 0]), pat(A, [0, 6, 2, 0], up=1), pat(B, [5, 2, 6, 0])]
    return hm.build_mod("llamadrama hop", smp, pats, [0, 1, 2, 0], speed=3)


ALTS = {1: condor, 2: hop}


def main():
    if len(sys.argv) > 2:
        outdir, n = sys.argv[1], int(sys.argv[2])
        os.makedirs(outdir, exist_ok=True)
        with open(os.path.join(outdir, "tune.mod"), "wb") as f:
            f.write(ALTS[n]())
        return
    outdir = os.path.join(HERE, "..", "music-alt")
    os.makedirs(outdir, exist_ok=True)
    for n, fn in ALTS.items():
        with open(os.path.join(outdir, "alt%d.mod" % n), "wb") as f:
            f.write(fn())
        print("wrote %s" % os.path.normpath(os.path.join(outdir, "alt%d.mod" % n)))


if __name__ == "__main__":
    main()
