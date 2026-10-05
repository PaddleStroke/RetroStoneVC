#!/usr/bin/env python3
"""Hedgehog Hills alternative music: two 4-channel ProTracker MODs, for the owner to choose from (the game still plays
tools/make_music.py's tune.mod until it is switched). Written with the house MOD writer
(games/common/tools/house_music.py) and instruments synthesised here, as Pogo Mamie, Beaver Rush and Duck Parade do;
the mix sits at the house level (the music is played at HA_MUSIC_VOL like every house track).

  alt1  "Bramble Bounce": a bouncy chiptune: a 12.5% pulse lead, 25% pulse arpeggio chords (tracker arpeggios), a
        stepped triangle bass bouncing in octaves and noise drums; A major, 150 BPM. Hook: the roll down a hill and
        the launch, a quick falling run then a leap up, with a pitch slide on the big jump. Intro, A, B (call and
        response between the two pulses), A' in thirds, a minor 'frost' section in F# minor, a snare-roll build.
  alt2  "Homeward Jig": a cosy fireside jig in 6/8: a tin whistle, a strummed steel-string guitar, an upright bass and
        a bodhran; E dorian (the C# of the A chord), 90 dotted-quarter beats a minute. Hook: the whistle climbs the
        hill (E G B E') and runs back down; A, A' with rolls, a higher B, B', and a slow fireside interlude.

    make_music_alt.py [OUTDIR]                 -> OUTDIR/alt1.mod, alt2.mod (default: games/hedgehoghills/music-alt)
    make_music_alt.py --as-tune alt1 OUTDIR    -> OUTDIR/tune.mod (what build_assets.py packs)

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/hedgehoghills/LICENSE.
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


def harmony(m, chord):
    """a second voice under note m: the nearest chord tone a third, a sixth or a fourth below"""
    root, q = chord
    pcs = {(midi(root + "0") + t) % 12 for t in QUAL[q]}
    for d in (3, 4, 8, 9, 5):
        if (m - d) % 12 in pcs:
            return m - d
    return m - 5


# ---- alt1: Bramble Bounce ------------------------------------------------------------------------------------------
def pulse(n, duty, amp=40):
    """a pulse wave with no DC offset: duty 0.125 the thin lead, 0.25 the arpeggios"""
    hi = int(round(n * duty))
    return [s8(2 * amp * (1 - duty)) if i < hi else s8(-2 * amp * duty) for i in range(n)]


def triangle():
    """a 4-bit stepped triangle (64-sample cycle), the console bass"""
    out = []
    for i in range(64):
        t = i / 64.0
        tri = 4 * t - 1 if t < 0.5 else 3 - 4 * t
        out.append(s8(round(tri * 7.5) / 7.5 * 90))
    return out


def lfsr_noise(n, short=False, seed=1):
    r, out = seed, []
    for _ in range(n):
        bit = (r ^ (r >> (6 if short else 1))) & 1
        r = (r >> 1) | (bit << 14)
        out.append(1.0 if r & 1 else -1.0)
    return out


def chip_kick():
    out, ph = [], 0.0
    for i in range(1300):
        ph += (55 + 260 * math.exp(-i / 160.0)) / 8287.0
        out.append((1.0 if math.sin(TWO_PI * ph) > 0 else -1.0) * math.exp(-i / 420.0))
    return norm(out, 90)


def chip_snare():
    nz = lfsr_noise(1100, seed=77)
    return norm([(0.8 * nz[i] + 0.5 * (1 if (i // 21) % 2 else -1) * math.exp(-i / 120.0)) * math.exp(-i / 300.0)
                 for i in range(1100)], 90)


def chip_hat():
    nz = lfsr_noise(300, short=True, seed=93)
    return norm([nz[i] * math.exp(-i / 55.0) for i in range(300)], 80)


def chiptune():
    s = Song("bramble bounce", 16, 36, gain=1.35)
    LEAD = s.add(Inst("pulse 12", pulse(16, 0.125), 20, 5, loop=(0, 16)))
    ARP = s.add(Inst("pulse 25", pulse(32, 0.25), 15, 4, loop=(0, 32)))
    TRI = s.add(Inst("triangle", triangle(), 40, 3, loop=(0, 64)))
    KICK = s.add(Inst("chip kick", chip_kick(), 30, 4))
    SNR = s.add(Inst("chip snare", chip_snare(), 20, 4))
    HAT = s.add(Inst("chip hat", chip_hat(), 12, 4))
    L, A, B, D = 3, 0, 1, 2

    Am, E, Fsm, Dm, Csm, C7, Bm = ("a", "M"), ("e", "M"), ("f#", "m"), ("d", "M"), ("c#", "m"), ("c#", "M"), ("b", "m")
    sec_a = [Am, E, Fsm, Dm, Am, E, Dm, E]
    sec_b = [Dm, E, Csm, Fsm, Dm, E, Am, Am]
    sec_c = [Fsm, Dm, E, C7, Fsm, Dm, Bm, E]
    chords = [Dm, E] + sec_a + sec_b + sec_a + sec_c + [Dm, E]
    mel_a = ["e5.2 c#5.1 a4.1 c#5.2 e5.2 a5.4 g#5.2 a5.2", "b5.6 g#5.2 e5.4 b4.4",
             "c#5.2 a4.1 f#4.1 a4.2 c#5.2 f#5.4 e5.2 f#5.2", "a5.6 f#5.2 d5.4 a4.4",
             "e5.2 c#5.1 a4.1 c#5.2 e5.2 a5.2 b5.2 c#6.4", "b5.4 g#5.2 b5.1~ e6.9^",
             "d6.2 c#6.2 b5.2 a5.2 f#5.2 a5.2 b5.4", "g#5.4 b5.4 e5.8"]
    mel_a2 = mel_a[:7] + ["e5.2 g#5.2 b5.2 e6.2 d6.4 c#6.4"]
    mel_b = ["f#5.2 a5.2 d6.4 r.8", "g#5.2 b5.2 e6.4 r.8", "e5.2 g#5.2 c#6.4 b5.2 g#5.2 e5.4", "f#5.8^ e5.4 c#5.4",
             "a5.2 f#5.2 d5.4 r.8", "b5.2 g#5.2 e5.4 r.8", "c#6.4 b5.2 a5.2 e5.4 c#5.4", "a5.12^ r.4"]
    resp_b = {0: "r.8 d5.2 c#5.2 a4.4", 1: "r.8 e5.2 d5.2 b4.4", 4: "r.8 d4.2 f#4.2 a4.4", 5: "r.8 e4.2 g#4.2 b4.4"}
    mel_c = ["c#6.8^ b5.4 a5.4", "f#5.8^ a5.4 d6.4", "b5.8^ g#5.4 e5.4", "f5.4 g#5.4 c#6.8^", "c#6.8^ d6.4 c#6.4",
             "a5.8^ b5.4 a5.4", "f#5.8^ d5.4 b4.4", "e5.4 g#5.4 b5.4 d6.4"]
    build = ["f#5.2! f#5.2 r.2 f#5.2 r.2 f#5.2 g#5.2 a5.2", "b5.2! b5.2 r.2 b5.2 r.2 d6.2 c#6.2 b5.2"]
    s.line(L, LEAD, s.bar(1), "b4.2 e5.2 g#5.2 b5.1~ e6.7^ r.2", slide=0x08)
    for k in range(8):
        s.line(L, LEAD, s.bar(2 + k), mel_a[k], slide=0x08)
        s.line(L, LEAD, s.bar(10 + k), mel_b[k])
        if k in resp_b:
            s.line(A, ARP, s.bar(10 + k), resp_b[k])
        s.line(L, LEAD, s.bar(18 + k), mel_a2[k], slide=0x08)
        s.line(L, LEAD, s.bar(26 + k), mel_c[k], vib=0x46)
    for k in range(2):
        s.line(L, LEAD, s.bar(34 + k), build[k])
    # A': the arpeggio pulse plays the tune in thirds and sixths below (an octave down at the top)
    for k, txt in enumerate(mel_a2):
        row = s.bar(18 + k)
        for tok in txt.split():
            name, ln = tok.split(".")[0], int(re.match(r"\d+", tok.split(".")[1]).group())
            if name != "r":
                h = harmony(midi(name), sec_a[k])
                s.note(row, A, ARP, h - 12 if h > midi("b5") else h, 0.85)
            row += ln
    for b, (root, q) in enumerate(chords):
        r0 = s.bar(b)
        R = fit(midi(root + "2"), midi("e2"), midi("d#3"))
        ar = fit(midi(root + "3"), midi("a3"), midi("g#4"))
        arp = 0x37 if q == "m" else 0x47
        sec = "intro" if b < 2 else "A" if b < 10 or 18 <= b < 26 else "B" if b < 18 else "C" if b < 34 else "build"
        # the chords: tracker arpeggios on a 3+3+2 rhythm (not where the pulse answers or harmonises)
        if sec in ("intro", "A", "B") and not (18 <= b < 26) and not (sec == "B" and (b - 10) in resp_b):
            for rr, ln in ((0, 3), (3, 3), (6, 2), (8, 3), (11, 3), (14, 2)):
                s.note(r0 + rr, A, ARP, ar, 1.0 if rr in (0, 8) else 0.7)
                s.fx(r0 + rr, A, 0, arp)
                for k in range(1, ln):
                    s.fx(r0 + rr + k, A, 0, arp)
        elif sec == "C":         # frost: a fast broken chord, up and down
            tones = [ar + t for t in QUAL[q][:3]] + [ar + 12]
            for k in range(16):
                s.note(r0 + k, A, ARP, tones[[0, 1, 2, 3, 2, 1, 2, 1][k % 8]], 0.8 if k % 4 else 1.0)
        elif sec == "build":     # a rising arpeggio, an octave a bar
            tones = [ar - 12 + t for t in QUAL[q][:3]] + [ar]
            for k in range(16):
                s.note(r0 + k, A, ARP, tones[k % 4] + 12 * (k // 8), 0.6 + 0.025 * k)
        # the triangle bass: octave bounces (A), root and fifth (B), driving roots (C), 16ths (build)
        if b >= 1:
            for k in range(8):
                rr = 2 * k
                if sec == "B":
                    n = R + [0, None, 7, 12, 0, None, 7, 12][k] if k not in (1, 5) else None
                elif sec == "C":
                    n = R + (7 if k == 7 else 0)
                elif sec == "build" or b == 1:
                    n = R + (12 if k % 2 else 0)
                    s.note(r0 + rr + 1, B, TRI, R + 12 if k % 2 == 0 else R, 0.8)
                else:
                    n = R + (12 if k % 2 else 0)
                if n is not None:
                    s.note(r0 + rr, B, TRI, n, 1.0 if k % 2 == 0 else 0.85)
                    if sec != "build" and b != 1:
                        s.fx(r0 + rr + 1, B, 0xC, 0)        # staccato: a rest on the off-row
        # the drums
        for rr in range(16):
            if b == 0:
                pat = {0: KICK, 4: HAT, 8: KICK, 12: HAT, 14: HAT, 15: HAT}
            elif sec == "C":
                pat = {0: KICK, 2: HAT, 3: KICK, 4: SNR, 6: HAT, 8: KICK, 10: HAT, 11: KICK, 12: SNR, 14: HAT, 15: SNR}
            elif sec == "build" or b == 1:
                pat = {r: (KICK if r % 4 == 0 else SNR) for r in range(0, 16, 2 if (b in (1, 34)) else 1)}
            else:
                pat = {0: KICK, 2: HAT, 4: SNR, 6: HAT, 7: KICK, 10: KICK, 12: SNR, 14: HAT, 15: HAT}
                if b % 8 == 1 and rr >= 12:
                    pat = {12: SNR, 13: SNR, 14: SNR, 15: SNR}
            if rr in pat:
                inst = pat[rr]
                v = 1.0 if rr % 4 == 0 else 0.75
                if sec == "build" or b == 1:
                    v = 0.5 + 0.03 * rr + (0.2 if b in (1, 35) else 0)
                s.note(r0 + rr, D, inst, "c5" if inst is HAT else "c4", min(1.2, v))
    return s.build(6, 150, 2)


# ---- alt2: Homeward Jig --------------------------------------------------------------------------------------------
def whistle():
    """A tin whistle: nearly a sine, a breathy chiff over 24 cycles, then one looped 16-sample cycle (C-2 = C5)."""
    rng = random.Random(14)
    out = []
    for i in range(400):
        ph = TWO_PI * i / 16.0
        v = math.sin(ph) + 0.08 * math.sin(2 * ph + 0.4) + 0.05 * math.sin(3 * ph)
        if i < 384:
            v = v * min(1.0, i / 90.0) + 0.3 * rng.uniform(-1, 1) * math.exp(-i / 90.0)
        out.append(v)
    return norm(out, 100, fade=0)


STRUM = {"M": [0, 7, 12, 16, 19], "m": [0, 7, 12, 15, 19]}


def steel_guitar(kind):
    """A strummed steel-string chord: five strings 7 ms apart, bright plucks; the root on a 64-sample cycle."""
    out = [0.0] * 5000
    for s_i, semi in enumerate(STRUM[kind]):
        f = 2 ** (semi / 12.0) / 64.0
        start = s_i * 60
        for i in range(len(out) - start):
            v = 0.0
            for k in range(1, 8):
                v += math.sin(TWO_PI * k * f * i + 0.7 * k) / k ** 1.1 * math.exp(-i * k ** 0.7 / 2000.0)
            out[start + i] += v * min(1.0, i / 6.0)
    return norm(out, 100)


def upright_bass(n=7000):
    out = []
    for i in range(n):
        ph = TWO_PI * i / 128.0
        v = math.sin(ph) + 0.25 * math.sin(2 * ph) * math.exp(-i / 900.0) + 0.1 * math.sin(3 * ph) * math.exp(-i / 500.0)
        out.append(v * math.exp(-i / 3000.0) * min(1.0, i / 12.0))
    return norm(out, 104)


def bodhran(low=True):
    """The frame drum: a deep skin (a sine falling onto ~75 Hz) or the tipper's light tap, with skin noise."""
    rng = random.Random(51 if low else 52)
    out, ph, lp = [], 0.0, 0.0
    n = 2400 if low else 800
    for i in range(n):
        f = (72 + 40 * math.exp(-i / 200.0)) if low else (170 + 60 * math.exp(-i / 60.0))
        ph += f / 8287.0
        lp += (rng.uniform(-1, 1) - lp) * 0.3
        out.append(math.sin(TWO_PI * ph) * math.exp(-i / (700.0 if low else 160.0)) +
                   (0.35 if low else 0.6) * lp * math.exp(-i / (60.0 if low else 40.0)))
    return norm(out, 110 if low else 90)


