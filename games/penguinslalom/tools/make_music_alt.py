#!/usr/bin/env python3
"""Penguin Slalom alternative music: two 4-channel ProTracker MODs written with the house music kit
(games/common/tools/house_music.py: the MOD writer and periods; docs/art-direction.md "Audio"), the instruments
synthesised here like Pogo Mamie's musette or Beaver Rush's banjo, at the house mix (sample volumes <= 40).

  alt1  "Valse des Flocons": an icy skaters' waltz, 3/4 at 180 (one in a bar), E minor with a G major middle and
        a C Lydian "aurora" bridge. Bowed strings sing the melody over a harp gliding up each bar, a pizzicato
        bass and a celesta whose notes fall like snowflakes. The hook: a zig-zag between the gates (G-E, F#-B,
        E-G) that lands on a bright C# (the raised sixth). Intro, A (16 bars), B and B' (the celesta leads, the
        strings answer), bridge, A', then the loop back to A.
  alt2  "Polar Polka": a fast brass polka on the piste, 2/4 at 152, E flat major with a C minor strain and a trio
        in A flat. A trumpet leads, a trombone smears and slides underneath, an oom-pah (a plucked bass on the beat,
        a short brass chord after it), a kick, a snare and a cymbal. The hook: a waddle (G F G B flat) and a run
        down the slope, with a trombone slide at the end of each phrase. Intro (a fanfare), A (16 bars), B (zig on
        the trumpet, zag on the trombone), A' (in harmony), the trio (the trombone sings, the trumpet twiddles), then
        the loop back to A.

    make_music_alt.py              -> ../music-alt/alt1.mod, alt2.mod (previews; the game is unchanged)
    make_music_alt.py OUTDIR N     -> OUTDIR/tune.mod: alternative N (1 or 2), for tools/build_assets.py

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/penguinslalom/LICENSE.
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


# ==== alt 1: Valse des Flocons ======================================================================================
def celesta():
    """A celesta: a bell-like sine with a quick fourth partial (16 samples a cycle: C-1 = C4)."""
    return norm(pluck(16, 9000, [(1, 1), (2, .1), (4, .32), (6, .05)], 5200, 1.3, attack=2))


def strings():
    """A string section: three detuned saws (2048-sample loop, 127/128/129 cycles), a slow bow attack
    (16 samples a cycle: C-1 = C4)."""
    L, A = 2048, 1200

    def ens(i):
        v = 0.0
        for cyc, ph in ((128, 0), (129, .3), (127, .7)):
            x = cyc * i / L + ph
            v += sum(math.sin(2 * math.pi * k * x) / k ** 1.3 for k in range(1, 7))
        return v
    return norm([ens(i) * min(1.0, i / A) for i in range(A + L)], 100)


def waltz():
    smp = [("celesta", celesta(), 30, 0, 0, 60),
           ("bass", norm(pluck(64, 10000, [(1, 1), (2, .35), (3, .12)], 5000, .6, attack=30)), 36, 0, 0, 36),
           ("harp", norm(pluck(32, 7000, [(1, 1), (2, .35), (3, .15), (4, .06)], 3500, .8, attack=4)), 28, 0, 0, 48),
           ("strings", strings(), 24, 1200, 2048, 60),
           ("triangle", noise_hit(HI, 900, 4, 51, 2, [(4180, 1, 380), (5960, .5, 260), (2710, .3, 200)]), 18, 0, 0, 36)]
    s = Song(smp, rpb=12, bpp=4)                      # 3/4 in 16ths: a crotchet = 4 rows
    CEL, BASS, HARP, STR = 0, 1, 2, 3
    VIB = (0x4, 0x23)

    def accomp(r0, chords, down=False, soft=0, bass=True):
        """The harp climbs the chord in eighths (and falls back on alternate bars), the bass on the downbeat."""
        for b, name in enumerate(chords):
            pc, q, bpc = chord(name)
            r = r0 + 12 * b
            if bass:
                s.put(r, BASS, "bass", fit(bpc, 40), 32 - soft)
            base = fit(pc, 48)
            ladder = sorted({base + k for k in QUAL[q]} | {base + 12 + k for k in QUAL[q]})[:6]
            if down and b % 2:
                ladder = ladder[::-1]
            for e, m in enumerate(ladder):
                s.put(r + 2 * e, HARP, "harp", m, (23 if e == 0 else 17) - soft)

    def flakes(r0, chords, every=2, vol=19):
        """The celesta: three chord tones falling on the second and third beats (snowflakes)."""
        for b, name in enumerate(chords):
            if b % every != every - 1:
                continue
            top = sorted(fit(p, 84) for p in tones(name))[::-1]
            for e, m in zip((6, 8, 10), top):
                s.put(r0 + 12 * b + e, CEL, "celesta", m, vol - (e - 6))

    A_CH = ["Em", "B7/D#", "Em/D", "A/C#", "Cmaj7", "Am", "B7", "B7",
            "Em", "B7/D#", "Em/D", "A/C#", "C", "B7", "Em", "Em"]
    A = ["g5 . e5", "f#5 . b4", "e5 . g5", "c#6 . .", "b5 . g5", "e5 . c5", "d#5 e5 f#5", "b5 . .",
         "g5 . e5", "f#5 . b4", "e5 . g5", "c#6 . .", "e6 . c6", "b5 a5 f#5", "e5 . .", ". . -"]
    B_CH = ["G", "Em", "C", "D7", "G", "Em", "Am7", "D7"]
    B = ["d6 b5 g5 b5 d6 g6", "e6 . b5 . g5 .", "e6 c6 g5 c6 e6 g6", "f#6 . . e6 c6 a5",
         "d6 b5 g5 b5 d6 g6", "g6 . e6 . b5 .", "c6 . a5 . e5 g5", "f#5 . a5 . c6 ."]
    B2 = B[:3] + ["f#6 a6 f#6 d6 c6 a5"] + B[4:7] + ["a5 . f#5 . d5 ."]
    B_STR = ["b4 . .", "b4 . .", "c5 . .", "c5 . .", "d5 . .", "e5 . .", "e5 . .", "d5 . ."]
    B2_STR = ["g5 . .", "g5 . .", "g5 . .", "f#5 . .", "g5 . .", "b5 . .", "a5 . .", "f#5 . -"]
    BR_CH = ["D/C", "C", "D/C", "C", "Am", "Am/G", "B7sus4", "B7"]
    BR_CEL = ["f#6 . d6 . a5 .", "g6 . e6 . c6 .", "a6 . f#6 . d6 .", "g6 . e6 . b5 .",
              "e6 . c6 . a5 .", "e6 . c6 . a5 .", "e6 . b5 . f#5 .", "d#6 . b5 . f#5 ."]
    BR_STR = ["e5 . .", ". . .", "f#5 . .", "e5 . .", "c5 . .", "e5 . .", "e5 . .", "d#5 . -"]

    # intro: the harp alone with a triangle, the bass and the snowflakes join, the strings pick up
    r = s.bars(4)
    accomp(r, ["Em", "Cmaj7", "Am", "B7"], down=True)
    for b in (0, 1):
        for e in range(12):
            s.rows[r + 12 * b + e][BASS] = [0, 0, 0, 0]
        s.put(r + 12 * b, CEL, "triangle", "c4", 26)
    flakes(r + 24, ["Am", "B7"], every=1)
    s.line(r + 36, STR, "strings", ". . b4", 4, 24, VIB)
    # A: the zig-zag melody on the strings
    s.loop_here()
    r = s.bars(16)
    accomp(r, A_CH)
    flakes(r, A_CH)
    s.line(r, STR, "strings", A, 4, 29, VIB, per_bar=3)
    # B: G major, the celesta leads, the strings hold an answer
    r = s.bars(8)
    accomp(r, B_CH, down=True)
    s.line(r, CEL, "celesta", B, 2, 28, per_bar=6)
    s.line(r, STR, "strings", B_STR, 4, 20, VIB, per_bar=3)
    # B': the same, the strings higher
    r = s.bars(8)
    accomp(r, B_CH, down=True)
    s.line(r, CEL, "celesta", B2, 2, 28, per_bar=6)
    s.line(r, STR, "strings", B2_STR, 4, 22, VIB, per_bar=3)
    # bridge: C Lydian (D over C), the aurora: the celesta arpeggiates, the strings glow, the harp softer
    r = s.bars(8)
    accomp(r, BR_CH, soft=6)
    s.line(r, CEL, "celesta", BR_CEL, 2, 24, per_bar=6)
    s.line(r, STR, "strings", BR_STR, 4, 22, VIB, per_bar=3)
    # A': the first half of A, the snowflakes on every bar
    r = s.bars(8)
    accomp(r, A_CH[:8])
    flakes(r, A_CH[:8], every=1)
    s.line(r, STR, "strings", A[:8], 4, 29, VIB, per_bar=3)
    return s.build("ps valse des flocons", 4, 120)   # speed 4 at 120 BPM: a crotchet 333 ms (180 a minute)


# ==== alt 2: Polar Polka ============================================================================================
def brass(P, harm, attack, seed):
    """A brass tone: harmonics over P samples, a quick lip "blat" (a swell and a puff of noise), then the loop."""
    rng = random.Random(seed)
    wave = [sum(a * math.sin(2 * math.pi * k * i / P) for k, a in harm) for i in range(P)]
    att = [wave[i % P] * (i / float(attack)) ** 1.5 + rng.uniform(-.2, .2) * math.exp(-i / 40.0) for i in range(attack)]
    return norm(att + wave, 100)


TPT_H = [(1, 1), (2, .9), (3, .75), (4, .6), (5, .45), (6, .3), (7, .2)]
TBN_H = [(1, 1), (2, .75), (3, .55), (4, .4), (5, .28), (6, .18), (7, .12), (8, .08)]


def polka():
    def pahwave(ph):
        return sum(a * math.sin(2 * math.pi * k * ph) for k, a in TBN_H[:5])
    pah = {q: chord_loop(32, v, pahwave) for q, v in (("", [0, 4, 7, 12]), ("m", [0, 3, 7, 12]), ("7", [0, 4, 7, 10]))}
    smp = [("trumpet", brass(16, TPT_H, 128, 1), 28, 128, 16, 60),
           ("trombone", brass(32, TBN_H, 256, 2), 28, 256, 32, 48),
           ("bass", norm(pluck(64, 6000, [(1, 1), (2, .5), (3, .25), (4, .1)], 2400, .7, attack=4)), 38, 0, 0, 36),
           ("pah_M", pah[""][0], 20, 0, pah[""][1], 48),
           ("pah_m", pah["m"][0], 20, 0, pah["m"][1], 48),
           ("pah_7", pah["7"][0], 20, 0, pah["7"][1], 48),
           ("kick", sweep(LO, 120, 50, 25, 100, 250, .3), 34, 0, 0, 48),
           ("snare", noise_hit(HI, 200, 60, 41, 1, [(190, .9, 40)]), 26, 0, 0, 36),
           ("cymbal", noise_hit(HI, 1200, 350, 42, 2, [(3400, .3, 450), (4700, .25, 380)], attack_ms=2), 22, 0, 0, 36)]
    s = Song(smp, rpb=8, bpp=4)                       # 2/4 in 16ths
    TPT, TBN, OOM, DR = 0, 1, 2, 3
    pah_of = {"": "pah_M", "m": "pah_m", "7": "pah_7"}

    def oompah(r0, chords, vol=26):
        """oom (the bass on the beat), pah (a short brass chord on the offbeat)"""
        for b, name in enumerate(chords):
            pc, q, bpc = chord(name)
            r = r0 + 8 * b
            s.put(r, OOM, "bass", fit(bpc, 39), 38)
            s.put(r + 4, OOM, "bass", fit(pc + 7, 39), 32)
            for e in (2, 6):
                s.put(r + e, OOM, pah_of[q], fit(pc, 51), vol if e == 2 else vol - 3)
                s.off(r + e + 1, OOM)

    def drums(r0, bars, light=False, crash=False, roll_every=8):
        for b in range(bars):
            r = r0 + 8 * b
            s.put(r, DR, "cymbal" if crash and b == 0 else "kick", "c4", 34 if not light else 28)
            s.put(r + 4, DR, "kick", "c4", 28 if not light else 22)
            if roll_every and b % roll_every == roll_every - 1 and not light:
                for e in range(2, 8):
                    s.put(r + e, DR, "snare", "c4", 14 + 4 * e)
                continue
            s.put(r + 2, DR, "snare", "c4", 22 if not light else 14)
            s.put(r + 6, DR, "snare", "c4", 24 if not light else 16)

    A_CH = ["Eb", "Eb", "Bb7", "Bb7", "Bb7", "Bb7", "Eb", "Eb", "Eb", "Eb7", "Ab", "Abm", "Eb/Bb", "Bb7", "Eb", "Eb"]
    A = ["g5 f5 g5 bb5", "g5 . eb5 .", "f5 eb5 f5 ab5", "f5 . d5 .", "bb4 c5 d5 eb5", "f5 g5 ab5 f5", "g5 . bb5 .",
         "eb6 . - .", "g5 f5 g5 bb5", "db6 . bb5 g5", "c6 bb5 ab5 c6", "b5 . ab5 eb5", "g5 . f5 eb5", "f5 . bb4 d5",
         "eb5 . g5 bb5", "eb5 . - ."]
    TBN_A = ["g3 . . .", "bb3 . . .", "ab3 . . .", "d4 . . .", "f3 . . .", "ab3 . bb3 c4", "bb3 . . .", "g3 eb4/ . -",
             "g3 . . .", "db4 . . .", "c4 . . .", "b3 . . .", "bb3 . . .", "ab3 . f3 .", "g3 . bb3 .", "eb3/ . - ."]
    B_CH = ["Cm", "G7", "Cm", "G7", "Fm", "Cm", "G7", "Bb7"]
    TPT_B = ["c5 eb5 g5 c6", "- . . .", "c6 g5 eb5 c5", "- . . .", "f5 ab5 c6 f5", "- . . .", "b4 d5 f5 ab5", "- . . ."]
    TBN_B = ["- . . .", "b3 d4 f4 g4", "- . . .", "g4 f4 d4 b3", "- . . .", "c4 eb4 g4 c4", "- . . .", "bb3 d4 f4 ab4"]
    T_CH = ["Ab", "Ab", "Eb7", "Eb7", "Eb7", "Eb7", "Ab", "Ab", "Db", "Db", "Ab", "F7", "Bb7", "Eb7", "Ab", "Bb7"]
    TBN_T = ["eb4 . ab4 .", "c5 . . .", "bb4 . g4 .", "eb4 . . .", "db4 . eb4 .", "g4 . bb4 .", "ab4 . . .",
             "c4/ . . -", "f4 . ab4 .", "db5 . . .", "c5 . ab4 .", "a4 . c5 .", "bb4 . f4 .", "g4 . db4 .",
             "c4 . eb4 .", "d4 . - ."]
    TPT_T = ["- . . .", "ab5 g5 ab5 c6", "- . . .", "g5 f5 eb5 db5", "- . . .", "bb5 ab5 g5 bb5", "- . . .",
             "eb6 c6 ab5 eb5", "- . . .", "f5 ab5 db6 f5", "- . . .", "eb5 f5 a5 c6", "d6 . bb5 .", "g5 . bb5 db6",
             "c6 . ab5 .", "d5 f5 ab5 bb5"]

    # intro: a fanfare up the B flat seventh in octaves, a cymbal, then the band vamps with a snare roll
    r = s.bars(4)
    s.line(r, TPT, "trumpet", ["bb4 d5 f5 ab5", "bb5 . - .", "- . . .", ". . . ."], 2, 30, per_bar=4)
    s.line(r, TBN, "trombone", ["bb3 d4 f4 ab4", "bb4 . - .", "- . . .", ". . . ."], 2, 26, per_bar=4)
    s.put(r + 8, DR, "cymbal", "c4", 30)
    oompah(r + 16, ["Bb7", "Bb7"])
    drums(r + 16, 2, roll_every=2)
    # A: the waddle and the slalom, the trombone smears underneath
    s.loop_here()
    r = s.bars(16)
    oompah(r, A_CH)
    drums(r, 16, crash=True)
    s.line(r, TPT, "trumpet", A, 2, 25, per_bar=4)
    s.line(r, TBN, "trombone", TBN_A, 2, 22, slide=0x10, per_bar=4)
    # B: C minor, zig (trumpet) and zag (trombone)
    r = s.bars(8)
    oompah(r, B_CH)
    drums(r, 8)
    s.line(r, TPT, "trumpet", TPT_B, 2, 25, per_bar=4)
    s.line(r, TBN, "trombone", TBN_B, 2, 26, per_bar=4)
    # A': the second half of A, the trombone in harmony an octave below
    r = s.bars(8)
    oompah(r, A_CH[8:])
    drums(r, 8, crash=True)
    s.line(r, TPT, "trumpet", A[8:], 2, 25)
    low = [" ".join(t if t[0] in ".-" else name_of(midi(t) - 12) for t in bar.split())
           for bar in harmony(A[8:], A_CH[8:])]
    s.line(r, TBN, "trombone", low, 2, 22)
    # trio: A flat, the trombone sings, the trumpet twiddles in the gaps, the drums lighter
    r = s.bars(16)
    oompah(r, T_CH, 22)
    drums(r, 16, light=True, crash=True)
    s.line(r, TBN, "trombone", TBN_T, 2, 26, slide=0x10, per_bar=4)
    s.line(r, TPT, "trumpet", TPT_T, 2, 22, per_bar=4)
    return s.build("ps polar polka", 6, 152)          # speed 6 at 152 BPM: a 16th 99 ms (crotchet 152)


TRACKS = [waltz, polka]


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
