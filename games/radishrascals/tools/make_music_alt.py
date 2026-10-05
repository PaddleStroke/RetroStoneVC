#!/usr/bin/env python3
"""Radish Rascals alternative music: two 4-channel ProTracker MODs written with the house music kit
(games/common/tools/house_music.py: the MOD writer and periods; docs/art-direction.md "Audio"), the instruments
synthesised here like Pogo Mamie's musette or Beaver Rush's banjo, at the house mix (sample volumes <= 40).

  alt1  "Radish Rag": a cheeky ragtime on an out-of-tune upright (two strings a note), 2/4 at 104, C major with
        a stop-time strain in A minor and a trio in F. The left hand plays oom-pah (a low bass, a chord), a
        washboard and a woodblock keep time, a xylophone joins in harmony, a slide whistle goes "wheee". The hook:
        a chromatic sneak up to the fifth (E F F# G) then a hop down, answered a third lower; a diminished
        sequence later. Intro (a chromatic creep down), A (16 bars), B (stop-time: the rabbits freeze, knock
        knock), A' (with the xylophone), the trio, then the loop back to A.
  alt2  "Rabbit Caper": a sneaky pizzicato caper, 4/4 at 116, G minor. Plucked violins tiptoe (tip, tip, tip...
        hop!), a plucked cello, a woodblock clock, a bassoon creeping up the chromatic scale; then a chase in
        E flat with tremolo strings, running bass, timpani and brushes; a hush where only the clock ticks. The
        hook: three tiptoe steps and a hop up on the offbeat, then a little twirl back down. Intro, A, A' (the
        bassoon creeps along), B (the chase), the hush, then the loop back to A.

    make_music_alt.py              -> ../music-alt/alt1.mod, alt2.mod (previews; the game is unchanged)
    make_music_alt.py OUTDIR N     -> OUTDIR/tune.mod: alternative N (1 or 2), for tools/build_assets.py

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/radishrascals/LICENSE.
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


def block(P, semis, n, partials, tau, power=0.7, detune=0.0):
    """A struck chord (one-shot), every note a (detuned) string, the root at the sample's pitch."""
    out = [0.0] * n
    for semi in semis:
        f = 2 ** (semi / 12.0)
        out = [a + b for a, b in zip(out, pluck(P / f, n, partials, tau, power, detune=detune))]
    return norm(out, 100)


def looped(P, harmonics, attack, L=None):
    """A sustained tone: harmonics (k, amplitude) over P samples, an `attack`-sample swell, then the loop."""
    L = L or P
    wave = [sum(a * math.sin(2 * math.pi * k * i / P) for k, a in harmonics) for i in range(L)]
    return norm([wave[i % L] * min(1.0, i / attack) for i in range(attack)] + wave, 100)


# ==== alt 1: Radish Rag =============================================================================================
PIANO = [(1, 1), (2, .7), (3, .45), (4, .3), (5, .2), (6, .12), (7, .07)]


