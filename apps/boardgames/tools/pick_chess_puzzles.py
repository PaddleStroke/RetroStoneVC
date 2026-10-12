#!/usr/bin/env python3
"""Chess Puzzles' collection: picks about 10,000 puzzles from the Lichess puzzle database and writes them as a packed
C table, src/games/chesspuzzles_data.h (included by chesspuzzles_rules.c only).

    zstd -dc lichess_db_puzzle.csv.zst | python3 -I apps/boardgames/tools/pick_chess_puzzles.py \
        --out apps/boardgames/src/games/chesspuzzles_data.h [--stats stats.txt]

The source: https://database.lichess.org/#puzzles (public domain, CC0: no attribution is required, and the app shows
none). The columns: PuzzleId, FEN, Moves, Rating, RatingDeviation, Popularity, NbPlays, Themes, GameUrl, OpeningTags
(and DailyDate in newer dumps). The file is untrusted data: every field is checked against a strict pattern and a row
that does not match is skipped; nothing in it is ever run or evaluated. The moves' legality is checked by the app's own
chess rules in tests/chesspuzzles_test.c (every puzzle, every move, the mates).

Deterministic: the same CSV gives the same table, byte for byte (no clock, no unseeded randomness; ties are broken by
a hash of the puzzle's id).

The choice:
- the quality filters, strictest first: popularity >= 90, plays >= 2000, rating deviation <= 80 (TIERS). A pack that
  cannot be filled from the strict tier takes the next, looser tier for the rest, and so on;
- no long solutions: at most 8 moves in all (the opponent's first move and up to 7 more: 4 of the player's);
- the player never under-promotes (the app promotes the player's pawns to a queen by itself);
- six packs, the app's variants: five by rating (BANDS, each a quarter from each of four steps of rating: STEPS,
  strict puzzles only; a step short of them leaves the rest to the band's best) and the mates (mate in 1, 2 and 3,
  all ratings);
- the themes spread evenly: in a rating pack the puzzles are taken in turn from one theme's list after another
  (SPREAD), the best of each list first; the mates there only in the mates' own lists (MATE_SPREAD: the most
  popular puzzles are mostly mates, and would crowd out the rest); the mates pack takes them in turn from each
  rating band, for each length;
- each pack's order is then shuffled (a fixed seed), so the next puzzle is never the same kind as the last.

The record of a puzzle (bytes, little-endian), the packs one after the other:
    0      the moves' count (1..8, the opponent's first move included) | the castling rights << 4 (chess.h's CASTLE_*)
    1      the en passant file + 1 (0: none) | the side to move << 7 (1: black; the opponent: the solver is the other)
    2      the rating / 16 (rounded)
    3..6   the themes (THEMES' bits)
    7..10  the Lichess id, base 62 (0-9 A-Z a-z, the first character the lowest digit)
    11..18 the squares holding a piece (bit s: square s, a1 = 0, h8 = 63)
    then   a nibble per piece in square order (the low nibble first): 1..6 white pawn..king, 9..14 black
    then   two bytes a move: from | (promotion & 3) << 6, to | (promotion >> 2) << 6 (0 none, 1 knight, 2 bishop,
           3 rook, 4 queen)
"""
import argparse
import csv
import hashlib
import io
import math
import random
import re
import sys

# ---- the packs ---------------------------------------------------------------------------------------------------
BANDS = [("beginner", 0, 1000), ("easy", 1000, 1400), ("medium", 1400, 1800), ("hard", 1800, 2200),
         ("expert", 2200, 10000)]
# each band in four steps of rating, a quarter of the pack each (else the most played puzzles, the easiest of the
# band, would fill it)
STEPS = [(0, 700, 800, 900, 1000), (1000, 1100, 1200, 1300, 1400), (1400, 1500, 1600, 1700, 1800),
         (1800, 1900, 2000, 2100, 2200), (2200, 2350, 2500, 2650, 10000)]
PER_BAND = 1600
MATES = [("mateIn1", 600), ("mateIn2", 900), ("mateIn3", 500)]   # the mates pack: 2000

