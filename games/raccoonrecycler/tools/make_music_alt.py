#!/usr/bin/env python3
"""Raccoon Recycler alternative music: two 4-channel ProTracker MODs written with the house music kit
(games/common/tools/house_music.py: the MOD writer and periods; docs/art-direction.md "Audio"), the instruments
synthesised here, at the house mix (sample volumes <= 40).

  alt1  "Junkyard Stomp": a stomp in 7/8 (2+2+3, the lopsided clank of a sorting machine), D dorian with an
        F major middle, an eighth at 260 a minute. Tuned bottles struck with a spoon and blown across the neck,
        a slapped-pipe bass, boots on a crate, bin lids and tin cans. The hook: a ping-pong between two bottles
        (left bin, right bin) that tumbles down three notes into the bin, sequenced down a step each bar. Intro
        (the stomp alone, then the pipes and a blown whoop), A (the tapped hook, blown bottles hum below), A'
        (the blown bottles take the hook, the taps harmonise, the cans run), B (F major: a blown tune over tapped
        arpeggios), a bridge over an A pedal (the sign-change chime, three falling bottles) that builds back up
        to A.
  alt2  "Greasy Paws": a greasy night-shift funk in 4/4, a crotchet of 100, E dorian (E7#9). A clavinet riff with
        choked ghost notes, a bass through an envelope filter (each note says "bwow"), a tight kit and a horn
        section that stabs and falls. The hook: the clav's syncopated "e-g-a, b-a-g" answered by two horn hits,
        "bwap bwaaa", the second one falling off. Intro (the kit, then the clav and the bass), A, B (G A C B7: the
        horns sing, the clav chanks), A' (the horns double the hook), a breakdown (the wah bass alone, the clav
        creeping back, a horn swell and a snare fill), then the loop back to A.

    make_music_alt.py              -> ../music-alt/alt1.mod, alt2.mod (previews; the game is unchanged)
    make_music_alt.py OUTDIR N     -> OUTDIR/tune.mod: alternative N (1 or 2), for tools/build_assets.py

MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/raccoonrecycler/LICENSE.
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
QUAL = {"": [0, 4, 7], "m": [0, 3, 7], "7": [0, 4, 7, 10], "m7": [0, 3, 7, 10], "maj7": [0, 4, 7, 11],
        "9": [0, 4, 7, 10, 14], "7#9": [0, 4, 7, 10, 15]}


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
    """'F#m7' -> (root pitch class, 'm7', bass pitch class); 'C/E' has the bass E."""
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
            core = tok.rstrip("!,/*")
            toks.append(tok if core in (".", "-") else name_of(below(midi(core), pcs, gap)) + tok[len(core):])
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

    def fx(self, r, ch, eff, par):
        """An effect alone (a volume change, a slide) on row r, if the cell is free."""
        if r < len(self.rows) and self.free(r, ch):
            self.rows[r][ch] = [0, 0, eff, par]

    def free(self, r, ch):
        return self.rows[r][ch] == [0, 0, 0, 0]

    def off(self, r, ch):
        self.fx(r, ch, 0xC, 0)

    def line(self, r, ch, inst, bars, step, vol=36, hold_fx=None, slide=0x18, per_bar=None):
        """A melody, one token per `step` rows: 'c5' a note ('!' louder, ',' softer, '/' slides to it from the
        previous note, '*' tremolo: repeated on every row), '.' holds, '-' stops, 'a4+b4' splits a step.
        hold_fx (effect, parameter) goes on the held rows (a vibrato)."""
        if isinstance(bars, str):
            bars = [bars]
        for b in bars:
            if per_bar and len(b.split()) != per_bar:
                raise ValueError("bar %r: %d tokens, not %d" % (b, len(b.split()), per_bar))
        state, cur = None, None
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
                for j in range(rr, r + (k + 1) * sub):
                    if state == "trem":
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


def pluck(P, n, partials, tau, power=0.7, attack=3, start=0):
    """An additive plucked string, P samples a cycle: partial k (ratio, amplitude) decays in tau / k**power."""
    out = [0.0] * n
    for k, a in partials:
        t = tau / k ** power
        for i in range(start, n):
            j = i - start
            e = math.exp(-j / t)
            if e < 1e-4:
                break
            out[i] += a * e * math.sin(2 * math.pi * k * j / P) * min(1.0, j / attack)
    return out


def noise_hit(sr, ms, decay_ms, seed, hp=1, tone=None, attack_ms=0.5, peak=105):
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
    return norm(out, peak)


def thump(sr, f0, f1, fall_ms, decay_ms, ms, click, seed, body=0.0):
    """A drum: a sine falling from f0 to f1 Hz, a click of noise and (body) a low-passed thud of noise."""
    rng = random.Random(seed)
    out, ph, lp = [], 0.0, 0.0
    for i in range(int(sr * ms / 1000)):
        t = i / sr * 1000
        ph += (f1 + (f0 - f1) * math.exp(-t / fall_ms)) / sr
        lp += 0.08 * (rng.uniform(-1, 1) - lp)
        v = math.sin(2 * math.pi * ph) * math.exp(-t / decay_ms)
        v += click * rng.uniform(-1, 1) * math.exp(-t / 3) + body * 4 * lp * math.exp(-t / 40)
        out.append(v * min(1.0, t / 0.3))
    return norm(out, 112)


HI = 16574          # the percussion rate: played at c4 with "the MIDI at C-1" 36


# ==== alt 1: Junkyard Stomp ==========================================================================================
def tapped_bottle():
    """A glass bottle struck with a spoon: a pure ring, a faint octave, a glassy inharmonic partial that dies fast
    and a bright tick (16 samples a cycle: C-1 = C4)."""
    rng = random.Random(3)
    out = []
    for i in range(5200):
        w = 2 * math.pi * i / 16
        v = math.sin(w) * math.exp(-i / 2600) + .16 * math.sin(2 * w) * math.exp(-i / 900)
        v += .24 * math.sin(2.76 * w + .5) * math.exp(-i / 380) + .12 * math.sin(5.4 * w) * math.exp(-i / 110)
        v += rng.uniform(-.7, .7) * max(0.0, 1 - i / 36)
        out.append(v * min(1.0, i / 2))
    return norm(out, 106)


def blown_bottle():
    """Blowing across a bottle's neck: a hollow near-sine, a breathy swell of noise that turns into the tone, then
    a 64-sample loop with a little air left in it (16 samples a cycle: C-1 = C4)."""
    rng = random.Random(8)
    air, lp = [], 0.0
    for i in range(64):
        lp += 0.35 * (rng.uniform(-1, 1) - lp)
        air.append(lp)
    air = [a - sum(air) / 64 for a in air]
    loop = [math.sin(2 * math.pi * i / 16) + .12 * math.sin(4 * math.pi * i / 16 + .4)
            + .05 * math.sin(6 * math.pi * i / 16) + .08 * air[i] for i in range(64)]
    att = []
    for i in range(480):
        t = i / 480.0
        att.append(loop[i % 64] * t ** 1.4 + rng.uniform(-.55, .55) * (1 - t) * min(1.0, i / 30))
    return norm(att + loop, 100)


def pipe():
    """A slapped plastic pipe, the junkyard bass: a hollow tube (strong odd harmonics), a little downward bend at
    the slap, a flat 'thwack' of noise (64 samples a cycle: C-1 = C2)."""
    rng = random.Random(21)
    out, ph = [], 0.0
    for i in range(5600):
        ph += (1 / 64.0) * (1 + .07 * math.exp(-i / 140))
        w = 2 * math.pi * ph
        v = (math.sin(w) + .25 * math.sin(2 * w) * math.exp(-i / 700) + .45 * math.sin(3 * w) * math.exp(-i / 520)
             + .22 * math.sin(5 * w) * math.exp(-i / 260) + .1 * math.sin(7 * w) * math.exp(-i / 150))
        v = v * math.exp(-i / 2000) + rng.uniform(-.9, .9) * max(0.0, 1 - i / 70)
        out.append(v * min(1.0, i / 3))
    return norm(out, 112)


def stomp():
    """A boot on a wooden crate: a low thump with a woody knock."""
    return thump(HI, 150, 50, 22, 170, 380, .35, 1, body=.5)


def lid(hit):
    """A dustbin lid: a crash of inharmonic metal (let ring) or (hit) slapped and choked by a hand, the snare of
    the junkyard."""
    metal = [(347, .5, 380), (813, .45, 330), (1290, .4, 280), (1765, .35, 240), (2410, .3, 200), (3120, .3, 170),
             (4380, .25, 140), (5650, .2, 110)]
    if hit:
        metal = [(f, a, d / 5.0) for f, a, d in metal] + [(190, .9, 40)]
        return noise_hit(HI, 260, 55, 41, 1, metal)
    return noise_hit(HI, 520, 120, 42, 1, metal + [(70, .6, 90)])


def can():
    """A tin can kicked or tapped: a short, bright clank."""
    return noise_hit(HI, 140, 14, 51, 2, [(1250, .7, 45), (2730, .5, 32), (3900, .45, 25), (5480, .3, 18)])


def stomp_track():
    smp = [("bottle", blown_bottle(), 30, 480, 64, 60),
           ("tap", tapped_bottle(), 36, 0, 0, 60),
           ("pipe", pipe(), 40, 0, 0, 36),
           ("stomp", stomp(), 40, 0, 0, 36),
           ("lid", lid(False), 30, 0, 0, 36),
           ("lidhit", lid(True), 34, 0, 0, 36),
           ("can", can(), 26, 0, 0, 36)]
    s = Song(smp, rpb=14, bpp=4)                      # 7/8 in 16ths: a bar = 14 rows, grouped 4 + 4 + 6
    BOT, PIPE, DR, TAP = 0, 1, 2, 3
    VIB = (0x4, 0x24)

    def bass(r0, chords, nxt, style="stomp"):
        for b, name in enumerate(chords):
            pc, q, bpc = chord(name)
            r, R = r0 + 14 * b, fit(bpc, 38)
            N = fit(chord(chords[b + 1] if b + 1 < len(chords) else nxt)[2], 38)
            seventh = R + (9 if q == "" else 10)
            appr = N - 1 if b % 2 == 0 else N + 2      # a chromatic step up, or a step down, into the next root
            if style == "stomp":
                notes = [(0, R, 40), (3, R, 26), (4, R + 7, 34), (8, R + 12, 36), (10, seventh, 30), (12, appr, 32)]
            elif style == "walk":
                third = R + (3 if q.startswith("m") else 4)
                notes = [(0, R, 40), (2, R + 12, 24), (4, R + 7, 34), (6, third, 28), (8, R, 36), (10, R + 12, 30),
                         (12, appr, 32)]
            else:                                       # a pedal: the root hammered
                notes = [(0, R, 40), (4, R, 28), (8, R, 36), (10, R + 12, 24), (12, R, 30)]
            for e, m, v in notes:
                s.put(r + e, PIPE, "pipe", m, v)

    GROOVE = [(0, "stomp", 40), (2, "can", 20), (4, "lidhit", 32), (6, "can", 18), (8, "stomp", 38),
              (10, "stomp", 30), (12, "lidhit", 34), (13, "can", 14)]
    KITS = {"groove": GROOVE,
            "busy": GROOVE + [(e, "can", 12 + 2 * (e % 4 == 3)) for e in (1, 3, 5, 7, 9, 11)],
            "light": [(0, "stomp", 38), (4, "can", 18), (8, "stomp", 34), (10, "can", 14), (12, "lidhit", 28)],
            "sparse": [(0, "lid", 30), (4, "can", 16), (8, "can", 20), (10, "can", 14), (12, "can", 22)],
            "fill": [(e, "stomp", 22 + 2 * e) for e in (0, 2, 4, 6, 8)]
            + [(e, "lidhit", v) for e, v in ((10, 30), (11, 32), (12, 36), (13, 38))]}

    def drums(r0, styles, crash=False):
        for b, style in enumerate(styles):
            for e, inst, v in KITS[style]:
                s.put(r0 + 14 * b + e, DR, inst, "c4", v)
        if crash:
            s.put(r0, DR, "lid", "c4", 34)

    def arps(r0, chords, vol=30):
        """tapped bottles up and down the chord, in eighths"""
        for b, name in enumerate(chords):
            pc, q, _ = chord(name)
            R = fit(pc, 65)
            up = [R, R + QUAL[q][1], R + 7, R + 12]
            for e, k in enumerate([0, 1, 2, 3, 2, 1, 2]):
                s.put(r0 + 14 * b + 2 * e, TAP, "tap", up[k], vol - (4 if e % 2 else 0))

    A_CH = ["Dm", "C", "Bb", "A7", "Dm", "C", "Gm", "A7"]
    HOOK = ["d5! a4 d5 a4 f5 e5 d5", "c5! g4 c5 g4 e5 d5 c5", "bb4! f4 bb4 f4 d5 c5 bb4", "a4 c#5 e5 g5 a5! . -",
            "d5! a4 d5 a4 f5 g5 a5", "g5 e5 c5 e5 g5 . e5", "f5 d5 bb4 d5 g5 f5 e5", "c#5 . e5 . a4 . -"]
    HUM = ["a4 . . . . . .", "g4 . . . e4 . .", "f4 . . . d4 . .", "e4 . . . c#4 . .",
           "f4 . . . a4 . .", "g4 . . . c5 . .", "bb4 . . . a4 . g4", "a4 . . . . . -"]
    B_CH = ["F", "C", "Dm", "Bb", "F", "Gm", "C7", "C7"]
    B_TUNE = ["a5 . . g5 f5 . c5", "e5 . . f5 g5 . .", "f5 . . e5 d5 . a4", "d5 . . c5 bb4 . .",
              "c5 . f5 . a5 . c6", "bb5 . a5 . g5 . d5", "e5 . g5 . bb5 . c6", "c6 . . . bb5 a5 g5"]
    BR_CH = ["A", "Bb", "A", "Bb", "Gm", "Gm", "A7", "A7"]
    CHIME = ["a5 . f5 . d5 . -", "bb5 . g5 . d5 . -", "a5 . e5 . c#5 . -", "bb5 . f5 . d5 . -",
             "g4 d5 g4 d5 bb4 a4 g4", "a4 e5 a4 e5 c5 bb4 a4", "c#5 g5 c#5 g5 e5 d5 c#5", "e5 g5 a5 c#6 e6! . -"]
    BR_HUM = ["e5 . . . . . .", "f5 . . . . . .", "e5 . . . . . .", "d5 . . . . . -",
              "d4 . . . . . .", "e4 . . . . . .", "e4 . . . g4 . .", "a4 . . . . . -"]

    # intro: the stomp alone, then the pipes and a blown bottle whooping up into the hook
    r = s.bars(4)
    drums(r, ["light", "groove", "groove", "groove"])
    bass(r + 28, ["Dm", "A7"], "Dm")
    s.line(r + 28, BOT, "bottle", ["d4 . . . a4/ . .", "d5/ . . . . . -"], 2, 28, slide=0x10, per_bar=7)
    s.line(r + 42, TAP, "tap", "a4 c#5 e5 g5 a5 . -", 2, 32, per_bar=7)
    # A: the tapped hook, the blown bottles hum the chords
    s.loop_here()
    r = s.bars(8)
    drums(r, ["groove"] * 8, crash=True)
    bass(r, A_CH, "Dm")
    s.line(r, TAP, "tap", HOOK, 2, 36, per_bar=7)
    s.line(r, BOT, "bottle", HUM, 2, 18, VIB, per_bar=7)
    # A': the blown bottles take the hook, the taps harmonise, the cans run on every sixteenth
    r = s.bars(8)
    drums(r, ["busy"] * 7 + ["fill"])
    bass(r, A_CH, "F", "walk")
    s.line(r, BOT, "bottle", HOOK, 2, 27, VIB)
    s.line(r, TAP, "tap", harmony(HOOK, A_CH), 2, 28)
    # B: F major, a blown tune over tapped arpeggios
    r = s.bars(8)
    drums(r, ["light"] * 7 + ["groove"], crash=True)
    bass(r, B_CH, "A")
    arps(r, B_CH)
    s.line(r, BOT, "bottle", B_TUNE, 2, 27, VIB, per_bar=7)
    # bridge: an A pedal, the sign-change chime falls three bottles at a time, then the hook climbs back
    r = s.bars(8)
    drums(r, ["sparse"] * 4 + ["groove", "groove", "busy", "fill"])
    bass(r, BR_CH[:4], "Gm", "pedal")
    bass(r + 56, BR_CH[4:], "Dm")
    s.line(r, TAP, "tap", CHIME, 2, 34, per_bar=7)
    s.line(r, BOT, "bottle", BR_HUM, 2, 18, VIB, per_bar=7)
    return s.build("rr junkyard stomp", 6, 130)       # speed 6 at 130 BPM: a sixteenth 115 ms (eighth 260/min)


# ==== alt 2: Greasy Paws ============================================================================================
def clav(muted):
    """A clavinet: a hammered string read by a pickup near the bridge (a bright, nasal comb of partials); the
    muted one is choked by the felt (32 samples a cycle: C-1 = C3)."""
    parts = [(k, abs(math.sin(math.pi * k * .13)) / k ** .3) for k in range(1, 16)]
    return norm(pluck(32, 3000 if muted else 9000, parts, 1400 if muted else 6000, .35, attack=2), 112)


def wah_bass():
    """A bass through an envelope filter: each note opens a resonant filter with a vocal 'bwow' and closes it
    again (64 samples a cycle: C-1 = C2)."""
    out = []
    for i in range(7000):
        env = (1 - math.exp(-i / 110.0)) * math.exp(-i / 1500.0)
        c = 1.3 + 8.5 * env                          # the resonance, as a harmonic number
        v = 0.0
        for k in range(1, 22):
            a = (1.0 / (1 + (k / c) ** 4)) / k ** .5 + 1.3 * math.exp(-((k - c) / 1.4) ** 2) / k ** .3
            v += a * math.sin(2 * math.pi * k * i / 64)
        out.append(v * math.exp(-i / 4200.0) * min(1.0, i / 6))
    return norm(out, 112)


def brass_partials(t, k):
    """Brass brightness: the upper partials bloom a few milliseconds after the lip buzz starts."""
    bright = min(1.0, 0.25 + t / 0.03) * (0.75 + 0.25 * math.exp(-t / 0.08))
    return bright ** (0.6 * (k - 1)) / k ** .9


def horn_stab(semis):
    """A horn section stab (one-shot): three horns on `semis` above the sample's root (64 samples a cycle: C-1 =
    C2), a scoop up into pitch, a bloom of brightness, a short body."""
    n, out = 6000, [0.0] * 6000
    rate = 4143.0                                   # the sample rate at C-1, for the envelopes
    for semi in semis:
        P = 64 / 2 ** (semi / 12.0)
        ph = 0.0
        for i in range(n):
            t = i / rate
            ph += (1 / P) * (1 - .03 * math.exp(-t / 0.02))
            v = sum(brass_partials(t, k) * math.sin(2 * math.pi * k * ph) for k in range(1, 9) if k / P < .45)
            out[i] += v * min(1.0, t / 0.008) * (0.55 + 0.45 * math.exp(-t / 0.12)) * math.exp(-t / 0.9)
    return norm(out, 104)


def horn():
    """A single brassy horn for the tunes: a bright attack, then a 32-sample loop (C-1 = C3)."""
    rate = 4143.0
    wave = [sum(brass_partials(1.0, k) * math.sin(2 * math.pi * k * i / 32) for k in range(1, 12)) for i in range(32)]
    att = [sum(brass_partials(i / rate, k) * math.sin(2 * math.pi * k * i / 32) for k in range(1, 12))
           * min(1.0, i / 30.0) for i in range(320)]
    return norm(att + wave, 100)


def funk():
    smp = [("clav", clav(False), 40, 0, 0, 48),
           ("clav_m", clav(True), 40, 0, 0, 48),
           ("wbass", wah_bass(), 40, 0, 0, 36),
           ("kick", thump(HI, 170, 52, 18, 150, 330, .45, 2), 40, 0, 0, 36),
           ("snare", noise_hit(HI, 230, 62, 5, 1, [(190, .9, 45), (330, .45, 28)]), 36, 0, 0, 36),
           ("hat", noise_hit(HI, 70, 13, 6, 2), 24, 0, 0, 36),
           ("ohat", noise_hit(HI, 260, 85, 7, 2, [(5600, .15, 80), (7300, .15, 60)]), 22, 0, 0, 36),
           ("st_7s9", horn_stab([16, 22, 27]), 32, 0, 0, 36),
           ("st_9", horn_stab([16, 22, 26]), 32, 0, 0, 36),
           ("horn", horn(), 26, 320, 32, 48)]
    s = Song(smp, rpb=16, bpp=2)                      # 4/4 in sixteenths
    CLV, BASS, DR, HRN = 0, 1, 2, 3

    def clav_line(r0, bars, vol=38):
        """sixteenths: 'e4' open, 'e4x' choked by the felt, '.' rings on, '-' stops"""
        for b, text in enumerate(bars):
            toks = text.split()
            if len(toks) != 16:
                raise ValueError("clav bar %r: %d tokens" % (text, len(toks)))
            for e, t in enumerate(toks):
                r = r0 + 16 * b + e
                if t == "-":
                    s.off(r, CLV)
                elif t != ".":
                    s.put(r, CLV, "clav_m" if t.endswith("x") else "clav", t.rstrip("x!"),
                          min(40, vol + (3 if "!" in t else 0)) - (6 if t.endswith("x") else 0))

    def stab(r, name, vol=34, fall=False):
        pc, q, _ = chord(name)
        s.put(r, HRN, "st_9" if q == "9" else "st_7s9", fit(pc, 40), vol)
        if fall:                                       # the section falls off the note
            for j in range(2, 6):
                s.fx(r + j, HRN, 0x2, 0x0C)
            s.off(r + 6, HRN)
        else:
            s.off(r + 2, HRN)

    GROOVE = [(0, "kick", 40), (2, "hat", 24), (3, "kick", 28), (4, "snare", 38), (6, "hat", 24), (7, "snare", 12),
              (8, "hat", 26), (10, "kick", 34), (11, "hat", 14), (12, "snare", 38), (14, "ohat", 20),
              (15, "snare", 12), (1, "hat", 12), (5, "hat", 12), (9, "hat", 12), (13, "hat", 14)]
    KITS = {"groove": GROOVE,
            "lean": [(0, "kick", 40), (2, "hat", 20), (4, "hat", 16), (6, "hat", 20), (8, "hat", 18),
                     (10, "kick", 32), (12, "hat", 16), (14, "hat", 20)],
            "fill": [(0, "kick", 40), (2, "hat", 22), (4, "snare", 36), (6, "kick", 30)]
            + [(e, "snare", 14 + 3 * (e - 8)) for e in range(8, 16)]}

    def drums(r0, styles):
        for b, style in enumerate(styles):
            for e, inst, v in KITS[style]:
                s.put(r0 + 16 * b + e, DR, inst, "c4", v)

    E1 = "e4! - e4x g4 - e4x a4 e4x b4 a4 g4 e4x - d4 e4 -"
    E2 = "e4! - e4x g4 - e4x a4 e4x d5 . b4 a4x g4 e4x d4 e4x"
    A1 = "a4! - a4x c5 - a4x d5 a4x e5 d5 c5 a4x - g4 a4 -"
    A2 = "a4! - a4x c5 - a4x d5 a4x g5 . e5 d5x c5 a4x g4 a4x"
    TURN = "c5 - c5x e5 - c5x bb4 c5x b4 - b4x d#5 - a4 b4x -"
    CLAV_A = [E1, E2, E1, E2, A1, A2, E1, TURN]
    EA = "e2 . . e2 - . e3 d3 . - b2 d3 e3 - d3 b2"
    EB = "e2 . . e2 - . g2 g#2 a2 - a2 b2 d3 c#3 c3 b2"
    AA = "a2 . . a2 - . a3 g3 . - e3 g3 a3 - g3 e3"
    AB = "a2 . . a2 - . c#3 d3 d#3 e3 . - g3 e3 d3 b2"
    TB = "c3 . . c3 - . g2 a#2 b2 . . b2 - . a2 b2"
    BASS_A = [EA, EB, EA, EB, AA, AB, EA, TB]
    B_CH = ["G", "A", "C", "B7#9"]
    BASS_B = ["g2 . . g2 - . g3 - d3 . . g2 - . a2 b2", "a2 . . a2 - . a3 - e3 . . a2 - . b2 c3",
              "c3 . . c3 - . c4 - g3 . . c3 - . b2 a2", "b2 . . b2 - . b3 - f#3 . . b2 - . d3 d#3"]
    HORN_B = ["b4 . . d5 . . e5 . g5 . . e5 . d5 b4 -", "c#5 . . e5 . . f#5 . a5 . . f#5 . e5 c#5 -",
              "e5 . . g5 . . a5 . b5 . . a5 . g5 e5 -", "d#5 . f#5 . a5 . f#5 . d5 . . . b4 . . -"]
    HORN_B2 = HORN_B[:3] + ["d#5 f#5 a5 b5 . . a5 . f#5 . d#5 . b4 . . -"]
    HOOK_E = "e5 . . g5 . . a5 . b5 a5 g5 . . d5 e5 -"
    HOOK_A = "a4 . . c5 . . d5 . e5 d5 c5 . . g4 a4 -"
    REST = ". . . . . . . . . . . . . . . ."

    def chanks(r0, chords, busy=False):
        """the clav on the offbeats: the chord's third, choked (and in sixteenths when busy)"""
        for b, name in enumerate(chords):
            pc, q, _ = chord(name)
            third, fifth = fit(pc + 4, 59), fit(pc + 7, 59)
            for e in ((2, 6, 10, 14) if not busy else range(1, 16)):
                if busy and e % 4 == 0:
                    continue
                s.put(r0 + 16 * b + e, CLV, "clav_m", third if e % 4 == 2 else fifth, 36 if e % 4 == 2 else 28)

    def section_a(r, horns_hook):
        drums(r, ["groove"] * 7 + ["fill"])
        clav_line(r, CLAV_A)
        s.line(r, BASS, "wbass", BASS_A, 1, 34, per_bar=16)
        if horns_hook:                                  # the horn doubles the hook an octave up; no falls into it
            s.line(r, HRN, "horn", [HOOK_E, REST, HOOK_E, REST, HOOK_A, REST, HOOK_E, REST], 1, 24, per_bar=16)
        else:
            stab(r, "E7#9", 34)
        for b in (1, 3, 5):
            stab(r + 16 * b + 12, "A9" if b == 5 else "E7#9", 30)
            stab(r + 16 * b + 14, "A9" if b == 5 else "E7#9", 34, fall=not horns_hook)
        stab(r + 112, "C9", 30)
        stab(r + 120, "B7#9", 34, fall=True)

    # intro: the kit alone, then the clav and the wah bass
    r = s.bars(2)
    drums(r, ["groove", "groove"])
    clav_line(r + 16, [E1])
    s.line(r + 16, BASS, "wbass", EA, 1, 38, per_bar=16)
    # A: the riff, the horns answer "bwap bwaaa"
    s.loop_here()
    section_a(s.bars(8), False)
    # B: G A C B7#9, the horns sing, the clav chanks on the offbeats (then in sixteenths)
    r = s.bars(8)
    drums(r, ["groove"] * 7 + ["fill"])
    s.line(r, BASS, "wbass", BASS_B * 2, 1, 36, per_bar=16)
    s.line(r, HRN, "horn", HORN_B + HORN_B2, 1, 28, (0x4, 0x22), per_bar=16)
    chanks(r, B_CH)
    chanks(r + 64, B_CH, busy=True)
    # A': the horn doubles the hook
    section_a(s.bars(8), True)
    # breakdown: the wah bass alone over a lean kit, the clav creeps back, a horn swell and a snare fill
    r = s.bars(4)
    drums(r, ["lean", "lean", "groove", "fill"])
    s.line(r, BASS, "wbass", ["e2 . . . e3 . d3 . e3 . g3 . e3 . d3 b2", "e2 . . . e3 . d3 . g3 . a3 . a#3 b3 . -",
                              EA, EB], 1, 38, per_bar=16)
    clav_line(r + 32, ["e4x e4x . e4x e4x . e4x . e4x e4x . e4x . g4x a4x b4x", REST], 30)
    s.put(r + 48, HRN, "horn", "b4", 8)
    for j in range(2, 16, 2):
        s.fx(r + 48 + j, HRN, 0xC, 8 + 2 * j)
    return s.build("rr greasy paws", 6, 100)          # speed 6 at 100 BPM: a sixteenth 150 ms (crotchet 100)


TRACKS = [stomp_track, funk]


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
