#!/usr/bin/env python3
"""Blueberry Tumble: the music is on the game's beat grid (make blueberrytumble-check).

For every track (build/gen/blueberrytumble/music/*.mod): 4 channels, a 24-bar song (6 patterns of 64 rows, 4 rows
per beat), the tempo and speed of its tier on the first row (BPM = 6 T / S = src/tuning.h TIER_BPM_X100, a whole
tick of 80000 / T samples, so the song never drifts from the frame clock), and a drum hit on every beat.

    test_music.py MUSIC_DIR
MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/blueberrytumble/LICENSE.
"""
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))


def tuning_list(name):
    src = open(os.path.join(HERE, "..", "src", "tuning.h")).read()
    return [int(v) for v in re.search(r"#define\s+%s\s+\{([^}]*)\}" % name, src).group(1).split(",")]


BPM100 = tuning_list("TIER_BPM_X100")
TRACK_TIER = {"summit": 0, "forest": 1, "meadow": 2, "village": 3}


def parse(data):
    samples = []
    for i in range(31):
        off = 20 + i * 30
        ln, _ft, vol, _ls, _ll = struct.unpack(">HBBHH", data[off + 22:off + 30])
        samples.append((ln * 2, vol))
    song_len = data[950]
    orders = list(data[952:952 + song_len])
    assert data[1080:1084] == b"M.K.", "not a 4-channel M.K. module"
    npat = max(data[952:1080]) + 1
    pats = []
    for p in range(npat):
        base = 1084 + p * 1024
        rows = []
        for r in range(64):
            cells = []
            for ch in range(4):
                b = data[base + r * 16 + ch * 4: base + r * 16 + ch * 4 + 4]
                smp = (b[0] & 0xF0) | (b[2] >> 4)
                period = ((b[0] & 0x0F) << 8) | b[1]
                cells.append((smp, period, b[2] & 0x0F, b[3]))
            rows.append(cells)
        pats.append(rows)
    return samples, orders, pats


def main():
    d = sys.argv[1]
    fails, n = 0, 0
    for f in sorted(os.listdir(d)):
        if not f.endswith(".mod"):
            continue
        n += 1
        name = f[:-4]
        tier = TRACK_TIER.get(name, len(BPM100) - 1)
        _samples, orders, pats = parse(open(os.path.join(d, f), "rb").read())
        errs = []
        if len(orders) != 6 or len(pats) != 6:
            errs.append("%d orders, %d patterns (want 6: 24 bars)" % (len(orders), len(pats)))
        first = pats[orders[0]][0]
        tempo = [c[3] for c in first if c[2] == 0xF and c[3] >= 32]
        speed = [c[3] for c in first if c[2] == 0xF and 0 < c[3] < 32]
        if not tempo or not speed:
            errs.append("no tempo/speed on the first row")
        else:
            T, S = tempo[0], speed[0]
            if abs(6.0 * T / S * 100 - BPM100[tier]) > 1:
                errs.append("tempo %d speed %d = %.2f BPM, tier %d wants %.2f" % (T, S, 6.0 * T / S, tier, BPM100[tier] / 100))
            if 80000 % T:
                errs.append("tempo %d: the tick is not a whole number of samples" % T)
        missing = 0
        for p in orders:
            for r in range(0, 64, 4):
                if pats[p][r][2][0] == 0:
                    missing += 1
        if missing:
            errs.append("%d beats without a drum hit" % missing)
        if errs:
            fails += 1
            print("  FAIL %s: %s" % (f, "; ".join(errs)))
        else:
            print("  ok   %s: tier %d, %.2f BPM, 24 bars, a drum hit on every beat" % (f, tier, BPM100[tier] / 100))
    if n < 8:
        print("  FAIL only %d tracks" % n)
        fails += 1
    print("music: %s" % ("FAILED" if fails else "all tracks on the grid"))
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