# (popularity >=, plays >=, rating deviation <=), strictest first
TIERS = [(90, 2000, 80), (88, 1000, 85), (85, 500, 90), (80, 300, 100)]
MAX_MOVES = 8

# ---- the themes: 32 bits ------------------------------------------------------------------------------------------
MATE_PATTERNS = {"anastasiaMate", "arabianMate", "bodenMate", "doubleBishopMate", "dovetailMate", "hookMate",
                 "killBoxMate", "vukovicMate", "balestraMate", "blindSwineMate", "cornerMate", "morphysMate",
                 "operaMate", "pillsburysMate", "triangleMate", "epauletteMate", "swallowstailMate"}
THEMES = ["mate", "mateIn1", "mateIn2", "mateIn3", "fork", "pin", "skewer", "discoveredAttack", "sacrifice",
          "deflection", "attraction", "clearance", "interference", "hangingPiece", "trappedPiece", "promotion",
          "advancedPawn", "endgame", "middlegame", "opening", "quietMove", "zugzwang", "defensiveMove", "xRayAttack",
          "capturingDefender", "doubleCheck", "backRankMate", "smotheredMate", "matePattern", "kingAttack",
          "intermezzo", "specialMove"]
assert len(THEMES) == 32
ALIASES = {"kingsideAttack": "kingAttack", "queensideAttack": "kingAttack", "exposedKing": "kingAttack",
           "enPassant": "specialMove", "castling": "specialMove", "underPromotion": "specialMove",
           "mateIn4": "mate", "mateIn5": "mate"}
for p in MATE_PATTERNS:
    ALIASES[p] = "matePattern"
for e in ("rookEndgame", "pawnEndgame", "queenEndgame", "bishopEndgame", "knightEndgame", "queenRookEndgame"):
    ALIASES[e] = "endgame"

# the rating packs take their puzzles in turn from these lists (Lichess's own theme names; "matePattern": any of the
# named mates above)
SPREAD = ["fork", "pin", "skewer", "discoveredAttack", "sacrifice", "deflection", "attraction", "clearance",
          "interference", "hangingPiece", "trappedPiece", "promotion", "advancedPawn", "quietMove", "zugzwang",
          "defensiveMove", "xRayAttack", "capturingDefender", "doubleCheck", "intermezzo", "exposedKing",
          "kingsideAttack", "endgame", "rookEndgame", "pawnEndgame", "mateIn1", "mateIn2", "mateIn3", "backRankMate",
          "smotheredMate", "matePattern", "enPassant", "castling"]
MATE_SPREAD = {"mateIn1", "mateIn2", "mateIn3", "backRankMate", "smotheredMate", "matePattern"}

# ---- the strict patterns of the fields ---------------------------------------------------------------------------
RE_ID = re.compile(r"^[0-9A-Za-z]{5}$")
RE_RANK = re.compile(r"^[pnbrqkPNBRQK1-8]{1,8}$")
RE_MOVE = re.compile(r"^([a-h][1-8])([a-h][1-8])([qrbn]?)$")
RE_INT = re.compile(r"^-?[0-9]{1,7}$")
RE_THEME = re.compile(r"^[A-Za-z0-9]{1,40}$")
PIECES = "PNBRQK"
B62 = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
PROMO = {"": 0, "n": 1, "b": 2, "r": 3, "q": 4}


def square(s):
    return (ord(s[0]) - 97) + (ord(s[1]) - 49) * 8


