#!/usr/bin/env python3
"""Biscuit Bandit alternative music: two 4-channel ProTracker MODs written with the house music kit
(games/common/tools/house_music.py: the MOD writer and periods; docs/art-direction.md "Audio"), the instruments
synthesised here like Mouse Espresso's or Spider Swing's alternatives, at the house mix (sample volumes <= 40).

  alt1  "Crumb Caper": a sneaky 60s spy-lounge for a raccoon in a bakery, swung 4/4 at a crotchet of 116, E minor
        with a G major middle. A vibraphone (its motor on) over a creeping upright bass, bongos and a combo organ;
        a muted trumpet steals in as the countermelody. The hook: a tiptoed climb (B-E, G-B, A) and a sly slide
        back to E. Intro (the bass creeps in alone), A, A' (the trumpet answers), B (G major, a walking bass, long
        vibraphone lines), a stop-time bridge creeping up by half steps (E, F, F sharp minor), A'' (the trumpet in
        harmony), then the loop back to A.
  alt2  "Conveyor Funk": a 70s funk strut, straight 16ths at a crotchet of 104, B flat (mixolydian) with a G minor
        middle. A clavinet ostinato that never stops, like the belt, a slap bass, a drum kit and a horn section.
        The hook: the horns' snatch-and-grab (F-A flat-B FLAT!, then A flat-B flat, F). Intro (the clavinet and the
        hi-hat, then the kit and the bass), A, A' (the section answers in chords, the bass gets busier), B (G minor,
        a lyrical trumpet), a breakdown (slap bass and drums), then the loop back to A.

    make_music_alt.py              -> ../music-alt/alt1.mod, alt2.mod (previews; the game is unchanged)
    make_music_alt.py OUTDIR N     -> OUTDIR/tune.mod: alternative N (1 or 2), for tools/build_assets.py

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/biscuitbandit/LICENSE.
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


QUAL.update({"7b9": [0, 4, 7, 10, 13], "9": [0, 4, 7, 10, 14]})


# ==== alt 1: Crumb Caper ============================================================================================
def vibraphone():
    """A vibraphone bar: the fundamental and the tuned fourth partial, a soft mallet tick, a long ring (16 samples a
    cycle: C-1 = C4). The motor is the tracker's tremolo on the held rows."""
    v = pluck(16, 14000, [(1, 1), (4, .28)], 7000, 1.0, attack=2)
    rng = random.Random(7)
    for i in range(24):
        v[i] += rng.uniform(-.25, .25) * (1 - i / 24.0)
    return norm(v, 105)


def upright(n=12000, tau=5000):
    """A plucked upright bass, 128 samples a cycle (C-1 = C1)."""
    return norm(pluck(128, n, [(1, 1), (2, .5), (3, .25), (4, .12), (5, .06)], tau, .7, attack=12))


def organ(ph):
    """A combo organ: the 8', 4', 2 2/3' and 2' drawbars."""
    return (math.sin(2 * math.pi * ph) + .5 * math.sin(4 * math.pi * ph) + .3 * math.sin(6 * math.pi * ph)
            + .22 * math.sin(8 * math.pi * ph))


def muted_trumpet():
    """A harmon-muted trumpet: a weak fundamental, a nasal middle, a breathy attack, then a 32-sample loop
    (C-1 = C3)."""
    rng = random.Random(21)
    h = [(1, .3), (2, .65), (3, 1), (4, .9), (5, .7), (6, .5), (7, .36), (8, .24), (9, .14)]
    wave = [sum(a * math.sin(2 * math.pi * k * i / 32 + .4 * k) for k, a in h) for i in range(32)]
    att = [wave[i % 32] * min(1.0, i / 160.0) + rng.uniform(-.3, .3) * math.exp(-i / 50.0) for i in range(224)]
    return norm(att + wave, 100)


def bongo(f0, f1, decay, seed):
    """A bongo head: a quick pitch drop and a slap of noise."""
    return sweep(HI, f0, f1, 6, decay, 4 * decay, click=.5, seed=seed)


