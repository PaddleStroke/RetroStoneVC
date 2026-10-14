#!/usr/bin/env python3
"""Cosmo Corgi alternative music: two 4-channel ProTracker MODs written with the house music kit
(games/common/tools/house_music.py: the MOD writer, periods and drums; docs/art-direction.md "Audio"), at the house
mix (sample volumes <= 40). The owner picks; the game plays tune.mod (make_music.py).

  alt1  "Nebula Groove": a bouncy space disco in E dorian, speed 6 (125 BPM). Kick and snare, open hats on the
        off-beats, a slap bass jumping octaves, a soft square lead with a call and its echo an octave up.
        A (Em7 A7 Em7 A7), A', B (Cmaj7 D Bm7 Em7), then again.
  alt2  "Comet Galop": a fast chiptune galop in C major, speed 4 (190 BPM). An oom-pah bass, a galloping square
        lead (long-short-short), the house pluck sparkling, a snare on every off-beat and a roll into the turn.
        A (C F G C), A' (the lead an octave up), B (Am F C G), then again.

    make_music_alt.py              -> ../music-alt/alt1.mod, alt2.mod (previews; the game is unchanged)
    make_music_alt.py OUTDIR N     -> OUTDIR/tune.mod: alternative N (1 or 2), for tools/build_assets.py

To switch, in tools/build_assets.py call make_music_alt.py with OUTDIR and N instead of make_music.py.

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/cosmocorgi/LICENSE.
"""
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "common", "tools"))
import house_music as hm  # noqa: E402
from house_music import cell, note, s8  # noqa: E402

_ = None


def slap_bass():
    """A bright, plucky bass: a sine with a decaying second and third harmonic (one-shot)."""
    out = []
    for i in range(2600):
        t = i / 32.0
        v = math.sin(2 * math.pi * t) + 0.6 * math.sin(4 * math.pi * t) * math.exp(-i / 300.0) \
            + 0.3 * math.sin(6 * math.pi * t) * math.exp(-i / 160.0)
        out.append(s8(70 * v * math.exp(-i / 1800.0)))
    return out


def rows_to_pattern(chans):
    return b"".join(b"".join(chans[k].get(r, cell()) for k in range(4)) for r in range(64))


# ---- alt1: Nebula Groove ----------------------------------------------------------------------------------------------
def groove():
    KEY, SC = 4, hm.DORIAN                           # E dorian
    SQ, BAS, KICK, HAT, SNR = 1, 2, 3, 4, 5
    smp = [("square", hm.square_lead(0.5), 18, 0, 32), ("slap", slap_bass(), 38, 0, 0), ("kick", hm.kick(), 36, 0, 0),
           ("hat", hm.hat(), 16, 0, 0), ("snare", hm.snare(), 24, 0, 0)]

    def dg(d, o):
        return hm.deg(d, o, KEY, SC)
    A = [[4, _, 4, 6, _, 4, 2, _], [3, _, _, 1, 3, _, 4, _], [4, _, 4, 6, 7, _, 6, 4], [3, _, 1, _, 0, _, _, _]]
    A2 = [[7, _, 7, 9, _, 7, 6, _], [8, _, _, 6, 8, _, 9, _], [11, _, 9, 7, 6, _, 4, _], [3, _, 4, _, 7, _, _, _]]
    B = [[2, _, 4, 6, 7, _, 6, _], [3, _, 5, 7, 8, _, 7, _], [1, _, 3, 5, 6, _, 5, 3], [4, _, _, _, 7, _, 6, _]]

    def pat(tune, chords, fill=False):
        ch = [{}, {}, {}, {}]
        for bar in range(4):
            c0, r0 = chords[bar], bar * 16
            for beat in range(4):
                r = r0 + beat * 4
                ch[2][r] = cell(KICK, note(12), 0xC, 34)                       # kick (snare on 2 and 4)
                ch[2][r + 2] = cell(HAT, note(30), 0xC, 20)                    # open hats on the off-beats
                if beat % 2:
                    ch[2][r] = cell(SNR, note(24), 0xC, 26)
                ch[1][r] = cell(BAS, note(dg(c0, 0)), 0xC, 40)                 # the slap bass: root, octave, root
                ch[1][r + 2] = cell(BAS, note(dg(c0, 1)), 0xC, 30)
                ch[1][r + 3] = cell(BAS, note(dg(c0 + 4, 0)), 0xC, 22)
            if fill and bar == 3:
                for k in (13, 14, 15):
                    ch[2][r0 + k] = cell(SNR, note(24), 0xC, 16 + k)
            for j, d in enumerate(tune[bar]):
                if d is not None:
                    ch[0][r0 + j * 2] = cell(SQ, note(dg(d, 1)), 0xC, 30)
                    ch[3][r0 + j * 2 + 3] = cell(SQ, note(dg(d + 7, 1)), 0xC, 12)   # its echo, an octave up
        return rows_to_pattern(ch)

    pats = [pat(A, [0, 3, 0, 3]), pat(A2, [0, 3, 0, 3]), pat(B, [5, 6, 4, 0], fill=True)]
    return hm.build_mod("cosmocorgi groove", smp, pats, [0, 1, 0, 2], speed=6)


