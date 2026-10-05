#!/usr/bin/env python3
"""Otter Rapids alternative music: two 4-channel ProTracker MODs written with the house music kit
(games/common/tools/house_music.py: the MOD writer and periods; docs/art-direction.md "Audio"), the instruments
synthesised here like Mouse Espresso's or Spider Swing's alternatives, at the house mix (sample volumes <= 40).

  alt1  "Otter Breakdown": a fiddle-led bluegrass breakdown in cut time, a crotchet of 232, A major with a
        mixolydian G and an F sharp minor tag. A fiddle over a flatpicked boom-chuck guitar, an upright bass and a
        mandolin chop; a dobro slides in as the countermelody and takes a break. No banjo. The hook: the fiddle's
        rolling river lick (C sharp-E-A, held, then down through the flat-seven G). Intro (the fiddle's shuffle
        kick-off), A, A' (a variation, the dobro answering), B (the high part, from D), B' (the dobro's break over
        the fiddle's double stops), a tag in F sharp minor (tremolo bowing, a walking bass), A'' (fiddle and dobro
        in twin harmony), then the loop back to A.
  alt2  "Kawauso": a river piece for koto and shakuhachi, 4/4 at a crotchet of 96, E miyako-bushi (E F A B C)
        with the rapids pushing to 108. A koto ripples in sixteenths like the current, a bass koto steps beneath,
        the shakuhachi sings with bends and breath; taiko, shime-daiko, a small bell and water drops. The hook: the
        shakuhachi's sigh (E, a bend up to F and back, then down to C and B). Intro (the koto ripple and the drops),
        A, B (the koto takes the tune with string bends, the shakuhachi holds long notes), the rapids (faster, koto
        runs, taiko eighths, the shakuhachi cries high), A' (the ripple flows back down), then the loop back to A.

    make_music_alt.py              -> ../music-alt/alt1.mod, alt2.mod (previews; the game is unchanged)
    make_music_alt.py OUTDIR N     -> OUTDIR/tune.mod: alternative N (1 or 2), for tools/build_assets.py

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/otterrapids/LICENSE.
"""
import math
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "common", "tools"))
import house_music as hm  # noqa: E402
from house_music import s8  # noqa: E402

# ---- the score engine: rows of 4 cells, melodies as text --------------------------------------------------------
NOTE_PC = {"c": 0, "d": 2, "e": 4, "f": 5, "g": 7, "a": 9, "b": 11}
NAMES = ["c", "c#", "d", "d#", "e", "f", "f#", "g", "g#", "a", "a#", "b"]
QUAL = {"": [0, 4, 7], "m": [0, 3, 7], "7": [0, 4, 7, 10], "maj7": [0, 4, 7, 11], "m7": [0, 3, 7, 10],
        "m6": [0, 3, 7, 9], "6": [0, 4, 7, 9], "dim7": [0, 3, 6, 9], "m7b5": [0, 3, 6, 10], "sus4": [0, 5, 7],
        "7sus4": [0, 5, 7, 10]}


def midi(name):
    """A note name ('c4' = middle C = 60, 'f#5', 'bb3') -> a MIDI number; numbers pass through."""
    if isinstance(name, int):
        return name
    pc, i = NOTE_PC[name[0]], 1
    while name[i] in "#b":
        pc += 1 if name[i] == "#" else -1
        i += 1
    return pc + 12 * (int(name[i:]) + 1)


