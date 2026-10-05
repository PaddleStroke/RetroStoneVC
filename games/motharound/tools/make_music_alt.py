#!/usr/bin/env python3
"""Moth Around alternative music: two 4-channel ProTracker MODs, for the owner to choose from (the game still plays
tools/make_music.py's tune.mod until it is switched). Written with the house MOD writer
(games/common/tools/house_music.py) and instruments synthesised here, as Pogo Mamie, Beaver Rush and Duck Parade do
(the glass pad is the house pad); the mix sits at the house level (the music is played at HA_MUSIC_VOL like every
house track).

  alt1  "Lantern Waltz": a music-box night waltz in 3/4: one music-box comb plays the tune and the 'pah-pahs', a
        harp bass, the house pad as a glass harmonica with a slow tremolo; G lydian (the dreamy C#), 96 BPM. Hook:
        the moth circling the lantern, D C# D B around the light. A, B, A' with a hemiola countermelody, and a coda
        where the music box winds down (96 -> 72 BPM) before it is wound up again for the loop.
  alt2  "Moonlit Orbit": a hypnotic night piece in 5/4: a kalimba ostinato circling on 3+3+2+2, a theremin gliding
        over it, a soft sub bass, a heartbeat drum, clock ticks and fluttering wings; F# minor (harmonic minor, a
        dorian bridge), 126 BPM. Hook: the theremin's slide up a fourth, C# to F#, the moth rising to the light.

    make_music_alt.py [OUTDIR]                 -> OUTDIR/alt1.mod, alt2.mod (default: games/motharound/music-alt)
    make_music_alt.py --as-tune alt1 OUTDIR    -> OUTDIR/tune.mod (what build_assets.py packs)

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/motharound/LICENSE.
"""
import math
import os
import random
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "common", "tools"))
import house_music as hm  # noqa: E402
from house_music import s8  # noqa: E402


# ---- the score kit (a timeline of rows, cut into whole-bar patterns) -----------------------------------------------
PC = {"c": 0, "d": 2, "e": 4, "f": 5, "g": 7, "a": 9, "b": 11}
QUAL = {"M": [0, 4, 7], "m": [0, 3, 7], "7": [0, 4, 7, 10], "m7": [0, 3, 7, 10], "M7": [0, 4, 7, 11]}


def midi(name):
    """'c4', 'f#5', 'bb3' -> a semitone number (c4 = 48)."""
    n = name.lower()
    pc, i = PC[n[0]], 1
    while i < len(n) and n[i] in "#b":
        pc += 1 if n[i] == "#" else -1
        i += 1
    return pc + 12 * int(n[i:])


def fit(n, lo, hi):
    while n < lo:
        n += 12
    while n > hi:
        n -= 12
    return n


class Inst:
    """A sample: data, house volume, loop, and c2: the octave whose C the sample plays at the MOD note C-2
    (a 32-sample cycle: c2 = 4; 64: 3; 16: 5)."""

    def __init__(self, name, data, vol, c2, loop=(0, 0)):
        self.name, self.data, self.vol, self.c2, self.loop = name, data, vol, c2, loop
        self.num = 0