def caper():
    voicing = {"m7": [0, 3, 7, 10], "maj7": [0, 4, 7, 11], "7": [0, 4, 7, 10], "7b9": [0, 4, 10, 13],
               "m7b5": [0, 3, 6, 10], "m": [0, 3, 7, 12]}
    smp = [("vibes", vibraphone(), 36, 0, 0, 60),
           ("bass", upright(), 38, 0, 0, 24),
           ("bongo_hi", bongo(760, 560, 45, 1), 26, 0, 0, 36),
           ("bongo_lo", bongo(500, 360, 70, 2), 28, 0, 0, 36),
           ("slap", noise_hit(HI, 100, 14, 3, 1, [(620, 1.0, 25), (1240, .4, 12)]), 22, 0, 0, 36),
           ("trumpet", muted_trumpet(), 26, 224, 32, 48)]
    for q, v in voicing.items():
        data, L = chord_loop(32, v, organ)
        smp.append(("org_" + q, data, 20, 0, L, 48))
    s = Song(smp, rpb=12, bpp=4)                      # swung 4/4 on a triplet grid: a beat = 3 rows
    VIB, BASS, BONG, ORG = 0, 1, 2, 3
    MOTOR, DEEP, TVIB = (0x7, 0x82), (0x7, 0x83), (0x4, 0x23)

    def bongos(r0, bars, style="groove", fill_last=False):
        pats = {"groove": ((0, "bongo_hi", 23), (2, "bongo_hi", 17), (3, "bongo_lo", 27), (5, "bongo_hi", 19),
                           (6, "bongo_hi", 23), (8, "slap", 21), (9, "bongo_lo", 29), (11, "bongo_hi", 18)),
                "sparse": ((3, "bongo_lo", 20), (8, "bongo_hi", 12), (9, "bongo_lo", 22), (11, "bongo_hi", 13)),
                "stop": ((0, "bongo_lo", 28), (5, "bongo_hi", 14), (6, "bongo_lo", 20), (11, "bongo_hi", 16))}
        for b in range(bars):
            r = r0 + 12 * b
            if fill_last and b == bars - 1:          # a triplet roll into the next section
                for e in range(12):
                    s.put(r + e, BONG, "bongo_hi" if e % 3 else "bongo_lo", "c4", 12 + e)
                continue
            for e, inst, v in pats[style]:
                s.put(r + e, BONG, inst, "c4", v)

    def organ_comp(r0, chords, style="stabs", vol=20):
        for b, name in enumerate(chords):
            pc, q, _ = chord(name)
            r, o, top = r0 + 12 * b, "org_" + q, fit(pc, 52)
            if style == "stabs":                      # short chords on the swung "and" of 2 and of 4
                for e in (5, 11):
                    s.put(r + e, ORG, o, top, vol)
                    s.off(r + e + 2, ORG)
            elif style == "held":                     # let ring, struck again on 3
                s.put(r, ORG, o, top, vol - 4)
                s.put(r + 6, ORG, o, top, vol - 8)
            else:                                     # stop-time: one hit, let ring to the third beat
                s.put(r, ORG, o, top, vol + 2)
                s.off(r + 7, ORG)

    def walk(r0, chords, nxt):
        """Crotchets: root, third, fifth and a half step into the next root."""
        for b, name in enumerate(chords):
            pc, q, bpc = chord(name)
            root = fit(bpc, 36)
            third = 3 if q in ("m7", "m7b5", "m") else 4
            fifth = 6 if q == "m7b5" else 7
            nroot = fit(chord(chords[b + 1] if b + 1 < len(chords) else nxt)[2], 36)
            app = nroot + 1 if nroot < root + fifth else nroot - 1
            for e, m in enumerate((root, root + third, root + fifth, app)):
                s.put(r0 + 12 * b + 3 * e, BASS, "bass", m, 36 if e == 0 else 30)

    A_CH = ["Em7", "Em7", "Am7", "Am7", "Cmaj7", "B7b9", "Em7", "B7"]
    BASS_A = ["e2 .+.+g2 a2 .+.+a#2", "b2 .+.+d3 b2+.+a2 g2+.+g#2", "a2 .+.+c3 d3 .+.+d#3", "e3 .+.+g3 e3+.+d3 c3+.+b2",
              "c3 .+.+e3 f#3 .+.+g3", "b2 .+.+a2 f#2 .+.+d#2", "e2 .+.+g2 a2 .+.+a#2", "b2 .+.+f#2 a2+.+g2 f#2+.+d#2"]
    VIB_A = ["- b4+.+e5 g5+.+b5 a5", "g5 . e5+.+d5 e5", "- c5+.+e5 a5+.+c6 b5", "a5 . g5+.+e5 -",
             "g5+.+f#5 g5 b5 .+.+e6", "d#6 . c6+.+b5 a5", "g5 . f#5+.+e5 b4", "d#5 . f#5+.+a5 -"]
    VIB_A2 = VIB_A[:6] + ["g5 . f#5+.+e5 d#5+.+e5", "f#5 . b4 -"]
    TPT_A = [". . . e4", "f#4 g4 . .", ". . . c5", "b4 a4 . -", ". e4 . g4", "f#4 . . a4", "g4 . . e4", "d#4 . . -"]
    B_CH = ["Cmaj7", "Bm7", "Am7", "D7", "Gmaj7", "Cmaj7", "F#m7b5", "B7"]
    VIB_B = ["e5 . . g5", "f#5 . . d5", "c6 . b5 a5", "a5 . f#5 d5",
             "b5 . . d6", "e6 . d6+.+b5 g5", "a5 . . c6", "b5 . a5+.+f#5 d#5"]
    BR_CH = ["Em", "Fm", "F#m", "B7b9"]
    BR_VIB = ["b4+.+e5 . g5+.+b5 .", "c5+.+f5 . g#5+.+c6 .", "c#5+.+f#5 . a5+.+c#6 .", "d#5 f#5 a5 c6"]
    BR_BASS = ["e2 . . b1", "f2 . . c2", "f#2 . . c#2", "b1 d#2 f#2 a2"]
    END_CH = ["Em7", "Em7", "Am7", "B7"]
    END_VIB = VIB_A[:3] + ["d#5 . f#5+.+b5 a5+.+-"]

    # intro: the bass creeps in alone, then the bongos, the organ, and the vibraphone sneaks up
    r = s.bars(4)
    s.line(r, BASS, "bass", [BASS_A[0], "b2 .+.+d3 b2+.+a2 g2+.+d#2", BASS_A[0], BASS_A[7]], 3, 34, per_bar=4)
    bongos(r + 12, 1, "sparse")
    bongos(r + 24, 2, fill_last=True)
    organ_comp(r + 24, ["Em7", "B7b9"])
    s.line(r + 36, VIB, "vibes", "- d#5+.+f#5 a5+.+f#5 d#5", 3, 28)
    # A: the hook over the creeping bass
    s.loop_here()
    r = s.bars(8)
    s.line(r, BASS, "bass", BASS_A, 3, 34, per_bar=4)
    bongos(r, 8)
    organ_comp(r, A_CH)
    s.line(r, VIB, "vibes", VIB_A, 3, 32, MOTOR, per_bar=4)
    # A': the muted trumpet steals in underneath
    r = s.bars(8)
    s.line(r, BASS, "bass", BASS_A, 3, 34)
    bongos(r, 8, fill_last=True)
    s.line(r, VIB, "vibes", VIB_A2, 3, 32, MOTOR, per_bar=4)
    s.line(r, ORG, "trumpet", TPT_A, 3, 24, TVIB, per_bar=4)
    # B: G major, a walking bass, long vibraphone lines with the motor deep, the organ held
    r = s.bars(8)
    walk(r, B_CH, "Em")
    bongos(r, 8, "sparse")
    organ_comp(r, B_CH, "held")
    s.line(r, VIB, "vibes", VIB_B, 3, 34, DEEP, per_bar=4)
    # bridge: stop-time, creeping up by half steps
    r = s.bars(4)
    s.line(r, BASS, "bass", BR_BASS, 3, 36, per_bar=4)
    organ_comp(r, BR_CH, "hit")
    bongos(r, 4, "stop", fill_last=True)
    s.line(r, VIB, "vibes", BR_VIB, 3, 34, per_bar=4)
    # A'': the hook again, the trumpet in harmony
    r = s.bars(4)
    s.line(r, BASS, "bass", BASS_A[:3] + [BASS_A[7]], 3, 34)
    bongos(r, 4)
    s.line(r, VIB, "vibes", END_VIB, 3, 32, MOTOR, per_bar=4)
    s.line(r, ORG, "trumpet", harmony(END_VIB, END_CH), 3, 22, TVIB)
    return s.build("bb crumb caper", 6, 87)           # speed 6 at 87 BPM: a beat (3 rows) 517 ms (crotchet 116)