# ---- alt2: Comet Galop ------------------------------------------------------------------------------------------------
def galop():
    KEY = 0                                          # C
    SQ, PLK, BAS, SNR = 1, 2, 3, 4
    smp = [("square", hm.square_lead(0.25), 20, 0, 32), ("pluck", hm.drop(), 24, 0, 0), ("bass", hm.bass(), 38, 0, 64),
           ("snare", hm.snare(), 24, 0, 0)]

    def dg(d, o):
        return hm.deg(d, o, KEY, hm.MAJOR)
    # a galloping lead: each beat long-short-short (rows 0, 2, 3)
    A = [[4, 5, 4], [3, 4, 5], [6, 5, 4], [7, 6, 5], [5, 6, 5], [4, 5, 6], [4, 3, 2], [0, _, _],
         [2, 3, 4], [5, 4, 3], [4, 5, 6], [7, _, _], [4, 3, 4], [5, 6, 7], [4, 2, 1], [0, _, _]]
    B = [[5, 6, 7], [7, 6, 5], [3, 4, 5], [5, 4, 3], [4, 5, 6], [7, 8, 9], [6, 5, 4], [4, _, _],
         [5, 6, 7], [7, 6, 5], [3, 4, 5], [5, 6, 7], [4, 5, 6], [7, 6, 5], [4, 5, 6], [4, _, _]]

    def pat(tune, chords, up=0, roll=False):
        ch = [{}, {}, {}, {}]
        for bar in range(4):
            c0, r0 = chords[bar], bar * 16
            for beat in range(4):
                r = r0 + beat * 4
                ch[1][r] = cell(BAS, note(dg(c0, 0)), 0xC, 40)                 # oom ...
                ch[1][r + 2] = cell(BAS, note(dg(c0 + 4, 0)), 0xC, 26)         # ... pah
                ch[2][r + 2] = cell(SNR, note(24), 0xC, 20)
                if roll and bar == 3 and beat >= 2:
                    ch[2][r + 1] = cell(SNR, note(24), 0xC, 16)
                    ch[2][r + 3] = cell(SNR, note(24), 0xC, 26)
                g = tune[bar * 4 + beat]
                for k, rr in enumerate((0, 2, 3)):
                    if g[k] is not None:
                        ch[0][r + rr] = cell(SQ, note(dg(g[k], 1 + up)), 0xC, 30 if k == 0 else 22)
                ch[3][r + 1] = cell(PLK, note(dg(c0 + [0, 2, 4, 2][beat], 2)), 0xC, 18)
        return rows_to_pattern(ch)

    pats = [pat(A[:16], [0, 3, 4, 0]), pat(A[:16], [0, 3, 4, 0], up=1), pat(B, [5, 3, 0, 4], roll=True)]
    return hm.build_mod("cosmocorgi galop", smp, pats, [0, 1, 0, 2], speed=4)


ALTS = {1: groove, 2: galop}


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
