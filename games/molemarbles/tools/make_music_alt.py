#!/usr/bin/env python3
"""Mole Marbles alternative music: two 4-channel ProTracker MODs written with the house music kit
(games/common/tools/house_music.py: the MOD writer and periods; docs/art-direction.md "Audio"), the instruments
synthesised here like Mouse Espresso's or Spider Swing's alternatives, at the house mix (sample volumes <= 40).

  alt1  "Crystal Seams": glassy mallet minimalism in 7/8 (2+2+3), an eighth of 400, F lydian with a B flat
        middle and a D flat bridge. A glockenspiel and a vibraphone (motor off) lock their patterns together over a
        bass marimba; a glass harmonica sings the long tune. The hook: the glockenspiel's bouncing-marble cell
        (high-low, high-low, then a roll down), with the lydian B natural ringing in the glass tune. Intro (the
        glockenspiel alone, then the vibraphone, then the bass), A, A' (the vibraphone in canon, the glass tune),
        B (B flat, the cell regrouped 3+2+2), B' (the glass tune a fourth up), a D flat bridge (the glockenspiel
        rests), A'', then the loop back to A.
  alt2  "Deep Seam": an underground mining work song in 12/8, a dotted crotchet of 84, C minor with an E flat
        major middle. A lead voice calls, a miners' choir answers on the pickaxe strokes, an upright bass swings
        and the percussion is the mine itself: pickaxe, anvil, boot stomp and shovel. The hook: the choir's answer
        ("HEY . . HO! ya-HO!") and its A flat-G cadence. Intro (the tools alone), A (call and response), A' (busier
        hammers, the lead ad-libs), B (E flat major, toward the lamplight), a bridge deep down (a D flat hum, the
        anvil tolling), A'', then the loop back to A.

    make_music_alt.py              -> ../music-alt/alt1.mod, alt2.mod (previews; the game is unchanged)
    make_music_alt.py OUTDIR N     -> OUTDIR/tune.mod: alternative N (1 or 2), for tools/build_assets.py

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/molemarbles/LICENSE.
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


# ==== alt 1: Crystal Seams ==========================================================================================
def glockenspiel():
    """A glockenspiel bar: a bright fundamental, the 2.76 partial that dies fast, a hard mallet click (8 samples a
    cycle: C-1 = C5)."""
    v = pluck(8, 9000, [(1, 1), (2.76, .45)], 3800, 1.7, attack=1)
    rng = random.Random(3)
    for i in range(6):
        v[i] += rng.uniform(-.4, .4)
    return norm(v, 105)


def vibraphone():
    """A vibraphone bar with the motor off: the fundamental and the tuned fourth partial (16 samples a cycle:
    C-1 = C4)."""
    return norm(pluck(16, 12000, [(1, 1), (4, .25)], 6000, 1.0, attack=2), 105)


def bass_marimba():
    """A bass marimba: a round fundamental and the tuned fourth partial of a wooden bar (64 samples a cycle:
    C-1 = C2)."""
    return norm(pluck(64, 8000, [(1, 1), (4, .18)], 2600, 1.2, attack=4))


def glass_harmonica():
    """A glass harmonica: two nearly pure voices a few cents apart (a slow shimmer), a soft swell, then a long loop
    of whole cycles (2048 samples = 128 cycles: C-1 = C4)."""
    h = [(1, 1), (2, .1), (3, .05)]
    return tone_loop(2048, [(128, 1, h), (129, .3, h)], 512)


def seams():
    smp = [("glock", glockenspiel(), 34, 0, 0, 72),
           ("vibes", vibraphone(), 34, 0, 0, 60),
           ("marimba", bass_marimba(), 40, 0, 0, 36),
           ("glass", glass_harmonica(), 30, 512, 2048, 60)]
    s = Song(smp, rpb=7, bpp=8)                       # 7/8 in eighths: a bar = 7 rows, grouped 2+2+3
    GLK, VIB, BASS, GLS = 0, 1, 2, 3
    SHIMMER = (0x4, 0x22)

    X = [4, 2, 5, 2, 6, 5, 3]                         # the bouncing marble: high-low, high-low, then a roll down
    XR = X[-1:] + X[:-1]                              # the same cell an eighth late (the canon)
    Y = [0, 2, 1, 3, 2, 1, 4]                         # the vibraphone's interlocking cell
    Z = [6, 4, 2, 5, 3, 4, 1]                         # regrouped 3+2+2
    ZR = Z[-1:] + Z[:-1]

    def cells(r, ch, inst, chords, lo, pat, vol):
        s.line(r, ch, inst, arps(chords, lo, pat), 1, vol * 5 // 8, per_bar=7)

    def bass(r0, chords, nxt, style="pulse"):
        for b, name in enumerate(chords):
            pc, q, bpc = chord(name)
            r, root = r0 + 7 * b, fit(bpc, 38)
            nroot = fit(chord(chords[b + 1] if b + 1 < len(chords) else nxt)[2], 38)
            if style == "long":                       # the bridge: root and fifth, let ring
                s.put(r, BASS, "marimba", root, 32)
                s.put(r + 4, BASS, "marimba", root + 7, 22)
                continue
            s.put(r, BASS, "marimba", root, 32)
            s.put(r + 2, BASS, "marimba", root + 12, 18)
            s.put(r + 4, BASS, "marimba", root + 7, 25)
            if nroot != root:                         # step toward the next root
                s.put(r + 6, BASS, "marimba", nroot + (2 if nroot < root else -2), 22)
            else:
                s.put(r + 6, BASS, "marimba", root + 10 if q in ("7", "m7", "7sus4") else root + 12, 17)

    INTRO_CH = ["Fmaj7"] * 4 + ["G/F", "G/F", "Gsus4", "G"]
    A_CH = ["Fmaj7", "Fmaj7", "G/F", "G/F", "Em7", "Am7", "Dm7", "G"]
    B_CH = ["Bbmaj7", "Bbmaj7", "C/Bb", "C/Bb", "Am7", "Dm7", "Gm7", "C"]
    C_CH = ["Dbmaj7", "Dbmaj7", "Eb/Db", "Eb/Db", "Cm7", "Bbm7", "C7sus4", "C7"]
    GLS_A = ["c6 . . . . b5 .", "a5 . . . . . .", "b5 . . . d6 . .", "c6 . . . . b5 .",
             "g5 . . . . . .", "e6 . . . . c6 .", "d6 . . . a5 . .", "b5 . . . . - ."]
    GLS_B = [" ".join(t if t in ".-" else name_of(midi(t) + 5) for t in bar.split()) for bar in GLS_A]
    GLS_C = ["f5 . . . . . .", "ab5 . . . g5 . .", "bb5 . . . . . .", "g5 . . . eb5 . .",
             "g5 . . . . . .", "f5 . . . db5 . .", "f5 . . . . . .", "e5 . . . . - ."]

    # intro: the glockenspiel alone, then the vibraphone locks in, then the bass marimba
    r = s.bars(8)
    cells(r, GLK, "glock", INTRO_CH, 72, X, 28)
    cells(r + 14, VIB, "vibes", INTRO_CH[2:], 60, Y, 26)
    bass(r + 28, INTRO_CH[4:], "Fmaj7")
    # A: the two cells interlock over the moving bass
    s.loop_here()
    r = s.bars(8)
    cells(r, GLK, "glock", A_CH, 72, X, 30)
    cells(r, VIB, "vibes", A_CH, 60, Y, 28)
    bass(r, A_CH, "Fmaj7")
    # A': the vibraphone follows the glockenspiel an eighth late; the glass harmonica's tune
    r = s.bars(8)
    cells(r, GLK, "glock", A_CH, 72, X, 30)
    cells(r, VIB, "vibes", A_CH, 60, XR, 24)
    bass(r, A_CH, "Bbmaj7")
    s.line(r, GLS, "glass", GLS_A, 1, 19, SHIMMER, per_bar=7)
    # B: B flat lydian, the cell regrouped 3+2+2
    r = s.bars(8)
    cells(r, GLK, "glock", B_CH, 72, Z, 30)
    cells(r, VIB, "vibes", B_CH, 60, Y, 28)
    bass(r, B_CH, "Bbmaj7")
    # B': the glass tune a fourth up, the vibraphone in canon
    r = s.bars(8)
    cells(r, GLK, "glock", B_CH, 72, Z, 28)
    cells(r, VIB, "vibes", B_CH, 60, ZR, 24)
    bass(r, B_CH, "Dbmaj7")
    s.line(r, GLS, "glass", GLS_B, 1, 19, SHIMMER, per_bar=7)
    # bridge: D flat, deeper; the glockenspiel rests, then bounces back in
    r = s.bars(8)
    cells(r, VIB, "vibes", C_CH, 60, Z, 28)
    bass(r, C_CH, "Fmaj7", "long")
    s.line(r, GLS, "glass", GLS_C, 1, 21, SHIMMER, per_bar=7)
    cells(r + 42, GLK, "glock", C_CH[6:], 72, X, 26)
    # A'': everything, the tune once more
    r = s.bars(8)
    cells(r, GLK, "glock", A_CH, 72, X, 30)
    cells(r, VIB, "vibes", A_CH, 60, XR, 26)
    bass(r, A_CH, "Fmaj7")
    s.line(r, GLS, "glass", GLS_A, 1, 18, SHIMMER, per_bar=7)
    return s.build("mm crystal seams", 6, 100)        # speed 6 at 100 BPM: an eighth 150 ms (400 a minute)


# ==== alt 2: Deep Seam ==============================================================================================
def vowel(f0, formants, nh=12):
    """The harmonics of a sung vowel at f0 Hz: formant peaks (Hz, width, gain) over a gently falling source."""
    return [(k, sum(g * math.exp(-((k * f0 - F) / bw) ** 2) for F, bw, g in formants) + .35 / k ** 1.5)
            for k in range(1, nh + 1)]


AH = [(650, 140, 1.0), (1080, 160, .55), (2500, 300, .1)]
OH = [(450, 120, 1.0), (800, 140, .5), (2600, 300, .06)]


def voice():
    """A lead voice singing "ah": the vowel's harmonics at C3 with a breath at the onset, then a 32-sample loop
    (C-1 = C3); the vibrato is the tracker's."""
    return tone_loop(32, [(1, 1, vowel(130.8, AH))], 256, .25, 5)


