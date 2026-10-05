#!/usr/bin/env python3
"""Generate sdk/src/rs_font_ext.c: the house 5x7 font beyond ASCII (docs/i18n.md "Fonts").

The house font (sdk/tools/gen_font.py) has capitals only, 5x7 in an 8x8 cell (columns 1..5, rows 0..6; row 7
holds the shadow). This adds, in the same style:
  - the accented capitals of Latin-1 and Latin Extended-A, built from a base letter and a MARK: the marks above
    (acute, grave, circumflex, tilde, diaeresis, ring, caron, breve, dot, double acute) are drawn in the bottom rows
    of the tile ABOVE the letter (the letter stays exactly the ASCII one, the accent sits over the cap line); the
    marks below (cedilla, ogonek) in the letter's own row 7;
  - the letters that are not a base plus a mark (AE, O slash, OE, L stroke, inverted ! and ?, ordinal o and a),
  - Cyrillic capitals (Russian, Ukrainian) and Greek capitals, reusing the Latin shapes where they are the same.
Lowercase letters map to the capitals (as in ASCII). Greek drops its accents (tonos) in capitals, as Greek does.

The glyphs below are original (drawn for RetroStone VC), MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
Run: python3 sdk/tools/gen_font_ext.py > sdk/src/rs_font_ext.c
     python3 sdk/tools/gen_font_ext.py --preview out.png      (a sheet of every glyph, 4x)
"""
import sys
import unicodedata

# ---- the marks: 8 rows of the 8x8 tile, 5 columns (1..5) -----------------------------------------------------------
# above: drawn in the tile above the letter, ink in rows 4..6 (row 7 is the gap, with the mark's shadow)
# below: drawn in the letter's own tile, row 7
MARKS = [
    None,                                                       # 0: none
    ("acute", 0x0301, {5: "   # ", 6: "  #  "}),
    ("grave", 0x0300, {5: " #   ", 6: "  #  "}),
    ("circumflex", 0x0302, {5: "  #  ", 6: " # # "}),
    ("tilde", 0x0303, {5: " ## #", 6: "#  # "}),
    ("diaeresis", 0x0308, {6: " # # "}),
    ("ring", 0x030A, {4: "  #  ", 5: " # # ", 6: "  #  "}),
    ("caron", 0x030C, {5: " # # ", 6: "  #  "}),
    ("breve", 0x0306, {5: "#   #", 6: " ### "}),
    ("dot", 0x0307, {6: "  #  "}),
    ("double acute", 0x030B, {5: "  # #", 6: " # # "}),
    ("cedilla", 0x0327, {7: "  #  "}),
    ("ogonek", 0x0328, {7: "   # "}),
]
MARK_OF = {m[1]: i for i, m in enumerate(MARKS) if m}
BELOW = {MARK_OF[0x0327], MARK_OF[0x0328]}

