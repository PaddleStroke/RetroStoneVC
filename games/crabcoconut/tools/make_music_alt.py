#!/usr/bin/env python3
"""Crab Coconut alternative music: two 4-channel ProTracker MODs, for the owner to choose from (the game still plays
tools/make_music.py's tune.mod until it is switched). Written with the house MOD writer
(games/common/tools/house_music.py) and instruments synthesised here, as Pogo Mamie, Beaver Rush and Duck Parade do;
the mix sits at the house level (the music is played at HA_MUSIC_VOL like every house track).

  alt1  "Coconut Calypso": steel pan, marimba, a calypso bass and hand percussion; C major, 4/4, 120 BPM.
        Hook: the coconut bounce, G-E-G-C on the 3+3+2 calypso rhythm; the marimba answers the pan in the B section
        and plays in thirds under it in A'. Intro, A, B, A', a minor bridge with long rolled pan notes.
  alt2  "Shell Surf": a surf-rock instrumental: a tremolo-picked twang guitar, power-chord chugs, a picked bass and a
        galloping tom beat; E minor (harmonic minor), 160 BPM. Hook: a rising E-minor riff that ends on a
        tremolo-picked D#, the surf "slide" down the neck in the intro and the breakdown.

    make_music_alt.py [OUTDIR]                 -> OUTDIR/alt1.mod, alt2.mod (default: games/crabcoconut/music-alt)
    make_music_alt.py --as-tune alt1 OUTDIR    -> OUTDIR/tune.mod (what build_assets.py packs)

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/crabcoconut/LICENSE.
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


# ---- alt1: Coconut Calypso -----------------------------------------------------------------------------------------
def steel_pan():
    """A steel pan: the fundamental, a blooming octave (the pan's 'wah'), a little twelfth and a metallic ping; a
    16-sample cycle (C-2 = C5), one-shot."""
    out = []
    for i in range(9000):
        ph = i / 16.0
        bloom = (1 - math.exp(-i / 160.0)) * math.exp(-i / 2200.0)
        v = (math.sin(TWO_PI * ph) * math.exp(-i / 3000.0) + 0.75 * bloom * math.sin(TWO_PI * 2 * ph + 0.4) +
             0.22 * math.sin(TWO_PI * 3 * ph) * math.exp(-i / 600.0) + 0.18 * math.sin(TWO_PI * 4.27 * ph) * math.exp(-i / 150.0))
        out.append(v * min(1.0, i / 24.0))
    return norm(out, 112)


def marimba():
    """A wooden bar: the fundamental with its tuned double-octave overtone and a mallet knock; one-shot."""
    rng = random.Random(7)
    out = []
    for i in range(4000):
        ph = i / 16.0
        v = (math.sin(TWO_PI * ph) * math.exp(-i / 1100.0) + 0.45 * math.sin(TWO_PI * 4 * ph) * math.exp(-i / 140.0) +
             0.3 * rng.uniform(-1, 1) * math.exp(-i / 14.0))
        out.append(v)
    return norm(out, 108)


def calypso_bass():
    """A warm plucked bass: a sine with a little second and third harmonic, a soft thump (64-sample cycle)."""
    out = []
    for i in range(5000):
        ph = TWO_PI * i / 64.0
        v = math.sin(ph) + 0.3 * math.sin(2 * ph) * math.exp(-i / 600.0) + 0.12 * math.sin(3 * ph)
        out.append(v * math.exp(-i / 1900.0) * min(1.0, i / 10.0))
    return norm(out, 100)


def shaker():
    rng = random.Random(31)
    out, prev = [], 0.0
    for i in range(420):
        x = rng.uniform(-1, 1)
        out.append((x - prev) * min(1.0, i / 40.0) * math.exp(-i / 120.0))   # a high-passed hiss
        prev = x
    return norm(out, 80)


def conga():
    """A hand drum: a sine falling a fifth onto its pitch (a 32-sample cycle at rest), a slap of noise."""
    rng = random.Random(5)
    out, ph = [], 0.0
    for i in range(1500):
        ph += (1.0 / 32) * (1 + 0.5 * math.exp(-i / 70.0))
        out.append(math.sin(TWO_PI * ph) * math.exp(-i / 380.0) + 0.35 * rng.uniform(-1, 1) * math.exp(-i / 25.0))
    return norm(out, 104)


C_MAJOR = [0, 2, 4, 5, 7, 9, 11]


def diatonic(m, steps, key=0, scale=C_MAJOR):
    """move a note by scale steps (a note off the scale snaps down to it first): -2 = a third below"""
    pcs = [(key + s) % 12 for s in scale]
    while m % 12 not in pcs:
        m -= 1
    notes = sorted(o * 12 + pc for o in range(10) for pc in pcs)
    return notes[notes.index(m) + steps]


def harmony(m, chord):
    """a second voice under note m: the nearest chord tone a third, a sixth or a fourth below"""
    root, q = chord
    pcs = {(midi(root + "0") + t) % 12 for t in QUAL[q]}
    for d in (3, 4, 8, 9, 5):
        if (m - d) % 12 in pcs:
            return m - d
    return m - 5


def calypso():
    s = Song("coconut calypso", 16, 30, gain=1.24)
    PAN = s.add(Inst("steel pan", steel_pan(), 30, 5))
    MAR = s.add(Inst("marimba", marimba(), 28, 5))
    BASS = s.add(Inst("bass", calypso_bass(), 38, 3))
    SHK = s.add(Inst("shaker", shaker(), 14, 4))
    CON = s.add(Inst("conga", conga(), 26, 4))
    KICK = s.add(Inst("kick", hm.kick(), 30, 4))
    M, MR, B, D = 3, 0, 1, 2                     # channels: pan lead, marimba, bass, percussion

    C, F, G, G7, Am, Dm = ("c", "M"), ("f", "M"), ("g", "M"), ("g", "7"), ("a", "m"), ("d", "m")
    chords = [G7, G7] + [C, F, G7, C, C, F, G7, C] + [F, C, G, C, Am, Dm, G7, C] + [C, F, G7, C, C, F, G7, C] + \
             [Am, F, Dm, G7]
    hook = ["g5.3! e5.3 g5.2 c6.4! b5.2 a5.2", "a5.3! f5.3 a5.2 c6.6! r.2", "b5.3! g5.3 d6.2 f6.4 e6.2 d6.2",
            "c6.8*! g5.2 e5.2 g5.4", "g5.3! e5.3 g5.2 c6.4! b5.2 a5.2", "a5.3! f5.3 a5.2 c6.2 d6.2 c6.2 a5.2",
            "g5.3! b5.3 d6.2 f6.3 e6.3 d6.2", "c6.12*! r.4"]
    # the intro: a rolled D over G7, a falling arpeggio into the tune
    s.line(M, PAN, 0, "d6.12*? b5.2 c6.2 d6.4 b5.4 g5.4 f5.4")
    # A and A': the hook; in A' the marimba plays it in thirds below
    for sec, b0 in (("A", 2), ("A'", 18)):
        for k, txt in enumerate(hook):
            if sec == "A'" and k == 7:
                txt = "c6.4! e6.4 g6.8*"
            end = s.line(M, PAN, s.bar(b0 + k), txt)
            if sec == "A'":
                row = s.bar(b0 + k)
                for t in txt.split():
                    name, ln = t.split(".")[0], int(re.match(r"\d+", t.split(".")[1]).group())
                    if name != "r":
                        s.note(row, MR, MAR, harmony(midi(name), chords[b0 + k]), 0.8)
                    row += ln
            assert end == s.bar(b0 + k + 1)
    # B: the pan calls, the marimba answers
    calls = {0: "c6.2 a5.2 c6.2 d6.4! c6.2 a5.4", 2: "d6.2 b5.2 d6.2 e6.4! d6.2 b5.4",
             4: "e6.3! c6.3 a5.2 e6.4 d6.4", 6: "b5.2 d6.2 f6.4! e6.2 d6.2 b5.4", 7: "c6.16*"}
    answers = {1: "e5.2 g5.2 e5.2 c5.2 d5.2 e5.2 c5.4", 3: "e5.2 g5.2 c6.4! a5.2 g5.2 e5.4",
               5: "f5.2 a5.2 d6.2 a5.2 f5.2 d5.2 f5.4"}
    for k in range(8):
        if k in calls:
            s.line(M, PAN, s.bar(10 + k), calls[k])
        else:
            s.off(s.bar(10 + k), M)
        if k in answers:
            s.line(MR, MAR, s.bar(10 + k), answers[k])
    # bridge: long rolled pan notes over A minor, the marimba ripples
    s.line(M, PAN, s.bar(26), "e6.16*? c6.16* d6.8* f6.8* d6.6* b5.6 g5.4")
    # the marimba comping: offbeat chops on the chord tones (where it does not answer or harmonise)
    busy = {11, 13, 15} | set(range(18, 26))
    for b, (root, q) in enumerate(chords):
        r0 = s.bar(b)
        tones = [fit(midi(root + "4") + t, 52, 64) for t in QUAL[q][:3]]
        tones.sort()
        if b >= 2 and b not in busy:
            if b >= 26:      # bridge: a soft rippling arpeggio
                for k in range(8):
                    s.note(r0 + 2 * k, MR, MAR, tones[[0, 1, 2, 1][k % 4]] + (12 if k in (2, 6) else 0), 0.55)
            else:
                for rr, t, v in ((2, 1, 0.85), (3, 2, 0.6), (6, 0, 0.8), (10, 1, 0.85), (11, 2, 0.6), (14, 0, 0.8)):
                    s.note(r0 + rr, MR, MAR, tones[t] + (12 if t == 0 else 0), v)
        # the bass: dum . . . . . dum . dum . . . dum . dum . (root, fifth, root, fifth, a step to the next chord)
        if b >= 1:
            R = fit(midi(root + "2"), midi("e2"), midi("d#3"))
            nxt = fit(midi(chords[(b + 1) % len(chords)][0] + "2"), midi("e2"), midi("d#3")) if b + 1 < len(chords) \
                else midi("c3")
            if b == 1:
                for rr, n in ((0, R), (8, R), (12, R + 2), (14, R + 4)):
                    s.note(r0 + rr, B, BASS, n)
            else:
                third = 3 if q == "m" else 4
                for rr, n, v in ((0, R, 1.0), (6, R + 7, 0.8), (8, R, 0.9), (12, R + third, 0.75),
                                 (14, diatonic(nxt, -1 if nxt > R else 1), 0.7)):
                    s.note(r0 + rr, B, BASS, n, v)
        # the percussion: kick 1 and 3, a shaker on the 16ths (the offbeats louder), congas in between
        for rr in range(16):
            if b == 0 and rr < 8:
                if rr % 2 == 0:
                    s.note(r0 + rr, D, SHK, "c5", 0.5 + rr * 0.06)
                continue
            if rr in (0, 8):
                s.note(r0 + rr, D, KICK, "c4", 1.0 if rr == 0 else 0.85)
            elif rr in (3, 11, 13):
                s.note(r0 + rr, D, CON, "e4" if rr != 13 else "g4", 0.8 if rr != 13 else 0.6)
            elif rr in (6, 14):
                s.note(r0 + rr, D, CON, "b3", 0.95)
            else:
                s.note(r0 + rr, D, SHK, "c5", 1.0 if rr % 4 == 2 else 0.55)
        if b == 25:          # a conga fill into the bridge
            for rr, n in ((12, "g4"), (13, "e4"), (14, "e4"), (15, "b3")):
                s.cells[r0 + rr][D] = [0, 0, 0, 0]
                s.note(r0 + rr, D, CON, n, 0.9)
    return s.build(6, 120, 2)


# ---- alt2: Shell Surf ----------------------------------------------------------------------------------------------
def twang():
    """A bright single-coil guitar: harmonics shaped by a bridge pickup, the high ones dying first (32-sample cycle)."""
    out = []
    for i in range(6000):
        ph = TWO_PI * i / 32.0
        v = 0.0
        for k in range(1, 9):
            a = abs(math.sin(k * math.pi * 0.18)) / k ** 0.8
            v += a * math.sin(k * ph) * math.exp(-i * k ** 0.7 / 2600.0)
        out.append(v * min(1.0, i / 6.0))
    return norm(out, 110)


def power_chord():
    """Root, fifth and octave, softly overdriven (tanh), one-shot: chugs are cut short with a note cut."""
    out = []
    for i in range(3400):
        ph = TWO_PI * i / 32.0
        v = math.sin(ph) + 0.8 * math.sin(1.5 * ph + 0.2) + 0.6 * math.sin(2 * ph + 0.5) + 0.3 * math.sin(3 * ph)
        out.append(math.tanh(1.6 * v) * math.exp(-i / 1600.0) * min(1.0, i / 8.0))
    return norm(out, 92)


def picked_bass():
    out = []
    for i in range(2600):
        ph = TWO_PI * i / 64.0
        v = math.sin(ph) + 0.35 * math.sin(2 * ph) * math.exp(-i / 500.0) + 0.2 * math.sin(3 * ph) * math.exp(-i / 300.0)
        out.append(v * math.exp(-i / 1300.0) * min(1.0, i / 6.0))
    return norm(out, 104)


def tom():
    rng = random.Random(3)
    out, ph = [], 0.0
    for i in range(1800):
        ph += (1.0 / 40) * (1 + 0.8 * math.exp(-i / 160.0))
        out.append(math.sin(TWO_PI * ph) * math.exp(-i / 520.0) + 0.2 * rng.uniform(-1, 1) * math.exp(-i / 40.0))
    return norm(out, 108)


def surf():
    s = Song("shell surf", 16, 36, gain=1.16)
    GTR = s.add(Inst("twang", twang(), 30, 4))
    CHUG = s.add(Inst("power chord", power_chord(), 22, 4))
    BASS = s.add(Inst("bass", picked_bass(), 38, 3))
    KICK = s.add(Inst("kick", hm.kick(), 32, 4))
    SNR = s.add(Inst("snare", hm.snare(9), 24, 4))
    HAT = s.add(Inst("hat", hm.hat(11), 12, 4))
    TOM = s.add(Inst("tom", tom(), 28, 4))
    L, R, B, D = 3, 0, 1, 2

    Em, C, B7, G, Dc, Am = ("e", "m"), ("c", "M"), ("b", "7"), ("g", "M"), ("d", "M"), ("a", "m")
    chords = [Em] * 4 + [Em, Em, C, B7] * 2 + [G, Dc, Am, B7, G, Dc, C, B7] + [Em, Dc, C, B7] + \
             [Em, Em, C, B7] * 2 + [Em, C, Am, B7]
    # intro: the slide down the neck (tremolo picking, a semitone a row), then the riff on the low strings
    s.line(L, GTR, 0, " ".join("%s.1*" % n for n in ["e5", "d#5", "d5", "c#5", "c5", "b4", "a#4", "a4", "g#4", "g4",
                                                      "f#4", "f4", "e4", "d#4", "d4", "c#4"]), retrig=0x92)
    s.line(L, GTR, s.bar(1), "e3.2! e3.2 g3.2 a3.2 b3.4 a3.2 g3.2")
    s.line(L, GTR, s.bar(2), "e3.2! e3.2 g3.2 a3.2 b3.2 d4.2 b3.4*")
    s.line(L, GTR, s.bar(3), "e4.16*!")
    riff = ["b3.4! e4.4 g4.6 f#4.2", "e4.4 g4.4 b4.8*!", "c5.4! e5.2 c5.2 g4.4 e4.4", "d#4.8*! f#4.4 a4.4",
            "b3.4! e4.4 g4.6 a4.2", "b4.4 a4.4 g4.4 b4.4", "c5.6! g4.2 a4.4 c5.4", "b4.16*!"]
    for k, t in enumerate(riff):
        s.line(L, GTR, s.bar(4 + k), t)
    # B: the guitar calls on the first half of the bar, the chords answer
    b_calls = ["d4.2! g4.2 b4.4 r.8", "f#4.2! a4.2 d5.4 r.8", "e4.2! a4.2 c5.4 r.8", "d#4.2 f#4.2 a4.2 b4.2 r.8",
               "d4.2! g4.2 b4.4 r.8", "f#4.2! a4.2 d5.4 r.8", "e5.4! d5.2 c5.2 g4.4 e4.4", "b4.16*!"]
    for k, t in enumerate(b_calls):
        s.line(L, GTR, s.bar(12 + k), t)
    # the breakdown: drums and bass, the guitar slides down again
    s.line(L, GTR, s.bar(20), "r.16")
    s.line(L, GTR, s.bar(22), "r.8 " + " ".join("%s.1*" % n for n in ["b4", "a#4", "a4", "g#4", "g4", "f#4", "f4", "e4"]),
           retrig=0x92)
    s.line(L, GTR, s.bar(23), "b3.4 d#4.4 f#4.4 a4.4")
    # A': the riff, the chords' channel doubling it a third below in harmonic minor (twin guitars)
    riff2 = riff[:7] + ["b4.4! d#5.4 f#5.8*"]
    tail = ["e5.4! d#5.2 e5.2 g5.4 e5.4", "c5.6 g4.2 a4.4 c5.4", "a4.4 c5.4 e5.8*", "d#5.4 b4.4 f#4.4 d#4.4"]
    for k, t in enumerate(riff2 + tail):
        b = 24 + k
        s.line(L, GTR, s.bar(b), t)
        if k < 8:
            row = s.bar(b)
            for tok in t.split():
                name, ln = tok.split(".")[0], int(re.match(r"\d+", tok.split(".")[1]).group())
                if name != "r":
                    s.note(row, R, GTR, harmony(midi(name), chords[b]), 0.7)
                    if "*" in tok:
                        for q in range(1, ln):
                            s.fx(row + q, R, 0xE, 0x93)
                row += ln
    for b, (root, _q) in enumerate(chords):
        r0 = s.bar(b)
        R0 = fit(midi(root + "2"), midi("e2"), midi("d#3"))
        # power-chord chugs (8ths, cut short; a long ring on the last 8th), where the chords' channel is free
        if 4 <= b < 24 and b not in (20, 21):
            in_b = 12 <= b < 20
            for k in range(8):
                rr = 2 * k
                if in_b and k < 4 and b != 18:
                    continue
                s.note(r0 + rr, R, CHUG, R0 + 12, 1.0 if k % 4 == 0 else 0.75)
                if k != 7:
                    s.fx(r0 + rr, R, 0xE, 0xC3)
        if b >= 32:
            for rr in (0, 6, 12):
                s.note(r0 + rr, R, CHUG, R0 + 12, 0.9)
        # the bass: driving 8ths, the octave on the offbeats
        if b >= 1:
            for k in range(8):
                n = R0 + (12 if k % 2 else 0)
                if b in (20, 21) and k % 2:
                    n = R0 + [7, 12, 10, 7][k // 2]
                if k == 7 and b % 2 == 1:
                    n = R0 + 10
                s.note(r0 + 2 * k, B, BASS, n, 1.0 if k % 2 == 0 else 0.8)
        # the drums: a galloping tom beat (A), a stomp (B), toms only in the breakdown
        for rr in range(16):
            if b == 0:
                if rr % 4 == 0:
                    s.note(r0 + rr, D, TOM, "c4" if rr < 8 else "g3", 0.6 + rr * 0.03)
                continue
            if b in (20, 21):
                pat = {0: (KICK, "c4"), 4: (TOM, "g3"), 6: (TOM, "c4"), 8: (KICK, "c4"), 10: (TOM, "g3"),
                       12: (TOM, "c4"), 14: (TOM, "g3")}
            elif 12 <= b < 20:
                pat = {0: (KICK, "c4"), 2: (HAT, "c5"), 4: (SNR, "c4"), 6: (HAT, "c5"), 7: (KICK, "c4"),
                       8: (KICK, "c4"), 10: (HAT, "c5"), 12: (SNR, "c4"), 14: (HAT, "c5")}
            else:
                pat = {0: (KICK, "c4"), 2: (TOM, "c4"), 3: (TOM, "c4"), 4: (SNR, "c4"), 6: (TOM, "g3"),
                       7: (TOM, "g3"), 8: (KICK, "c4"), 10: (TOM, "c4"), 11: (TOM, "c4"), 12: (SNR, "c4"),
                       14: (HAT, "c5"), 15: (HAT, "c5")}
            if b % 4 == 3 and rr >= 12:      # a snare fill at the end of each phrase
                pat = {12: (SNR, "c4"), 13: (SNR, "c4"), 14: (SNR, "c4"), 15: (TOM, "g3")}
            if rr in pat:
                inst, n = pat[rr]
                acc = 1.0 if rr % 4 == 0 else 0.75
                if inst is HAT:
                    acc = 0.9
                s.note(r0 + rr, D, inst, n, acc)
    return s.build(6, 160, 4)


TRACKS = {"alt1": calypso, "alt2": surf}


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