def rag():
    smp = [("piano", norm(pluck(16, 9000, PIANO, 3000, .8, attack=2, detune=9)), 34, 0, 0, 60),
           ("piano_lo", norm(pluck(64, 9000, PIANO[:5], 4000, .7, attack=3, detune=7)), 36, 0, 0, 36)]
    for q, v in (("M", [0, 4, 7, 12]), ("m", [0, 3, 7, 12]), ("7", [0, 4, 7, 10]), ("dim7", [0, 3, 6, 9])):
        smp.append(("ch_" + q, block(32, v, 7000, PIANO[:5], 2600, .8, 8), 24, 0, 0, 48))
    rng = random.Random(71)
    wash = [rng.uniform(-1, 1) * (.55 + .45 * math.sin(2 * math.pi * 170 * i / HI)) * math.exp(-i / (HI * .03))
            * min(1.0, i / 40.0) for i in range(int(HI * .11))]
    smp += [("washboard", norm([b - a for a, b in zip([0.0] + wash, wash)], 100), 22, 0, 0, 36),
            ("woodblock", noise_hit(HI, 120, 2, 72, 1, [(1250, 1.2, 28), (2930, .45, 14)]), 26, 0, 0, 36),
            ("xylo", norm(pluck(16, 5000, [(1, 1), (3, .4), (6, .1)], 1800, 1.0, attack=2)), 28, 0, 0, 60),
            ("whistle", looped(16, [(1, 1)], 64), 22, 64, 16, 60)]
    s = Song(smp, rpb=8, bpp=4)                       # 2/4 in 16ths: a bar = 8 rows
    RH, LH, CH, PERC = 0, 1, 2, 3
    smp_of = {"": "ch_M", "m": "ch_m", "7": "ch_7", "dim7": "ch_dim7"}

    def left(r0, chords, stop=()):
        """oom-pah: a low bass on the beats (root, then the fifth), the chord on the offbeats; stop-time bars play
        only the downbeat."""
        for b, name in enumerate(chords):
            pc, q, bpc = chord(name)
            r = r0 + 8 * b
            s.put(r, LH, "piano_lo", fit(bpc, 36), 40)
            if b in stop:
                s.put(r, CH, smp_of[q], fit(pc, 52), 30)
                continue
            s.put(r + 2, CH, smp_of[q], fit(pc, 52), 28)
            s.put(r + 4, LH, "piano_lo", fit(pc + (7 if q != "dim7" else 6), 36), 34)
            s.put(r + 6, CH, smp_of[q], fit(pc, 52), 26)

    def board(r0, bars, skip=()):
        for b in range(bars):
            if b in skip:
                continue
            r = r0 + 8 * b
            s.put(r + 2, PERC, "washboard", "c4", 24)
            s.put(r + 3, PERC, "washboard", "c4", 12)
            s.put(r + 6, PERC, "washboard", "c4", 24)
            s.put(r + 7, PERC, "woodblock", "c4", 12)

    def whistle(r, up=True):
        s.put(r, PERC, "whistle", "c5" if up else "c6", 26)
        for k in range(1, 4):
            s.rows[r + k][PERC] = [0, 0, 0x1 if up else 0x2, 0x0C]
        s.off(r + 4, PERC)

    INTRO = ["g5 . f#5 . f5 . e5 .", "d#5 . d5 . c#5 . c5 .", "b4 . . . d5 . f5 .", "g5 . . . - . . ."]
    A_CH = ["C", "C", "G7", "G7", "G7", "G7", "C", "C7", "F", "F#dim7", "C/G", "A7", "D7", "G7", "C", "G7"]
    A = ["e5 f5 f#5 g5 . e5 c5 .", "a5 g5 . e5 g5 . . .", "d5 d#5 e5 f5 . d5 b4 .", "g5 f5 . d5 f5 . . .",
         "b5 a5 g5 f5 . d5 . f5", ". e5 d5 . b4 . g4 .", "c5 e5 g5 c6 . g5 . e5", "a#5 . g5 . e5 . c5 .",
         "f5 a5 c6 . a5 f5 . a5", "c6 b5 c6 d#6 . c6 a5 .", "e5 f5 f#5 g5 . e5 c5 .", "c#6 . a5 . e5 . g5 .",
         "f#5 g5 . f#5 d5 . a4 .", "b4 c5 d5 f5 . a5 g5 .", "c6 . g5 . e5 . c5 .", "- . d5 . f5 . b4 ."]
    B_CH = ["Am", "E7", "Am", "E7", "F", "E7", "Am", "G7"]
    B = ["a4 . . c5 e5 a5 g#5 a5", "b5 . g#5 . e5 . d5 .", "c6 . . a5 e5 c5 b4 c5", "d5 . . . - . . .",
         "a5 g5 f5 e5 d5 c5 b4 a4", "g#4 b4 d5 f5 . e5 d5 .", "c5 . a4 . e4 . a4 .", "b4 . d5 . g5 . f5 ."]
    T_CH = ["F", "F", "C7", "C7", "C7", "C7", "F", "F", "Bb", "Bdim7", "F/C", "D7", "G7", "C7", "F", "G7"]
    T = ["c5 . f5 . a5 . . c6", ". . a5 . g5 . f5 .", "e5 . g5 . c6 . . a#5", ". . g5 . e5 . c5 .",
         "d5 . e5 . g5 . a#5 .", "d6 . c6 . a#5 . g5 .", "a5 . . . f5 . c5 .", "f5 . . . - . . .",
         "d5 . f5 . a#5 . . d6", ". . b5 . g#5 . f5 .", "a5 . . f5 c6 . a5 .", "f#5 . a5 . d6 . c6 .",
         "b5 . . a5 g5 . f5 .", "e5 . g5 . a#5 . g5 .", "f5 . a5 . c6 . a5 .", "g5 . f5 . d5 . b4 ."]

    # intro: creeping down chromatically over G7, the left hand waits, then oom-pah
    r = s.bars(4)
    left(r, ["G7"] * 4, stop=(0, 1))
    s.line(r, RH, "piano", INTRO, 1, 40, per_bar=8)
    board(r + 16, 2)
    # A: the sneaky hook
    s.loop_here()
    r = s.bars(16)
    left(r, A_CH)
    board(r, 16)
    s.line(r, RH, "piano", A, 1, 42, per_bar=8)
    # B: stop-time in A minor, the rabbits freeze (knock knock), then run
    r = s.bars(8)
    left(r, B_CH, stop=(0, 1, 2, 3))
    board(r + 32, 4)
    for b in range(3):
        s.put(r + 8 * b, PERC, "woodblock", "g4", 20)
    s.put(r + 28, PERC, "woodblock", "c4", 26)
    s.put(r + 30, PERC, "woodblock", "c4", 26)
    s.line(r, RH, "piano", B, 1, 42, per_bar=8)
    # A': the xylophone in harmony
    r = s.bars(16)
    left(r, A_CH)
    s.line(r, RH, "piano", A, 1, 42)
    s.line(r, PERC, "xylo", harmony(A, A_CH), 1, 28)
    # trio: F major, a lyrical tune, the woodblock clip-clops, the slide whistle
    r = s.bars(16)
    left(r, T_CH)
    for b in range(16):
        if b in (7, 15):
            continue
        s.put(r + 8 * b, PERC, "woodblock", "g4", 14)
        s.put(r + 8 * b + 3, PERC, "woodblock", "c4", 11)
        s.put(r + 8 * b + 4, PERC, "woodblock", "g4", 14)
    whistle(r + 60)
    whistle(r + 124, up=False)
    s.line(r, RH, "piano", T, 1, 40, per_bar=8)
    return s.build("rr radish rag", 6, 104)           # speed 6 at 104 BPM: a 16th 144 ms (crotchet 104)