def name_of(m):
    return NAMES[m % 12] + str(m // 12 - 1)


def chord(name):
    """'F#m7' -> (root pitch class, 'm7', bass pitch class); 'Dm/C' has the bass C."""
    bass = None
    if "/" in name:
        name, b = name.split("/")
        bass = midi(b.lower() + "0") % 12
    pc, i = NOTE_PC[name[0].lower()], 1
    while i < len(name) and name[i] in "#b":
        pc += 1 if name[i] == "#" else -1
        i += 1
    pc %= 12
    return pc, name[i:], pc if bass is None else bass


def tones(name):
    pc, q, _ = chord(name)
    return {(pc + k) % 12 for k in QUAL[q]}


def fit(pc, lo):
    """The pitch of class pc in [lo, lo + 11]."""
    return lo + (pc - lo) % 12


def below(m, pcs, gap=3):
    """The nearest chord tone at least `gap` semitones below m (a harmony voice)."""
    m -= gap
    while m % 12 not in pcs:
        m -= 1
    return m


def harmony(bars, chords, gap=3):
    """A second voice: each note of the bars moved down to the nearest chord tone (rests and holds kept)."""
    out = []
    for text, ch in zip(bars, chords):
        pcs, toks = tones(ch), []
        for tok in text.split():
            parts = []
            for p in tok.split("+"):
                core = p.rstrip("!,/*")
                parts.append(p if core in (".", "-") else name_of(below(midi(core), pcs, gap)) + p[len(core):])
            toks.append("+".join(parts))
        out.append(" ".join(toks))
    return out


class Song:
    """Rows of 4 cells [sample, period, effect, parameter]; bars of `rpb` rows; patterns of `bpp` bars (a pattern
    break ends a pattern shorter than 64 rows). Samples: (name, data, volume, loop start, loop length, the MIDI
    note heard at C-1)."""

    def __init__(self, samples, rpb, bpp):
        self.samples, self.rpb, self.bpp = samples, rpb, bpp
        self.num = {s[0]: (i + 1, s[5]) for i, s in enumerate(samples)}
        self.rows, self.loop_row = [], 0

    def bars(self, n):
        r = len(self.rows)
        self.rows += [[[0, 0, 0, 0] for _ in range(4)] for _ in range(n * self.rpb)]
        return r

    def loop_here(self):
        self.loop_row = len(self.rows)

    def put(self, r, ch, inst, pitch, vol=None, eff=None, par=0):
        num, base = self.num[inst]
        n = midi(pitch) - base
        if not 0 <= n < 36:
            raise ValueError("row %d: %s is out of range for %s" % (r, pitch, inst))
        if eff is None:
            eff, par = (0xC, vol) if vol is not None else (0, 0)
        self.rows[r][ch] = [num, hm.PERIODS[n], eff, par]

    def free(self, r, ch):
        return self.rows[r][ch] == [0, 0, 0, 0]

    def off(self, r, ch):
        if r < len(self.rows) and self.free(r, ch):
            self.rows[r][ch] = [0, 0, 0xC, 0]

    def line(self, r, ch, inst, bars, step, vol=36, hold_fx=None, slide=0x18, per_bar=None):
        """A melody, one token per `step` rows: 'c5' a note ('!' louder, ',' softer, '/' slides to it from the
        previous note, '*' tremolo-picked: repeated on every row), '.' holds, '-' stops, 'a4+b4' splits a step.
        hold_fx (effect, parameter) goes on the held rows (a vibrato)."""
        if isinstance(bars, str):
            bars = [bars]
        for b in bars:
            if per_bar and len(b.split()) != per_bar:
                raise ValueError("bar %r: %d tokens, not %d" % (b, len(b.split()), per_bar))
        state, cur = None, None                       # 'hold' / 'slide' / 'trem' and the tremolo note
        for tok in " ".join(bars).split():
            parts = tok.split("+")
            sub = step // len(parts)
            for k, p in enumerate(parts):
                rr = r + k * sub
                core = p.rstrip("!,/*")
                flags = p[len(core):]
                if core == "-":
                    self.off(rr, ch)
                    state = cur = None
                elif core == ".":
                    pass
                else:
                    v = vol + (6 if "!" in flags else 0) - (8 if "," in flags else 0)
                    if "/" in flags:
                        base = self.num[inst][1]          # no sample number: the slide keeps the volume
                        self.rows[rr][ch] = [0, hm.PERIODS[midi(core) - base], 0x3, slide]
                        state = "slide"
                    else:
                        self.put(rr, ch, inst, core, v)
                        state = "hold"
                    cur = (core, v, rr) if "*" in flags else None
                    if cur:
                        state = "trem"
                    rr += 1
                for j in range(rr, r + (k + 1) * sub):        # the rest of this part: held rows
                    if state == "trem":                       # down and up strokes
                        self.put(j, ch, inst, cur[0], cur[1] - (7 if (j - cur[2]) % 2 else 0))
                    elif state == "slide" and self.free(j, ch):
                        self.rows[j][ch] = [0, 0, 0x3, 0]
                    elif state == "hold" and hold_fx and self.free(j, ch):
                        self.rows[j][ch] = [0, 0, hold_fx[0], hold_fx[1]]
            r += step
        return r

    def flow(self, r, eff, par):
        """A song-wide effect (speed, tempo, pattern break, jump) on a free effect slot of row r."""
        for ch in (3, 2, 1, 0):
            c = self.rows[r][ch]
            if c[2] == 0 and c[3] == 0:
                c[2], c[3] = eff, par
                return
        for ch in (2, 3, 1, 0):                        # else a note's volume gives way (it plays at its default)
            c = self.rows[r][ch]
            if c[2] == 0xC and c[0]:
                c[2], c[3] = eff, par
                return
        raise ValueError("row %d: no free effect slot" % r)

    def build(self, title, speed, bpm):
        ppat = self.rpb * self.bpp
        if len(self.rows) % ppat or self.loop_row % ppat:
            raise ValueError("the sections must fill whole patterns")
        for r in sorted({0, self.loop_row}):
            self.flow(r, 0xF, speed)
            self.flow(r, 0xF, bpm)
        npat = len(self.rows) // ppat
        for p in range(npat):
            last = (p + 1) * ppat - 1
            if p == npat - 1:
                self.flow(last, 0xB, self.loop_row // ppat)          # the loop: back to the first section
            elif ppat < 64:
                self.flow(last, 0xD, 0)
        pats, orders = [], []
        for p in range(npat):
            rows = self.rows[p * ppat:(p + 1) * ppat] + [[[0, 0, 0, 0]] * 4] * (64 - ppat)
            data = b"".join(hm.cell(*c) for row in rows for c in row)
            if data not in pats:
                pats.append(data)
            orders.append(pats.index(data))
        return hm.build_mod(title, [s[:5] for s in self.samples], pats, orders)


# ---- the instruments ---------------------------------------------------------------------------------------------
def norm(v, peak=110):
    m = max(abs(x) for x in v) or 1.0
    return [s8(x * peak / m) for x in v]


def pluck(P, n, partials, tau, power=0.7, attack=3, detune=0.0, start=0):
    """An additive plucked string, P samples a cycle: partial k (ratio, amplitude) decays in tau / k**power
    samples; a second string `detune` cents sharp (a double course)."""
    out = [0.0] * n
    det = 2 ** (detune / 1200.0)
    for k, a in partials:
        t = tau / k ** power
        for i in range(start, n):
            j = i - start
            e = math.exp(-j / t)
            if e < 1e-4:
                break
            v = math.sin(2 * math.pi * k * j / P)
            if detune:
                v = 0.6 * v + 0.6 * math.sin(2 * math.pi * k * det * j / P + 0.9)
            out[i] += a * e * v * min(1.0, j / attack)
    return out


def strum(P, semis, n, partials, tau, gap, power=0.7):
    """A strummed chord (one-shot): the strings `gap` samples apart, the root at the sample's pitch."""
    out = [0.0] * n
    for s, semi in enumerate(semis):
        f = 2 ** (semi / 12.0)
        v = pluck(P / f, n, partials, tau, power, start=s * gap)
        out = [a + b * (1.0 - 0.08 * s) for a, b in zip(out, v)]
    return norm(out, 100)


def noise_hit(sr, ms, decay_ms, seed, hp=1, tone=None, attack_ms=0.5):
    """Percussion at sr Hz: noise (high-passed `hp` times) and optional tones (Hz, amplitude, decay ms)."""
    rng = random.Random(seed)
    n = int(sr * ms / 1000)
    out, prev = [], [0.0] * 3
    for i in range(n):
        x = rng.uniform(-1, 1)
        for k in range(hp):
            x, prev[k] = x - prev[k], x
        t = i / sr * 1000
        v = x * math.exp(-t / decay_ms)
        for (f, a, d) in tone or []:
            v += a * math.sin(2 * math.pi * f * i / sr) * math.exp(-t / d)
        out.append(v * min(1.0, t / attack_ms))
    return norm(out, 105)


HI, LO = 16574, 8287          # the percussion rates: played at c4 with "the MIDI at C-1" 36 (HI) or 48 (LO)


def chord_loop(P, semis, wave, tol=4.0):
    """A looped chord: R cycles of the root (P samples each) in which every note fits a whole number of cycles
    (within tol cents of equal temperament). Returns (data, loop length)."""
    for R in range(1, 600):
        cyc = [round(R * 2 ** (s / 12.0)) for s in semis]
        if all(abs(1200 * math.log2(c / (R * 2 ** (s / 12.0)))) <= tol for c, s in zip(cyc, semis)):
            break
    L = R * P
    return norm([sum(wave(c * i / L) for c in cyc) for i in range(L)], 100), L


def sweep(sr, f0, f1, fall_ms, decay_ms, ms, click=0.0, seed=5):
    """A drum: a sine falling from f0 to f1 Hz, an exponential decay, a little noise click."""
    rng = random.Random(seed)
    out, ph = [], 0.0
    for i in range(int(sr * ms / 1000)):
        t = i / sr * 1000
        ph += (f1 + (f0 - f1) * math.exp(-t / fall_ms)) / sr
        v = math.sin(2 * math.pi * ph) + click * rng.uniform(-1, 1) * math.exp(-t / 3)
        out.append(v * math.exp(-t / decay_ms))
    return norm(out, 110)


def tone_loop(L, voices, attack, noise=0.0, seed=1):
    """A sustained tone from voices (cycles in L samples, amplitude, harmonics [(k, a)]): an `attack`-sample swell
    with a breath of noise, then the L-sample loop (whole cycles: seamless)."""
    rng = random.Random(seed)

    def f(i):
        return sum(a * sum(h * math.sin(2 * math.pi * k * c * i / L) for k, h in harm) for c, a, harm in voices)
    return norm([f(i) * min(1.0, i / attack) + noise * rng.uniform(-1, 1) * math.exp(-i / (attack / 3.0))
                 * (1 if i < attack else 0) for i in range(attack + L)], 100)


def arps(chords, lo, pattern):
    """Broken chords: for each chord, the chord tones from `lo` upwards picked by the index pattern."""
    out = []
    for name in chords:
        pc, q, _ = chord(name)
        base = fit(pc, lo)
        ladder = sorted({base + k + 12 * o for k in QUAL[q] for o in (0, 1, 2)})
        out.append(" ".join("." if i is None else name_of(ladder[i]) for i in pattern))
    return out


# ==== alt 1: Otter Breakdown ========================================================================================
STEEL = [(1, 1), (2, .7), (3, .5), (4, .38), (5, .26), (6, .18), (7, .12), (8, .08)]


def saw(ph):
    return sum(math.sin(2 * math.pi * k * ph) / k for k in range(1, 9))


def fiddle():
    """A fiddle: a bowed, sawtooth-like spectrum with a body resonance on the 3rd and 4th harmonics, a scratchy bow
    attack, then a 32-sample loop (C-1 = C3)."""
    h = [(k, (1.4 if k in (3, 4) else 1.0) / k) for k in range(1, 11)]
    return tone_loop(32, [(1, 1, h)], 96, .35, 31)


def breakdown():
    smp = [("fiddle", fiddle(), 32, 96, 32, 48),
           ("guitar", norm(pluck(64, 7000, STEEL, 2600, .6, attack=2)), 36, 0, 0, 36),
           ("bass", norm(pluck(128, 10000, [(1, 1), (2, .5), (3, .25), (4, .12), (5, .06)], 4000, .7, attack=10)),
            38, 0, 0, 24),
           ("dobro", norm(pluck(32, 10000, STEEL, 5200, .55, attack=2, detune=4)), 34, 0, 0, 48),
           ("chop", noise_hit(HI, 80, 10, 51, 1, [(1180, .6, 14), (1770, .4, 10), (880, .5, 18)]), 24, 0, 0, 36)]
    for q, v in (("", [0, 7, 12, 16]), ("m", [0, 7, 12, 15]), ("7", [0, 7, 10, 16])):
        smp.append(("strum_" + q, strum(32, v, 5000, STEEL[:5], 1400, 50), 26, 0, 0, 48))
    data, L = chord_loop(32, [0, 7], saw)
    smp.append(("fid_5", data, 30, 0, L, 48))
    s = Song(smp, rpb=8, bpp=4)                       # cut time in eighths: a bar = 8 rows, patterns of 4 bars
    FID, GTR, BASS, DOB = 0, 1, 2, 3
    FVIB = (0x4, 0x33)

    def boom_chuck(r0, chords, run=None):
        """Bass note, strum, the alternate bass, strum; a bass run on the guitar in the last bar."""
        for b, name in enumerate(chords):
            pc, q, bpc = chord(name)
            r = r0 + 8 * b
            if run and b == len(chords) - 1:
                s.line(r, GTR, "guitar", run, 1, 30, per_bar=8)
                continue
            s.put(r, GTR, "guitar", fit(bpc, 40), 32)
            s.put(r + 2, GTR, "strum_" + q, fit(pc, 50), 24)
            s.put(r + 4, GTR, "guitar", fit(pc + 7, 40), 28)
            s.put(r + 6, GTR, "strum_" + q, fit(pc, 50), 22)

    def bass(r0, chords, nxt, walk=False, walk_last=False):
        for b, name in enumerate(chords):
            pc, q, bpc = chord(name)
            r, root = r0 + 8 * b, fit(bpc, 33)
            nroot = fit(chord(chords[b + 1] if b + 1 < len(chords) else nxt)[2], 33)
            if walk or (walk_last and b == len(chords) - 1):
                third = 3 if q.startswith("m") else 4
                app = nroot + 1 if nroot < root + 7 else nroot - 1
                for e, m in enumerate((root, root + third, root + 7, app)):
                    s.put(r + 2 * e, BASS, "bass", m, 32 if e == 0 else 26)
            else:
                s.put(r, BASS, "bass", root, 33)
                s.put(r + 4, BASS, "bass", root + 7 if root + 7 <= 45 else root - 5, 28)

    def chop(r0, bars, vol=22):
        for b in range(bars):
            s.put(r0 + 8 * b + 2, DOB, "chop", "c4", vol)
            s.put(r0 + 8 * b + 6, DOB, "chop", "c4", vol - 2)

    def shuffle(r0, chords, vol=30):
        """The fiddle's double-stop shuffle: dah . da da dah . da da."""
        for b, name in enumerate(chords):
            pc = chord(name)[0]
            for e, v in ((0, vol), (2, vol - 8), (3, vol - 6), (4, vol - 2), (6, vol - 8), (7, vol - 6)):
                s.put(r0 + 8 * b + e, FID, "fid_5", fit(pc, 57), v)

    A_CH = ["A", "A", "D", "A", "A", "A", "E", "A"]
    FID_A = ["c#5 e5 a5 . a5 g5 e5 c#5", "d5 c#5 b4 a4 b4 c#5 e5 .", "f#5 a5 f#5 d5 e5 f#5 a5 .",
             "e5 . c#5 a4 c#5 e5 c#5 a4", "c#5 e5 a5 . a5 g5 e5 c#5", "d5 c#5 b4 a4 c5 c#5/ e5 .",
             "b4 g#4 e4 g#4 b4 d5 e5 g#5", "a5 e5 c#5 e5 a4 . - ."]
    FID_A2 = ["c#5 e5 a5 b5 a5 g5 e5 c#5", "d5 e5 f#5 e5 d5 c#5 b4 a4", "f#5 a5 b5 a5 f#5 d5 f#5 a5",
              "e5 c#5 e5 a5 e5 c#5 a4 c#5", "c#5 e5 a5 . a5 g5 e5 c#5", "d5 c#5 b4 a4 c5 c#5/ e5 .",
              "b4 d5 g#5 b5 a5 g#5 f#5 g#5", "a5 . e5 . a4 . - ."]
    DOB_A2 = ["e4 . . . a4/ . . .", "c#5 . . . b4 . a4 .", "a4 . . . d5/ . . .", "c#5 . . . e4 . . .",
              "e4 . . . a4/ . . .", "g4 . . . f#4 . e4 .", "d4 . . . e4/ . g#4 .", "a4 . . . - . . ."]
    B_CH = ["D", "D", "A", "A", "E", "E", "A", "E7"]
    FID_B = ["a5 . f#5 a5 b5 a5 f#5 d5", "e5 f#5 a5 . f#5 e5 d5 b4", "c#5 . e5 c#5 a4 c#5 e5 a5",
             "g5 . e5 c#5 e5 . a5 .", "g#5 . b5 g#5 e5 g#5 b5 .", "a5 g#5 f#5 e5 d5 b4 g#4 b4",
             "a4 c#5 e5 a5 g#5 e5 f#5 g#5", "a5 . e5 . d5 . b4 ."]
    DOB_B = ["e5 f#5/ a5 . b5 a5 f#5 d5", "e5 f#5 a5 . f#5 e5 d5 b4", "b4 c#5/ e5 c#5 a4 c#5 e5 a5",
             "f#5 g5/ e5 c#5 e5 . a5 .", "f#5 g#5/ b5 g#5 e5 g#5 b5 .", "a5 g#5 f#5 e5 d5 b4 g#4 b4",
             "a4 c#5 e5 a5 g#5 e5 f#5 g#5", "a5 . e5 . d5 . b4 ."]
    TAG_CH = ["F#m", "F#m", "D", "E", "F#m", "D", "E", "E7"]
    FID_TAG = ["f#5* . . . c#5* . . .", "a5* . . . f#5* . . .", "a5* . . . f#5* . . .", "g#5* . . . b5* . . .",
               "a5* . . . c#5* . . .", "f#5* . . . a5* . . .", "g#5* . . . e5* . . .",
               "e5 f#5 g#5 a5 b5 a5 g#5 e5"]
    RUN_A, RUN_E = "a2 . c3 c#3 e3 c#3 a2 .", "e2 . f#2 g#2 b2 . d3 ."

    # intro: the fiddle's shuffle and the chop, then the bass and the guitar's run
    r = s.bars(4)
    shuffle(r, ["A", "A", "A", "E"], 26)
    chop(r, 4)
    bass(r + 16, ["A", "E7"], "A")
    boom_chuck(r + 16, ["A", "E7"], RUN_E)
    # A: the rolling river lick
    s.loop_here()
    r = s.bars(8)
    boom_chuck(r, A_CH)
    bass(r, A_CH, "A")
    chop(r, 8)
    s.line(r, FID, "fiddle", FID_A, 1, 27, FVIB, per_bar=8)
    # A': the variation, the dobro answering with slides
    r = s.bars(8)
    boom_chuck(r, A_CH)
    bass(r, A_CH, "D", walk_last=True)
    s.line(r, FID, "fiddle", FID_A2, 1, 27, FVIB, per_bar=8)
    s.line(r, DOB, "dobro", DOB_A2, 1, 28, per_bar=8)
    # B: the high part
    r = s.bars(8)
    boom_chuck(r, B_CH)
    bass(r, B_CH, "D")
    chop(r, 8)
    s.line(r, FID, "fiddle", FID_B, 1, 27, FVIB, per_bar=8)
    # B': the dobro's break, the fiddle's double stops behind, a walking bass
    r = s.bars(8)
    boom_chuck(r, B_CH)
    bass(r, B_CH, "F#m", walk=True)
    shuffle(r, B_CH, 20)
    s.line(r, DOB, "dobro", DOB_B, 1, 32, per_bar=8)
    # the tag: F sharp minor, tremolo bowing, the bass walks, the guitar runs back home
    r = s.bars(8)
    boom_chuck(r, TAG_CH, RUN_E)
    bass(r, TAG_CH, "A", walk=True)
    chop(r, 8, 24)
    s.line(r, FID, "fiddle", FID_TAG, 1, 26, per_bar=8)
    # A'': fiddle and dobro in twin harmony
    r = s.bars(8)
    boom_chuck(r, A_CH, RUN_A)
    bass(r, A_CH, "A")
    s.line(r, FID, "fiddle", FID_A, 1, 27, FVIB, per_bar=8)
    s.line(r, DOB, "dobro", harmony(FID_A, A_CH), 1, 24, per_bar=8)
    return s.build("or otter breakdown", 6, 116)      # speed 6 at 116 BPM: an eighth 129 ms (crotchet 232)


# ==== alt 2: Kawauso ================================================================================================
def koto():
    """A koto: a silk string plucked with an ivory pick, bright, a little beating, a medium ring (16 samples a
    cycle: C-1 = C4)."""
    return norm(pluck(16, 10000, [(1, 1), (2, .65), (3, .5), (4, .32), (5, .22), (6, .14)], 3600, .75, attack=1,
                      detune=3))


def bass_koto():
    """The 17-string bass koto (128 samples a cycle: C-1 = C1)."""
    return norm(pluck(128, 12000, [(1, 1), (2, .6), (3, .4), (4, .25), (5, .15), (6, .08)], 5500, .7, attack=3))


def shakuhachi():
    """A shakuhachi: a soft fundamental with a little 2nd and 3rd, a chiff at the onset, and breath noise baked into
    a long loop (16 samples a cycle: C-1 = C4)."""
    rng = random.Random(77)
    L, A = 4096, 512

    def tone(i):
        return (math.sin(2 * math.pi * i / 16) + .2 * math.sin(4 * math.pi * i / 16)
                + .06 * math.sin(6 * math.pi * i / 16))
    lp, breath = 0.0, []
    for i in range(L):
        lp += .3 * (rng.uniform(-1, 1) - lp)
        breath.append(lp)
    att = [tone(i) * min(1.0, i / 300.0) + .45 * breath[i] * min(1.0, i / 60.0)
           + .8 * rng.uniform(-1, 1) * math.exp(-i / 70.0) for i in range(A)]
    return norm(att + [tone(i) + .45 * breath[i] for i in range(L)], 100)


def kawauso():
    smp = [("koto", koto(), 34, 0, 0, 60),
           ("shakuhachi", shakuhachi(), 32, 512, 4096, 60),
           ("bass", bass_koto(), 38, 0, 0, 24),
           ("taiko", sweep(HI, 120, 68, 25, 200, 600, click=.25, seed=81), 36, 0, 0, 36),
           ("shime", sweep(HI, 540, 410, 3, 30, 110, click=.8, seed=82), 26, 0, 0, 36),
           ("drop", sweep(HI, 650, 1600, 14, 28, 110, seed=83), 24, 0, 0, 36),
           ("kane", noise_hit(HI, 700, 3, 84, 2, [(1650, 1.0, 320), (2480, .6, 220), (4100, .35, 160)]), 24, 0, 0, 36)]
    s = Song(smp, rpb=16, bpp=2)                      # 4/4 in 16ths, patterns of 2 bars
    KOTO, SHK, BASS, PERC = 0, 1, 2, 3
    BREATH = (0x4, 0x24)

    PERC_PAT = {"flow": ((0, "taiko", 30), (6, "shime", 16), (8, "drop", 20), (10, "shime", 14), (12, "taiko", 22),
                         (14, "shime", 14)),
                "still": ((0, "taiko", 24), (8, "kane", 18), (12, "drop", 18)),
                "rapids": ((0, "taiko", 34), (2, "shime", 18), (3, "shime", 12), (4, "taiko", 26), (6, "shime", 18),
                           (7, "shime", 12), (8, "taiko", 32), (10, "shime", 18), (11, "shime", 12), (12, "taiko", 26),
                           (13, "shime", 14), (14, "shime", 16), (15, "shime", 20)),
                "drops": ((4, "drop", 18), (10, "drop", 14), (13, "drop", 20)),
                "bell": ((0, "kane", 22), (8, "drop", 16), (12, "taiko", 20))}

    def perc(r0, bars, style):
        for b in range(bars):
            for e, inst, v in PERC_PAT[style]:
                s.put(r0 + 16 * b + e, PERC, inst, "c4", v)

    RIP = {"E": "e4 b4 c5 e5 f5 e5 c5 b4 e4 b4 c5 e5 b5 a5 f5 e5",
           "A": "a4 c5 e5 f5 a5 f5 e5 c5 a4 c5 e5 f5 c6 b5 a5 f5",
           "F": "f4 a4 c5 e5 f5 e5 c5 a4 f4 a4 c5 f5 a5 f5 e5 c5",
           "D": "d4 a4 b4 d5 e5 d5 b4 a4 d4 a4 b4 d5 f5 e5 d5 b4"}
    RAPID = {"E": "e4 f4 a4 b4 c5 e5 f5 a5 b5 a5 f5 e5 c5 b4 a4 f4",
             "A": "a4 b4 c5 e5 f5 a5 b5 c6 b5 a5 f5 e5 c5 b4 a4 b4",
             "F": "f4 a4 b4 c5 e5 f5 a5 b5 c6 b5 a5 f5 e5 c5 b4 a4"}

    def ripple(r, areas, vol, table=RIP, down=False):
        bars = [table[a] for a in areas]
        if down:                                      # the same notes flowing the other way
            bars = [" ".join(b.split()[::-1]) for b in bars]
        s.line(r, KOTO, "koto", bars, 1, vol, per_bar=16)

    A_AREAS = ["E", "E", "A", "A", "F", "E", "A", "E"]
    SHK_A = ["e5 . . . . . f5/ e5/", "c5 . . . b4 . . .", "a4 . c5 . e5 . . f5", "e5 . . . . . - .",
             "f5 . . . a5 . . b5", "c6/ . b5 . a5 . f5 .", "e5 . . . c5 . b4 .", "e5 . . . . . - ."]
    BASS_A = ["e2 . . b1", "e2 . b1 c2", "a1 . . e2", "a1 . e2 f2", "f1 . . c2", "e2 . . b1", "a1 . c2 b1", "e2 . . -"]
    KOTO_B = ["a5 . e5 . f5 e5 c5 .", "b4 . c5 . e5 . . .", "f5 . a5 . b5 a5 f5 .", "e5 . f5 . a5 . . .",
              "d5 . f5 . a5 . c6 b5", "b5 . c6/ . b5/ . - ."]
    SHK_B = ["e5 . . . . . . .", "c5 . . . . . . .", "a4 . . . . . . .", "c5 . . . b4 . . .", "a4 . . . . . . .",
             "b4/ . . . . . - ."]
    BASS_B = ["a1 . . e2", "a1 . e2 a2", "f1 . . c2", "f1 . c2 f2", "d2 . . a1", "e2 . . b1"]
    SHK_C = ["e6 . . . . . . .", "f6/ . . . e6 . . .", "c6 . . . b5 . a5 .", "b5 . . . - . . ."]
    BASS_C = ["e2 . e2 b1 e2 . b1 .", "a1 . a1 e2 a1 . e2 .", "f1 . f1 c2 f1 . c2 .", "e2 e2 b1 b1 c2 c2 b1 b1"]

    # intro: the koto ripple and the water drops, then the bass and the bell
    r = s.bars(2)
    ripple(r, ["E", "E"], 24)
    perc(r, 1, "drops")
    perc(r + 16, 1, "bell")
    s.line(r + 16, BASS, "bass", "e2 . . b1", 4, 34)
    # A: the shakuhachi's sigh over the ripple
    s.loop_here()
    r = s.bars(8)
    ripple(r, A_AREAS, 24)
    s.line(r, SHK, "shakuhachi", SHK_A, 2, 25, BREATH, slide=0x10, per_bar=8)
    s.line(r, BASS, "bass", BASS_A, 4, 36, per_bar=4)
    perc(r, 8, "flow")
    # B: the koto takes the tune with string bends, the shakuhachi holds long notes
    r = s.bars(6)
    s.line(r, KOTO, "koto", KOTO_B, 2, 34, slide=0x10, per_bar=8)
    s.line(r, SHK, "shakuhachi", SHK_B, 2, 21, BREATH, slide=0x08, per_bar=8)
    s.line(r, BASS, "bass", BASS_B, 4, 36, per_bar=4)
    perc(r, 6, "still")
    # the rapids: faster, koto runs, taiko eighths, the shakuhachi cries high
    r = s.bars(4)
    ripple(r, ["E", "A", "F", "E"], 26, RAPID)
    s.line(r, SHK, "shakuhachi", SHK_C, 2, 24, BREATH, slide=0x18, per_bar=8)
    s.line(r, BASS, "bass", BASS_C, 2, 34, per_bar=8)
    perc(r, 4, "rapids")
    s.flow(r, 0xF, 108)
    # A': calm again, the ripple flows back down under the sigh
    r = s.bars(6)
    ripple(r, A_AREAS[:4] + A_AREAS[6:], 22, down=True)
    s.line(r, SHK, "shakuhachi", SHK_A[:4] + SHK_A[6:], 2, 25, BREATH, slide=0x10, per_bar=8)
    s.line(r, BASS, "bass", BASS_A[:4] + BASS_A[6:], 4, 36, per_bar=4)
    perc(r, 6, "flow")
    s.flow(r, 0xF, 96)
    return s.build("or kawauso", 6, 96)               # speed 6 at 96 BPM: a 16th 156 ms (crotchet 96)


TRACKS = [breakdown, kawauso]


def main():
    args = sys.argv[1:]
    if len(args) >= 2:                                  # OUTDIR N: alternative N as OUTDIR/tune.mod
        os.makedirs(args[0], exist_ok=True)
        with open(os.path.join(args[0], "tune.mod"), "wb") as f:
            f.write(TRACKS[int(args[1]) - 1]())
        return
    outdir = args[0] if args else os.path.join(HERE, "..", "music-alt")
    os.makedirs(outdir, exist_ok=True)
    for k, fn in enumerate(TRACKS):
        with open(os.path.join(outdir, "alt%d.mod" % (k + 1)), "wb") as f:
            f.write(fn())


if __name__ == "__main__":
    main()