class Song:
    def __init__(self, title, rows_per_bar, bars, gain=1.0):
        self.title, self.rpb, self.bars, self.gain = title, rows_per_bar, bars, gain
        self.n = rows_per_bar * bars
        self.cells = [[[0, 0, 0, 0] for _ in range(4)] for _ in range(self.n)]
        self.insts = []

    def add(self, inst):
        self.insts.append(inst)
        inst.num = len(self.insts)
        return inst

    def vol(self, inst, vel):
        return max(0, min(64, int(round(inst.vol * self.gain * vel))))

    def note(self, row, ch, inst, n, vel=1.0):
        """n: a note name or a semitone number; the volume effect only when it differs from the sample's."""
        m = midi(n) if isinstance(n, str) else n
        idx = m - 12 * inst.c2 + 12
        assert 0 <= idx < 36, (self.title, inst.name, n, row)
        if row >= self.n:
            return
        c = self.cells[row][ch]
        c[0], c[1] = inst.num, hm.PERIODS[idx]
        v = self.vol(inst, vel)
        c[2], c[3] = (0xC, v) if v != self.vol(inst, 1.0) else (0, 0)

    def porta(self, row, ch, inst, n, speed):
        idx = (midi(n) if isinstance(n, str) else n) - 12 * inst.c2 + 12
        assert 0 <= idx < 36
        if row < self.n:
            self.cells[row][ch] = [0, hm.PERIODS[idx], 3, speed]

    def fx(self, row, ch, eff, par):
        if row < self.n:
            c = self.cells[row][ch]
            if c[2] == 0 and c[3] == 0:
                c[2], c[3] = eff, par

    def off(self, row, ch):
        if row < self.n and self.cells[row][ch] == [0, 0, 0, 0]:
            self.cells[row][ch] = [0, 0, 0xC, 0]

    def glob(self, row, eff, par):
        """a song-wide effect (speed, tempo, break, jump) in the first free effect slot of the row"""
        for ch in (3, 2, 1, 0):
            c = self.cells[row][ch]
            if c[2] == 0 and c[3] == 0:
                c[2], c[3] = eff, par
                return
        for ch in (3, 2, 1, 0):                      # else one row of a vibrato is not missed
            c = self.cells[row][ch]
            if c[1] == 0 and c[2] == 4:
                c[2], c[3] = eff, par
                return
        raise ValueError("%s: no free effect slot on row %d" % (self.title, row))

    def line(self, ch, inst, row, text, vel=1.0, vib=0x34, slide=0x18, retrig=0x93):
        """A melody: 'name.len' tokens (len in rows); 'r.len' a rest. Suffixes after the length: ! accent, ? soft,
        ^ vibrato, * tremolo (retriggered), ~ slide into the next note."""
        slide_in = False
        for t in text.split():
            name, rest = t.split(".", 1)
            m = re.match(r"(\d+)([!?^~*]*)$", rest)
            ln, mods = int(m.group(1)), m.group(2)
            v = vel * (1.2 if "!" in mods else 0.65 if "?" in mods else 1.0)
            if name == "r":
                self.off(row, ch)
            else:
                if slide_in:                         # a tone portamento, kept going for up to 3 more rows
                    self.porta(row, ch, inst, name, slide)
                    for k in range(1, min(ln, 4)):
                        self.fx(row + k, ch, 3, 0)
                else:
                    self.note(row, ch, inst, name, v)
                for k in range(1, ln):
                    if "*" in mods:
                        self.fx(row + k, ch, 0xE, retrig)
                    elif "^" in mods and k >= (4 if slide_in else 2):
                        self.fx(row + k, ch, 4, vib)
            slide_in = "~" in mods
            row += ln
        return row

    def bar(self, b):
        return b * self.rpb

    def build(self, speed, tempo, loop_bar, breaks=()):
        """Cut the bars into patterns (whole bars, at most 64 rows, a new one at loop_bar and at each break), set the
        speed and tempo on the first row and at the loop start, break short patterns, jump back to the loop."""
        for r in (0, self.bar(loop_bar)):
            self.glob(r, 0xF, speed)
            self.glob(r, 0xF, tempo)
        bounds, start = [], 0
        for b in range(1, self.bars + 1):
            if b == self.bars or (b - start + 1) * self.rpb > 64 or b == loop_bar or b in breaks:
                bounds.append((start, b))
                start = b
        loop_order = [s for s, _ in bounds].index(loop_bar)
        pats = []
        for k, (b0, b1) in enumerate(bounds):
            last = self.bar(b1) - 1
            if k == len(bounds) - 1:
                self.glob(last, 0xB, loop_order)
            elif (b1 - b0) * self.rpb < 64:
                self.glob(last, 0xD, 0)
            rows = self.cells[self.bar(b0):self.bar(b1)]
            data = b"".join(hm.cell(c[0], c[1], c[2], c[3]) for row in rows for c in row)
            pats.append(data.ljust(1024, b"\0"))
        assert len(pats) <= 64
        smp = []
        for i in self.insts:
            d = list(i.data) + ([0] if len(i.data) % 2 else [])
            ls, ll = i.loop
            assert ls % 2 == 0 and ll % 2 == 0 and ls + ll <= len(d)
            smp.append((i.name, d, max(1, min(64, int(round(i.vol * self.gain)))), ls, ll))
        return hm.build_mod(self.title, smp, pats, list(range(len(pats))))


def norm(xs, peak, fade=0.3):
    """scale to a peak; a one-shot's last part fades out (a raised cosine) so it never ends with a click"""
    m = max(abs(v) for v in xs) or 1.0
    n, f0 = len(xs), int(len(xs) * (1 - fade))
    return [s8(v * peak / m * (1.0 if i < f0 or not fade else 0.5 + 0.5 * math.cos(math.pi * (i - f0) / (n - f0))))
            for i, v in enumerate(xs)]


TWO_PI = 2 * math.pi