# ==== alt 2: Rabbit Caper ===========================================================================================
def caper():
    rng = random.Random(81)
    timp = pluck(64, 12000, [(1, 1), (1.5, .5), (1.99, .35), (2.44, .2)], 6000, .5, attack=6)
    timp = [v + .5 * rng.uniform(-1, 1) * math.exp(-i / 300.0) for i, v in enumerate(timp)]

    def ens(i, L=2048):
        return sum(sum(math.sin(2 * math.pi * k * (c * i / L + ph)) / k ** 1.3 for k in range(1, 7))
                   for c, ph in ((128, 0), (129, .3), (127, .7)))
    pizz = pluck(16, 4000, [(1, 1), (2, .5), (3, .3), (4, .15), (5, .08)], 1400, .9, attack=6)
    reed = [(1, .6), (2, 1.0), (3, .8), (4, .5), (5, .3), (6, .15), (7, .08)]
    smp = [("pizz", norm(pizz), 34, 0, 0, 60),
           ("cello", norm(pluck(64, 6000, [(1, 1), (2, .45), (3, .2)], 2600, .8, attack=8)), 38, 0, 0, 36),
           ("bassoon", looped(32, reed, 64), 28, 64, 32, 48),
           ("tick", noise_hit(HI, 90, 2, 82, 1, [(1480, 1.2, 22), (3300, .4, 10)]), 22, 0, 0, 36),
           ("tock", noise_hit(HI, 110, 2, 83, 1, [(980, 1.2, 30), (2210, .4, 12)]), 22, 0, 0, 36),
           ("timpani", norm(timp), 34, 0, 0, 36),
           ("piccolo", looped(16, [(1, 1), (2, .1)], 96), 22, 96, 16, 60),
           ("strings", norm([ens(i) * min(1.0, i / 60.0) for i in range(60 + 2048)], 100), 24, 60, 2048, 60),
           ("brush", noise_hit(HI, 160, 45, 84, 1, [(200, .4, 30)], attack_ms=4), 18, 0, 0, 36)]
    # (the strings: loop from 60, 2048 samples: 3 detuned saws, a quick bow for the tremolo)
    s = Song(smp, rpb=16, bpp=2)                      # 4/4 in 16ths, patterns of 2 bars
    MEL, BASS, MID, PERC = 0, 1, 2, 3

    def pizz_bass(r0, chords, vol=38, run=False):
        for b, name in enumerate(chords):
            pc, q, bpc = chord(name)
            r, root = r0 + 16 * b, fit(bpc, 40)
            if run:                                    # the chase: running eighths
                for e, k in enumerate([0, 7, 12, 7, 0, 7, 12, 7]):
                    s.put(r + 2 * e, BASS, "cello", root + k, vol if e % 2 == 0 else vol - 8)
                continue
            s.put(r, BASS, "cello", root, vol)
            s.put(r + 8, BASS, "cello", fit(pc + 7, 40), vol - 6)

    def clock(r0, bars, vol=18, every=8):
        for b in range(bars):
            for e in range(4, 16, every):
                s.put(r0 + 16 * b + e, PERC, "tick" if (e // every) % 2 == 0 else "tock", "c4", vol)

    def peep(r, note="d6"):
        s.put(r, PERC, "piccolo", note, 26)
        s.off(r + 2, PERC)

    A_CH = ["Gm", "Gm", "Cm", "D7", "Gm", "Eb", "D7", "Gm"]
    A = ["g4 . a4 . a#4 . . d5", ". . c#5+d5 . a#4 . g4 .", "c5 . d5 . d#5 . . g5", ". . f#5+g5 . d5 . c5 .",
         "a#4 . c5 . d5 . . a#5", ". . a5+a#5 . g5 . d#5 .", "d5 . c5 . a4 . f#4 .", "g4 . . . . . . ."]
    BSN = ["g3 - a3 - a#3 - b3 -", "c4 - b3 - a#3 - a3 -", "c4 - d4 - d#4 - g3 -", "f#3 - a3 - c4 - a3 -",
           "g3 - a3 - a#3 - b3 -", "c4 - a#3 - g3 - d#3 -", "d3 - f#3 - a3 - c4 -", "a#3 - a3 - g3 - . ."]
    B_CH = ["Eb", "Bb", "C7", "F7", "Eb", "Bb", "Cm", "D7"]
    B = ["g5* . . f5* g5* . a#5* .", "f5* . . d5* f5* . a#5* .", "e5* . . c5* e5* . g5* a#5*", "a5* . . . f5* . . .",
         "g5* . . f5* g5* . c6* .", "a#5* . . f5* d5* . a#4* .", "c5* . d#5* . g5* . c6* .", "a5* . f#5* . d5* . . -"]
    H_CH = ["Gm", "Eb", "Gm", "D7"]
    H_BSN = ["d3 . . . . . . -", "d#3 . . . . . . -", "d3 . . . . . . -", "c4 . a3 . f#3 . d3 -"]

    # intro: the clock and the cello on tiptoe
    r = s.bars(2)
    pizz_bass(r, ["Gm", "Gm"], 30)
    clock(r, 2, every=4)
    # A: tip, tip, tip... hop!
    s.loop_here()
    r = s.bars(8)
    pizz_bass(r, A_CH)
    clock(r, 8)
    s.line(r, MEL, "pizz", A, 2, 46, per_bar=8)
    peep(r + 7 * 16 + 8)
    # A': the bassoon creeps up behind
    r = s.bars(8)
    pizz_bass(r, A_CH)
    clock(r, 8)
    s.line(r, MEL, "pizz", A, 2, 46)
    s.line(r, MID, "bassoon", BSN, 2, 30, per_bar=8)
    peep(r + 7 * 16 + 8, "g6")
    # B: the chase in E flat: tremolo strings, running bass, timpani, brushes
    r = s.bars(8)
    pizz_bass(r, B_CH, 38, run=True)
    for b, name in enumerate(B_CH):
        pc, q, _ = chord(name)
        s.put(r + 16 * b, MID, "timpani", fit(pc, 38), 34)
        s.put(r + 16 * b + 8, MID, "timpani", fit(pc + 7, 38), 26)
        for e in range(0, 16, 2):
            s.put(r + 16 * b + e, PERC, "brush", "c4", 20 if e in (4, 12) else 12)
    s.line(r, MEL, "strings", B, 2, 31, per_bar=8)
    peep(r + 7 * 16 + 14, "a6")
    # the hush: only the clock, the cello and the bassoon low; a hop up into A
    r = s.bars(4)
    pizz_bass(r, H_CH, 24)
    clock(r, 4, 12, every=4)
    s.line(r, MID, "bassoon", H_BSN, 2, 22, per_bar=8)
    s.put(r + 3 * 16 + 14, MEL, "pizz", "d5", 36)
    s.rows[r + 3 * 16 + 15][MEL] = [0, 0, 0x1, 0x20]         # boing
    return s.build("rr rabbit caper", 6, 116)         # speed 6 at 116 BPM: a 16th 129 ms (crotchet 116)


TRACKS = [rag, caper]


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
