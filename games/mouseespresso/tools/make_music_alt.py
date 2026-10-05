#!/usr/bin/env python3
"""Mouse Espresso alternative music: two 4-channel ProTracker MODs written with the house music kit
(games/common/tools/house_music.py: the MOD writer and periods; docs/art-direction.md "Audio"), the instruments
synthesised here like Pogo Mamie's musette or Beaver Rush's banjo, at the house mix (sample volumes <= 40).

  alt1  "Tarantella del Topolino": an Italian cafe tarantella, 6/8 at a dotted crotchet of 138, A minor with a
        sunny A major middle. A mandolin over a "boom-chick-chick" guitar and a tambourine; a clarinet answers.
        The hook: a little spinning figure (A B C B A G#) that leaps up an octave like the steam. Intro, A, A' (the
        second mandolin in harmony), B (call and response), B' (roles swapped), a D minor bridge (tremolo
        mandolin over a falling bass), A'' with an accelerando, then the loop back to A.
  alt2  "Espresso in Five": cool jazz in 5/4 for the night shift, a crotchet of 168, F minor. A dry alto sax over
        an electric piano, an upright bass and a ride cymbal, the five beats grouped 2+3 (swung on a triplet grid).
        The hook: a little rising "sip" (C, E flat-F, A flat) and a bluesy C flat that slips up to C. Intro (the
        bass ostinato alone), A, B (a walking bass, long lines from B flat minor), A' (more notes), a break for the
        electric piano, then the loop back to A.

    make_music_alt.py              -> ../music-alt/alt1.mod, alt2.mod (previews; the game is unchanged)
    make_music_alt.py OUTDIR N     -> OUTDIR/tune.mod: alternative N (1 or 2), for tools/build_assets.py

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/mouseespresso/LICENSE.
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


def mandolin():
    """A bright double-course pluck, a quick decay (16 samples a cycle: C-1 = C4)."""
    return norm(pluck(16, 5000, [(1, 1), (2, .7), (3, .5), (4, .4), (5, .25), (6, .15), (7, .1)], 2200, .8,
                      detune=5))


def nylon_bass(tau=2600, n=6000):
    """A low plucked nylon string (64 samples a cycle: C-1 = C2)."""
    return norm(pluck(64, n, [(1, 1), (2, .55), (3, .3), (4, .18), (5, .1), (6, .06)], tau, .6, attack=8))


def clarinet():
    """A woody clarinet: odd harmonics, a breathy 8-cycle attack, then a 32-sample loop (C-1 = C3)."""
    rng = random.Random(4)
    wave = [math.sin(2 * math.pi * i / 32) + .04 * math.sin(4 * math.pi * i / 32) + .42 * math.sin(6 * math.pi * i / 32)
            + .2 * math.sin(10 * math.pi * i / 32) + .09 * math.sin(14 * math.pi * i / 32) for i in range(32)]
    att = [wave[i % 32] * (i / 256.0) + rng.uniform(-.25, .25) * (1 - i / 256.0) * (i / 256.0) for i in range(256)]
    return norm(att + wave, 100)


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


def tambourine(hit):
    """Jingles (and a palm thud on the downbeat)."""
    jing = [(3950, .5, 90), (5230, .45, 70), (6110, .4, 60), (7280, .3, 50)]
    if hit:
        jing = jing + [(110, 1.4, 70), (220, .4, 40)]
    return noise_hit(HI, 200, 45, 31 if hit else 32, 2, jing)


# ==== alt 1: Tarantella del Topolino ================================================================================
def tarantella():
    smp = [("mandolin", mandolin(), 38, 0, 0, 60),
           ("guitar", nylon_bass(), 38, 0, 0, 36),
           ("gtr_m", strum(32, [0, 7, 12, 15], 5000, [(1, 1), (2, .5), (3, .3), (4, .15)], 1500, 40), 26, 0, 0, 48),
           ("gtr_M", strum(32, [0, 7, 12, 16], 5000, [(1, 1), (2, .5), (3, .3), (4, .15)], 1500, 40), 26, 0, 0, 48),
           ("gtr_7", strum(32, [0, 7, 10, 16], 5000, [(1, 1), (2, .5), (3, .3), (4, .15)], 1500, 40), 26, 0, 0, 48),
           ("tamb", tambourine(False), 22, 0, 0, 36),
           ("tamb_hit", tambourine(True), 28, 0, 0, 36),
           ("clarinet", clarinet(), 24, 256, 32, 48)]
    s = Song(smp, rpb=12, bpp=4)                      # 6/8: an eighth = 2 rows, a bar = 12 rows
    MAND, GTR, TAMB, CLAR = 0, 1, 2, 3
    strum_of = {"m": "gtr_m", "": "gtr_M", "7": "gtr_7"}

    def guitar(r0, chords, rolled=False):
        for b, name in enumerate(chords):
            pc, q, bass = chord(name)
            r, top = r0 + 12 * b, fit(pc, 52)
            s.put(r, GTR, "guitar", fit(bass, 40), 40)
            if rolled:                                # the bridge: a chord let ring, a soft fifth
                s.put(r + 2, GTR, strum_of[q], top, 30)
                s.put(r + 8, GTR, "guitar", fit(pc + 7, 40), 28)
                continue
            s.put(r + 2, GTR, strum_of[q], top, 30)     # boom-chick-chick, boom-chick-chick
            s.put(r + 4, GTR, strum_of[q], top, 22)
            s.put(r + 6, GTR, "guitar", fit(pc + 7, 40), 34)
            s.put(r + 8, GTR, strum_of[q], top, 30)
            s.put(r + 10, GTR, strum_of[q], top, 22)

    def tamb(r0, bars, busy=False, sparse=False, roll_last=False):
        for b in range(bars):
            r = r0 + 12 * b
            s.put(r, TAMB, "tamb_hit", "c4", 34 if not sparse else 26)
            hits = ((6, 22),) if sparse else ((2, 16), (4, 20), (6, 30), (8, 16), (10, 20))
            for e, v in hits:
                s.put(r + e, TAMB, "tamb", "c4", v)
            if busy:
                for e in (1, 3, 5, 7, 9, 11):
                    s.put(r + e, TAMB, "tamb", "c4", 11)
            if roll_last and b == bars - 1:           # a shaken roll into the next section
                for e in range(12):
                    s.put(r + e, TAMB, "tamb", "c4", 12 + 2 * e)

    A_CH = ["Am", "Am", "Dm", "Am", "E", "E7", "Am", "Am"]
    A = ["a4 b4 c5 b4 a4 g#4", "a4 c5 e5 a5 . e5", "f5 . d5 a4 . d5", "c5 b4 a4 b4 c5 d5",
         "e5 . g#5 b5 . g#5", "a5 g#5 f5 e5 d5 b4", "c5 b4 a4 e5 c5 a4", "a4* . . . - ."]
    B_CH = ["A", "A", "E", "E", "D", "A", "E", "A"]
    B_CALL = ["c#5 . e5 a5 . e5", "c#5 . a4 e4 . -", ". . . . . .", ". . . . . ."]
    B_ANSWER = [". . . . . .", ". . . . . .", "b4 . d5 g#5 . d5", "b4 . g#4 e4 . -"]
    B_TUNE = ["f#5 . a5 f#5 . d5", "e5 . c#5 a4 . c#5", "b4 c#5 d5 e5 f#5 g#5", "a5 . e5 a4 . -"]
    B_COUNTER = ["a4 . . f#4 . .", "e4 . . a4 . .", "g#4 . . b4 . .", "c#5 . . . - ."]
    BR_CH = ["Dm", "Dm/C", "Bb", "A7", "Dm", "F", "E", "E7"]
    BR = ["f5* . . . . .", "e5* . . . . .", "d5* . . f5* . .", "e5* . . c#5* . .",
          "d5* . . a5* . .", "c6* . . a5* . .", "g#5* . . b5* . .", "e5 d5 c5 b4 a4 g#4"]
    BR_LOW = ["a3 . . . . .", "g3 . . . . .", "f3 . . . . .", "e3 . . . . .",
              "f3 . . . . .", "a3 . . . . .", "b3 . . . . .", "g#3 . . . - ."]

    # intro: the tambourine alone, then the guitar vamp, the mandolin runs down into the hook
    r = s.bars(4)
    tamb(r, 1, roll_last=True)
    guitar(r + 12, ["Am", "E7", "E7"])
    tamb(r + 12, 3)
    s.line(r + 36, MAND, "mandolin", "e5 d5 c5 b4 a4 g#4", 2, 34)
    # A: the hook
    s.loop_here()
    r = s.bars(8)
    guitar(r, A_CH)
    tamb(r, 8)
    s.line(r, MAND, "mandolin", A, 2, 38, per_bar=6)
    # A': a second mandolin in harmony
    r = s.bars(8)
    guitar(r, A_CH)
    tamb(r, 8)
    s.line(r, MAND, "mandolin", A, 2, 38)
    s.line(r, CLAR, "mandolin", harmony(A, A_CH), 2, 23)
    # B: A major, the mandolin calls, the clarinet answers, then together
    r = s.bars(8)
    guitar(r, B_CH)
    tamb(r, 8)
    s.line(r, MAND, "mandolin", B_CALL + B_TUNE, 2, 38, per_bar=6)
    s.line(r, CLAR, "clarinet", B_ANSWER + B_COUNTER, 2, 25, per_bar=6)
    # B': the roles swapped, the mandolin answers in tremolo
    r = s.bars(8)
    guitar(r, B_CH)
    tamb(r, 8, busy=True)
    s.line(r, CLAR, "clarinet", B_CALL + B_COUNTER, 2, 25)
    s.line(r, MAND, "mandolin", [". . . . . .", ". . . . . .", "b4* . d5* g#5* . d5*", "b4* . g#4* e4* . -"] + B_TUNE,
           2, 38)
    # bridge: D minor, tremolo mandolin over a falling bass, the clarinet low; the tambourine rolls into A''
    r = s.bars(8)
    guitar(r, BR_CH[:7], rolled=True)
    guitar(r + 84, BR_CH[7:])
    tamb(r, 7, sparse=True)
    tamb(r + 84, 1, roll_last=True)
    s.line(r, MAND, "mandolin", BR, 2, 36, per_bar=6)
    s.line(r, CLAR, "clarinet", BR_LOW, 2, 20, per_bar=6)
    # A'': everything, the clarinet an octave below, and the tarantella speeds up
    r = s.bars(8)
    guitar(r, A_CH)
    tamb(r, 8, busy=True)
    s.line(r, MAND, "mandolin", A, 2, 40)
    low = [" ".join(t if t[0] in ".-" else name_of(midi(t.rstrip("*")) - 12) for t in bar.split()) for bar in A]
    s.line(r, CLAR, "clarinet", low, 2, 18)
    for b, bpm in enumerate([104, 105, 106, 107, 108, 110, 112, 114]):
        if b:
            s.flow(r + 12 * b, 0xF, bpm)
    return s.build("me tarantella", 3, 104)       # speed 3 at 104 BPM: an eighth 145 ms (dotted crotchet 138)


# ==== alt 2: Espresso in Five =======================================================================================
def alto_sax():
    """A dry alto saxophone: a reedy spectrum, a breathy 12-cycle attack, then a 16-sample loop (C-1 = C4)."""
    rng = random.Random(12)
    h = [(1, 1), (2, .8), (3, .62), (4, .45), (5, .34), (6, .2), (7, .12)]
    wave = [sum(a * math.sin(2 * math.pi * k * i / 16 + .3 * k) for k, a in h) for i in range(16)]
    att = [wave[i % 16] * min(1.0, i / 120.0) + rng.uniform(-.4, .4) * math.exp(-i / 60.0) for i in range(192)]
    return norm(att + wave, 100)


def rhodes(semis):
    """An electric piano chord (one-shot): soft tines with a bell overtone that fades fast; the semitones are above
    the sample's pitch (32 samples a cycle: C-1 = C3), a rootless voicing over the bass."""
    out = [0.0] * 9000
    for semi in semis:
        P = 32 / 2 ** (semi / 12.0)
        tine = pluck(P, 9000, [(1, 1), (2, .22), (3, .05)], 5000, .8, attack=12)
        bell = pluck(P, 9000, [(7, .25)], 700, 0, attack=2) if 7 / P < .45 else [0.0] * 9000
        out = [a + x + y for a, x, y in zip(out, tine, bell)]
    return norm(out, 100)