# ---- the letters that are not ASCII shapes -------------------------------------------------------------------------
E = {
    # Latin
    0x00C6: ["#####", "# #  ", "# #  ", "#####", "# #  ", "# #  ", "# ###"],     # AE
    0x00D8: [" ### ", "#  ##", "# # #", "# # #", "# # #", "##  #", " ### "],     # O slash
    0x0152: [" ####", "# #  ", "# #  ", "# ###", "# #  ", "# #  ", " ####"],     # OE
    0x0141: ["#    ", "#    ", "# #  ", "##   ", "#    ", "#    ", "#####"],     # L stroke
    0x0110: ["###  ", "#  # ", "#   #", "### #", "#   #", "#  # ", "###  "],     # D stroke
    0x00D0: ["###  ", "#  # ", "#   #", "### #", "#   #", "#  # ", "###  "],     # Eth
    0x00A1: ["  #  ", "     ", "  #  ", "  #  ", "  #  ", "  #  ", "  #  "],     # inverted !
    0x00BF: ["  #  ", "     ", "  #  ", " #   ", "#    ", "#   #", " ### "],     # inverted ?
    0x00BA: ["  #  ", " # # ", "  #  ", "     ", " ### ", "     ", "     "],     # ordinal o
    0x00AA: [" ##  ", "  #  ", " ##  ", "     ", " ### ", "     ", "     "],     # ordinal a
    0x00AB: ["     ", "  # #", " # # ", "# #  ", " # # ", "  # #", "     "],     # <<
    0x00BB: ["     ", "# #  ", " # # ", "  # #", " # # ", "# #  ", "     "],     # >>
    0x00B7: ["     ", "     ", "     ", "  #  ", "     ", "     ", "     "],     # middle dot
    0x00D7: ["     ", "#   #", " # # ", "  #  ", " # # ", "#   #", "     "],     # multiplication x
    # Cyrillic
    0x0411: ["#####", "#    ", "#    ", "#### ", "#   #", "#   #", "#### "],     # Be
    0x0413: ["#####", "#    ", "#    ", "#    ", "#    ", "#    ", "#    "],     # Ghe
    0x0490: ["    #", "#####", "#    ", "#    ", "#    ", "#    ", "#    "],     # Ghe with upturn
    0x0414: ["  ###", " #  #", " #  #", " #  #", " #  #", "#####", "#   #"],     # De
    0x0416: ["# # #", "# # #", " ### ", "  #  ", " ### ", "# # #", "# # #"],     # Zhe
    0x0417: [" ### ", "#   #", "    #", "  ## ", "    #", "#   #", " ### "],     # Ze
    0x0418: ["#   #", "#   #", "#  ##", "# # #", "##  #", "#   #", "#   #"],     # I
    0x041B: ["  ###", " #  #", " #  #", " #  #", " #  #", " #  #", "#   #"],     # El
    0x041F: ["#####", "#   #", "#   #", "#   #", "#   #", "#   #", "#   #"],     # Pe
    0x0423: ["#   #", "#   #", "#   #", " ####", "    #", "#   #", " ### "],     # U
    0x0424: ["  #  ", " ### ", "# # #", "# # #", "# # #", " ### ", "  #  "],     # Ef
    0x0426: ["#  # ", "#  # ", "#  # ", "#  # ", "#  # ", "#####", "    #"],     # Tse
    0x0427: ["#   #", "#   #", "#   #", " ####", "    #", "    #", "    #"],     # Che
    0x0428: ["# # #", "# # #", "# # #", "# # #", "# # #", "# # #", "#####"],     # Sha
    0x0429: ["# # #", "# # #", "# # #", "# # #", "# # #", "#####", "    #"],     # Shcha
    0x042A: ["##   ", " #   ", " #   ", " ### ", " #  #", " #  #", " ### "],     # hard sign
    0x042B: ["#   #", "#   #", "#   #", "##  #", "# # #", "# # #", "##  #"],     # Yeru
    0x042C: ["#    ", "#    ", "#    ", "#### ", "#   #", "#   #", "#### "],     # soft sign
    0x042D: [" ### ", "#   #", "    #", " ####", "    #", "#   #", " ### "],     # E
    0x042E: ["#  # ", "# # #", "# # #", "### #", "# # #", "# # #", "#  # "],     # Yu
    0x042F: [" ####", "#   #", "#   #", " ####", "  # #", " #  #", "#   #"],     # Ya
    0x0404: [" ### ", "#   #", "#    ", "#### ", "#    ", "#   #", " ### "],     # Ukrainian Ie
    # Greek
    0x0394: ["  #  ", "  #  ", " # # ", " # # ", "#   #", "#   #", "#####"],     # Delta
    0x0398: [" ### ", "#   #", "#   #", "# # #", "#   #", "#   #", " ### "],     # Theta
    0x039B: ["  #  ", " # # ", " # # ", "#   #", "#   #", "#   #", "#   #"],     # Lambda
    0x039E: ["#####", "     ", "     ", " ### ", "     ", "     ", "#####"],     # Xi
    0x03A3: ["#####", "#    ", " #   ", "  #  ", " #   ", "#    ", "#####"],     # Sigma
    0x03A8: ["# # #", "# # #", "# # #", " ### ", "  #  ", "  #  ", "  #  "],     # Psi
    0x03A9: [" ### ", "#   #", "#   #", "#   #", "#   #", " # # ", "## ##"],     # Omega
}
# letters drawn like another one (same shape): Cyrillic and Greek capitals that are Latin ones, and copies
SAME = {
    0x0410: 'A', 0x0412: 'B', 0x0415: 'E', 0x041A: 'K', 0x041C: 'M', 0x041D: 'H', 0x041E: 'O', 0x0420: 'P',
    0x0421: 'C', 0x0422: 'T', 0x0425: 'X', 0x0406: 'I', 0x0408: 'J', 0x0405: 'S',
    0x0391: 'A', 0x0392: 'B', 0x0395: 'E', 0x0396: 'Z', 0x0397: 'H', 0x0399: 'I', 0x039A: 'K', 0x039C: 'M',
    0x039D: 'N', 0x039F: 'O', 0x03A1: 'P', 0x03A4: 'T', 0x03A5: 'Y', 0x03A7: 'X',
    0x0393: 0x0413, 0x03A0: 0x041F, 0x03A6: 0x0424,   # Gamma = Ghe, Pi = Pe, Phi = Ef
    0x0131: 'I',                                      # dotless i
    0x00DF: 'S',                                      # sharp s alone (the build tool writes SS)
}