# ---- alt1: Lantern Waltz -------------------------------------------------------------------------------------------
def music_box():
    """A music-box tine: the fundamental, a quick octave, the tine's high inharmonic mode and a pin's click
    (16-sample cycle: C-2 = C5), one-shot with a long ring."""
    rng = random.Random(9)
    out = []
    for i in range(12000):
        ph = i / 16.0
        v = (math.sin(TWO_PI * ph) * math.exp(-i / 4200.0) + 0.3 * math.sin(TWO_PI * 2 * ph) * math.exp(-i / 700.0) +
             0.22 * math.sin(TWO_PI * 6.27 * ph) * math.exp(-i / 260.0) + 0.25 * rng.uniform(-1, 1) * math.exp(-i / 10.0))
        out.append(v * min(1.0, i / 4.0))
    return norm(out, 108, fade=0.4)


def harp_bass():
    out = []
    for i in range(6000):
        ph = TWO_PI * i / 64.0
        v = math.sin(ph) + 0.45 * math.sin(2 * ph) * math.exp(-i / 500.0) + 0.2 * math.sin(3 * ph) * math.exp(-i / 300.0)
        out.append(v * math.exp(-i / 2600.0) * min(1.0, i / 10.0))
    return norm(out, 100)


def waltz():
    s = Song("lantern waltz", 12, 30, gain=1.65)
    BOX = s.add(Inst("music box", music_box(), 30, 5))
    HARP = s.add(Inst("harp bass", harp_bass(), 34, 3))
    GLASS = s.add(Inst("glass pad", hm.pad(), 22, 3, loop=(0, 64)))
    M, A, B, P = 3, 0, 1, 2

    G, AG, Bm, D, Em, Fsm, A_ = ("g", "M"), ("a", "M"), ("b", "m"), ("d", "M"), ("e", "m"), ("f#", "m"), ("a", "M")
    sec_a = [G, AG, G, AG, Em, Fsm, A_, D]
    sec_b = [Em, Bm, A_, D, G, Fsm, Em, A_]
    coda = [G, AG, G, G]
    chords = [G, AG] + sec_a + sec_b + sec_a + coda
    mel_a = ["d6.6 c#6.2 d6.2 b5.2", "e6.6 d6.2 e6.2 c#6.2", "d6.4 b5.4 g5.4", "a5.8 e5.4", "g5.6 f#5.2 g5.2 b5.2",
             "a5.6 c#6.2 e6.2 f#6.2", "e6.4 c#6.4 a5.4", "d6.8 r.4"]
    mel_b = ["b5.4 e6.4 g6.4", "f#6.6 e6.2 d6.4", "c#6.4 e6.4 a6.4", "f#6.8 e6.4", "d6.4 b5.4 g5.4",
             "a5.4 c#6.4 f#6.4", "e6.6 d6.2 c#6.2 b5.2", "c#6.8 e6.4"]
    mel_a2 = mel_a[:3] + ["a5.4 b5.2 c#6.2 e6.4"] + mel_a[4:7] + ["d6.4 f#6.4 a6.4"]
    counter = ["b4.6 a4.6", "c#5.6 e5.6", "d5.6 b4.6", "c#5.6 a4.6", "b4.6 e5.6", "c#5.6 a4.6", "e5.6 c#5.6",
               "f#5.6 a5.6"]
    mel_coda = ["d6.6 c#6.2 d6.2 b5.2", "e6.6 d6.2 e6.2 c#6.2", "d6.4 b5.4 g5.4", "g5.12"]
    # the intro: the music box being wound, twinkles high up
    s.line(M, BOX, 0, "r.4 g6.2? d6.2? b5.4? r.4 a6.2? e6.2? c#6.2? a5.2?")
    for k in range(8):
        s.line(M, BOX, s.bar(2 + k), mel_a[k])
        s.line(M, BOX, s.bar(10 + k), mel_b[k])
        s.line(M, BOX, s.bar(18 + k), mel_a2[k])
        s.line(A, BOX, s.bar(18 + k), counter[k], vel=0.6)
    for k in range(4):
        s.line(M, BOX, s.bar(26 + k), mel_coda[k])
        s.glob(s.bar(26 + k), 0xF, [90, 84, 78, 72][k])          # the music box winds down
    for b, (root, q) in enumerate(chords):
        r0 = s.bar(b)
        bass_root = "g" if b in (1, 3, 5, 19, 21, 27) else root          # A over a G pedal: the lydian colour
        R = fit(midi(bass_root + "2"), midi("e2"), midi("d#3"))
        tones = sorted(fit(midi(root + "4") + t, midi("f#4"), midi("f5")) for t in QUAL[q])
        # the pah-pahs on beats 2 and 3 (not under the countermelody)
        if b >= 2 and not (18 <= b < 26):
            s.note(r0 + 4, A, BOX, tones[1], 0.45)
            s.note(r0 + 8, A, BOX, tones[2], 0.4)
            if b >= 26:
                s.note(r0 + 10, A, BOX, tones[0] + 12, 0.3)
        # the harp bass on beat 1, the fifth softly on beat 3 in B
        if b >= 1:
            s.note(r0, B, HARP, R, 1.0)
            if 10 <= b < 18:
                s.note(r0 + 8, B, HARP, R + 7, 0.6)
        # the glass pad: the chord's third, a slow tremolo
        if b >= 1:
            s.note(r0, P, GLASS, fit(midi(root + "3") + QUAL[q][1], midi("a3"), midi("g#4")), 0.9)
            for k in range(1, 12):
                s.fx(r0 + k, P, 7, 0x43)
    return s.build(6, 96, 2, breaks=(26, 27, 28, 29))


