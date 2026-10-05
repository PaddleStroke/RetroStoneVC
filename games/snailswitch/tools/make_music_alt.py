#!/usr/bin/env python3
"""Snail Switch alternative music: two 4-channel ProTracker MODs written with the house music kit
(games/common/tools/house_music.py: the MOD writer and periods; docs/art-direction.md "Audio"), the instruments
synthesised here, at the house mix (sample volumes <= 40).

  alt1  "Invention on Both Sides": a baroque two-part invention for harpsichord, 4/4, a crotchet of 84, F major
        with a D minor middle (written in D major below and played a minor third up). The hook: a subject that
        curls up the vine (a leap, a turn, a leap) and is answered upside down in the other hand, mirrored like the
        snail flipping to the underside. Intro (the subject alone, then its mirror), A (subject and mirror, a
        sequence down the vine, the subject on the dominant), B (D minor: the subject and its mirror at the same
        time, then a recorder joins), A' (the reprise with an alto voice and the recorder), a coda over a dominant
        pedal with a trill and a ritardando, then the loop back to A.
  alt2  "Dub on the Underside": a gentle roots dub in 4/4, a crotchet of 72, G minor (dorian), a one-drop beat.
        A melodica over a deep round bass, an offbeat guitar skank and the echoes of a tape delay (dotted
        eighths), a spring-reverb snare. The hook: a melodica phrase that climbs a fourth and curls down, then is
        played upside down over the C chord (the snail on the underside). Intro (the echoing skank, then the
        drums and the bass), A, B (the dub: the skank drops out, the melodica echoes, the bass leaves holes, then
        everything comes back), a dub-out (single echoed notes, a stop, a rim roll), then the loop back to A.

    make_music_alt.py              -> ../music-alt/alt1.mod, alt2.mod (previews; the game is unchanged)
    make_music_alt.py OUTDIR N     -> OUTDIR/tune.mod: alternative N (1 or 2), for tools/build_assets.py

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/snailswitch/LICENSE.
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


def midi(name):
    """A note name ('c4' = middle C = 60, 'f#5', 'bb3') -> a MIDI number; numbers pass through."""
    if isinstance(name, int):
        return name
    pc, i = NOTE_PC[name[0]], 1
    while name[i] in "#b":
        pc += 1 if name[i] == "#" else -1
        i += 1
    return pc + 12 * (int(name[i:]) + 1)


def chord(name):
    """'F#m7' -> (root pitch class, 'm7')."""
    pc, i = NOTE_PC[name[0].lower()], 1
    while i < len(name) and name[i] in "#b":
        pc += 1 if name[i] == "#" else -1
        i += 1
    return pc % 12, name[i:]


def fit(pc, lo):
    """The pitch of class pc in [lo, lo + 11]."""
    return lo + (pc - lo) % 12


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

    def line(self, r, ch, inst, bars, step, vol=36, hold_fx=None, per_bar=None, shift=0):
        """A melody, one token per `step` rows: 'c5' a note ('!' louder, ',' softer), '.' holds, '-' stops.
        hold_fx (effect, parameter) goes on the held rows (a vibrato); shift transposes (semitones)."""
        if isinstance(bars, str):
            bars = [bars]
        for b in bars:
            if per_bar and len(b.split()) != per_bar:
                raise ValueError("bar %r: %d tokens, not %d" % (b, len(b.split()), per_bar))
        held = False
        for tok in " ".join(bars).split():
            core = tok.rstrip("!,")
            if core == "-":
                self.off(r, ch)
                held = False
            elif core != ".":
                self.put(r, ch, inst, midi(core) + shift, vol + (5 if "!" in tok else 0) - (8 if "," in tok else 0))
                held = True
            for j in range(r + (core != "."), r + step):
                if held and hold_fx and self.free(j, ch):
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


def pluck(P, n, partials, tau, power=0.7, attack=3, detune=0.0, start=0, seed=None):
    """An additive plucked string, P samples a cycle: partial k (ratio, amplitude) decays in tau / k**power
    samples; a second string `detune` cents sharp (a second choir of strings); seed: random partial phases."""
    out = [0.0] * n
    det = 2 ** (detune / 1200.0)
    rng = random.Random(seed)
    for k, a in partials:
        t = tau / k ** power
        p0 = rng.uniform(0, 2 * math.pi) if seed is not None else 0.0
        for i in range(start, n):
            j = i - start
            e = math.exp(-j / t)
            if e < 1e-4:
                break
            v = math.sin(2 * math.pi * k * j / P + p0)
            if detune:
                v = 0.6 * v + 0.6 * math.sin(2 * math.pi * k * det * j / P + 0.9 + p0)
            out[i] += a * e * v * min(1.0, j / attack)
    return out


def noise_hit(sr, ms, decay_ms, seed, hp=1, tone=None, attack_ms=0.5, tail=None):
    """Percussion at sr Hz: noise (high-passed `hp` times), optional tones (Hz, amplitude, decay ms) and an
    optional reverb tail (amplitude, decay ms) of low-passed noise."""
    rng = random.Random(seed)
    n = int(sr * ms / 1000)
    out, prev, lp = [], [0.0] * 3, 0.0
    for i in range(n):
        x = rng.uniform(-1, 1)
        for k in range(hp):
            x, prev[k] = x - prev[k], x
        t = i / sr * 1000
        v = x * math.exp(-t / decay_ms)
        for (f, a, d) in tone or []:
            v += a * math.sin(2 * math.pi * f * i / sr) * math.exp(-t / d)
        if tail:
            lp += 0.3 * (rng.uniform(-1, 1) - lp)
            v += tail[0] * lp * (1 - math.exp(-t / 25)) * math.exp(-t / tail[1])
        out.append(v * min(1.0, t / attack_ms))
    return norm(out, 105)


HI, LO = 16574, 8287          # the percussion rates: played at c4 with "the MIDI at C-1" 36 (HI) or 48 (LO)


# ==== alt 1: Invention on Both Sides ================================================================================
def harpsichord(P, n, tau):
    """A harpsichord: a quill plucks the string near its end (a bright, even comb of partials), two 8-foot choirs
    a few cents apart, a long ring that loses its brilliance first."""
    parts = [(k, (.45 + abs(math.sin(math.pi * k * .085))) / k ** .55) for k in range(1, P // 2)]
    return norm(pluck(P, n, parts, tau, .45, attack=1, detune=4, seed=P), 110)


def recorder():
    """An alto recorder: a pure, hollow tone (a little octave and twelfth), a breathy chiff at the start, then a
    64-sample loop (16 samples a cycle: C-1 = C4)."""
    rng = random.Random(17)
    wave = [math.sin(2 * math.pi * i / 16) + .14 * math.sin(4 * math.pi * i / 16 + .3)
            + .07 * math.sin(6 * math.pi * i / 16 + .6) for i in range(64)]
    att = [wave[i % 64] * min(1.0, i / 180.0) + rng.uniform(-.35, .35) * math.exp(-i / 70.0) for i in range(384)]
    return norm(att + wave, 100)


def invention():
    smp = [("harpsi", harpsichord(16, 12000, 6000), 40, 0, 0, 60),
           ("harpsi_lo", harpsichord(64, 16000, 8000), 40, 0, 0, 36),
           ("recorder", recorder(), 20, 384, 64, 60)]
    s = Song(smp, rpb=16, bpp=2)                      # 4/4 in sixteenths
    RH, LH, ALT, REC = 0, 1, 2, 3
    K = 3                                              # written in D major, played in F major
    VIB = (0x4, 0x22)

    def rh(r, bars, vol=38):
        s.line(r, RH, "harpsi", bars, 1, vol, per_bar=16, shift=K)

    def lh(r, bars, vol=38):
        s.line(r, LH, "harpsi_lo", bars, 1, vol, per_bar=16, shift=K)

    # the subject curls up the vine; its mirror (around F#, so the tonic chord maps onto itself) hangs below
    S = "a4 d5 c#5 d5 f#5 e5 f#5 a5 g5 f#5 e5 d5 c#5 a4 b4 c#5"
    S_INV = "d4 a3 b3 a3 f#3 g3 f#3 d3 e3 f#3 g3 a3 b3 d4 c#4 b3"
    CS = "f#5 . a5 . d6 . f#5 . g5 . e5 . c#5 . e5 ."
    CS_LOW = "d3 . f#3 . a3 . d4 . c#4 . a3 . e3 . a2 ."
    REST = ". . . . . . . . . . . . . . . ."
    EPISODE_RH = ["f#4 b4 a4 b4 d5 c#5 d5 f#5 d4 g4 f#4 g4 b4 a4 b4 d5",
                  "b4 e5 d5 e5 g5 f#5 g5 b5 e5 d5 b4 g#4 e4 g#4 b4 d5"]
    EPISODE_LH = ["b2 . d3 . f#3 . b2 . g2 . b2 . d3 . g2 .",
                  "e3 . g3 . b3 . e3 . e3 . d3 . b2 . g#2 ."]
    A_RH = [S, CS] + EPISODE_RH + [
        "e5 a5 g#5 a5 c#6 b5 c#6 e6 d6 c#6 b5 a5 g#5 e5 f#5 g#5",
        "c#5 . e5 . a5 . c#5 . d5 . b4 . g#4 . b4 .",
        "f#5 a5 d6 a5 f#5 a5 d5 f#5 e5 g#5 b5 g#5 e5 g#5 d5 b4",
        "c#6 a5 e5 c#5 a4 c#5 e5 a5 g5 e5 c#5 e5 g5 a5 a#5 ."]
    A_LH = [CS_LOW, S_INV] + EPISODE_LH + [
        "a2 . c#3 . e3 . a3 . g#3 . e3 . b2 . e2 .",
        "a3 e3 f#3 e3 c#3 d3 c#3 a2 b2 c#3 d3 e3 f#3 a3 g#3 f#3",
        "d3 . f#3 . a3 . d3 . e3 . g#3 . b2 . e2 .",
        "a2 . e3 . a3 . e3 . a2 . c#3 . e3 . f#3 ."]
    B_RH = ["b5 f#5 g5 f#5 d5 e5 d5 b4 c#5 d5 e5 f#5 g5 b5 a#5 g5",
            "f#5 b5 a5 b5 d6 c#6 d6 f#6 e6 d6 c#6 b5 a#5 f#5 g5 a#5",
            "g5 b5 a5 b5 d6 c#6 d6 b5 e5 g5 f#5 g5 b5 a5 b5 g5",
            "c#5 e5 g5 e5 c#5 e5 g5 b5 a#5 g5 f#5 e5 d5 c#5 b4 a#4",
            "d6 . c#6 . b5 . a#5 . b5 . g5 . e5 . g5 .",
            "c#6 . e5 . g5 . a5 . f#5 . d5 . a4 . d5 .",
            "b5 g5 d5 g5 b5 d6 c#6 b5 a5 e5 c#5 e5 a5 g5 f#5 e5",
            "f#5 d5 a4 d5 f#5 a5 d6 c#6 b5 a5 g5 f#5 e5 d5 c#5 b4"]
    B_LH = ["f#2 b2 a2 b2 d3 c#3 d3 f#3 e3 d3 c#3 b2 a#2 f#2 g2 a#2",
            "b3 f#3 g3 f#3 d3 e3 d3 b2 c#3 d3 e3 f#3 g3 b3 a#3 g3",
            "g2 . b2 . d3 . g3 . e3 . g3 . b3 . e3 .",
            "c#3 . e3 . g3 . c#3 . f#2 . a#2 . c#3 . f#3 .",
            "b2 c#3 d3 e3 f#3 e3 d3 c#3 e3 f#3 g3 a3 b3 a3 g3 f#3",
            "a2 b2 c#3 d3 e3 d3 c#3 b2 d3 e3 f#3 g3 a3 g3 f#3 e3",
            "g2 . b2 . d3 . g3 . a2 . c#3 . e3 . a2 .",
            "d3 . a2 . f#2 . d2 . a2 . . . a2 . . ."]
    B_REC = [REST, REST, REST, REST, "d5 . . . . . . . b4 . . . . . . .", "g4 . . . . . . . a4 . . . . . . .",
             "b4 . . . . . . . c#5 . . . . . . .", "a4 . . . . . . . . . . . . . . -"]
    PEDAL = "a2 . a3 . a2 . a3 . a2 . a3 . a2 . a3 ."
    CODA_RH = ["a4 d5 c#5 d5 f#5 e5 f#5 a5 d6 a5 b5 a5 f#5 g5 f#5 d5",
               "c#5 e5 d5 e5 g5 f#5 g5 b5 e6 b5 c#6 b5 g5 a5 g5 e5",
               "a4 b4 c#5 d5 e5 f#5 g5 a5 b5 c#6 d6 e6 f#6 e6 d6 c#6",
               "c#6 d6 c#6 d6 c#6 d6 c#6 b5 a5 . g5 . e5 . c#5 ."]
    CODA_LH = [PEDAL, PEDAL, "a2 . . . c#3 . . . e3 . . . g3 . . .", "a2 . . . e3 . . . a2 . c#3 . e3 . g3 ."]

    # intro: the subject alone, then its mirror in the left hand under the countersubject
    r = s.bars(2)
    rh(r, [S, CS])
    lh(r + 16, S_INV)
    # A: subject and mirror, the sequence down the vine, the subject on the dominant, a half close
    s.loop_here()
    r = s.bars(8)
    rh(r, A_RH)
    lh(r, A_LH)
    # B: D minor, the subject and its mirror at once (then the hands swap), the recorder joins
    r = s.bars(8)
    rh(r, B_RH)
    lh(r, B_LH)
    s.line(r, REC, "recorder", B_REC, 1, 18, VIB, per_bar=16, shift=K)
    # A': the reprise with an alto voice and the recorder holding the harmony
    r = s.bars(4)
    rh(r, [S, CS] + EPISODE_RH, 40)
    lh(r, [CS_LOW, S_INV] + EPISODE_LH, 40)
    s.line(r, ALT, "harpsi", ["f#4 . . . a4 . . . e4 . . . g4 . . .", "a4 . . . f#4 . . . e4 . . . c#4 . . .",
                              "d4 . . . f#4 . . . d4 . . . b3 . . .", "g4 . . . e4 . . . d4 . . . b3 . . ."],
           1, 30, per_bar=16, shift=K)
    s.line(r, REC, "recorder", ["f#5 . . . . . . . e5 . . . . . . .", "d5 . . . . . . . c#5 . . . . . . .",
                                "d5 . . . . . . . b4 . . . . . . .", "b4 . . . . . . . g#4 . . . . . . -"],
           1, 18, VIB, per_bar=16, shift=K)
    # coda: a dominant pedal, the subject and its mirror on top, a run up, a trill and a ritardando
    r = s.bars(4)
    rh(r, CODA_RH)
    lh(r, CODA_LH)
    s.line(r + 16, ALT, "harpsi", "e4 . . . . . . . g4 . . . . . . -", 1, 30, per_bar=16, shift=K)
    for j, bpm in ((4, 80), (8, 76), (10, 72), (12, 68), (14, 62)):
        s.flow(r + 48 + j, 0xF, bpm)
    return s.build("ss invention", 6, 84)            # speed 6 at 84 BPM: a sixteenth 179 ms (crotchet 84)


# ==== alt 2: Dub on the Underside ===================================================================================
def melodica():
    """A melodica: a free reed blown through a keyboard (a reedy, slightly hollow spectrum), a breathy start, then
    a 16-sample loop (C-1 = C4)."""
    rng = random.Random(23)
    h = [(1, 1), (2, .48), (3, .5), (4, .22), (5, .26), (6, .1), (7, .09)]
    wave = [sum(a * math.sin(2 * math.pi * k * i / 16 + .25 * k) for k, a in h) for i in range(16)]
    att = [wave[i % 16] * min(1.0, (i / 200.0) ** 1.3) + rng.uniform(-.3, .3) * math.exp(-i / 90.0)
           for i in range(256)]
    return norm(att + wave, 100)


def dub_bass():
    """A deep round bass: nearly a sine, a touch of the octave so it carries on small speakers, a soft attack,
    then a 64-sample loop (C-1 = C2)."""
    wave = [math.sin(2 * math.pi * i / 64) + .28 * math.sin(4 * math.pi * i / 64 + .3)
            + .06 * math.sin(6 * math.pi * i / 64) for i in range(64)]
    return norm([wave[i % 64] * min(1.0, i / 90.0) for i in range(192)] + wave, 110)


def skank(semis):
    """A muted guitar chop (one-shot): the strings strummed down fast and choked (32 samples a cycle at the
    root: C-1 = C3)."""
    rng = random.Random(31)
    out = [0.0] * 2400
    for s_, semi in enumerate(semis):
        P = 32 / 2 ** (semi / 12.0)
        parts = [(k, 1.0 / k ** .8) for k in range(1, 9) if k / P < .45]
        v = pluck(P, 2400, parts, 800, .6, attack=2, start=s_ * 14)
        out = [a + b for a, b in zip(out, v)]
    out = [x + rng.uniform(-.5, .5) * math.exp(-i / 40.0) for i, x in enumerate(out)]
    return norm(out, 104)


def drop():
    """The one-drop: a soft kick and a rimshot together, on the third beat."""
    rng = random.Random(41)
    out, ph = [], 0.0
    for i in range(int(HI * .36)):
        t = i / HI * 1000
        ph += (48 + 80 * math.exp(-t / 20)) / HI
        v = math.sin(2 * math.pi * ph) * math.exp(-t / 160)
        v += .55 * math.sin(2 * math.pi * 1750 * i / HI) * math.exp(-t / 12) + .3 * math.sin(
            2 * math.pi * 520 * i / HI) * math.exp(-t / 25) + .6 * rng.uniform(-1, 1) * math.exp(-t / 5)
        out.append(v * min(1.0, t / .3))
    return norm(out, 110)


def spring_snare():
    """A snare through a spring reverb: the crack, then a long boingy tail (played at the LO rate)."""
    base = noise_hit(LO, 1100, 50, 43, 1, [(200, .8, 40), (330, .3, 25)], tail=(.55, 320))
    out = []
    for i, v in enumerate(base):                       # the spring: a chirp that wobbles down
        t = i / LO
        f = 300 + 700 * math.exp(-t / .08)
        out.append(v + 28 * math.sin(2 * math.pi * f * t + 3 * math.sin(2 * math.pi * 7 * t)) * math.exp(-t / .25)
                   * (1 - math.exp(-t / .02)))
    return norm(out, 105)


def dub():
    smp = [("melodica", melodica(), 30, 256, 16, 60),
           ("bass", dub_bass(), 40, 192, 64, 36),
           ("drop", drop(), 38, 0, 0, 36),
           ("hat", noise_hit(HI, 110, 28, 6, 2), 24, 0, 0, 36),
           ("ohat", noise_hit(HI, 220, 70, 7, 2), 18, 0, 0, 36),
           ("rim", noise_hit(HI, 90, 6, 8, 1, [(1750, .8, 14), (520, .4, 22)]), 26, 0, 0, 36),
           ("splash", spring_snare(), 30, 0, 0, 48),
           ("sk_m", skank([0, 7, 12, 15]), 30, 0, 0, 48),
           ("sk_M", skank([0, 7, 12, 16]), 30, 0, 0, 48),
           ("sk_7", skank([0, 10, 12, 16]), 30, 0, 0, 48)]
    s = Song(smp, rpb=16, bpp=2)                      # 4/4 in sixteenths
    MEL, BASS, DR, SK = 0, 1, 2, 3
    VIB = (0x4, 0x32)

    def echo(r, inst, pitch, vol, taps):
        """the tape delay: copies of a note every dotted eighth (3 rows), quieter each time"""
        for k, f in enumerate(taps):
            rr = r + 3 * (k + 1)
            if rr < len(s.rows) and s.free(rr, SK):
                s.put(rr, SK, inst, pitch, max(2, int(vol * f)))

    def skanks(r0, chords, vol=36, taps=(.45, .2), beats=(4, 12)):
        for b, name in enumerate(chords):
            pc, q = chord(name)
            inst = {"m": "sk_m", "": "sk_M", "7": "sk_7"}[q]
            for e in beats:
                r = r0 + 16 * b + e
                s.put(r, SK, inst, fit(pc, 55), vol)
                echo(r, inst, fit(pc, 55), vol, taps)

    def melodica_echo(r0, r1, taps=(.42, .18)):
        """the melodica's echoes on the skank channel (the first tap wins, the second fills the gaps)"""
        for k, f in enumerate(taps):
            for r in range(r0, r1):
                c = s.rows[r][MEL]
                rr = r + 3 * (k + 1)
                if rr >= len(s.rows) or not s.free(rr, SK):
                    continue
                if c[0]:
                    s.rows[rr][SK] = [c[0], c[1], 0xC, max(2, int((c[3] if c[2] == 0xC else 30) * f))]
                elif c == [0, 0, 0xC, 0] and k == 0:
                    s.rows[rr][SK] = [0, 0, 0xC, 0]

    KITS = {"drop": [(0, "hat", 18), (2, "hat", 28), (4, "hat", 18), (6, "hat", 28), (8, "drop", 40),
                     (10, "hat", 28), (12, "hat", 18), (14, "hat", 28), (15, "hat", 14)],
            "open": [(0, "hat", 18), (2, "hat", 28), (4, "hat", 18), (6, "ohat", 26), (8, "drop", 40),
                     (10, "hat", 28), (12, "hat", 18), (14, "ohat", 26)],
            "dub": [(6, "hat", 20), (8, "drop", 40), (14, "hat", 20)],
            "hats": [(2, "hat", 26), (6, "hat", 26), (10, "hat", 26), (14, "hat", 26)],
            "splash": [(0, "hat", 18), (2, "hat", 28), (4, "hat", 18), (6, "hat", 28), (8, "drop", 40),
                       (12, "splash", 38)],
            "roll": [(0, "hat", 18), (4, "hat", 20), (8, "drop", 38)] + [(e, "rim", 16 + 3 * (e - 9))
                                                                         for e in range(9, 16)]}

    def drums(r0, styles):
        for b, style in enumerate(styles):
            for e, inst, v in KITS[style]:
                s.put(r0 + 16 * b + e, DR, inst, "c4", v)

    G1 = "g2 . . . . . . - bb2 . c3 . d3 . . -"
    G2 = "f3 . . - d3 . . - c3 . . - bb2 . a2 ."
    C1 = "c3 . . . . . . - e3 . f3 . g3 . . -"
    C2 = "bb3 . . - g3 . . - f3 . . - e3 . d3 ."
    EB = "eb3 . . . . . . - g3 . bb3 . g3 . . -"
    D7 = "d3 . . . . . . - f#2 . a2 . c3 . a2 -"
    A_CH = ["Gm", "Gm", "C7", "C7", "Gm", "Gm", "Eb", "D7"]
    A_BASS = [G1, G2, C1, C2, G1, G2, EB, D7]
    # the hook climbs a fourth and curls down; over C it is played upside down
    HOOK = ["- . d5 g5 . . bb5 a5", "g5 . . . f5 d5 . -", "- . c6 g5 . . e5 f5", "g5 . . . a5 c6 . -",
            "- . d5 g5 . bb5 d6 .", "c6 bb5 a5 g5 f5 . d5 .", "eb5 . g5 . bb5 . g5 .", "f#5 . . . a5 . . -"]
    B_CH = ["Eb", "F", "Gm", "Gm", "Cm", "F", "D7", "D7"]
    B_BASS = ["- . . . . . . . eb3 . . - bb2 . . -", "f2 . . . . . . - a2 . c3 . f3 . . -", G1, G2,
              "c3 . . . . . . - eb3 . f3 . g3 . . -", "f3 . . - c3 . . - a2 . . - f2 . . -", D7,
              "d3 . . - a2 . . - c3 . . - d3 . f#2 ."]
    B_MEL = ["bb5 . . . . . g5 .", "a5 . . . . . f5 .", "g5 . . . d5 . . -", "- . . . . . . .",
             "- . c5 eb5 g5 . . .", "f5 . eb5 . c5 . a4 .", "a4 . c5 . d5 . f#5 .", "a5 . . . . . . -"]

    # intro: the skank alone with its echoes over the hats, then the drums and the bass
    r = s.bars(2)
    drums(r, ["hats", "splash"])
    skanks(r, ["Gm", "Gm"], 34, (.5, .3))
    s.line(r + 16, BASS, "bass", D7, 1, 27, per_bar=16)
    # A: the hook
    s.loop_here()
    r = s.bars(8)
    drums(r, ["drop"] * 7 + ["splash"])
    s.line(r, BASS, "bass", A_BASS, 1, 27, per_bar=16)
    skanks(r, A_CH)
    s.line(r, MEL, "melodica", HOOK, 2, 32, VIB, per_bar=8)
    # B: the dub, the skank drops out and the melodica echoes, the bass leaves holes; then all of it comes back
    r = s.bars(8)
    drums(r, ["dub", "dub", "drop", "splash", "open", "open", "open", "roll"])
    s.line(r, BASS, "bass", B_BASS, 1, 27, per_bar=16)
    s.line(r, MEL, "melodica", B_MEL, 2, 32, VIB, per_bar=8)
    melodica_echo(r, r + 64)
    skanks(r + 64, B_CH[4:], 36, (.5, .25))
    # dub-out: a melodica note thrown into the delay, a lone skank, a stop, a rim roll back into A
    r = s.bars(4)
    drums(r, ["dub", "drop", "dub", "roll"])
    s.line(r, BASS, "bass", [G1, G2, "- . . . . . . . . . . . . . . .", D7], 1, 27, per_bar=16)
    s.put(r, MEL, "melodica", "d6", 30)
    s.off(r + 2, MEL)
    echo(r, "melodica", "d6", 30, (.55, .38, .26, .17, .11))
    skanks(r + 16, ["Gm"], 36, (.55, .38, .26, .17), beats=(4,))
    s.put(r + 32, MEL, "melodica", "g5", 28)
    s.off(r + 34, MEL)
    echo(r + 32, "melodica", "g5", 28, (.55, .38, .26, .17, .11))
    s.off(r + 50, SK)
    return s.build("ss dub underside", 6, 72)        # speed 6 at 72 BPM: a sixteenth 208 ms (crotchet 72)


TRACKS = [invention, dub]


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
