#!/usr/bin/env python3
"""Spider Swing alternative music: two 4-channel ProTracker MODs written with the house music kit
(games/common/tools/house_music.py: the MOD writer and periods; docs/art-direction.md "Audio"), the instruments
synthesised here like Pogo Mamie's musette or Beaver Rush's banjo, at the house mix (sample volumes <= 40).

  alt1  "Tango de la Tarantula": a spooky-cute tango, 4/4 at 120, D minor with an F major middle. A violin with
        glissandi, a bandoneon playing marcato chords (then the 3-3-2 sincopa), a plucked double bass and a
        harpsichord. The hook: eight legs creeping down chromatically (A G# G F# F E D# D), then the violin
        swings up a whole octave on its silk (a glide). Intro, A, A' (the sincopa, the harpsichord doubling the
        legs), B (lyrical, a dark B flat minor turn), a fast 16th-note variacion, the "chan-chan" tango ending,
        then the loop back to A.
  alt2  "Trapeze Calliope": a swinging circus tune, 6/8 at a dotted crotchet of 96, B flat major. A wobbly steam
        calliope over oom-pah-pah pipes, a bass drum, snare and cymbal, a glockenspiel. The hook: the trapeze arc,
        a low note leaping up to the peak on the downbeat then falling back like a pendulum. Intro (a fanfare),
        A, A' (the drums drop out: mid-air, the glockenspiel rings), B (E flat with a daring minor turn and a snare
        roll), a drum-roll bridge climbing chromatically to a cymbal crash, then the loop back to A.

    make_music_alt.py              -> ../music-alt/alt1.mod, alt2.mod (previews; the game is unchanged)
    make_music_alt.py OUTDIR N     -> OUTDIR/tune.mod: alternative N (1 or 2), for tools/build_assets.py

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/spiderswing/LICENSE.
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


# ==== alt 1: Tango de la Tarantula ==================================================================================
def reed(ph):
    return (math.sin(2 * math.pi * ph) + .55 * math.sin(4 * math.pi * ph) + .45 * math.sin(6 * math.pi * ph)
            + .28 * math.sin(8 * math.pi * ph) + .16 * math.sin(10 * math.pi * ph) + .08 * math.sin(12 * math.pi * ph))


def tango():
    band = {q: chord_loop(32, v, reed) for q, v in (("m", [0, 3, 7, 12]), ("", [0, 4, 7, 12]), ("7", [0, 4, 7, 10]))}
    rng = random.Random(93)
    scratch = [rng.uniform(-1, 1) * (1 if (i // 90) % 2 == 0 else .25) * math.exp(-i / 900.0) for i in range(2000)]
    smp = [("violin", tone_loop(16, [(1, 1, [(1, 1), (2, .55), (3, .45), (4, .35), (5, .22), (6, .15), (7, .1)])],
                                320, .35, 94), 30, 320, 16, 60),
           ("bass", norm(pluck(64, 7000, [(1, 1), (2, .45), (3, .2), (4, .08)], 3000, .7, attack=6)), 38, 0, 0, 36),
           ("band_m", band["m"][0], 22, 0, band["m"][1], 48),
           ("band_M", band[""][0], 22, 0, band[""][1], 48),
           ("band_7", band["7"][0], 22, 0, band["7"][1], 48),
           ("harpsi", norm(pluck(16, 7000, [(1, 1), (2, .8), (3, .7), (4, .6), (5, .45), (6, .3), (7, .2)], 2600, .6,
                                 attack=1)), 26, 0, 0, 60),
           ("knock", noise_hit(HI, 80, 2, 95, 1, [(620, 1.2, 25), (1450, .5, 12)]), 26, 0, 0, 36),
           ("scratch", norm(scratch, 90), 14, 0, 0, 48)]
    s = Song(smp, rpb=16, bpp=2)                      # 4/4 in 16ths, patterns of 2 bars
    VLN, BASS, BAND, HARP = 0, 1, 2, 3
    band_of = {"m": "band_m", "": "band_M", "7": "band_7"}

    def bandoneon(r0, chords, style, vol=36):
        for b, name in enumerate(chords):
            pc, q, _ = chord(name)
            r, top, o = r0 + 16 * b, fit(pc, 50), band_of[q]
            if style == "marcato":                     # four short, heavy chords
                hits = ((0, vol), (4, vol - 8), (8, vol - 2), (12, vol - 8))
            elif style == "sincopa":                   # 3-3-2
                hits = ((0, vol), (6, vol - 4), (12, vol - 2))
            else:                                      # held, two a bar
                s.put(r, BAND, o, top, vol - 10)
                s.put(r + 8, BAND, o, top, vol - 14)
                continue
            for e, v in hits:
                s.put(r + e, BAND, o, top, v)
                s.off(r + e + 2, BAND)

    def bass(r0, chords, style, nxt=None):
        for b, name in enumerate(chords):
            pc, q, bpc = chord(name)
            r, root = r0 + 16 * b, fit(bpc, 38)
            following = chords[b + 1] if b + 1 < len(chords) else (nxt or name)
            if style == "marcato":
                s.put(r, BASS, "bass", root, 40)
                s.put(r + 8, BASS, "bass", fit(pc + 7, 38), 32)
                s.put(r + 14, BASS, "bass", fit(chord(following)[0] - 1, 38), 26)      # the drag into the next bar
            elif style == "sincopa":
                for e, k, v in ((0, 0, 40), (6, 0, 34), (12, 7, 34)):
                    s.put(r + e, BASS, "bass", fit(pc + k, 38) if e else root, v)
            else:                                      # walking crotchets
                third = 3 if q == "m" else 4
                npc = chord(following)[0]
                for e, m in ((0, root), (4, fit(pc + third, 38)), (8, fit(pc + 7, 38)), (12, fit(npc + 1, 38))):
                    s.put(r + e, BASS, "bass", m, 36 if e == 0 else 30)

    def knocks(r0, bars):
        for b in range(bars):
            s.put(r0 + 16 * b + 14, HARP, "knock", "c4", 22)
            if b % 2:
                s.put(r0 + 16 * b + 6, HARP, "scratch", "c4", 20)

    A_CH = ["Dm", "A7", "Dm", "A7", "Gm", "Dm", "E7", "A7"]
    A = ["a5 g#5 g5 f#5 f5 e5 d#5 d5 - . . . a4 . . .", "a5/ . . . . . . . g5 . f5 . e5 . c#5 .",
         "d5 . . . f5 . . . a5 . . . d6 . . .", "c#6 . . . . . . . - . . . e5 . f5 .",
         "g5 . . . a#5 . . . d6 . c6 a#5 a5 . g5 .", "f5 . . . a5 . . . d5 . . . . . . .",
         "e5 . g#5 . b5 . d6 . c6 . b5 . a5 . g#5 .", "a5 . . . . . . . c#5 . e5 . g5 . . -"]
    LEGS = "a4 g#4 g4 f#4 f4 e4 d#4 d4 . . . . . . . ."
    B_CH = ["F", "C7", "F", "D7", "Gm", "Bbm", "F", "A7"]
    B = ["c5 . f5 . a5 . . .", "g5 . . . a#5 . g5 .", "a5 . . . . . c6 .", "c6 . a5 . f#5 . d5 .",
         "d5 . g5 . a#5 . . .", "c#6 . . . a#5 . f5 .", "a5 . . . c6 . . a5", "g5 . . . e5 . c#5 -"]
    V_CH = ["Gm", "Dm", "E7", "A7", "Gm", "Dm", "E7", "A7"]
    VIB = (0x4, 0x34)

    # intro: the bandoneon alone, then the bass; the harpsichord previews the eight legs
    r = s.bars(2)
    bandoneon(r, ["Dm", "Dm"], "marcato")
    bass(r + 16, ["Dm"], "marcato", "Dm")
    s.line(r + 16, HARP, "harpsi", "a5 g#5 g5 f#5 f5 e5 d#5 d5 . . . . . . . .", 1, 26)
    # A: marcato, the legs and the glide
    s.loop_here()
    r = s.bars(8)
    bandoneon(r, A_CH, "marcato")
    bass(r, A_CH, "marcato", "Dm")
    knocks(r, 8)
    s.line(r, VLN, "violin", A, 1, 22, VIB, slide=0x0C, per_bar=16)
    # A': the 3-3-2 sincopa, the harpsichord doubles the legs an octave down, then broken chords
    r = s.bars(8)
    bandoneon(r, A_CH, "sincopa")
    bass(r, A_CH, "sincopa")
    s.line(r, VLN, "violin", A, 1, 22, VIB, slide=0x0C)
    broken = arps(A_CH[1:], 62, [0, None, 2, None, 4, None, 2, None, 1, None, 3, None, 5, None, 3, None])
    s.line(r, HARP, "harpsi", [LEGS] + broken, 1, 22, per_bar=16)
    # B: F major, lyrical, a walking bass, the harpsichord arpeggiates
    r = s.bars(8)
    bandoneon(r, B_CH, "held")
    bass(r, B_CH, "walk", "Gm")
    s.line(r, VLN, "violin", B, 2, 21, VIB, slide=0x0C, per_bar=8)
    s.line(r, HARP, "harpsi", arps(B_CH, 60, [0, 2, 4, 5, 4, 2, 3, 1]), 2, 20, per_bar=8)
    # the variacion: fast 16ths on the violin, marcato underneath, then the chan-chan ending
    r = s.bars(8)
    bandoneon(r, V_CH, "marcato")
    bass(r, V_CH, "sincopa")
    runs = arps(V_CH, 62, [0, 1, 2, 3, 4, 5, 6, 7, 6, 5, 4, 3, 2, 1, 2, 3])
    runs = [b if k % 2 == 0 else " ".join(b.split()[::-1]) for k, b in enumerate(runs)]
    s.line(r, VLN, "violin", runs[:7] + ["c#6 . . . . . . . - . . . d5 . - ."], 1, 19, per_bar=16)
    s.line(r, HARP, "harpsi", arps(V_CH[:7], 62, [None, None, 0, None, None, None, 2, None,
                                                  None, None, 1, None, None, None, 4, None]), 1, 20, per_bar=16)
    last = r + 7 * 16                                  # chan ... chan!
    for e in range(16):
        for ch in (BASS, BAND, HARP):
            s.rows[last + e][ch] = [0, 0, 0, 0]
    s.put(last, BASS, "bass", "a2", 42)
    s.put(last, BAND, "band_7", "a3", 32)
    s.off(last + 3, BAND)
    s.put(last + 12, BASS, "bass", "d2", 42)
    s.put(last + 12, BAND, "band_m", "d3", 32)
    s.off(last + 14, BAND)
    s.put(last + 12, HARP, "knock", "c4", 30)
    return s.build("ss tango tarantula", 6, 120)      # speed 6 at 120 BPM: a 16th 125 ms (crotchet 120)


# ==== alt 2: Trapeze Calliope =======================================================================================
def calliope():
    pipe = [(1, 1), (2, .3), (3, .12)]
    org = {q: chord_loop(32, v, lambda ph: math.sin(2 * math.pi * ph) + .3 * math.sin(4 * math.pi * ph)
                         + .1 * math.sin(6 * math.pi * ph))
           for q, v in (("", [0, 4, 7, 12]), ("m", [0, 3, 7, 12]), ("7", [0, 4, 7, 10]))}
    smp = [("calliope", tone_loop(1024, [(64, 1, pipe), (65, .35, pipe)], 256, .4, 101), 30, 256, 1024, 60),
           ("pipe_lo", tone_loop(64, [(1, 1, [(1, 1), (2, .25), (3, .1)])], 64), 34, 64, 64, 36),
           ("org_M", org[""][0], 20, 0, org[""][1], 48),
           ("org_m", org["m"][0], 20, 0, org["m"][1], 48),
           ("org_7", org["7"][0], 20, 0, org["7"][1], 48),
           ("glock", norm(pluck(16, 9000, [(1, 1), (2.76, .3), (5.4, .12)], 7000, 1.0, attack=2)), 26, 0, 0, 60),
           ("snare", noise_hit(HI, 180, 55, 91, 1, [(210, .7, 35)]), 26, 0, 0, 36),
           ("crash", noise_hit(HI, 1500, 420, 92, 2, [(3400, .3, 500), (4700, .25, 400), (5900, .2, 350)],
                               attack_ms=2), 26, 0, 0, 36),
           ("drum", sweep(LO, 90, 55, 40, 160, 400, .25, 9), 34, 0, 0, 48)]
    s = Song(smp, rpb=12, bpp=4)                      # 6/8: an eighth = 2 rows, a bar = 12 rows
    CAL, BASS, CHORD, DR = 0, 1, 2, 3
    org_of = {"": "org_M", "m": "org_m", "7": "org_7"}
    VIB = (0x4, 0x46)                                  # the steam wobble

    def oompah(r0, chords, pedal=False, vol=30):
        for b, name in enumerate(chords):
            pc, q, bpc = chord(name)
            r = r0 + 12 * b
            s.put(r, BASS, "pipe_lo", fit(bpc, 41), 30)
            if pedal:
                s.put(r + 2, CHORD, org_of[q], fit(pc, 53), vol - 8)
                s.off(r + 3, CHORD)
                continue
            s.off(r + 4, BASS)
            s.put(r + 6, BASS, "pipe_lo", fit(pc + 7, 41), 25)
            s.off(r + 10, BASS)
            for e in (2, 4, 8, 10):                     # pah-pah, pah-pah
                s.put(r + e, CHORD, org_of[q], fit(pc, 53), vol if e in (2, 8) else vol - 5)
                s.off(r + e + 1, CHORD)

    def drums(r0, bars, crash_first=True, roll_bar=None):
        for b in range(bars):
            r = r0 + 12 * b
            if b == roll_bar:
                for e in range(12):
                    s.put(r + e, DR, "snare", "c4", 12 + 2 * e)
                continue
            s.put(r, DR, "crash" if (crash_first and b == 0) else "drum", "c4", 30 if b == 0 and crash_first else 34)
            s.put(r + 4, DR, "snare", "c4", 16)
            s.put(r + 6, DR, "drum", "c4", 26)
            s.put(r + 10, DR, "snare", "c4", 20)

    def bells(r0, chords):
        for b, name in enumerate(chords):
            top = sorted(fit(p, 84) for p in tones(name))[::-1]
            s.put(r0 + 12 * b, DR, "glock", top[0], 26)
            s.put(r0 + 12 * b + 6, DR, "glock", top[1], 20)

    I_CH = ["F7", "F7", "Bb", "F7"]
    INTRO = ["f4 f4 f4 a#4 . .", "d5 d5 d5 f5 . .", "a#5 . . . . .", "a5 . . - . ."]
    A_CH = ["Bb", "Bb", "F7", "F7", "Gm", "Eb", "F7", "Bb"]
    A = ["f4 . a#4 d5 . f5", "a#5 . . f5 d5 a#4", "a4 . c5 d#5 . f5", "a5 . . f5 d#5 c5",
         "d5 . g5 a#5 . d6", "c6 . . g5 d#5 a#4", "c5 d5 d#5 f5 g5 a5", "a#5 . . . - ."]
    B_CH = ["Eb", "Ebm", "Bb", "G7", "Cm", "F7", "Bb", "F7"]
    B = ["d#5 . g5 a#5 . .", "a#5 . . f#5 . d#5", "d5 . . f5 . a#5", "b5 . . . g5 f5",
         "d#5 . c5 g4 . c5", "d#5 . . c5 . a4", "a#4 . d5 f5 . a#5", "a5 g5 f5 d#5 c5 a4"]
    BR_CH = ["F", "F", "F", "F", "Gm", "C7", "F", "F7"]
    BR = ["c5 . . . . .", "c#5 . . . . .", "d5 . . . . .", "d#5 . . . . -",
          "d5 . . g5 . a#5", "e5 . . g5 . a#5", "c6 . . a5 f5 c5", "a4 . c5 d#5 f5 a5"]

    # intro: a fanfare (ta-ta-ta-taa), the cymbal, a roll into A
    r = s.bars(4)
    for b in (0, 1):
        for e in (0, 2, 4, 6):
            s.put(r + 12 * b + e, CHORD, "org_7", "f3", 26)
            s.off(r + 12 * b + e + 1, CHORD)
        s.put(r + 12 * b, BASS, "pipe_lo", "f2", 30)
        s.off(r + 12 * b + 8, BASS)
        s.put(r + 12 * b + 6, DR, "drum", "c4", 32)
    oompah(r + 24, I_CH[2:])
    drums(r + 24, 2, roll_bar=1)
    s.line(r, CAL, "calliope", INTRO, 2, 21, VIB, per_bar=6)
    # A: the trapeze arc
    s.loop_here()
    r = s.bars(8)
    oompah(r, A_CH)
    drums(r, 8)
    s.line(r, CAL, "calliope", A, 2, 20, VIB, per_bar=6)
    # A': mid-air: the drums drop out, the glockenspiel rings
    r = s.bars(8)
    oompah(r, A_CH, vol=26)
    bells(r, A_CH)
    s.line(r, CAL, "calliope", A, 2, 20, VIB)
    # B: E flat, the daring turn to E flat minor with a snare roll, the cymbal
    r = s.bars(8)
    oompah(r, B_CH)
    drums(r, 2, roll_bar=1)
    drums(r + 24, 6)
    s.line(r, CAL, "calliope", B, 2, 20, VIB, per_bar=6)
    # bridge: the drum roll over an F pedal, the calliope climbs; the crash, then the leaps back to A
    r = s.bars(8)
    oompah(r, BR_CH[:4], pedal=True)
    oompah(r + 48, BR_CH[4:])
    for e in range(48):
        s.put(r + e, DR, "snare", "c4", 8 + e * 30 // 48)
    drums(r + 48, 4)
    s.line(r, CAL, "calliope", BR, 2, 19, VIB, per_bar=6)
    return s.build("ss trapeze calliope", 6, 144)     # speed 6 at 144 BPM: an eighth 208 ms (dotted crotchet 96)


TRACKS = [tango, calliope]


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