# ---- alt2: Moonlit Orbit -------------------------------------------------------------------------------------------
def kalimba():
    """A thumb piano: a sine, its high tine mode (~5.9x) dying fast, a little buzz; 32-sample cycle, one-shot."""
    rng = random.Random(23)
    out = []
    for i in range(6000):
        ph = i / 32.0
        v = (math.sin(TWO_PI * ph) + 0.35 * math.sin(TWO_PI * 5.93 * ph) * math.exp(-i / 180.0) +
             0.08 * math.sin(TWO_PI * 2 * ph) + 0.15 * rng.uniform(-1, 1) * math.exp(-i / 12.0))
        out.append(v * math.exp(-i / 1900.0) * min(1.0, i / 4.0))
    return norm(out, 106)


def theremin():
    """A theremin: nearly a pure sine, swelling in over 40 cycles, then one looped 16-sample cycle (C-2 = C5)."""
    out = []
    for i in range(656):
        ph = TWO_PI * i / 16.0
        v = math.sin(ph) + 0.07 * math.sin(2 * ph + 0.3) + 0.03 * math.sin(3 * ph)
        if i < 640:
            v *= (i / 640.0) ** 0.7
        out.append(v)
    return norm(out, 104, fade=0)


def tick():
    rng = random.Random(29)
    return norm([(rng.uniform(-1, 1) + math.sin(TWO_PI * i * 0.3)) * math.exp(-i / 18.0) for i in range(160)], 90)


def heartbeat():
    out, ph = [], 0.0
    for i in range(1700):
        ph += (52 + 30 * math.exp(-i / 140.0)) / 8287.0
        out.append(math.sin(TWO_PI * ph) * math.exp(-i / 520.0))
    return norm(out, 110)


def wings():
    """a moth's wing beat: a soft band of noise, a quick swell and fall"""
    rng = random.Random(41)
    out, lp, hp = [], 0.0, 0.0
    for i in range(420):
        x = rng.uniform(-1, 1)
        lp += (x - lp) * 0.35
        out.append((lp - hp) * math.sin(math.pi * i / 420.0))
        hp += (lp - hp) * 0.05
    return norm(out, 80)