def parse_fen(fen):
    """The board as {square: nibble}, the side to move (0 white, 1 black), the castling bits, the en passant file + 1;
    None when it is not a plain FEN."""
    parts = fen.split(" ")
    if len(parts) != 6:
        return None
    rows = parts[0].split("/")
    if len(rows) != 8:
        return None
    board = {}
    kings = [0, 0]
    for i, row in enumerate(rows):
        if not RE_RANK.match(row):
            return None
        rank, f = 7 - i, 0
        for c in row:
            if c.isdigit():
                f += int(c)
                continue
            if f > 7:
                return None
            color = 1 if c.islower() else 0
            t = PIECES.index(c.upper()) + 1
            if t == 6:
                kings[color] += 1
            board[rank * 8 + f] = t | color << 3
            f += 1
        if f != 8:
            return None
    if kings != [1, 1] or parts[1] not in ("w", "b"):
        return None
    castle = 0
    if parts[2] != "-":
        if not re.match(r"^K?Q?k?q?$", parts[2]) or not parts[2]:
            return None
        for c, bit in (("K", 1), ("Q", 2), ("k", 4), ("q", 8)):
            if c in parts[2]:
                castle |= bit
    ep = 0
    if parts[3] != "-":
        if not re.match(r"^[a-h][36]$", parts[3]):
            return None
        ep = ord(parts[3][0]) - 96
    if not RE_INT.match(parts[4]) or not RE_INT.match(parts[5]):
        return None
    return board, (1 if parts[1] == "b" else 0), castle, ep


def theme_bits(themes):
    bits = 0
    for t in themes:
        t = ALIASES.get(t, t)
        if t in THEMES:
            bits |= 1 << THEMES.index(t)
    if themes & {"mateIn1", "mateIn2", "mateIn3", "mateIn4", "mateIn5"}:
        bits |= 1 << THEMES.index("mate")
    return bits


def b62(pid):
    v = 0
    for c in reversed(pid):
        v = v * 62 + B62.index(c)
    return v


def tiebreak(pid):
    return hashlib.sha1(("rsos-chess-puzzles:" + pid).encode()).hexdigest()


class Puzzle:
    __slots__ = ("pid", "fen", "moves", "rating", "rd", "pop", "plays", "themes", "tier", "score", "key")


def read(stream, stats):
    """Every row passing the loosest tier and the shape rules, as Puzzles."""
    loose = TIERS[-1]
    out = []
    reader = csv.reader(stream)
    head = next(reader, None)
    if not head:
        sys.exit("empty CSV")
    col = {name: i for i, name in enumerate(head)}
    need = ["PuzzleId", "FEN", "Moves", "Rating", "RatingDeviation", "Popularity", "NbPlays", "Themes"]
    for n in need:
        if n not in col:
            sys.exit("missing column " + n)
    for row in reader:
        stats["rows"] += 1
        if len(row) < len(head) - 1:
            stats["bad"] += 1
            continue
        try:
            pid, fen, moves, rating, rd, pop, plays, themes = (row[col[n]] for n in need)
        except IndexError:
            stats["bad"] += 1
            continue
        if not (RE_ID.match(pid) and RE_INT.match(rating) and RE_INT.match(rd) and RE_INT.match(pop)
                and RE_INT.match(plays)):
            stats["bad"] += 1
            continue
        rating, rd, pop, plays = int(rating), int(rd), int(pop), int(plays)
        if pop < loose[0] or plays < loose[1] or rd > loose[2]:
            continue
        ms = moves.split(" ")
        if not 2 <= len(ms) <= MAX_MOVES or not all(RE_MOVE.match(m) for m in ms):
            stats["long" if len(ms) > MAX_MOVES else "bad"] += 1
            continue
        if any(RE_MOVE.match(m).group(3) not in ("", "q") for m in ms[1::2]):
            stats["underpromotion"] += 1        # the player's own under-promotions: left out
            continue
        th = set(themes.split(" ")) if themes else set()
        if not all(RE_THEME.match(t) for t in th):
            stats["bad"] += 1
            continue
        if len(fen) > 100 or parse_fen(fen) is None:
            stats["bad"] += 1
            continue
        p = Puzzle()
        p.pid, p.fen, p.moves, p.rating, p.rd, p.pop, p.plays, p.themes = pid, fen, ms, rating, rd, pop, plays, th
        p.tier = next(i for i, (a, b, c) in enumerate(TIERS) if pop >= a and plays >= b and rd <= c)
        p.score = pop + 3 * math.log10(plays) - 0.05 * rd
        p.key = (-p.score, tiebreak(pid))
        out.append(p)
    return out