def jig():
    s = Song("homeward jig", 12, 40, gain=1.13)
    WH = s.add(Inst("whistle", whistle(), 22, 5, loop=(384, 16)))
    GT = {k: s.add(Inst("guitar " + k, steel_guitar(k), 22, 3)) for k in ("M", "m")}
    BASS = s.add(Inst("upright", upright_bass(), 36, 2))
    LOW = s.add(Inst("bodhran", bodhran(True), 30, 4))
    TIP = s.add(Inst("tip", bodhran(False), 18, 4))
    W, G, B, D = 3, 0, 1, 2

    Em, Dc, Ac, Gc = ("e", "m"), ("d", "M"), ("a", "M"), ("g", "M")
    # one or two chords a bar (a pair: the first and the second dotted quarter)
    sec_a = [[Em], [Dc], [Em], [Ac], [Em], [Dc], [Gc, Ac], [Em]]
    sec_b = [[Gc], [Dc], [Ac], [Em], [Gc], [Dc], [Ac], [Em]]
    inter = [[Em], [Dc], [Em], [Dc], [Gc], [Ac]]
    bars = [[Em], [Em]] + sec_a + sec_a + sec_b + sec_b + inter
    mel_a = ["e5.2 g5.2 b5.2 e6.4 d6.2", "d6.2 a5.2 f#5.2 d5.4 f#5.2", "e5.2 g5.2 b5.2 a5.2 g5.2 f#5.2",
             "e5.2 c#5.2 e5.2 a5.6^", "b5.2 e6.2 b5.2 g5.2 b5.2 g5.2", "a5.2 d6.2 a5.2 f#5.2 e5.2 d5.2",
             "b4.2 d5.2 g5.2 c#5.2 e5.2 a5.2", "b5.4 g5.2 e5.6^"]
    mel_a2 = ["e5.2 g5.2 b5.2 e6.4 d6.2", "d6.1 e6.1 d6.2 a5.2 f#5.2 d5.2 f#5.2", "e5.2 g5.2 b5.2 a5.2 g5.2 f#5.2",
              "e5.2 c#5.2 e5.2 a5.2 c#6.2 e6.2", "f#6.2 e6.2 b5.2 g5.2 b5.2 g5.2", "a5.2 d6.2 a5.2 f#5.2 e5.2 d5.2",
              "b4.2 d5.2 g5.2 a5.2 c#6.2 e6.2", "e6.6^ e5.6"]
    mel_b = ["d6.4 b5.2 g5.2 b5.2 d6.2", "f#6.4 d6.2 a5.2 d6.2 f#6.2", "e6.4 c#6.2 a5.2 c#6.2 e6.2",
             "g6.6^ f#6.2 e6.2 d6.2", "b5.2 g5.2 b5.2 d6.2 b5.2 g5.2", "a5.2 f#5.2 a5.2 d6.2 a5.2 f#5.2",
             "e5.2 a5.2 c#6.2 e6.2 c#6.2 a5.2", "b5.6^ e5.6"]
    mel_b2 = ["d6.4 b5.2 g5.2 a5.2 b5.2", "a5.4 f#5.2 d5.2 e5.2 f#5.2", "e5.4 c#5.2 a4.2 c#5.2 e5.2",
              "g5.6^ f#5.2 e5.2 d5.2", "b5.2 g5.2 b5.2 d6.2 g6.2 d6.2", "f#6.2 d6.2 a5.2 f#5.2 a5.2 d6.2",
              "c#6.2 e6.2 a6.2 e6.2 c#6.2 a5.2", "b5.4 g5.2 e5.6^"]
    mel_i = ["e5.12^", "f#5.12^", "g5.6 b5.6^", "a5.12^", "b5.6 d6.6^", "c#6.6 e6.6^"]
    for k in range(8):
        s.line(W, WH, s.bar(2 + k), mel_a[k], vib=0x33)
        s.line(W, WH, s.bar(10 + k), mel_a2[k], vib=0x33)
        s.line(W, WH, s.bar(18 + k), mel_b[k], vib=0x33)
        s.line(W, WH, s.bar(26 + k), mel_b2[k], vib=0x33)
    for k in range(6):
        s.line(W, WH, s.bar(34 + k), mel_i[k], vib=0x24, vel=0.85)
    for b, chs in enumerate(bars):
        r0 = s.bar(b)
        fire = b >= 34
        halves = chs if len(chs) == 2 else chs * 2
        for h, (root, q) in enumerate(halves):
            rr = 6 * h
            gr = fit(midi(root + "3"), midi("c3"), midi("b3"))
            R = fit(midi(root + "2"), midi("a1"), midi("g#2"))
            # the guitar: down on the beat, up on the third eighth (only the downstrokes by the fire)
            if b >= 1:
                s.note(r0 + rr, G, GT[q], gr, 1.0 if h == 0 else 0.85)
                if not fire:
                    s.note(r0 + rr + 4, G, GT[q], gr, 0.55)
            # the bass: the root on each dotted quarter (the fifth on the second, unless the chord changes); a
            # passing note in B
            if b >= 1 and not (fire and h == 1):
                n = R if (h == 0 or len(chs) == 2) else R + 7
                if n > midi("b2"):
                    n -= 12
                s.note(r0 + rr, B, BASS, n, 1.0 if h == 0 else 0.8)
                if 18 <= b < 34 and h == 1:
                    s.note(r0 + rr + 4, B, BASS, R + 4 if q == "M" else R + 3, 0.6)
        # the bodhran: DUM ta ta DUM ta ta, a triplet flick at the end of each phrase; soft by the fire
        for rr in range(0, 12, 2):
            if fire:
                if rr in (0, 6):
                    s.note(r0 + rr, D, LOW, "c4", 0.7 if rr == 0 else 0.5)
                continue
            if b == 0 and rr < 6:
                continue
            if rr in (0, 6):
                s.note(r0 + rr, D, LOW, "c4" if rr == 0 else "a#3", 1.0 if rr == 0 else 0.8)
            else:
                s.note(r0 + rr, D, TIP, "c4", 0.8 if rr in (2, 8) else 0.6)
        if b % 8 == 1 and not fire:
            for rr, v in ((9, 0.5), (10, 0.65), (11, 0.8)):
                s.cells[r0 + rr][D] = [0, 0, 0, 0]
                s.note(r0 + rr, D, TIP, "d4", v)
    return s.build(6, 135, 2)


TRACKS = {"alt1": chiptune, "alt2": jig}


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