def five():
    voicing = {"m7": [3, 7, 10, 14], "maj7": [4, 7, 11, 14], "7": [4, 10, 14, 21], "m7b5": [3, 6, 10, 12]}
    smp = [("sax", alto_sax(), 30, 192, 16, 60),
           ("bass", norm(pluck(64, 9000, [(1, 1), (2, .4), (3, .15)], 3800, .6, attack=10)), 38, 0, 0, 36)]
    smp += [("ep_" + q, rhodes(v), 26, 0, 0, 48) for q, v in voicing.items()]
    bell = [(3150, .5, 320), (4230, .4, 260), (5310, .32, 220), (6620, .25, 180)]
    smp += [("ride", noise_hit(HI, 700, 90, 13, 2, bell, attack_ms=1), 18, 0, 0, 36)]
    s = Song(smp, rpb=15, bpp=4)                      # 5/4 in triplets: a beat = 3 rows, swung eighths on rows 0 and 2
    SAX, BASS, EP, DR = 0, 1, 2, 3
    VIB = (0x4, 0x23)

    def bass(r0, chords, nxt, walk=False):
        for b, name in enumerate(chords):
            pc, q, bpc = chord(name)
            r, root = r0 + 15 * b, fit(bpc, 41)
            npc = chord(chords[b + 1] if b + 1 < len(chords) else nxt)[0]
            if walk:                                   # crotchets up the chord, a half step into the next root
                third = 3 if q in ("m7", "m7b5") else 4
                seventh = 11 if q == "maj7" else 10
                for e, m in enumerate([root, root + third, root + 7, root + seventh, fit(npc + 1, 41)]):
                    s.put(r + 3 * e, BASS, "bass", m, 38 if e == 0 else 32)
                continue
            for e, k, v in ((0, 0, 40), (3, 7, 30), (6, 12, 36), (9, 7, 30), (12, 10, 32)):     # the 2+3 ostinato
                s.put(r + e, BASS, "bass", root + k, v)
            s.put(r + 14, BASS, "bass", fit(npc - 1, 41), 24)

    def comp(r0, chords, style="vamp", vol=26):
        for b, name in enumerate(chords):
            pc, q, _ = chord(name)
            hits = {"vamp": ((0, vol), (5, vol - 4), (11, vol - 6)), "held": ((0, vol - 4), (9, vol - 10)),
                    "break": ((0, vol), (2, vol - 8), (5, vol - 4), (8, vol - 8), (11, vol - 4), (14, vol - 8))}[style]
            for e, v in hits:
                s.put(r0 + 15 * b + e, EP, "ep_" + q, fit(pc, 50), v)

    def ride(r0, bars, vol=26):
        """ding, ding-da ding, ding-da ding: the swung ride over the 2+3"""
        for b in range(bars):
            for e, d in ((0, 2), (3, -4), (5, -8), (6, 2), (9, -4), (11, -8), (12, -4)):
                s.put(r0 + 15 * b + e, DR, "ride", "c4", vol + d)

    A_CH = ["Fm7", "Fm7", "Dbmaj7", "Dbmaj7", "Fm7", "Fm7", "Gm7b5", "C7"]
    A = ["c5 . eb5+.+f5 ab5 .", "g5+.+f5 eb5 . c5 .", "f5 . . ab5+.+c6 .", "c6 . bb5+.+ab5 f5 .",
         "c5 . eb5+.+f5 ab5 .", "b5+.+c6 ab5 . f5 .", "f5 . db5+.+f5 g5 .", "e5 . g5+.+bb5 c6 -"]
    A2 = ["c5 . eb5+.+f5 ab5 .", "g5+.+f5 eb5+.+c5 . . .", "f5 . . ab5+.+c6 eb6", "c6 . bb5+.+ab5 f5+.+eb5 .",
          "c5 . eb5+.+f5 ab5 .", "b5+.+c6 ab5 . f5+.+eb5 c5", "db5 . f5+.+ab5 g5 .", "e5+.+g5 bb5 . c6 -"]
    B_CH = ["Bbm7", "Eb7", "Abmaj7", "Dbmaj7", "Gm7b5", "C7", "Fm7", "C7"]
    B = ["f5 . . . db5", "g5 . . . f5+.+eb5", "c5 . . eb5+.+g5 .", "f5 . . . -",
         "bb5 . . db6+.+bb5 .", "g5 . e5 . c5", "ab5 . . g5+.+f5 .", "e5 . . . -"]
    BRK_CH = ["Fm7", "Fm7", "Dbmaj7", "C7"]

    # intro: the bass ostinato alone, then the ride and the electric piano
    r = s.bars(4)
    bass(r, BRK_CH, "Fm7")
    ride(r + 30, 2)
    comp(r + 30, BRK_CH[2:])
    # A: the sax hook over the 2+3 vamp
    s.loop_here()
    r = s.bars(8)
    bass(r, A_CH, "Bbm7")
    comp(r, A_CH)
    ride(r, 8)
    s.line(r, SAX, "sax", A, 3, 22, VIB, per_bar=5)
    # B: a walking bass, long sax lines, the piano holds
    r = s.bars(8)
    bass(r, B_CH, "Fm7", walk=True)
    comp(r, B_CH, "held")
    ride(r, 8, 22)
    s.line(r, SAX, "sax", B, 3, 22, VIB, per_bar=5)
    # A': the hook with more notes
    r = s.bars(8)
    bass(r, A_CH, "Fm7")
    comp(r, A_CH)
    ride(r, 8)
    s.line(r, SAX, "sax", A2, 3, 22, VIB, per_bar=5)
    # the break: the electric piano alone over the ostinato
    r = s.bars(4)
    bass(r, BRK_CH, "Fm7")
    comp(r, BRK_CH, "break", 28)
    ride(r, 4, 20)
    return s.build("me espresso in five", 6, 126)     # speed 6 at 126 BPM: a beat 357 ms (crotchet 168)


TRACKS = [tarantella, five]


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
