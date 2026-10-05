#!/usr/bin/env python3
"""Frog Reach alternative music: two 4-channel ProTracker MODs, for the owner to choose from (the game still plays
tools/make_music.py's tune.mod until it is switched). Written with the house MOD writer
(games/common/tools/house_music.py) and instruments synthesised here, as Pogo Mamie, Beaver Rush and Duck Parade do;
the mix sits at the house level (the music is played at HA_MUSIC_VOL like every house track).

  alt1  "Bayou Blues": a slow swamp shuffle in 12/8: a bending harmonica, a boogie upright bass, a tremolo
        dominant-seventh guitar, brushes and a croaking frog; A blues, 80 BPM. Two 12-bar choruses: the harmonica
        tune (hook: a long bent C to C#, the stretching tongue, then a quick snap down), then call and response
        with a slide guitar on a quick-change chorus.
  alt2  "Lily-Pad Bossa": a lazy bossa nova: nylon-guitar seventh chords on the bossa rhythm, a soft upright bass,
        rim clicks on the clave, a shaker and a croaking guiro, a breathy flute; D major, 132 BPM. Hook: the
        'ribbit-hop', two quick repeated notes and a leap up to the next pad; a B section with a borrowed G minor.

    make_music_alt.py [OUTDIR]                 -> OUTDIR/alt1.mod, alt2.mod (default: games/frogreach/music-alt)
    make_music_alt.py --as-tune alt1 OUTDIR    -> OUTDIR/tune.mod (what build_assets.py packs)

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/frogreach/LICENSE.
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


# ---- alt1: Bayou Blues ---------------------------------------------------------------------------------------------
def harmonica():
    """A reedy, nasal harmonica: strong low harmonics, a breathy attack (20 cycles) then one looped 32-sample cycle."""
    rng = random.Random(12)
    out = []
    for i in range(672):
        ph = TWO_PI * i / 32.0
        v = (math.sin(ph) + 0.55 * math.sin(2 * ph + 0.3) + 0.42 * math.sin(3 * ph) + 0.26 * math.sin(4 * ph + 0.9) +
             0.16 * math.sin(5 * ph) + 0.09 * math.sin(6 * ph + 0.4))
        v = math.tanh(0.9 * v)
        if i < 640:
            v = v * min(1.0, i / 120.0) + 0.35 * rng.uniform(-1, 1) * math.exp(-i / 150.0)
        out.append(v)
    return norm(out, 100, fade=0)


def seventh_chord():
    """A harmonic dominant seventh (4:5:6:7, the barbershop/blues chord) for a swampy tremolo guitar: a bright pluck
    (1024 samples) then a loop of 256 samples (4 cycles of the 64-sample root)."""
    out = []
    for i in range(1280):
        v = 0.0
        for ratio, a in ((4, 1.0), (5, 0.8), (6, 0.75), (7, 0.6)):
            ph = TWO_PI * ratio * i / 256.0
            bright = math.exp(-i / 260.0) if i < 1024 else 0.0
            v += a * (math.sin(ph) + (0.3 + 0.5 * bright) * math.sin(2 * ph) + 0.4 * bright * math.sin(3 * ph))
        out.append(v * (1.0 if i >= 1024 else 1.0 + 0.6 * math.exp(-i / 200.0)))
    return norm(out, 96, fade=0)


def slide_guitar():
    """A resonator-ish slide guitar: a bright pluck that rings (32-sample cycle), one-shot."""
    out = []
    for i in range(6000):
        ph = TWO_PI * i / 32.0
        v = 0.0
        for k in range(1, 7):
            v += (1.0 / k) * math.sin(k * ph + 0.3 * k) * math.exp(-i * k ** 0.5 / 3600.0)
        out.append(v * min(1.0, i / 10.0))
    return norm(out, 104)


def upright_bass(n=8000, soft=False):
    """A plucked upright bass, a 128-sample cycle (C-2 = C2): a round sine, a little string, a thumb thump."""
    out = []
    for i in range(n):
        ph = TWO_PI * i / 128.0
        v = math.sin(ph) + (0.18 if soft else 0.3) * math.sin(2 * ph) * math.exp(-i / 900.0) + \
            0.1 * math.sin(3 * ph) * math.exp(-i / 500.0)
        out.append(v * math.exp(-i / 3200.0) * min(1.0, i / (24.0 if soft else 8.0)))
    return norm(out, 104)


def croak():
    """The frog: 'rib-bit', two nasal bursts trilling at 30 Hz with a falling pitch."""
    out, ph = [], 0.0
    for i in range(2200):
        t = i / 8287.0
        f = 118 * (1.0 - 0.25 * (t if t < 0.11 else t - 0.12))
        ph += f / 8287.0
        burst = (t < 0.09) or (0.12 < t < 0.24)
        amp = (0.5 + 0.5 * math.sin(TWO_PI * 30 * t)) if burst else 0.0
        env = math.exp(-(t if t < 0.11 else t - 0.12) / 0.07)
        out.append(math.tanh(3 * math.sin(TWO_PI * ph)) * amp * env)
    return norm(out, 100)


def blues():
    s = Song("bayou blues", 24, 26, gain=1.15)
    HARP = s.add(Inst("harmonica", harmonica(), 24, 4, loop=(640, 32)))
    CH7 = s.add(Inst("tremolo 7th", seventh_chord(), 18, 3, loop=(1024, 256)))
    SLIDE = s.add(Inst("slide guitar", slide_guitar(), 24, 4))
    BASS = s.add(Inst("upright", upright_bass(), 38, 2))
    KICK = s.add(Inst("kick", hm.kick(), 26, 4))
    BRUSH = s.add(Inst("brush", hm.snare(13), 16, 4))
    HAT = s.add(Inst("shuffle", hm.hat(17), 10, 4))
    CROAK = s.add(Inst("croak", croak(), 26, 4))
    H, G, B, D = 3, 0, 1, 2

    A7, D7, E7 = "a", "d", "e"
    chorus1 = [A7, A7, A7, A7, D7, D7, A7, A7, E7, D7, A7, E7]
    chorus2 = [A7, D7, A7, A7, D7, D7, A7, A7, E7, D7, A7, E7]
    chords = [A7, E7] + chorus1 + chorus2
    # intro: the harmonica wails over a tremolo A7, the frog croaks
    s.line(H, HARP, 0, "g4.1~ a4.17^ e4.6", vib=0x44, slide=0x02)
    s.line(H, HARP, s.bar(1), "b4.4 g4.2 e4.6 r.12", vib=0x44)
    tune = ["c5.1~ c#5.9^ a4.2 c5.4 a4.2 g4.6", "a4.12^ r.6 e4.2 g4.4", "c5.1~ c#5.5 e5.4 c#5.2 e5.6 g5.4 e5.2",
            "a5.4 g5.2 e5.4 c#5.2 a4.12^", "d5.10^ f#5.2 a5.4 f#5.2 d5.6", "c5.4 a4.2 c5.4 d5.2 d#5.1~ e5.11^",
            "c#5.4 a4.2 c#5.4 e5.2 a5.12^", "g5.4 e5.2 c5.4 a4.2 r.12", "b4.4 e5.2 g#5.4 b5.2 a5.6 g5.6",
            "f#5.6 d5.4 a4.2 c5.12^", "a4.4 c5.2 c#5.4 e5.2 a4.12^", "e5.4 d5.2 c5.4 b4.2 g#4.6 b4.6"]
    calls = ["e5.4 g5.2 a5.6^ r.12", "a5.4 f#5.2 d5.6^ r.12", "c5.1~ c#5.5 e5.6^ r.12", "g5.4 e5.2 c#5.6 a4.12^",
             "a5.4 f#5.2 d5.6 r.12", "c5.4 d5.2 d#5.1~ e5.5 r.12", "c#5.4 e5.2 a5.6^ r.12",
             "a5.2 g5.2 e5.2 c5.2 a4.4 g4.12^", "b5.6^ g#5.2 e5.4 r.12", "a5.6 f#5.2 c5.4 r.12",
             "e5.4 c#5.2 a4.6 r.12", "e5.4 d5.2 b4.4 g#4.2 e4.12^"]
    answers = {0: "e4.1~ a4.5 g4.2 e4.4", 1: "f#4.1~ a4.5 f#4.2 d4.4", 2: "g4.1~ a4.5 c#5.2 a4.4",
               4: "c4.1~ d4.5 f#4.2 a4.4", 5: "a4.1~ c5.5 a4.2 f#4.4", 6: "e4.1~ g4.5 a4.2 c#5.4",
               8: "d5.1~ e5.5 d5.2 b4.4", 9: "a4.1~ c5.5 a4.2 f#4.4", 10: "c#5.1~ e5.5 c#5.2 a4.4"}
    for k in range(12):
        s.line(H, HARP, s.bar(2 + k), tune[k], vib=0x44, slide=0x02)
        s.line(H, HARP, s.bar(14 + k), calls[k], vib=0x44, slide=0x02)
        if k in answers:
            s.line(G, SLIDE, s.bar(14 + k) + 12, answers[k], slide=0x04)
    BOOGIE = [0, 4, 7, 9, 10, 9, 7, 4]
    for b, root in enumerate(chords):
        r0 = s.bar(b)
        R = fit(midi(root + "1"), midi("a1"), midi("g#2"))
        c3 = fit(midi(root + "3"), midi("a2"), midi("g#3"))
        # the tremolo guitar: held with tremolo in the intro; 'chank' on beats 2 and 4 in the first chorus; a held
        # stab before the slide answers in the second
        if b == 0:
            s.note(r0, G, CH7, c3, 1.0)
            for k in range(1, 24):
                s.fx(r0 + k, G, 7, 0x64)
        elif b < 14 or (b - 14) not in answers:
            s.off(r0, G)
            for rr in (6, 18):
                s.note(r0 + rr, G, CH7, c3, 0.9 if rr == 6 else 0.8)
                s.fx(r0 + rr + 1, G, 7, 0x84)
                s.fx(r0 + rr + 2, G, 7, 0x84)
                s.off(r0 + rr + 4, G)
        else:
            s.note(r0, G, CH7, c3, 0.85)
            for k in range(1, 9):
                s.fx(r0 + k, G, 7, 0x64)
            s.off(r0 + 10, G)
        # the boogie bass: long-short shuffle 8ths, root third fifth sixth seventh sixth fifth third
        if b >= 1:
            for k, st in enumerate(BOOGIE):
                s.note(r0 + 6 * (k // 2) + (4 if k % 2 else 0), B, BASS, R + st, 1.0 if k % 2 == 0 else 0.75)
        # the drums: kick 1 and 3, brushes 2 and 4, the shuffle hat on the 'let'; the frog in the gaps
        if b == 0:
            for rr, v in ((0, 1.0), (3, 0.8), (12, 0.9), (15, 0.7)):
                s.note(r0 + rr, D, CROAK, "c4" if rr < 12 else "a#3", v)
            continue
        gap = (b in (9, 21)) or b == 1
        for rr in range(0, 24, 2):
            if rr in (0, 12):
                s.note(r0 + rr, D, KICK, "c4", 1.0 if rr == 0 else 0.85)
            elif rr in (6, 18):
                s.note(r0 + rr, D, BRUSH, "c4", 1.0)
            elif gap and rr in (14, 20):
                s.note(r0 + rr, D, CROAK, "c4" if rr == 14 else "a#3", 0.9)
            elif rr % 6 == 4:
                s.note(r0 + rr, D, HAT, "c5", 0.9)
            elif rr % 6 == 2 and b >= 14:
                s.note(r0 + rr, D, HAT, "c5", 0.5)
    return s.build(6, 120, 2)


# ---- alt2: Lily-Pad Bossa ------------------------------------------------------------------------------------------
VOICINGS = {"M7": [0, 7, 11, 16], "m7": [0, 7, 10, 15], "7": [0, 7, 10, 16]}


def nylon(kind):
    """A nylon-string chord, strummed with the thumb: four plucked strings 10 ms apart, soft harmonics; the root on a
    64-sample cycle (C-2 = C3), one-shot."""
    out = [0.0] * 4200
    for s_i, semi in enumerate(VOICINGS[kind]):
        f = 2 ** (semi / 12.0) / 64.0
        start = s_i * 85
        for i in range(len(out) - start):
            v = 0.0
            for k in range(1, 6):
                v += math.sin(TWO_PI * k * f * i + k) / k ** 1.4 * math.exp(-i * k ** 0.6 / 1700.0)
            out[start + i] += v * min(1.0, i / 12.0) * (1.0 if s_i else 1.15)
    return norm(out, 100)


def flute():
    """A breathy flute: a chiff of noise over 16 cycles, then one looped 32-sample cycle (nearly a sine)."""
    rng = random.Random(4)
    out = []
    for i in range(544):
        ph = TWO_PI * i / 32.0
        v = math.sin(ph) + 0.1 * math.sin(2 * ph + 0.5) + 0.04 * math.sin(3 * ph)
        if i < 512:
            v = v * min(1.0, i / 200.0) + 0.3 * rng.uniform(-1, 1) * math.exp(-i / 140.0)
        out.append(v)
    return norm(out, 96, fade=0)


def rim():
    rng = random.Random(8)
    return norm([(math.sin(TWO_PI * i * 0.21) + 0.6 * rng.uniform(-1, 1)) * math.exp(-i / 45.0) for i in range(300)], 100)


def shaker():
    rng = random.Random(31)
    out, prev = [], 0.0
    for i in range(420):
        x = rng.uniform(-1, 1)
        out.append((x - prev) * min(1.0, i / 40.0) * math.exp(-i / 120.0))
        prev = x
    return norm(out, 80)


def guiro():
    """A guiro scrape, the frog's croak in the band: noise ratcheting at 34 Hz, a little body."""
    rng = random.Random(19)
    out, lp = [], 0.0
    for i in range(2200):
        t = i / 8287.0
        lp += (rng.uniform(-1, 1) - lp) * 0.5
        tooth = 1.0 if (t * 34) % 1.0 < 0.45 else 0.15
        out.append((lp + 0.4 * math.sin(TWO_PI * 210 * t)) * tooth * min(1.0, i / 60.0) * math.exp(-t / 0.18))
    return norm(out, 90)