# ==== alt 2: Conveyor Funk ==========================================================================================
def clavinet(n=6000, tau=2400):
    """A clavinet: a hammered string picked up near its end (a comb of bright partials) and a quick decay
    (32 samples a cycle: C-1 = C3)."""
    partials = [(k, abs(math.sin(math.pi * k * .16)) / k ** .35) for k in range(1, 12)]
    return norm(pluck(32, n, partials, tau, .5, attack=1))


def brass(P, harm, attack, seed):
    """A brass tone: harmonics over P samples, a quick lip "blat" (a swell and a puff of noise), then the loop."""
    rng = random.Random(seed)
    wave = [sum(a * math.sin(2 * math.pi * k * i / P) for k, a in harm) for i in range(P)]
    att = [wave[i % P] * (i / float(attack)) ** 1.5 + rng.uniform(-.2, .2) * math.exp(-i / 40.0) for i in range(attack)]
    return norm(att + wave, 100)


TPT_H = [(1, 1), (2, .9), (3, .75), (4, .6), (5, .45), (6, .3)]
SECTION_H = [(1, 1), (2, .8), (3, .6), (4, .45), (5, .3), (6, .2)]
DEG = {"1": 0, "2": 2, "4": 5, "6": 9, "8": 12, "9": 14, "11": 17, "-5": -5}