RANGES = [(0x00A0, 0x0180), (0x0386, 0x03D0), (0x0400, 0x0460), (0x0490, 0x0492)]


def upper(cp):
    """The capital the font shows for cp (one code point)."""
    if cp == 0x0131 or cp == 0x00DF:
        return cp
    u = chr(cp).upper()
    if cp == 0x03C2:            # final sigma
        u = "Σ"
    return ord(u) if len(u) == 1 else cp


def shape(cp):
    """('ascii', index) or ('ext', cp) for a code point without marks, else None."""
    if cp < 128 and 32 <= cp:
        return ('ascii', cp - 32)
    if cp in E:
        return ('ext', cp)
    if cp in SAME:
        s = SAME[cp]
        return shape(ord(s) if isinstance(s, str) else s)
    return None


def compose(cp):
    """(shape, above mark, below mark) or None."""
    u = upper(cp)
    s = shape(u)
    if s:
        return s, 0, 0
    d = unicodedata.normalize("NFD", chr(u))
    base, marks = ord(d[0]), [ord(c) for c in d[1:]]
    greek = 0x0370 <= u < 0x0400
    if greek:
        marks = [m for m in marks if m != 0x0301]       # capitals drop the tonos
    s = shape(base)
    if not s or len(marks) > 2:
        return None
    above = below = 0
    for m in marks:
        if m not in MARK_OF:
            return None
        k = MARK_OF[m]
        if k in BELOW:
            if below:
                return None
            below = k
        else:
            if above:
                return None
            above = k
    return s, above, below


def rows_of(g):
    assert len(g) == 7 and all(len(r) == 5 for r in g), g
    out = []
    for r in g + ["     "]:
        v = 0
        for i, c in enumerate(r):
            if c == '#':
                v |= 0x80 >> (i + 1)
        out.append(v)
    return out


def mark_rows(m):
    out = [0] * 8
    for y, r in m[2].items():
        for i, c in enumerate(r):
            if c == '#':
                out[y] |= 0x80 >> (i + 1)
    return out


def table():
    ext = sorted(E)
    ext_index = {cp: 96 + i for i, cp in enumerate(ext)}
    entries = []
    for lo, hi in RANGES:
        for cp in range(lo, hi):
            if cp == 0x00A0:
                entries.append((cp, 0, 0, 0))            # no-break space = space
                continue
            c = compose(cp)
            if not c:
                continue
            (kind, v), above, below = c
            body = v if kind == 'ascii' else ext_index[v]
            entries.append((cp, body, above, below))
    return ext, entries