def bossa():
    s = Song("lily-pad bossa", 16, 34, gain=1.16)
    FL = s.add(Inst("flute", flute(), 22, 4, loop=(512, 32)))
    GT = {k: s.add(Inst("nylon " + k, nylon(k), 24, 3)) for k in ("M7", "m7", "7")}
    BASS = s.add(Inst("upright", upright_bass(7000, soft=True), 36, 2))
    RIM = s.add(Inst("rim", rim(), 16, 4))
    SHK = s.add(Inst("shaker", shaker(), 9, 4))
    GUI = s.add(Inst("guiro", guiro(), 18, 4))
    F, G, B, D = 3, 0, 1, 2

    Dm7, Em7, A7, Fsm7, B7, Gm7_, Gm7 = ("d", "M7"), ("e", "m7"), ("a", "7"), ("f#", "m7"), ("b", "7"), ("g", "M7"), \
        ("g", "m7")
    sec_a = [Dm7, Dm7, Em7, A7, Fsm7, B7, Em7, A7]
    sec_b = [Gm7_, Gm7, Fsm7, B7, Em7, A7, Dm7, A7]
    chords = [Gm7_, A7] + sec_a + sec_b + sec_a + sec_b
    mel_a = ["r.2 a4.1 a4.1 f#5.6 e5.2 c#5.4", "d5.10^ r.6", "r.2 b4.1 b4.1 g5.6 f#5.2 d5.4", "e5.10^ r.2 c#5.2 e5.2",
             "r.2 c#5.1 c#5.1 a5.6 f#5.2 e5.4", "d#5.8^ r.2 f#5.2 a5.4", "g5.6 f#5.2 e5.4 d5.4", "c#5.8^ e5.4 a4.4"]
    mel_b = ["b5.6 a5.2 f#5.8^", "bb5.6 a5.2 f5.8^", "a5.4 e5.4 c#5.8^", "d#5.4 f#5.4 b5.8^", "g5.6 e5.2 b4.4 d5.4",
             "c#5.6 e5.2 g5.8^", "f#5.12^ e5.2 d5.2", "e5.6 r.2 a4.1 a4.1 c#5.6"]
    mel_a2 = ["r.2 a4.1 a4.1 f#5.6 e5.2 c#5.4", "d5.6 e5.2 f#5.2 a5.2 b5.4", "r.2 b4.1 b4.1 g5.6 f#5.2 d5.4",
              "e5.6 g5.2 b5.4 a5.4", "r.2 c#5.1 c#5.1 a5.6 f#5.2 e5.4", "d#5.4 f#5.2 a5.2 b5.8^",
              "g5.4 b5.4 a5.2 g5.2 e5.4", "c#5.6 d5.2 e5.8^"]
    mel_b2 = mel_b[:6] + ["d5.16^", mel_b[7]]
    for k in range(8):
        s.line(F, FL, s.bar(2 + k), mel_a[k], vib=0x23)
        s.line(F, FL, s.bar(10 + k), mel_b[k], vib=0x23)
        s.line(F, FL, s.bar(18 + k), mel_a2[k], vib=0x23)
        s.line(F, FL, s.bar(26 + k), mel_b2[k], vib=0x23)
    COMP = ([0, 3, 6, 10, 12], [2, 5, 8, 12])
    CLAVE = ([0, 6, 12], [4, 10])
    for b, (root, q) in enumerate(chords):
        r0 = s.bar(b)
        gr = fit(midi(root + "3"), midi("a2"), midi("g#3"))
        R = fit(midi(root + "2"), midi("a1"), midi("g#2"))
        nxt = chords[(b + 1) % len(chords)] if b + 1 < len(chords) else chords[2]
        # the guitar: the bossa comp (sparser in A': long chords on 1 and the 'and' of 2)
        hits = [0, 6] if 18 <= b < 26 else COMP[b % 2]
        for rr in hits:
            s.note(r0 + rr, G, GT[q], gr, 1.0 if rr in (0, 2) else 0.8)
        # the bass: the root on 1, the fifth on the 'and' of 2 and on 3, a step towards the next root
        if b >= 0:
            n2 = fit(midi(nxt[0] + "2"), midi("a1"), midi("g#2"))
            for rr, n, v in ((0, R, 1.0), (6, R + 7, 0.75), (8, R + 7, 0.9), (14, n2 + (2 if n2 < R else -1), 0.6)):
                if b < 1 and rr < 8:
                    continue
                s.note(r0 + rr, B, BASS, n, v)
        # the percussion: a shaker on the 16ths, rim clicks on the bossa clave, the guiro croaks now and then
        croak_bar = b in (0, 1) or b % 4 == 3
        for rr in range(16):
            if croak_bar and rr == 12:
                s.note(r0 + rr, D, GUI, "c4" if b % 8 != 7 else "a3", 1.0)
            elif croak_bar and rr in (13, 14, 15):
                continue
            elif rr in CLAVE[b % 2] and b >= 1:
                s.note(r0 + rr, D, RIM, "c4", 1.0)
            elif b >= 1 or rr >= 8:
                s.note(r0 + rr, D, SHK, "c5", 1.0 if rr % 4 == 2 else 0.6 if rr % 2 == 0 else 0.4)
        if b == 0:
            s.note(r0, D, GUI, "c4", 1.0)
    return s.build(6, 132, 2)


TRACKS = {"alt1": blues, "alt2": bossa}


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