def degree(q, d):
    """A chord degree -> semitones above the root, for the chord's quality ('b10' is the blue note)."""
    minor = q.startswith("m") and q != "maj7"
    third = 3 if minor else 4
    return {"b3": 2 if minor else 3, "3": third, "5": 6 if q == "m7b5" else 7, "7": 11 if q == "maj7" else 10,
            "b10": 14 if minor else 15, "10": 12 + third, "-7": -1 if q == "maj7" else -2}.get(d, DEG.get(d))


def funk():
    smp = [("clav", clavinet(), 34, 0, 0, 48),
           ("clav_mute", clavinet(700, 140), 26, 0, 0, 48),
           ("bass", norm(pluck(128, 9000, [(1, 1), (2, .7), (3, .5), (4, .35), (5, .25), (6, .18), (7, .12), (8, .08)],
                               4500, .8, attack=2)), 38, 0, 0, 24),
           ("pop", norm(pluck(128, 7000, [(k, 1 / k ** .5) for k in range(1, 15)], 2400, .9, attack=1)), 34, 0, 0, 24),
           ("kick", sweep(HI, 160, 52, 14, 110, 300, click=.35, seed=4), 38, 0, 0, 36),
           ("snare", noise_hit(HI, 200, 50, 41, 1, [(185, 1.0, 45), (320, .45, 25)]), 34, 0, 0, 36),
           ("hat", noise_hit(HI, 60, 9, 42, 2), 20, 0, 0, 36),
           ("ohat", noise_hit(HI, 260, 75, 43, 2), 20, 0, 0, 36),
           ("trumpet", brass(16, TPT_H, 128, 1), 30, 128, 16, 60)]

    def section(ph):
        return sum(a * math.sin(2 * math.pi * k * ph) for k, a in SECTION_H)
    for q, v in (("9", [0, 4, 10, 14]), ("7", [0, 4, 7, 10]), ("m7", [0, 3, 7, 10]), ("maj7", [0, 4, 7, 11])):
        data, L = chord_loop(32, v, section)
        smp.append(("horns_" + q, data, 26, 0, L, 48))
    s = Song(smp, rpb=16, bpp=2)                      # 4/4 in 16ths, patterns of 2 bars
    CLAV, BASS, DR, HORN = 0, 1, 2, 3
    VIB = (0x4, 0x23)

    KIT = {"groove": ((0, "kick", 40), (1, "hat", 10), (2, "hat", 18), (3, "hat", 10), (4, "snare", 34), (5, "hat", 10),
                      (6, "kick", 30), (7, "hat", 14), (8, "hat", 20), (9, "kick", 30), (10, "hat", 16),
                      (11, "snare", 12), (12, "snare", 34), (13, "hat", 10), (14, "hat", 18), (15, "ohat", 18)),
           "busy": ((0, "kick", 40), (1, "hat", 10), (2, "hat", 18), (3, "snare", 12), (4, "snare", 34), (5, "hat", 10),
                    (6, "kick", 30), (7, "snare", 12), (8, "hat", 20), (9, "kick", 30), (10, "ohat", 18),
                    (11, "snare", 12), (12, "snare", 34), (13, "kick", 26), (14, "hat", 18), (15, "snare", 14)),
           "hats": tuple((e, "hat", 18 if e % 4 == 2 else 11) for e in range(16)),
           "break": ((0, "kick", 38), (3, "kick", 30), (4, "snare", 34), (6, "hat", 14), (7, "snare", 14),
                     (8, "hat", 18), (10, "kick", 32), (11, "hat", 12), (12, "snare", 36), (13, "snare", 12),
                     (14, "kick", 28), (15, "snare", 20))}

    def drums(r0, bars, style="groove", fill_last=True):
        for b in range(bars):
            r = r0 + 16 * b
            for e, inst, v in KIT[style]:
                if fill_last and b == bars - 1 and e >= 12:
                    s.put(r + e, DR, "snare", "c4", 22 + 4 * (e - 12))      # a snare roll into the next section
                else:
                    s.put(r + e, DR, inst, "c4", v)

    def riff(r0, chords, pat, ch, lo, inst, mute, gain=0):
        """A one-bar riff on chord degrees: (step, degree, volume[, sample]); 'x' mutes the last note, 'x8' a muted
        octave, '-' stops."""
        for b, name in enumerate(chords):
            pc, q, bpc = chord(name)
            root, last = fit(bpc, lo), None
            for step in pat:
                e, d = step[0], step[1]
                r = r0 + 16 * b + e
                if d == "-":
                    s.off(r, ch)
                    continue
                if d.startswith("x"):
                    s.put(r, ch, mute, last if d == "x" else root + degree(q, d[1:]), step[2] + gain)
                    continue
                last = root + degree(q, d)
                s.put(r, ch, step[3] if len(step) > 3 else inst, last, step[2] + gain)

    def stabs(r0, chords, hits=((0, 34), (3, 28), (6, 32), (10, 34))):
        for b, name in enumerate(chords):
            pc, q, _ = chord(name)
            for e, v in hits:
                s.put(r0 + 16 * b + e, HORN, "horns_" + q, fit(pc, 53), v)
                s.off(r0 + 16 * b + e + 2, HORN)

    CLAV_A = ((0, "8", 30), (1, "x", 16), (2, "7", 22), (3, "8", 24), (4, "x", 18), (5, "5", 24), (6, "7", 22),
              (7, "x", 16), (8, "8", 30), (10, "b10", 24), (11, "10", 28), (12, "x", 18), (13, "7", 24), (14, "8", 26),
              (15, "x", 16))
    CLAV_B = ((0, "5", 24), (1, "x", 14), (2, "7", 20), (3, "x", 14), (4, "8", 24), (5, "x", 14), (6, "7", 20),
              (7, "x", 14), (8, "5", 24), (9, "x", 14), (10, "10", 20), (11, "x", 14), (12, "9", 22), (13, "8", 20),
              (14, "7", 22), (15, "x", 14))
    CLAV_SCRATCH = tuple((e, "x8", 20 if e % 4 == 3 else 12) for e in range(1, 16, 2))
    BASS_A = ((0, "1", 40), (2, "-", 0), (3, "1", 30), (5, "b3", 24), (6, "3", 32), (7, "-", 0), (8, "5", 34),
              (10, "7", 32, "pop"), (11, "-", 0), (12, "8", 36, "pop"), (13, "-", 0), (14, "5", 30), (15, "-7", 28))
    BASS_A2 = ((0, "1", 40), (1, "8", 24, "pop"), (2, "-", 0), (3, "1", 30), (4, "8", 30, "pop"), (5, "b3", 24),
               (6, "3", 32), (8, "5", 34), (9, "6", 26), (10, "7", 32, "pop"), (11, "-", 0), (12, "8", 36, "pop"),
               (13, "7", 26, "pop"), (14, "5", 30), (15, "-7", 28))
    BASS_B = ((0, "1", 38), (3, "5", 30), (4, "8", 30, "pop"), (6, "7", 28, "pop"), (7, "-", 0), (8, "5", 32),
              (10, "1", 30), (11, "-", 0), (12, "3", 30), (14, "5", 28), (15, "-", 0))
    BASS_BRK = ((0, "1", 40), (1, "1", 22), (2, "8", 34, "pop"), (3, "-", 0), (4, "1", 30), (5, "b3", 28), (6, "3", 32),
                (7, "5", 30), (8, "-7", 34), (9, "1", 34), (10, "8", 36, "pop"), (11, "-", 0), (12, "b10", 34, "pop"),
                (13, "10", 34, "pop"), (14, "8", 30, "pop"), (15, "-", 0))

    A_CH = ["Bb7", "Bb7", "Bb7", "Bb7", "Eb9", "Eb9", "Bb7", "F7"]
    HOOK = ["- . . . - . f5+ab5 bb5!", "- ab5+bb5 - f5 - . . .", "- . . . - . f5+ab5 bb5!",
            "- db6+c6 - bb5 ab5+f5 - . .", "g5! . . - g5 - bb5 .",
            "db6 . c6 bb5 - g5 - .", "f5 - ab5 - bb5! . . .", "a5! - . . c6 - eb6 -"]
    HOOK2 = ["- . . . - . f5+ab5 bb5!", "- ab5+bb5 - f5 - db5+d5 - f5", "- . . . - . f5+ab5 bb5!",
             "- db6+c6 - bb5 ab5+f5 - db5+d5 bb4"]
    B_CH = ["Gm7", "C7", "Gm7", "C7", "Ebmaj7", "D7", "Gm7", "F7"]
    TPT_B = ["d5 . . . f5 . g5 .", "bb5 . a5 . g5 . e5 .", "f5 . g5 . bb5 . d6 .", "c6 . . . bb5 . g5 .",
             "d6 . . . bb5 . g5 .", "f#5 . a5 . c6 . a5 .", "g5 . . . d5 . f5 g5", "a5 . c6 . eb6! . - ."]

    # intro: the clavinet and the hi-hat (the belt starts), then the kit and the bass
    r = s.bars(2)
    riff(r, ["Bb7", "Bb7"], CLAV_A, CLAV, 53, "clav", "clav_mute", 10)
    drums(r, 1, "hats", False)
    drums(r + 16, 1)
    riff(r + 16, ["Bb7"], BASS_A, BASS, 29, "bass", "bass")
    # A: the horns' snatch-and-grab
    s.loop_here()
    r = s.bars(8)
    riff(r, A_CH, CLAV_A, CLAV, 53, "clav", "clav_mute", 10)
    riff(r, A_CH, BASS_A, BASS, 29, "bass", "bass")
    drums(r, 8)
    s.line(r, HORN, "trumpet", HOOK, 2, 34, per_bar=8)
    # A': the trumpet varies the hook, then the section answers in chords; the bass gets busier
    r = s.bars(8)
    riff(r, A_CH[:4], CLAV_A, CLAV, 53, "clav", "clav_mute", 10)
    riff(r + 64, A_CH[4:], CLAV_B, CLAV, 53, "clav", "clav_mute", 10)
    riff(r, A_CH, BASS_A2, BASS, 29, "bass", "bass")
    drums(r, 8, "busy")
    s.line(r, HORN, "trumpet", HOOK2, 2, 34, per_bar=8)
    stabs(r + 64, A_CH[4:7])
    stabs(r + 112, A_CH[7:], ((0, 36), (6, 30), (10, 36)))
    # B: G minor, a lyrical trumpet, the clavinet steady
    r = s.bars(8)
    riff(r, B_CH, CLAV_B, CLAV, 53, "clav", "clav_mute", 10)
    riff(r, B_CH, BASS_B, BASS, 29, "bass", "bass")
    drums(r, 8)
    s.line(r, HORN, "trumpet", TPT_B, 2, 33, VIB, per_bar=8)
    # the breakdown: slap bass and drums, the clavinet scratching; the horns' pickup
    r = s.bars(4)
    riff(r, ["Bb7"] * 4, CLAV_SCRATCH, CLAV, 53, "clav", "clav_mute", 10)
    riff(r, ["Bb7"] * 4, BASS_BRK, BASS, 29, "bass", "bass")
    drums(r, 4, "break")
    s.line(r + 48, HORN, "trumpet", "- . . . - . db5+d5 f5", 2, 32)
    return s.build("bb conveyor funk", 6, 104)        # speed 6 at 104 BPM: a 16th 144 ms (crotchet 104)


TRACKS = [caper, funk]


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