def emit():
    ext, entries = table()
    print("/* Generated by sdk/tools/gen_font_ext.py: the RetroStone VC house font beyond ASCII (docs/i18n.md).")
    print(" * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft). */")
    print('#include "rs_i18n.h"\n')
    print("/* The letters that are not ASCII shapes: 8 rows each, bit 7 = leftmost pixel (like rs_font_5x7). */")
    print("static const uint8_t ext_glyphs[%d][8] = {" % len(ext))
    for cp in ext:
        print("    {%s}, /* U+%04X */" % (", ".join("0x%02x" % v for v in rows_of(E[cp])), cp))
    print("};\n")
    print("/* The marks (RS_MARK_*): above = the bottom rows of the tile above the letter, below = row 7 of its own. */")
    print("static const uint8_t mark_rows[%d][8] = {" % len(MARKS))
    for m in MARKS:
        r = mark_rows(m) if m else [0] * 8
        print("    {%s}, /* %s */" % (", ".join("0x%02x" % v for v in r), m[0] if m else "none"))
    print("};\n")
    print("/* Every code point the font draws beyond ASCII, sorted: body (0..95 = rs_font_5x7, 96.. = ext_glyphs),")
    print(" * the mark above (low nibble) and the mark below (high nibble). */")
    print("static const struct { uint16_t cp; uint8_t body, marks; } ext_map[%d] = {" % len(entries))
    line = "   "
    for cp, body, above, below in entries:
        item = " {0x%04x, %d, 0x%02x}," % (cp, body, above | below << 4)
        if len(line) + len(item) > 118:
            print(line)
            line = "   "
        line += item
    print(line)
    print("};\n")
    print("""extern const uint8_t rs_font_5x7[96][8];

int rs_font_ext(uint32_t cp, const uint8_t **body, int *above, int *below)
{
    int lo = 0, hi = (int)(sizeof ext_map / sizeof ext_map[0]) - 1;
    if (cp >= 32 && cp < 128) {
        if (body) *body = rs_font_5x7[cp - 32];
        if (above) *above = 0;
        if (below) *below = 0;
        return 1;
    }
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (ext_map[mid].cp == cp) {
            int b = ext_map[mid].body;
            if (body) *body = b < 96 ? rs_font_5x7[b] : ext_glyphs[b - 96];
            if (above) *above = ext_map[mid].marks & 15;
            if (below) *below = ext_map[mid].marks >> 4;
            return 1;
        }
        if (ext_map[mid].cp < cp) lo = mid + 1;
        else hi = mid - 1;
    }
    return 0;
}

const uint8_t *rs_font_mark(int mark)
{
    return mark > 0 && mark < RS_MARK_COUNT ? mark_rows[mark] : mark_rows[0];
}

int rs_font_ext_base(uint32_t cp)
{
    int lo = 0, hi = (int)(sizeof ext_map / sizeof ext_map[0]) - 1;
    if (cp >= 32 && cp < 128) return (int)cp;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (ext_map[mid].cp == cp) return ext_map[mid].body < 96 ? ext_map[mid].body + 32 : '?';
        if (ext_map[mid].cp < cp) lo = mid + 1;
        else hi = mid - 1;
    }
    return '?';
}""")


def preview(path):
    from PIL import Image
    sys.path.insert(0, __file__.rsplit("/", 1)[0])
    import gen_font
    ext, entries = table()
    asc = {}
    for code in range(32, 128):
        ch = chr(code).upper() if 'a' <= chr(code) <= 'z' else chr(code)
        asc[code - 32] = rows_of(gen_font.G[ch])
    caps = [e for e in entries if upper(e[0]) == e[0]]
    cols = 32
    rows = (len(caps) + cols - 1) // cols
    img = Image.new("RGB", (cols * 10 + 4, rows * 18 + 4), (42, 68, 118))
    px = img.load()
    for i, (cp, body, above, below) in enumerate(caps):
        ox, oy = 2 + (i % cols) * 10, 2 + (i // cols) * 18 + 8
        b = asc[body] if body < 96 else rows_of(E[ext[body - 96]])
        ink = set()
        for y in range(8):
            for x in range(8):
                if b[y] & (0x80 >> x):
                    ink.add((x, y))
        if above:
            m = mark_rows(MARKS[above])
            for y in range(8):
                for x in range(8):
                    if m[y] & (0x80 >> x):
                        ink.add((x, y - 8))
        if below:
            m = mark_rows(MARKS[below])
            for x in range(8):
                if m[7] & (0x80 >> x):
                    ink.add((x, 7))
        for (x, y) in ink:
            if (x + 1, y + 1) not in ink:
                px[ox + x + 1, oy + y + 1] = (16, 24, 56)
        for (x, y) in ink:
            px[ox + x, oy + y] = (255, 246, 220)
    img = img.resize((img.width * 4, img.height * 4), Image.NEAREST)
    img.save(path)


if __name__ == "__main__":
    if len(sys.argv) > 2 and sys.argv[1] == "--preview":
        preview(sys.argv[2])
    else:
        emit()