def choir_wave(ph):
    return sum(a * math.sin(2 * math.pi * k * ph) for k, a in vowel(130.8, OH, 10))


def mine_kit():
    return [("pick", noise_hit(HI, 220, 28, 61, 1, [(2350, .8, 45), (3900, .5, 28), (170, 1.2, 50)]), 34, 0, 0, 36),
            ("anvil", noise_hit(HI, 700, 4, 62, 2, [(1180, 1.0, 380), (3010, .6, 260), (4570, .4, 180),
                                                    (6200, .2, 120)]), 30, 0, 0, 36),
            ("stomp", sweep(HI, 110, 55, 20, 130, 360, click=.25, seed=63), 34, 0, 0, 36),
            ("shovel", noise_hit(HI, 230, 55, 64, 1, attack_ms=60), 20, 0, 0, 36)]


def work_song():
    smp = [("voice", voice(), 32, 256, 32, 48),
           ("bass", norm(pluck(128, 12000, [(1, 1), (2, .5), (3, .25), (4, .12), (5, .06)], 5200, .7, attack=12)),
            38, 0, 0, 24)]
    for q, v in (("m", [0, 3, 7, 12]), ("", [0, 4, 7, 12]), ("7", [0, 4, 7, 10]), ("m7", [0, 3, 7, 10])):
        data, L = chord_loop(32, v, choir_wave)
        smp.append(("choir_" + q, data, 28, 0, L, 48))
    smp += mine_kit()
    s = Song(smp, rpb=12, bpp=2)                      # 12/8: an eighth = 1 row, a bar = 12 rows
    LEAD, CHOIR, BASS, PERC = 0, 1, 2, 3
    VIB, WIDE = (0x4, 0x24), (0x4, 0x35)

    PERC_PAT = {"work": ((0, "pick", 40), (2, "anvil", 16), (3, "stomp", 36), (6, "pick", 38), (8, "anvil", 16),
                         (9, "anvil", 30), (11, "shovel", 22)),
                "busy": ((0, "pick", 40), (1, "anvil", 15), (2, "anvil", 18), (3, "stomp", 38), (5, "anvil", 17),
                         (6, "pick", 40), (7, "anvil", 15), (8, "anvil", 18), (9, "anvil", 32), (10, "stomp", 26),
                         (11, "shovel", 24)),
                "lamp": ((0, "stomp", 36), (3, "anvil", 18), (6, "stomp", 30), (9, "anvil", 24), (11, "shovel", 18)),
                "deep": ((0, "stomp", 40), (6, "anvil", 32))}

    def perc(r0, bars, style):
        for b in range(bars):
            for e, inst, v in PERC_PAT[style]:
                s.put(r0 + 12 * b + e, PERC, inst, "c4", v)

    def bass(r0, chords, nxt, style="work"):
        for b, name in enumerate(chords):
            pc, q, bpc = chord(name)
            r, root = r0 + 12 * b, fit(bpc, 29)
            third = 3 if q.startswith("m") else 4
            nroot = fit(chord(chords[b + 1] if b + 1 < len(chords) else nxt)[2], 29)
            if style == "work":                       # dum . . dum . . (down a fourth) . . (a step below) . .
                notes = ((0, root, 38), (3, root, 26), (6, root - 5, 34), (9, root - 2, 28))
            elif style == "walk":
                app = nroot + 1 if nroot < root + 7 else nroot - 1
                notes = ((0, root, 36), (3, root + third, 28), (6, root + 7, 32), (9, app, 28))
            else:                                     # deep: one long note and a low fifth
                notes = ((0, root, 38), (8, root - 5, 26))
            for e, m, v in notes:
                s.put(r + e, BASS, "bass", m, v)

    def hum(r0, chords, vol=22, again=True):
        """The choir hums under the call: held, sung again on the third beat."""
        for b, name in enumerate(chords):
            pc, q, _ = chord(name)
            s.put(r0 + 12 * b, CHOIR, "choir_" + q, fit(pc, 55), vol)
            if again:
                s.put(r0 + 12 * b + 6, CHOIR, "choir_" + q, fit(pc, 55), vol - 3)

    def answer(r0, hits, vol=0):
        """The choir's answer: (row, chord, volume, rows held)."""
        for e, name, v, dur in hits:
            pc, q, _ = chord(name)
            s.put(r0 + e, CHOIR, "choir_" + q, fit(pc, 55), v + vol)
            s.off(r0 + e + dur, CHOIR)

    HEY_C = ((0, "Cm", 34, 2), (3, "Cm", 36, 2), (5, "Bb", 28, 1), (6, "Cm", 36, 4))
    HEY_G = ((0, "G7", 34, 2), (3, "G7", 36, 2), (5, "Ab", 30, 1), (6, "G", 36, 4))
    A_CH = ["Cm", "Cm", "Fm", "G7"]
    CALL = ["g4 . . bb4 . c5 eb5 . . c5 . .", "- . . . . . . . . . . .",
            "f4 . . ab4 . bb4 c5 . . bb4 . ab4", "g4 . . . . . - . . . . ."]
    CALL2 = ["c5 . . eb5 . f5 g5 . . eb5 . c5", "- . . . . . bb4 . c5 . . -",
             "ab4 . . c5 . eb5 f5 . . eb5 . c5", "b4 . . - . . d5 . . b4 . ."]
    B_CH = ["Ab", "Eb", "Fm7", "Bb7"]
    LAMP = ["c5 . . . . . eb5 . . c5 . .", "bb4 . . . . . g4 . . bb4 . .",
            "ab4 . . c5 . . eb5 . . f5 . .", "d5 . . . . . . . . - . ."]
    D_CH = ["Cm", "Db", "Ab", "G7"]
    DEEP = ["- . . . . . . . . . . .", ". . . . . . . . . ab4 . bb4",
            "c5 . . . . . bb4 . . ab4 . .", "g4 . . . . . . . . - . ."]

    # intro: the tools alone, then the bass and a hum; the lead takes a breath
    r = s.bars(2)
    perc(r, 1, "work")
    perc(r + 12, 1, "work")
    hum(r, ["Cm"], 16, False)
    bass(r + 12, ["Cm"], "Cm")
    hum(r + 12, ["Cm"], 20, False)
    s.line(r + 12, LEAD, "voice", "- . . . . . . . . c4 . eb4", 1, 28)
    # A: the call and the choir's answer on the pickaxe strokes
    s.loop_here()
    r = s.bars(4)
    perc(r, 4, "work")
    bass(r, A_CH, "Cm")
    s.line(r, LEAD, "voice", CALL, 1, 32, VIB, per_bar=12)
    hum(r, A_CH[0:1])
    answer(r + 12, HEY_C)
    hum(r + 24, A_CH[2:3])
    answer(r + 36, HEY_G)
    # A': harder hammers, a higher call, the lead ad-libs over the answers
    r = s.bars(4)
    perc(r, 4, "busy")
    bass(r, A_CH, "Ab")
    s.line(r, LEAD, "voice", CALL2, 1, 32, VIB, per_bar=12)
    hum(r, A_CH[0:1], 24)
    answer(r + 12, HEY_C, 2)
    hum(r + 24, A_CH[2:3], 24)
    answer(r + 36, HEY_G, 2)
    # B: E flat major, toward the lamplight: long lines, the choir sustains, the bass walks
    r = s.bars(4)
    perc(r, 4, "lamp")
    bass(r, B_CH, "Cm", "walk")
    s.line(r, LEAD, "voice", LAMP, 1, 32, WIDE, per_bar=12)
    hum(r, B_CH, 26)
    # bridge: deep down; a D flat hum, the anvil tolls, the lead comes back up
    r = s.bars(4)
    perc(r, 4, "deep")
    bass(r, D_CH, "Cm", "deep")
    hum(r, D_CH, 28, False)
    s.line(r, LEAD, "voice", DEEP, 1, 30, WIDE, per_bar=12)
    # A'': the answers loudest
    r = s.bars(4)
    perc(r, 4, "busy")
    bass(r, A_CH, "Cm")
    s.line(r, LEAD, "voice", CALL2[:3] + ["g4 . . . . . . . . c4 . eb4"], 1, 32, VIB, per_bar=12)
    hum(r, A_CH[0:1], 24)
    answer(r + 12, HEY_C, 4)
    hum(r + 24, A_CH[2:3], 24)
    answer(r + 36, HEY_G, 4)
    return s.build("mm deep seam", 6, 63)             # speed 6 at 63 BPM: an eighth 238 ms (dotted crotchet 84)


TRACKS = [seams, work_song]


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