def orbit():
    s = Song("moonlit orbit", 20, 30, gain=1.04)
    KAL = s.add(Inst("kalimba", kalimba(), 28, 4))
    THR = s.add(Inst("theremin", theremin(), 20, 5, loop=(640, 16)))
    SUB = s.add(Inst("sub bass", hm.bass(), 32, 3, loop=(0, 64)))
    TICK = s.add(Inst("tick", tick(), 10, 4))
    HEART = s.add(Inst("heartbeat", heartbeat(), 30, 4))
    WING = s.add(Inst("flutter", wings(), 14, 4))
    K, T, B, D = 0, 3, 1, 2

    Fsm, Dc, Bm, C7, Ac, Ec, Bc = ("f#", "m"), ("d", "M"), ("b", "m"), ("c#", "7"), ("a", "M"), ("e", "M"), ("b", "M")
    sec_a = [Fsm, Dc, Bm, C7, Fsm, Dc, Bm, C7]
    sec_b = [Ac, Ec, Fsm, C7, Ac, Ec, Dc, C7]
    bridge = [Fsm, Bc, Fsm, Bc]
    chords = [Fsm, Fsm] + sec_a + sec_b + bridge + sec_a
    mel_a = ["c#5.8~ f#5.6 e5.2 c#5.4", "d5.12^ a4.4 d5.4", "f#5.8 e5.4 d5.4 b4.4", "f5.6 g#5.6 c#5.8^",
             "a5.8~ c#6.6 b5.2 a5.4", "f#5.12^ e5.4 d5.4", "d5.6 c#5.2 b4.4 d5.4 f#5.4", "f5.4 g#5.4 b5.4 c#6.8^"]
    mel_b = ["e5.4 a5.4 c#6.12^", "b5.8 g#5.4 e5.8", "a5.6 f#5.6 c#5.8^", "f5.6 g#5.6 c#6.8", "e6.8~ c#6.4 a5.8",
             "g#5.6 b5.6 e6.8^", "f#6.8 e6.4 d6.8", "c#6.10^ b5.4 g#5.6"]
    mel_br = ["a4.10~ c#5.10^", "d#5.20^", "c#5.10~ e5.10^", "f#5.10~ d#5.6 b4.4"]
    mel_a2 = mel_a[:4] + ["c#6.8~ a5.4 f#5.4 a5.4", "d6.8 c#6.4 a5.8^", "b5.6 a5.2 f#5.4 d5.4 b4.4",
                          "c#5.6 f5.6 g#5.8"]
    for k in range(8):
        s.line(T, THR, s.bar(2 + k), mel_a[k], vib=0x46, slide=0x06)
        s.line(T, THR, s.bar(10 + k), mel_b[k], vib=0x46, slide=0x06)
        s.line(T, THR, s.bar(22 + k), mel_a2[k], vib=0x46, slide=0x06)
    for k in range(4):
        s.line(T, THR, s.bar(18 + k), mel_br[k], vib=0x57, slide=0x08, vel=0.9)
    # the ostinato: ten eighths grouped 3+3+2+2, circling the chord
    ORBIT = [0, 1, 2, 3, 1, 2, 4, 3, 1, 2]
    ACC = {0, 3, 6, 8}
    for b, (root, q) in enumerate(chords):
        r0 = s.bar(b)
        r3 = fit(midi(root + "3"), midi("f#3"), midi("f4"))
        third = QUAL[q][1]
        tones = [r3, r3 + 7, r3 + 12 + third, r3 + 12, r3 + 19]
        for e, t in enumerate(ORBIT):
            n = tones[t]
            if n > midi("b5"):
                n -= 12
            s.note(r0 + 2 * e, K, KAL, n, 1.0 if e in ACC else 0.6)
            if b >= 22:                                          # A': a soft tracker echo, a moonlit shimmer
                s.note(r0 + 2 * e + 1, K, KAL, n, 0.25)
        # the sub bass: 3+3+2+2 (root, root, fifth, octave), staccato; only beats 1 and 4 in the bridge
        R = fit(midi(root + "2"), midi("e2"), midi("d#3"))
        if b >= 1:
            steps = ((0, 0, 5), (6, 0, 5), (12, 7, 3), (16, 12, 3)) if not 18 <= b < 22 else ((0, 0, 11), (12, 7, 7))
            for rr, st, ln in steps:
                s.note(r0 + rr, B, SUB, R + st, 1.0 if rr in (0, 12) else 0.75)
                s.off(r0 + rr + ln, B)
        # the percussion: a heartbeat on 1 and 4, clock ticks on the eighths, wings fluttering at the bar's end
        for rr in range(0, 20, 2):
            if rr in (0, 12) and b >= 1 and not (18 <= b < 22 and rr == 12):
                s.note(r0 + rr, D, HEART, "c4", 1.0 if rr == 0 else 0.8)
            elif rr == 16 and (b % 2 == 1 or 18 <= b < 22):
                s.note(r0 + rr, D, WING, "c5", 0.9)
                for k in range(1, 4):
                    s.fx(r0 + rr + k, D, 0xE, 0x92)
            elif rr in (18,) and (b % 2 == 1 or 18 <= b < 22):
                continue
            else:
                s.note(r0 + rr, D, TICK, "c5", 0.9 if rr in (6, 16) else 0.55)
    return s.build(6, 126, 2)


TRACKS = {"alt1": waltz, "alt2": orbit}


def main():
    args = sys.argv[1:]
    if args[:1] == ["--as-tune"]:
        name, outdir = args[1], args[2]
        os.makedirs(outdir, exist_ok=True)
        with open(os.path.join(outdir, "tune.mod"), "wb") as f:
            f.write(TRACKS[name]())
        return
    outdir = args[0] if args else os.path.join(HERE, "..", "music-alt")
    os.makedirs(outdir, exist_ok=True)
    for name, fn in TRACKS.items():
        with open(os.path.join(outdir, name + ".mod"), "wb") as f:
            f.write(fn())


if __name__ == "__main__":
    main()