def has(p, theme):
    if theme == "matePattern":
        return bool(p.themes & MATE_PATTERNS)
    return theme in p.themes


def round_robin(lists, target, taken):
    """Take from each list in turn (each sorted best first), skipping the puzzles already taken, until `target` are
    taken or every list is empty."""
    got, pos = [], [0] * len(lists)
    while len(got) < target:
        moved = False
        for i, lst in enumerate(lists):
            while pos[i] < len(lst) and lst[pos[i]].pid in taken:
                pos[i] += 1
            if pos[i] < len(lst):
                p = lst[pos[i]]
                taken.add(p.pid)
                got.append(p)
                moved = True
                if len(got) >= target:
                    break
        if not moved:
            break
    return got


def pick_tiered(pool, make_lists, target, taken, tiers=len(TIERS)):
    """The strict tier first; the next tiers (up to `tiers`) only for what is still missing."""
    got = []
    for tier in range(tiers):
        if len(got) >= target:
            break
        sub = [p for p in pool if p.tier <= tier]
        got += round_robin(make_lists(sub), target - len(got), taken)
    return got


def band_of(rating):
    for i, (_, lo, hi) in enumerate(BANDS):
        if lo <= rating < hi:
            return i
    return len(BANDS) - 1


def choose(puzzles):
    puzzles.sort(key=lambda p: p.key)
    taken = set()
    packs = []
    for b, (_, lo, hi) in enumerate(BANDS):
        pool = [p for p in puzzles if lo <= p.rating < hi]

        def lists(sub):                         # the mates only in the mates' own lists: they would crowd the rest
            ls = [[p for p in sub if has(p, t) and (t in MATE_SPREAD or "mate" not in p.themes)] for t in SPREAD]
            return ls + [[p for p in sub if "mate" not in p.themes]]   # then any other puzzle, the best first
        got = []
        for s in range(4):
            a, z = STEPS[b][s], STEPS[b][s + 1]
            got += pick_tiered([p for p in pool if a <= p.rating < z], lists, PER_BAND // 4, taken, 1)
        got += pick_tiered(pool, lists, PER_BAND - len(got), taken)   # the steps short: the band's best
        packs.append(got)
    mates = []
    for theme, target in MATES:
        pool = [p for p in puzzles if theme in p.themes]

        def lists(sub):
            return [[p for p in sub if band_of(p.rating) == b] for b in range(len(BANDS))]
        mates += pick_tiered(pool, lists, target, taken)
    packs.append(mates)
    for i, pack in enumerate(packs):            # a fixed order: by id, then shuffled with the pack's own seed
        pack.sort(key=lambda p: p.pid)
        random.Random(1000 + i).shuffle(pack)
    return packs


# ---- the record -------------------------------------------------------------------------------------------------
def record(p):
    board, side, castle, ep = parse_fen(p.fen)
    out = bytearray()
    out.append(len(p.moves) | castle << 4)
    out.append(ep | side << 7)
    out.append(min(255, (p.rating + 8) // 16))
    out += theme_bits(p.themes).to_bytes(4, "little")
    out += b62(p.pid).to_bytes(4, "little")
    occ = 0
    for s in board:
        occ |= 1 << s
    out += occ.to_bytes(8, "little")
    nib = [board[s] for s in sorted(board)]
    if len(nib) % 2:
        nib.append(0)
    for i in range(0, len(nib), 2):
        out.append(nib[i] | nib[i + 1] << 4)
    for m in p.moves:
        g = RE_MOVE.match(m)
        pr = PROMO[g.group(3)]
        out.append(square(g.group(1)) | (pr & 3) << 6)
        out.append(square(g.group(2)) | (pr >> 2) << 6)
    return bytes(out)


def c_string(data):
    """The bytes as C string literals (printable characters as they are, the others as 3-digit octal escapes), 100
    characters a line."""
    lines, cur = [], ""
    for b in data:
        c = chr(b)
        if 32 <= b < 127 and c not in '\\"?':
            piece = c
        else:
            piece = "\\%03o" % b
        cur += piece
        if len(cur) >= 100:
            lines.append('    "' + cur + '"')
            cur = ""
    if cur:
        lines.append('    "' + cur + '"')
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--csv", help="the decompressed CSV (default: standard input)")
    ap.add_argument("--out", required=True, help="the C header to write")
    ap.add_argument("--stats", help="also write the statistics there")
    a = ap.parse_args()
    csv.field_size_limit(1 << 16)
    stats = {"rows": 0, "bad": 0, "long": 0, "underpromotion": 0}
    if a.csv:
        with open(a.csv, newline="", encoding="utf-8") as f:
            puzzles = read(f, stats)
    else:
        puzzles = read(io.TextIOWrapper(sys.stdin.buffer, encoding="utf-8", newline=""), stats)
    packs = choose(puzzles)
    names = [n for n, _, _ in BANDS] + ["mates"]
    blob = bytearray()
    for pack in packs:
        for p in pack:
            blob += record(p)

    total = sum(len(p) for p in packs)
    rep = ["rows read %d, malformed %d, too long %d, under-promotion %d, candidates %d" %
           (stats["rows"], stats["bad"], stats["long"], stats["underpromotion"], len(puzzles)),
           "puzzles %d, data %d bytes (%.1f a puzzle)" % (total, len(blob), len(blob) / max(1, total))]
    for n, pack in zip(names, packs):
        tiers = [sum(1 for p in pack if p.tier == t) for t in range(len(TIERS))]
        rs = sorted(p.rating for p in pack)
        rep.append("  %-9s %5d  ratings %4d..%4d  by tier %s" % (n, len(pack), rs[0] if rs else 0,
                                                                rs[-1] if rs else 0, tiers))
    allp = [p for pack in packs for p in pack]
    counts = {}
    for p in allp:
        for t in p.themes:
            counts[t] = counts.get(t, 0) + 1
    rep.append("themes (Lichess's names, a puzzle counts in each of its themes):")
    for t, c in sorted(counts.items(), key=lambda kv: (-kv[1], kv[0])):
        rep.append("  %-20s %5d" % (t, c))
    rep.append("the table's theme bits by pack (%s):" % " ".join(n[:6] for n in names))
    for i, t in enumerate(THEMES):
        row = [sum(1 for p in pack if theme_bits(p.themes) >> i & 1) for pack in packs]
        rep.append("  %-18s %s" % (t, " ".join("%6d" % c for c in row)))
    report = "\n".join(rep)
    print(report, file=sys.stderr)
    if a.stats:
        with open(a.stats, "w") as f:
            f.write(report + "\n")

    with open(a.out, "w", newline="\n") as f:
        f.write("/* Chess Puzzles' collection: %d puzzles from the Lichess puzzle database (public domain, CC0),\n"
                " * %d bytes. Generated by apps/boardgames/tools/pick_chess_puzzles.py (the record's layout is\n"
                " * described there): do not edit. */\n" % (total, len(blob)))
        f.write("#ifndef CHESSPUZZLES_DATA_H\n#define CHESSPUZZLES_DATA_H\n\n#include <stdint.h>\n\n")
        for i, t in enumerate(THEMES):
            f.write("#define CPZ_TH_%s (1u << %d)\n" % (re.sub(r"(?<!^)([A-Z])", r"_\1", t).upper(), i))
        f.write("\n#define CPZ_PACKS %d\n#define CPZ_TOTAL %d\n#define CPZ_DATA_BYTES %d\n"
                % (len(packs), total, len(blob)))
        f.write("static const uint16_t cpz_pack_size[CPZ_PACKS] = {%s};\n" % ", ".join(str(len(p)) for p in packs))
        f.write("static const uint8_t cpz_data[CPZ_DATA_BYTES + 1] =\n")
        f.write(c_string(blob))
        f.write(";\n\n#endif\n")


if __name__ == "__main__":
    main()
