#!/usr/bin/env python3
"""Map Quiz's maps, made from Natural Earth (public domain, naturalearthdata.com): src/games/mapquiz_data.h (the
outlines, the regions, the levels) and help/<lang>/mapquiz_words.txt (the prompt and every place's name).

    python3 -I apps/boardgames/tools/mapquiz_gen.py \\
        --a0 <dir>/ne_50m_admin_0_countries --a1 <dir>/ne_10m_admin_1_states_provinces --app apps/boardgames

--a0 / --a1: the two shapefiles without their extension (ne_50m_admin_0_countries.zip and
ne_10m_admin_1_states_provinces.zip unpacked; .shp and .dbf read here by a small reader of our own, standard library
only). Natural Earth itself stays out of the repository: only what this tool makes is kept. Deterministic: the same
files give the same output.

- The geometry is the admin-1 file's (states and provinces, 1:10m) for every map: a country is its states put
  together, so every border matches its neighbours' and a disputed part (Crimea, Kashmir, Abkhazia, Transnistria...)
  can be set apart. The admin-0 file (1:50m) gives the countries' names, kinds and continents.
- Disputed areas (DISPUTED below) are drawn as they are held in fact, in neutral grey, never asked and never named.
- Each map: a projection (Robinson for the world; Lambert's equal-area or Albers' conic for a continent or a country),
  a window, insets (Alaska and Hawaii, the Canaries, Okinawa); the outlines projected onto a grid of 16000 at most,
  clipped to the window, made into arcs shared by the regions on both sides (a border is one line, simplified once:
  no gaps between neighbours), each arc simplified (Douglas-Peucker, the map's tolerance), islands too small to see
  dropped (a region keeps at least its biggest piece; the game marks small regions with a circle).
- A region: its name (a line of mapquiz_words.txt), its kind (asked, other land, disputed), its tone (neighbours
  differ), its label point and area. The levels (LEVELS below) name their regions by code.
MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
"""
import argparse
import math
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mapquiz_names as NM  # noqa: E402

LANGS = NM.LANGS
S = 16000                                       # the grid's longest side

# ---- reading Natural Earth ---------------------------------------------------------------------------------------


def read_dbf(path):
    with open(path, 'rb') as f:
        data = f.read()
    n, hlen, rlen = struct.unpack('<IHH', data[4:12])
    fields = []
    p = 32
    while data[p] != 0x0D:
        name = data[p:p + 11].split(b'\0')[0].decode('ascii').upper()
        fields.append((name, chr(data[p + 11]), data[p + 16]))
        p += 32
    recs = []
    for i in range(n):
        r = data[hlen + i * rlen: hlen + (i + 1) * rlen]
        o = 1
        rec = {}
        for name, ftype, flen in fields:
            s = r[o:o + flen].replace(b'\0', b' ').decode('utf-8', 'replace').strip()
            o += flen
            if ftype in 'NF':
                try:
                    rec[name] = float(s) if s else None
                except ValueError:
                    rec[name] = None
            else:
                rec[name] = ' '.join(s.split())
        recs.append(rec)
    return recs


def read_shp(path):
    """each record: its rings, each a list of (lon, lat)"""
    with open(path, 'rb') as f:
        data = f.read()
    p = 100
    out = []
    while p < len(data):
        _, clen = struct.unpack('>II', data[p:p + 8])
        p += 8
        st = struct.unpack('<i', data[p:p + 4])[0]
        rings = []
        if st in (5, 15, 25):
            nparts, npts = struct.unpack('<ii', data[p + 36:p + 44])
            parts = list(struct.unpack('<%di' % nparts, data[p + 44:p + 44 + 4 * nparts])) + [npts]
            q = p + 44 + 4 * nparts
            pts = struct.unpack('<%dd' % (2 * npts), data[q:q + 16 * npts])
            for k in range(nparts):
                a, b = parts[k], parts[k + 1]
                ring = [(pts[2 * j], pts[2 * j + 1]) for j in range(a, b)]
                if len(ring) > 1 and ring[0] == ring[-1]:
                    ring.pop()
                if len(ring) >= 3:
                    rings.append(ring)
        if rings:                               # an outline turning the wrong way (Ceuta's): all of it turned
            big = max(rings, key=lambda q: abs(area2(q)))
            if area2(big) > 0:
                rings = [q[::-1] for q in rings]
        out.append(rings)
        p += 2 * clen
    return out

# ---- projections (lon, lat in degrees, relative longitude unwrapped by the caller) -------------------------------


def laea(lat0):
    p0 = math.radians(lat0)
    s0, c0 = math.sin(p0), math.cos(p0)

    def f(rl, lat):
        l, p = math.radians(rl), math.radians(lat)
        cp, sp, cl = math.cos(p), math.sin(p), math.cos(l)
        k = math.sqrt(2 / max(1e-9, 1 + s0 * sp + c0 * cp * cl))
        return k * cp * math.sin(l), k * (c0 * sp - s0 * cp * cl)
    return f


def albers(lat0, lat1, lat2):
    p0, p1, p2 = math.radians(lat0), math.radians(lat1), math.radians(lat2)
    n = (math.sin(p1) + math.sin(p2)) / 2
    c = math.cos(p1) ** 2 + 2 * n * math.sin(p1)
    r0 = math.sqrt(c - 2 * n * math.sin(p0)) / n

    def f(rl, lat):
        r = math.sqrt(max(0.0, c - 2 * n * math.sin(math.radians(lat)))) / n
        t = n * math.radians(rl)
        return r * math.sin(t), r0 - r * math.cos(t)
    return f


ROBINSON = [(1.0000, 0.0000), (0.9986, 0.0620), (0.9954, 0.1240), (0.9900, 0.1860), (0.9822, 0.2480),
            (0.9730, 0.3100), (0.9600, 0.3720), (0.9427, 0.4340), (0.9216, 0.4958), (0.8962, 0.5571),
            (0.8679, 0.6176), (0.8350, 0.6769), (0.7986, 0.7346), (0.7597, 0.7903), (0.7186, 0.8435),
            (0.6732, 0.8936), (0.6213, 0.9394), (0.5722, 0.9761), (0.5322, 1.0000)]


def robinson():
    def f(rl, lat):
        a = min(abs(lat), 90.0) / 5
        i = min(int(a), 17)
        t = a - i
        X = ROBINSON[i][0] + (ROBINSON[i + 1][0] - ROBINSON[i][0]) * t
        Y = ROBINSON[i][1] + (ROBINSON[i + 1][1] - ROBINSON[i][1]) * t
        return 0.8487 * X * math.radians(rl), 1.3523 * Y * (1 if lat >= 0 else -1)
    return f


def wrap(d):
    return (d + 180) % 360 - 180


def unwrap_ring(ring, lon0):
    """the ring's longitudes relative to lon0, unwrapped point to point, shifted so the ring sits near 0"""
    out = []
    prev = None
    for lon, lat in ring:
        rl = wrap(lon - lon0)
        if prev is not None:
            while rl - prev > 180:
                rl -= 360
            while rl - prev < -180:
                rl += 360
        out.append((rl, lat))
        prev = rl
    mean = sum(p[0] for p in out) / len(out)
    k = round(mean / 360)
    if k:
        out = [(rl - 360 * k, lat) for rl, lat in out]
    return out

# ---- the places --------------------------------------------------------------------------------------------------


# the sovereign states (the United Nations' members and the Vatican), by Natural Earth's ADM0_A3: the countries asked
STATES = set("""AFG ALB DZA AND AGO ATG ARG ARM AUS AUT AZE BHS BHR BGD BRB BLR BEL BLZ BEN BTN BOL BIH BWA BRA BRN BGR
BFA BDI CPV KHM CMR CAN CAF TCD CHL CHN COL COM COG COD CRI CIV HRV CUB CYP CZE DNK DJI DMA DOM ECU EGY SLV GNQ ERI EST
SWZ ETH FJI FIN FRA GAB GMB GEO DEU GHA GRC GRD GTM GIN GNB GUY HTI HND HUN ISL IND IDN IRN IRQ IRL ISR ITA JAM JPN JOR
KAZ KEN KIR PRK KOR KWT KGZ LAO LVA LBN LSO LBR LBY LIE LTU LUX MDG MWI MYS MDV MLI MLT MHL MRT MUS MEX FSM MDA MCO MNG
MNE MAR MOZ MMR NAM NRU NPL NLD NZL NIC NER NGA MKD NOR OMN PAK PLW PAN PNG PRY PER PHL POL PRT QAT ROU RUS RWA KNA LCA
VCT WSM SMR STP SAU SEN SRB SYC SLE SGP SVK SVN SLB SOM ZAF SDS ESP LKA SDN SUR SWE CHE SYR TJK TZA THA TLS TGO TON TTO
TUN TUR TKM TUV UGA UKR ARE GBR USA URY UZB VUT VEN VNM YEM ZMB ZWE VAT""".split())

# disputed: whole entries of the admin-1 file (their ADM0_A3), and states or provinces of a country (code, NAME):
# grey, never asked, never named; one region each (the parts of one dispute together)
DISPUTED_CODES = {"TWN": "D_TWN", "KOS": "D_KOS", "SAH": "D_SAH", "SOL": "D_SOL", "CYN": "D_CYN", "PSX": "D_PSX",
                  "FLK": "D_FLK", "IOT": "D_IOT", "KAS": "D_KASHMIR", "PGA": "D_SPRATLY"}
DISPUTED_UNITS = {("RUS", "Crimea"): "D_CRIMEA", ("RUS", "Sevastopol"): "D_CRIMEA",
                  ("IND", "Jammu and Kashmir"): "D_KASHMIR", ("IND", "Ladakh"): "D_KASHMIR",
                  ("PAK", "Azad Kashmir"): "D_KASHMIR", ("PAK", "Northern Areas"): "D_KASHMIR",
                  ("GEO", "Abkhazia"): "D_ABKHAZIA",
                  ("MDA", "Stîngă Nistrului"): "D_TRANSNISTRIA", ("MDA", "Transnistria"): "D_TRANSNISTRIA",
                  ("MDA", "Bender"): "D_TRANSNISTRIA", ("MDA", "Grigoriopol"): "D_TRANSNISTRIA",
                  ("MDA", "Camenca"): "D_TRANSNISTRIA",
                  ("CHN", "Paracel Islands"): "D_PARACEL"}
# entries of the admin-1 file that belong to another country here (the Baikonur lease, Åland, the United States' bases
# and minor islands)
SAME_AS = {"KAB": "KAZ", "ALD": "FIN", "USG": "USA", "UMI": "USA"}

K_ASK, K_OTHER, K_DISPUTED = 0, 1, 2

# the continents' countries (the random tab's continent cards and the chapters' last levels): Natural Earth's
# CONTINENT, the open-ocean islands placed by their region, Cyprus in Europe
def continent_sets(a0):
    sets = {"europe": set(), "asia": set(), "africa": set(), "americas": set(), "oceania": set()}
    for r in a0:
        c = r['ADM0_A3']
        if c not in STATES:
            continue
        cont, sub = r['CONTINENT'], r['SUBREGION']
        if c == "CYP":
            sets["europe"].add(c)
        elif cont == "Europe":
            sets["europe"].add(c)
        elif cont == "Asia":
            sets["asia"].add(c)
        elif cont == "Africa" or (cont.startswith("Seven") and "Africa" in sub):
            sets["africa"].add(c)
        elif cont in ("North America", "South America"):
            sets["americas"].add(c)
        elif cont == "Oceania":
            sets["oceania"].add(c)
        elif c == "MDV":
            sets["asia"].add(c)
    return sets

# ---- the maps ----------------------------------------------------------------------------------------------------
# kind "countries": a region a country; "states": the country's states (or groups of them, `group`: a field of the
# admin-1 file), the other countries round it as land not asked. lon0: the projection's middle; box: the window
# (relative longitudes from lon0, latitudes); tol: Douglas-Peucker's tolerance (grid units); insets: (which units,
# their projection's middle and box, where they go: a part of the map's grid x0, y0, x1, y1)


def inset_alaska(u):
    return u['code'] == 'USA' and u['name'] == 'Alaska'


def inset_hawaii(u):
    return u['code'] == 'USA' and u['name'] == 'Hawaii'


def inset_canaries(u):
    return u['code'] == 'ESP' and u['region'] in ('Canary Is.', 'Canarias', 'Islas Canarias')


def inset_okinawa(u):
    return u['code'] == 'JPN' and u['name'] == 'Okinawa'


MAPS = [
    dict(id="world", kind="countries", lon0=0, proj=robinson(), box=(-180, 180, -57, 84), tol=8.0),
    dict(id="europe", kind="countries", lon0=15, proj=laea(52), box=(-38, 28, 34, 71.5), tol=6.0),
    dict(id="asia", kind="countries", lon0=88, proj=laea(28), box=(-62, 60, -11, 56), tol=7.0),
    dict(id="africa", kind="countries", lon0=17, proj=laea(2), box=(-43, 42, -36, 38), tol=5.0),
    dict(id="americas", kind="countries", lon0=-82, proj=laea(15), box=(-86, 52, -56.5, 72), tol=7.0),
    dict(id="oceania", kind="countries", lon0=163, proj=laea(-15), box=(-53, 47, -48, 16), tol=5.0),
    dict(id="usa", kind="states", country="USA", lon0=-96, proj=albers(37.5, 29.5, 45.5), box=(-29, 30, 24, 49.6),
         tol=5.0, insets=[(inset_alaska, -152, albers(50, 55, 65), (-36, 25, 51, 72), (0.0, 0.70, 0.27, 1.0)),
                          (inset_hawaii, -157, laea(20.5), (-4, 3, 18.5, 22.5), (0.27, 0.80, 0.42, 0.98))],
         skip={"District of Columbia"}),
    dict(id="canada", kind="states", country="CAN", lon0=-96, proj=albers(60, 50, 70), box=(-45, 44, 41.5, 83.5),
         tol=8.0),
    dict(id="mexico", kind="states", country="MEX", lon0=-102, proj=laea(23), box=(-16, 15.5, 14, 33), tol=4.0),
    dict(id="brazil", kind="states", country="BRA", lon0=-54, proj=laea(-14), box=(-20, 20, -34.2, 5.6), tol=4.0),
    dict(id="argentina", kind="states", country="ARG", lon0=-64, proj=laea(-39), box=(-10, 11, -55.2, -21.5),
         tol=4.0),
    dict(id="china", kind="states", country="CHN", lon0=105, proj=albers(35, 25, 47), box=(-32, 30, 17.5, 54),
         tol=5.0),
    dict(id="india", kind="states", country="IND", lon0=80, proj=laea(22), box=(-12.5, 18, 6, 37.5), tol=4.0),
    dict(id="russia", kind="states", country="RUS", lon0=100, proj=albers(60, 50, 70), box=(-81, 91, 41, 82),
         tol=7.0),
    dict(id="australia", kind="states", country="AUS", lon0=134, proj=laea(-27), box=(-22, 21, -44, -9.5), tol=4.0,
         skip={"Jervis Bay Territory", "Macquarie Island", "Lord Howe Island"}),
    dict(id="germany", kind="states", country="DEU", lon0=10.4, proj=laea(51.2), box=(-4.8, 4.8, 47.2, 55.1),
         tol=3.5),
    dict(id="france", kind="states", country="FRA", group="REGION", lon0=2.5, proj=laea(46.5),
         box=(-7.5, 7.3, 41.3, 51.2), tol=3.5),
    dict(id="spain", kind="states", country="ESP", group="REGION", lon0=-3.7, proj=laea(40), box=(-5.6, 8.2, 35.1, 44),
         tol=3.5, insets=[(inset_canaries, -15.7, laea(28.3), (-3, 2.6, 27.5, 29.5), (0.0, 0.84, 0.24, 1.0))]),
    dict(id="italy", kind="states", country="ITA", group="REGION", lon0=12.5, proj=laea(42), box=(-6, 6.1, 36.6, 47.1),
         tol=3.5),
    dict(id="japan", kind="states", country="JPN", lon0=137, proj=laea(37), box=(-8.5, 9, 30.8, 45.6), tol=3.5,
         insets=[(inset_okinawa, 127, laea(26.3), (-4.5, 4.5, 24, 28), (0.0, 0.0, 0.33, 0.24))]),
]

# ---- the levels: chapters of levels, each a map and the codes of its regions asked ("*": the map's continent or
# country whole). A chapter: (icon map, [levels]) ---------------------------------------------------------------
LEVELS = [
    ("europe", [
        ("europe", "FRA DEU ITA ESP GBR"),
        ("europe", "FRA DEU ITA ESP GBR POL UKR SWE NOR FIN"),
        ("europe", "PRT ESP FRA BEL NLD LUX DEU CHE AUT IRL GBR"),
        ("europe", "ISL NOR SWE FIN DNK EST LVA LTU"),
        ("europe", "DEU POL CZE SVK HUN AUT CHE SVN LIE"),
        ("europe", "SVN HRV BIH SRB MNE MKD ALB GRC BGR ROU"),
        ("europe", "POL UKR BLR MDA ROU LTU LVA EST RUS HUN"),
        ("europe", "AND MCO SMR VAT LIE LUX MLT CYP"),
        ("europe", "PRT ESP FRA ITA MLT GRC CYP HRV SVN ALB MNE"),
        ("europe", "*"),
    ]),
    ("asia", [
        ("asia", "CHN IND JPN IDN SAU IRN TUR KAZ MNG PAK"),
        ("asia", "CHN MNG PRK KOR JPN"),
        ("asia", "IND PAK BGD NPL BTN LKA MDV AFG"),
        ("asia", "MMR THA LAO KHM VNM MYS SGP IDN PHL BRN TLS"),
        ("asia", "TUR SYR LBN ISR JOR IRQ SAU YEM OMN ARE QAT BHR KWT IRN"),
        ("asia", "KAZ UZB TKM KGZ TJK AFG GEO ARM AZE"),
        ("asia", "*"),
    ]),
    ("africa", [
        ("africa", "EGY DZA NGA ZAF ETH COD KEN MAR LBY SDN"),
        ("africa", "MAR DZA TUN LBY EGY SDN MRT MLI NER TCD"),
        ("africa", "SEN GMB GNB GIN SLE LBR CIV GHA TGO BEN NGA BFA CPV"),
        ("africa", "CMR CAF TCD GAB COG COD GNQ STP AGO SDS"),
        ("africa", "ETH ERI DJI SOM KEN UGA RWA BDI TZA"),
        ("africa", "ZAF NAM BWA ZWE ZMB MOZ MWI LSO SWZ MDG AGO"),
        ("africa", "MDG COM MUS SYC CPV STP"),
        ("africa", "*"),
    ]),
    ("americas", [
        ("americas", "CAN USA MEX BRA ARG CHL PER COL VEN CUB"),
        ("americas", "COL VEN GUY SUR ECU PER BRA BOL PRY CHL ARG URY"),
        ("americas", "MEX GTM BLZ SLV HND NIC CRI PAN"),
        ("americas", "CUB HTI DOM JAM BHS TTO"),
        ("americas", "ATG DMA LCA VCT GRD BRB KNA TTO"),
        ("americas", "*"),
        ("oceania", "AUS NZL PNG FJI SLB VUT"),
        ("oceania", "*"),
    ]),
    ("world", [
        ("world", "RUS CAN USA CHN BRA AUS IND ARG KAZ DZA COD SAU MEX IDN"),
        ("world", "CHN IND USA IDN PAK NGA BRA BGD RUS MEX JPN ETH PHL EGY VNM"),
        ("world", "GBR IRL ISL JPN NZL PHL IDN MDG CUB LKA"),
        ("world", "*"),
    ]),
    ("usa", [
        ("usa", "US-WA US-OR US-CA US-NV US-ID US-MT US-WY US-UT US-CO US-AZ US-NM US-AK US-HI"),
        ("usa", "US-ND US-SD US-NE US-KS US-MN US-IA US-MO US-WI US-IL US-MI US-IN US-OH"),
        ("usa", "US-TX US-OK US-AR US-LA US-MS US-AL US-TN US-KY US-GA US-FL US-SC US-NC US-VA US-WV"),
        ("usa", "US-ME US-NH US-VT US-MA US-RI US-CT US-NY US-NJ US-PA US-DE US-MD"),
        ("usa", "*"),
        ("canada", "*"),
        ("mexico", "MX-BCN MX-BCS MX-SON MX-CHH MX-COA MX-NLE MX-TAM MX-SIN MX-DUR MX-ZAC MX-SLP MX-NAY"),
        ("mexico", "*"),
    ]),
    ("germany", [
        ("germany", "*"),
        ("france", "*"),
        ("spain", "*"),
        ("italy", "*"),
    ]),
    ("china", [
        ("china", "CN-XJ CN-XZ CN-NM CN-QH CN-SC CN-HL CN-GS CN-YN CN-GX CN-GD"),
        ("china", "*"),
        ("india", "IN-RJ IN-MP IN-MH IN-UP IN-GJ IN-KA IN-AP IN-TN IN-WB IN-OR IN-BR IN-KL"),
        ("india", "*"),
        ("japan", "JP-01 JP-13 JP-27 JP-26 JP-23 JP-40 JP-47 JP-34 JP-04 JP-22"),
        ("japan", "*"),
        ("russia", "RU-MOW RU-SPE RU-KGD RU-MOS RU-LEN RU-MUR RU-ARK RU-KR RU-KO RU-TA RU-BA RU-ROS RU-KDA RU-VGG"),
        ("russia", "*"),
    ]),
    ("brazil", [
        ("brazil", "BR-AM BR-PA BR-MT BR-BA BR-MG BR-SP BR-RJ BR-RS BR-PR BR-GO"),
        ("brazil", "*"),
        ("argentina", "*"),
        ("australia", "*"),
    ]),
]

# ---- geometry helpers --------------------------------------------------------------------------------------------


def clip_ring(pts, W, H):
    """Sutherland-Hodgman against [0, W] x [0, H]; the crossing of an edge computed from its two ends in a fixed
    order, so two rings sharing the edge get the same point"""
    def cut(a, b, axis, v):
        if (a[0], a[1]) > (b[0], b[1]):
            a, b = b, a
        t = (v - a[axis]) / (b[axis] - a[axis])
        if axis == 0:
            return (v, a[1] + (b[1] - a[1]) * t)
        return (a[0] + (b[0] - a[0]) * t, v)

    for axis, v, keep_low in ((0, 0.0, False), (0, float(W), True), (1, 0.0, False), (1, float(H), True)):
        if not pts:
            break
        out = []
        n = len(pts)
        for i in range(n):
            a, b = pts[i - 1], pts[i]
            ina = a[axis] <= v if keep_low else a[axis] >= v
            inb = b[axis] <= v if keep_low else b[axis] >= v
            if inb:
                if not ina:
                    out.append(cut(a, b, axis, v))
                out.append(b)
            elif ina:
                out.append(cut(a, b, axis, v))
        pts = out
    return pts


def area2(pts):
    a = 0
    n = len(pts)
    for i in range(n):
        x0, y0 = pts[i - 1]
        x1, y1 = pts[i]
        a += x0 * y1 - x1 * y0
    return a


def dp_idx(pts, i0, i1, tol):
    """Douglas-Peucker on pts[i0..i1]: the indices kept, its ends included"""
    if i1 - i0 < 2:
        return list(range(i0, i1 + 1))
    keep = {i0, i1}
    stack = [(i0, i1)]
    t2 = tol * tol
    while stack:
        i, j = stack.pop()
        ax, ay = pts[i]
        bx, by = pts[j]
        dx, dy = bx - ax, by - ay
        L = dx * dx + dy * dy
        best, bi = -1.0, -1
        for k in range(i + 1, j):
            px, py = pts[k]
            if L == 0:
                d = (px - ax) ** 2 + (py - ay) ** 2
            else:
                c = (px - ax) * dy - (py - ay) * dx
                d = c * c / L
            if d > best:
                best, bi = d, k
        if bi >= 0 and best > t2:
            keep.add(bi)
            stack.append((i, bi))
            stack.append((bi, j))
    return sorted(keep)


def dp(pts, tol):
    """Douglas-Peucker on an open polyline, its ends kept"""
    n = len(pts)
    if n <= 2:
        return list(pts)
    keep = [False] * n
    keep[0] = keep[-1] = True
    stack = [(0, n - 1)]
    t2 = tol * tol
    while stack:
        i, j = stack.pop()
        ax, ay = pts[i]
        bx, by = pts[j]
        dx, dy = bx - ax, by - ay
        L = dx * dx + dy * dy
        best, bi = -1.0, -1
        for k in range(i + 1, j):
            px, py = pts[k]
            if L == 0:
                d = (px - ax) ** 2 + (py - ay) ** 2
            else:
                c = (px - ax) * dy - (py - ay) * dx
                d = c * c / L
            if d > best:
                best, bi = d, k
        if bi >= 0 and best > t2:
            keep[bi] = True
            stack.append((i, bi))
            stack.append((bi, j))
    return [p for p, k in zip(pts, keep) if k]


def inside(pts, x, y):
    c = False
    n = len(pts)
    for i in range(n):
        x0, y0 = pts[i - 1]
        x1, y1 = pts[i]
        if (y0 > y) != (y1 > y) and x < x0 + (x1 - x0) * (y - y0) / (y1 - y0):
            c = not c
    return c


def label_points(ring, holes=()):
    """points well inside the ring and out of its holes (an enclave), the best first: its centroid if it is, then the
    middles of the spans across it, over a few rows, the widest near its centroid first"""
    rings = [ring] + list(holes)

    def within(x, y):
        return sum(inside(r, x, y) for r in rings) % 2 == 1
    a = area2(ring)
    cx = cy = 0.0
    n = len(ring)
    for i in range(n):
        x0, y0 = ring[i - 1]
        x1, y1 = ring[i]
        c = x0 * y1 - x1 * y0
        cx += (x0 + x1) * c
        cy += (y0 + y1) * c
    if a != 0:
        cx /= 3 * a
        cy /= 3 * a
    ys = [p[1] for p in ring]
    y0, y1 = min(ys), max(ys)
    cands = []
    for k in range(1, 32):
        y = y0 + (y1 - y0) * k / 32 + 0.37
        xs = []
        for r in rings:
            for i in range(len(r)):
                ax, ay = r[i - 1]
                bx, by = r[i]
                if (ay > y) != (by > y):
                    xs.append(ax + (bx - ax) * (y - ay) / (by - ay))
        xs.sort()
        for j in range(0, len(xs) - 1, 2):
            w = xs[j + 1] - xs[j]
            score = w - 0.6 * abs(y - cy)
            cands.append((-score, (xs[j] + xs[j + 1]) / 2, y))
    cands.sort()
    out = [(round(cx), round(cy))] if within(round(cx), round(cy)) else []
    out += [(round(x), round(y)) for s, x, y in cands if within(round(x), round(y))]
    return out or [(round(cx), round(cy))]

def ring_crossings(arcs, refs):
    """the segments of a ring's arcs (arc, its segment) that cross another of the ring (a proper crossing, not a
    shared corner)"""
    segs = []
    for ai, fwd in refs:
        s = arcs[ai]["s"]
        if arcs[ai]["closed"]:
            s = s + [s[0]]
        for i in range(len(s) - 1):
            segs.append((s[i], s[i + 1], (ai, i)))
    if len(segs) < 4:
        return set()
    C = 96.0
    cells = {}
    for k, (a, b, ai) in enumerate(segs):
        for gx in range(int(min(a[0], b[0]) // C), int(max(a[0], b[0]) // C) + 1):
            for gy in range(int(min(a[1], b[1]) // C), int(max(a[1], b[1]) // C) + 1):
                cells.setdefault((gx, gy), []).append(k)

    def orient(p, q, r):
        v = (q[0] - p[0]) * (r[1] - p[1]) - (q[1] - p[1]) * (r[0] - p[0])
        return (v > 0) - (v < 0)
    out = set()
    seen = set()
    for lst in cells.values():
        for i in range(len(lst)):
            for j in range(i + 1, len(lst)):
                k1, k2 = lst[i], lst[j]
                if (k1, k2) in seen:
                    continue
                seen.add((k1, k2))
                a, b, a1 = segs[k1]
                c, d, a2 = segs[k2]
                if orient(a, b, c) * orient(a, b, d) < 0 and orient(c, d, a) * orient(c, d, b) < 0:
                    out.add(a1)
                    out.add(a2)
    return out

# ---- making a map ------------------------------------------------------------------------------------------------


def make_map(M, units, country_name, region_names, cont):
    """the map's regions, arcs and rings"""
    lon0, proj = M['lon0'], M['proj']
    rl0, rl1, la0, la1 = M['box']
    insets = M.get('insets', [])
    # the window's rectangle: the box's edges projected
    xs, ys = [], []
    for k in range(201):
        t = k / 200
        for rl, la in ((rl0 + (rl1 - rl0) * t, la0), (rl0 + (rl1 - rl0) * t, la1), (rl0, la0 + (la1 - la0) * t),
                       (rl1, la0 + (la1 - la0) * t)):
            x, y = proj(rl, la)
            xs.append(x)
            ys.append(y)
    x0, x1, y0, y1 = min(xs), max(xs), min(ys), max(ys)
    scale = S / max(x1 - x0, y1 - y0)
    W, H = int(round((x1 - x0) * scale)), int(round((y1 - y0) * scale))

    # the regions: key -> index
    regions, rindex = [], {}

    def region(key, kind, name, country):
        if key not in rindex:
            rindex[key] = len(regions)
            regions.append(dict(key=key, kind=kind, name=name, country=country))
        return rindex[key]

    def unit_region(u):
        code = SAME_AS.get(u['code'], u['code'])
        d = DISPUTED_UNITS.get((u['code'], u['name'])) or DISPUTED_CODES.get(code)
        if d:
            return region(d, K_DISPUTED, None, d)
        if M["kind"] == "states" and u["code"] == M["country"]:
            if not u['name']:
                return None                     # a sliver without a name
            if u['name'] in M.get('skip', ()):
                return region(u['key'], K_OTHER, ('unit', u), code)   # land not asked (Washington DC)
            if M.get('group'):
                g = u['region']
                return region(M['id'] + ':' + g, K_ASK, ('group', M['id'], g, u), code)
            return region(u['key'], K_ASK, ('unit', u), code)
        if code == 'ATA':
            return None
        kind = K_ASK if code in STATES and M['kind'] == "countries" else K_OTHER
        return region(code, kind, ('country', code), code)

    # every unit's rings projected to the grid, clipped
    pieces = []                                 # (region, ring of integer points)
    for u in units:
        ins = None
        for k, (sel, ilon0, iproj, ibox, frac) in enumerate(insets):
            if sel(u):
                ins = k
        if ins is None:
            src_lon0, src_proj, box = lon0, proj, M['box']
        else:
            _, src_lon0, src_proj, box, _ = insets[ins]
        # a quick test on the unit's bounding box first
        r = None
        for ring in u['rings']:
            ur = unwrap_ring(ring, src_lon0)
            mx0 = min(p[0] for p in ur)
            mx1 = max(p[0] for p in ur)
            my0 = min(p[1] for p in ur)
            my1 = max(p[1] for p in ur)
            margin = 25 if ins is None else 0.5
            if mx1 < box[0] - margin or mx0 > box[1] + margin or my1 < box[2] - margin or my0 > box[3] + margin:
                continue
            if ins is None and M['id'] != 'world' and (mx0 < -120 or mx1 > 120):
                continue                        # the far side of the globe
            if r is None:
                r = unit_region(u)
                if r is None:
                    break
            pts = [src_proj(rl, la) for rl, la in ur]
            pieces.append((r, ('proj', ins, pts), ins))
            if ins is not None:
                regions[r]["inset"] = True
    # the window: the box's rectangle, narrowed to the land of the map's set and a margin round it (no empty ocean in
    # the corners of a round projection)
    if M['id'] != 'world':
        want = cont.get(M['id']) if M['kind'] == 'countries' else None
        sx, sy = [], []
        for r, g, ins in pieces:
            R = regions[r]
            if ins is not None or R['kind'] == K_DISPUTED:
                continue
            if (want and R['key'] in want) or (not want and R['country'] == M.get('country')):
                for x, y in g[2]:
                    if x0 <= x <= x1 and y0 <= y <= y1:
                        sx.append(x)
                        sy.append(y)
        if sx:
            m = 0.03 * max(max(sx) - min(sx), max(sy) - min(sy))
            x0, x1 = max(x0, min(sx) - m), min(x1, max(sx) + m)
            y0, y1 = max(y0, min(sy) - m), min(y1, max(sy) + m)
            scale = S / max(x1 - x0, y1 - y0)
            W, H = int(round((x1 - x0) * scale)), int(round((y1 - y0) * scale))
    for i, (r, g, ins) in enumerate(pieces):
        if ins is None:
            pieces[i] = (r, [((x - x0) * scale, (y1 - y) * scale) for x, y in g[2]], ins)
    # the insets: fitted into their part of the grid
    for k, (sel, ilon0, iproj, ibox, frac) in enumerate(insets):
        allp = [p for r, g, ins in pieces if ins == k for p in g[2]]
        if not allp:
            continue
        ix0, ix1 = min(p[0] for p in allp), max(p[0] for p in allp)
        iy0, iy1 = min(p[1] for p in allp), max(p[1] for p in allp)
        fx0, fy0, fx1, fy1 = frac[0] * W, frac[1] * H, frac[2] * W, frac[3] * H
        sc = min((fx1 - fx0) / (ix1 - ix0), (fy1 - fy0) / (iy1 - iy0)) * 0.94
        ox = (fx0 + fx1) / 2 - (ix0 + ix1) / 2 * sc
        oy = (fy0 + fy1) / 2 + (iy0 + iy1) / 2 * sc
        M.setdefault('frames', []).append((int(fx0), int(fy0), int(fx1), int(fy1)))
        for i, (r, g, ins) in enumerate(pieces):
            if ins == k:
                pieces[i] = (r, [(ox + x * sc, oy - y * sc) for x, y in g[2]], ins)
    rings = []
    raw = {}                                    # a region's points before anything is dropped: where a speck lies
    for r, g, ins in pieces:
        a = abs(area2(g))
        cx = sum(p[0] for p in g) / len(g)
        cy = sum(p[1] for p in g) / len(g)
        if 0 <= cx <= W and 0 <= cy <= H and a >= raw.get(r, (0, 0, -1.0))[2]:
            raw[r] = (cx, cy, a)                # its biggest piece's middle
        c = clip_ring(g, W, H)
        q = []
        for x, y in c:
            p = (int(round(x)), int(round(y)))
            if not q or q[-1] != p:
                q.append(p)
        while len(q) > 1 and q[0] == q[-1]:
            q.pop()
        if len(q) >= 3 and area2(q) != 0:
            rings.append((r, q))

    # the edges and who is on their sides: each region's net count of an edge each way (its pieces' edges inside
    # it cancel out; a region's boundary stays a set of closed loops whatever the data's overlaps)
    net = {}                                    # (a, b) with a < b -> {region: net count a -> b}
    for r, q in rings:
        n = len(q)
        for i in range(n):
            a, b = q[i], q[(i + 1) % n]
            if a < b:
                d = net.setdefault((a, b), {})
                d[r] = d.get(r, 0) + 1
            else:
                d = net.setdefault((b, a), {})
                d[r] = d.get(r, 0) - 1

    def on_frame(a, b):
        return (a[0] == b[0] and a[0] in (0, W)) or (a[1] == b[1] and a[1] in (0, H))

    out_edges = {}                              # region -> {a: [(b, other)]}
    # the window's edges: clipping leaves a region's pieces joined by runs along the edge, overlapping each other;
    # counted along the edge, a stretch at a time, they cancel out where they should
    along = {}                                  # (region, side) -> [(t0, t1, count from t0 to t1)]
    for (a, b), d in net.items():
        if on_frame(a, b):
            side = ('x', a[0]) if a[0] == b[0] else ('y', a[1])
            t0, t1 = (a[1], b[1]) if side[0] == 'x' else (a[0], b[0])
            for r, k in d.items():
                if k:
                    along.setdefault((r, side), []).append((t0, t1, k) if t0 < t1 else (t1, t0, -k))
    for (r, side), lst in sorted(along.items()):
        ts = sorted({t for t0, t1, k in lst for t in (t0, t1)})
        for u, v in zip(ts, ts[1:]):
            k = sum(c for t0, t1, c in lst if t0 <= u and v <= t1)
            if not k:
                continue
            pu = (side[1], u) if side[0] == 'x' else (u, side[1])
            pv = (side[1], v) if side[0] == 'x' else (v, side[1])
            p, q = (pu, pv) if k > 0 else (pv, pu)
            oe = out_edges.setdefault(r, {})
            for _ in range(abs(k)):
                oe.setdefault(p, []).append((q, -2))
    for (a, b), d in sorted(net.items()):
        if on_frame(a, b):
            continue
        for r, k in d.items():
            if k == 0:
                continue                        # inside the region
            others = [s for s, j in d.items() if s != r and j * k < 0]
            other = others[0] if others else (-2 if on_frame(a, b) else -1)
            p, q = (a, b) if k > 0 else (b, a)
            oe = out_edges.setdefault(r, {})
            for _ in range(abs(k)):
                oe.setdefault(p, []).append((q, other))
    # the boundary's vertices: how many boundary edges meet at each
    degree = {}
    for r, oe in out_edges.items():
        for a, lst in oe.items():
            for b, o in lst:
                key = (a, b) if a < b else (b, a)
                degree.setdefault(a, set()).add(b)
                degree.setdefault(b, set()).add(a)
    # the regions' rings: their boundary edges chained
    # (a walk that comes back to a corner it passed closes a loop there: each ring is a simple loop, an outline or a
    # hole, never the two joined at a point)
    rrings = []                                 # (region, [vertices], [other of edge i])
    for r in sorted(out_edges):
        oe = out_edges[r]
        for start in sorted(oe):
            while oe.get(start):
                vs, os_, at = [], [], {}
                v = start
                while True:
                    lst = oe.get(v)
                    if not lst:
                        break
                    b, o = lst.pop()
                    at[v] = len(vs)
                    vs.append(v)
                    os_.append(o)
                    v = b
                    if v == start:
                        break
                    if v in at:                 # a loop closed at v: cut it out
                        j = at[v]
                        loop, lo = vs[j:], os_[j:]
                        for u in loop:
                            del at[u]
                        del vs[j:], os_[j:]
                        if len(loop) >= 3:
                            rrings.append((r, loop, lo))
                if len(vs) >= 3 and v == start:
                    rrings.append((r, vs, os_))
    # arcs: each ring cut where the neighbour changes or several boundaries meet
    arcs, akey = [], {}

    def arc_of(seq, closed, sides):
        if closed:
            m = min(range(len(seq)), key=lambda i: seq[i])
            s1 = seq[m:] + seq[:m]
            s2 = [s1[0]] + s1[1:][::-1]
            fwd = s1[1] <= s2[1]
            canon = s1 if fwd else s2
            key = ('c', tuple(canon))
        else:
            fwd = tuple(seq) <= tuple(seq[::-1])
            canon = seq if fwd else seq[::-1]
            key = ('o', tuple(canon))
        if key not in akey:
            akey[key] = len(arcs)
            arcs.append(dict(pts=list(canon), closed=closed, sides=set()))
        a = akey[key]
        arcs[a]['sides'].update(sides)
        return a, fwd

    region_rings = []                           # (region, [(arc, fwd)])
    for r, vs, os_ in rrings:
        n = len(vs)
        cuts = [i for i in range(n) if os_[i - 1] != os_[i] or len(degree.get(vs[i], ())) > 2]
        refs = []
        if not cuts:
            refs.append(arc_of(vs, True, {r, os_[0]}))
        else:
            for k, c in enumerate(cuts):
                e = cuts[(k + 1) % len(cuts)]
                seq = [vs[(c + j) % n] for j in range(((e - c) % n or n) + 1)]
                refs.append(arc_of(seq, False, {r, os_[c]}))
        region_rings.append((r, refs))
    # each arc simplified once
    tol = M['tol']

    def simplify(a, t):
        """the points kept (indices into the arc's points; a closed arc's last segment goes back to the first)"""
        p = a['pts']
        if a['closed']:
            far = max(range(len(p)), key=lambda i: (p[i][0] - p[0][0]) ** 2 + (p[i][1] - p[0][1]) ** 2)
            k = sorted(set(dp_idx(p, 0, far, t)) | set(i % len(p) for i in dp_idx(p + [p[0]], far, len(p), t)))
            if len(k) < 3:
                k = [0, len(p) // 3, 2 * len(p) // 3]
        else:
            k = dp_idx(p, 0, len(p) - 1, t)
        a['k'] = k
        a['s'] = [p[i] for i in k]
    for a in arcs:
        simplify(a, tol)
    # a ring that crosses itself once simplified (a narrow gulf cut across) cannot be cut into triangles: each segment
    # crossing another gets back the point of its stretch farthest from it, until no ring crosses itself
    for _ in range(24):
        bad = set()
        for r, refs in region_rings:
            bad |= ring_crossings(arcs, refs)
        if not bad:
            break
        M["repaired"] = M.get("repaired", 0) + len(bad)
        for ai, i in sorted(bad, reverse=True):
            a = arcs[ai]
            p, k = a['pts'], a['k']
            lo, hi = k[i], (k[i + 1] if i + 1 < len(k) else len(p))
            if hi - lo < 2:
                continue                        # the data's own crossing: left as it is
            q = p + [p[0]] if a['closed'] else p
            (ax, ay), (bx, by) = q[lo], q[hi]
            far = max(range(lo + 1, hi), key=lambda j: abs((q[j][0] - ax) * (by - ay) - (q[j][1] - ay) * (bx - ax)))
            a['k'] = sorted(set(k) | {far})
            a['s'] = [p[j] for j in a['k']]

    def ring_points(refs):
        out = []
        for ai, fwd in refs:
            s = arcs[ai]['s']
            if not fwd:
                s = s[::-1]
            if arcs[ai]['closed']:
                out.extend(s)
            else:
                out.extend(s[:-1])
        return out
    # rings too small to see dropped (a region keeps its biggest)
    best = {}
    full = []
    for r, refs in region_rings:
        pts = ring_points(refs)
        a = area2(pts) / 2 if len(pts) >= 3 else 0
        full.append((r, refs, pts, a))
        if a > 0 and a > best.get(r, (0, None))[0]:
            best[r] = (a, pts)
    min_area = (3.0 * tol) ** 2
    kept = []
    for r, refs, pts, a in full:
        if len(pts) < 3:
            continue
        if abs(a) < min_area and not (a > 0 and best.get(r, (0, None))[1] is pts):
            continue
        kept.append((r, refs, a > 0))
    # the regions' figures
    for i, R in enumerate(regions):
        R['area'] = 0.0
        R['label'] = None
    for r, refs, outer in kept:
        if outer:
            regions[r]['area'] += area2(ring_points(refs)) / 2
    holes = {}
    for r, refs, outer in kept:
        if not outer:
            holes.setdefault(r, []).append(ring_points(refs))
    # its label: a point of it in no other region (a neighbour's simplified line may stray over it by a hair)
    others = []
    for r, refs, outer in kept:
        pts = ring_points(refs)
        xs_ = [p[0] for p in pts]
        ys_ = [p[1] for p in pts]
        others.append((r, pts, min(xs_), min(ys_), max(xs_), max(ys_)))

    def in_other(r, x, y):
        c = {}
        for s, pts, bx0, by0, bx1, by1 in others:
            if s != r and bx0 <= x <= bx1 and by0 <= y <= by1 and inside(pts, x, y):
                c[s] = not c.get(s, False)
        return any(c.values())
    for r, (a, pts) in best.items():
        cands = label_points(pts, holes.get(r, ()))
        regions[r]['label'] = next((p for p in cands if not in_other(r, p[0], p[1])), cands[0])
    # neighbours (shared arcs) and tones
    nb = {i: set() for i in range(len(regions))}
    used = set(ai for r, refs, o in kept for ai, f in refs)
    for ai in used:
        s = [x for x in arcs[ai]['sides'] if x >= 0]
        if len(s) == 2:
            nb[s[0]].add(s[1])
            nb[s[1]].add(s[0])
    for i in sorted(range(len(regions)), key=lambda i: -len(nb[i])):
        taken = {regions[j].get('tone') for j in nb[i]}
        regions[i]['tone'] = next(t for t in range(8) if t not in taken)
    # a region left without any piece (a speck smaller than the grid): its biggest piece's middle, for its marker
    for r, (cx, cy, a) in raw.items():
        if regions[r]['label'] is None:
            regions[r]['label'] = (cx, cy)
    # the regions with nothing in the window dropped, the others numbered again
    has = {r for r, refs, o in kept}
    alive = [i for i, R in enumerate(regions) if i in has or R['label'] is not None]
    renum = {old: new for new, old in enumerate(alive)}
    regions = [regions[i] for i in alive]
    kept = [(renum[r], refs, o) for r, refs, o in kept]
    for a in arcs:
        a['sides'] = {renum.get(s, -1) if s >= 0 else s for s in a['sides']}
    M.update(W=W, H=H, regions=regions, arcs=arcs, rings=kept, used=sorted(used))
    return M

# ---- names -------------------------------------------------------------------------------------------------------

NE_FIELD = {"en": "NAME_EN", "de": "NAME_DE", "el": "NAME_EL", "es": "NAME_ES", "fr": "NAME_FR", "id": "NAME_ID",
            "it": "NAME_IT", "ja": "NAME_JA", "ko": "NAME_KO", "nl": "NAME_NL", "pl": "NAME_PL", "pt_BR": "NAME_PT",
            "pt_PT": "NAME_PT", "ru": "NAME_RU", "sv": "NAME_SV", "tr": "NAME_TR", "uk": "NAME_UK",
            "zh_CN": "NAME_ZH"}
OWN = {"cs": NM.CS, "da": NM.DA, "fi": NM.FI, "nb": NM.NB}
LATIN = {"en", "cs", "da", "de", "es", "fi", "fr", "id", "it", "nb", "nl", "pl", "pt_BR", "pt_PT", "sv", "tr"}


def country_names(code, a0rec, unit):
    """{lang: name} of a country or territory (a0rec: its admin-0 record, else the admin-1 unit's own names)"""
    rec = a0rec or unit
    out = {}
    for lang in LANGS:
        n = None
        if lang in OWN:
            n = OWN[lang].get(code)
        else:
            n = NM.FIX.get(lang, {}).get(code)
            if not n and lang == "pt_BR":
                n = NM.PT_BR.get(code)
            if not n and lang == "pt_PT":
                n = NM.PT_PT.get(code)
            if not n:
                n = rec.get(NE_FIELD[lang]) if rec else None
        if not n:
            n = (rec.get('NAME_EN') or rec.get('NAME')) if rec else code
        out[lang] = n
    return out


def unit_names(u, fix):
    out = {}
    rec = u['rec']
    en = fix.get(('en', u['key'])) or rec.get('NAME_EN') or rec.get('NAME')
    plain = u['code'] == 'JPN'                  # Japan's prefectures: their plain names in Latin letters
    if plain and not fix.get(('en', u['key'])):
        en = rec.get('NAME')
    for lang in LANGS:
        n = fix.get((lang, u['key']))
        if not n and plain and lang in LATIN:
            n = rec.get('NAME')
        if not n and lang in NE_FIELD and lang != 'en':
            n = rec.get(NE_FIELD[lang])
            if n and lang == 'fr' and n[:2] in ("d'", "d’"):
                n = n[2].upper() + n[3:]        # "d'Orenbourg": the oblast's name cut short
        out[lang] = n or en
    out['en'] = en
    return out

# ---- output ------------------------------------------------------------------------------------------------------


def c_array(name, ctype, vals, per=24):
    lines = ["static const %s %s[%d] = {" % (ctype, name, max(1, len(vals)))]
    for i in range(0, len(vals), per):
        lines.append("    " + ", ".join(str(v) for v in vals[i:i + per]) + ",")
    if not vals:
        lines.append("    0,")
    lines.append("};")
    return "\n".join(lines)


def c_str(s):
    return '"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--a0', required=True)
    ap.add_argument('--a1', required=True)
    ap.add_argument('--app', required=True)
    ap.add_argument('--only', default='')
    ap.add_argument('--preview', default='', help='a folder: each map drawn as a PNG (PIL)')
    args = ap.parse_args()
    a0 = read_dbf(args.a0 + '.dbf')
    a0by = {r['ADM0_A3']: r for r in a0}
    recs = read_dbf(args.a1 + '.dbf')
    shapes = read_shp(args.a1 + '.shp')
    units = []
    for rec, rings in zip(recs, shapes):
        iso = rec.get('ISO_3166_2') or ''
        key = iso if iso and not iso.endswith('~') and '-' in iso else rec['ADM1_CODE']
        units.append(dict(code=rec['ADM0_A3'], name=rec['NAME'], region=rec.get('REGION') or '', key=key,
                          rec=rec, rings=rings))
    seen = {}
    for u in units:                             # the keys unique (Natural Earth repeats a few ISO codes)
        if u['key'] in seen:
            u['key'] = u['rec']['ADM1_CODE']
        seen[u['key']] = 1
    cont = continent_sets(a0)
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import mapquiz_states as ST                 # noqa: E402  the states' own names and codes
    maps = [m for m in MAPS if not args.only or m['id'] in args.only.split(',')]
    # the names: one list for the whole game (a country is named once, whatever its maps)
    names, nindex = [], {}

    def name_of(desc):
        if desc is None:
            return -1
        if desc[0] == 'country':
            code = desc[1]
            key = 'C:' + code
            if key not in nindex:
                unit = next((u['rec'] for u in units if SAME_AS.get(u['code'], u['code']) == code), None)
                nindex[key] = len(names)
                names.append((key, country_names(code, a0by.get(code), unit)))
            return nindex[key]
        if desc[0] == 'unit':
            u = desc[1]
            key = 'U:' + u['key']
            if key not in nindex:
                nindex[key] = len(names)
                names.append((key, unit_names(u, ST.FIX)))
            return nindex[key]
        if desc[0] == 'group':
            _, mid, g, u = desc
            key = 'G:' + mid + ':' + g
            if key not in nindex:
                nindex[key] = len(names)
                gn = ST.GROUPS.get((mid, g))
                if not gn:
                    raise SystemExit('no names for the group %s of %s' % (g, mid))
                names.append((key, {lang: gn.get(lang) or gn['en'] for lang in LANGS}))
            return nindex[key]
        raise ValueError(desc)

    out = []
    for M in maps:
        mu = units
        make_map(M, mu, None, None, cont)
        for R in M['regions']:
            R['nm'] = name_of(R['name']) if R['kind'] != K_DISPUTED else -1
            R['code'] = R['key']
            if R['name'] and R['name'][0] == 'unit':
                R['code'] = ST.CODE.get(R['key'], R['key'])
            if R['name'] and R['name'][0] == 'group':
                R['code'] = ST.GROUP_CODE.get((M['id'], R['name'][2]), R['key'])
        npts = sum(len(M['arcs'][a]['s']) for a in M['used'])
        print('%-10s %5d x %5d  regions %4d  arcs %5d  points %6d  rings %5d  repaired %d' %
              (M['id'], M['W'], M['H'], len(M['regions']), len(M['used']), npts, len(M['rings']), M.get('repaired', 0)), file=sys.stderr)
        out.append(M)
    for M in out:                               # a country's map is named by the country
        if M['kind'] == 'states':
            M['nm'] = name_of(('country', M['country']))
    write_data(args.app, out, names, cont)
    write_words(args.app, names, out)
    if args.preview:
        preview(args.preview, out)


def preview(folder, maps):
    """each map drawn as a PNG (python3's PIL): a look at the outlines while tuning the tool"""
    from PIL import Image, ImageDraw
    tones = [(236, 220, 186), (226, 204, 160), (240, 228, 200), (218, 196, 150), (232, 212, 172), (210, 190, 150),
             (244, 232, 210), (222, 200, 170)]
    for M in maps:
        k = 1400 / max(M['W'], M['H'])
        im = Image.new('RGB', (int(M['W'] * k) + 1, int(M['H'] * k) + 1), (176, 196, 206))
        d = ImageDraw.Draw(im)
        arcs = M['arcs']
        order = sorted(M['rings'], key=lambda t: -abs(M['regions'][t[0]]['area']))
        for r, refs, outer in order:
            if not outer:
                continue
            pts = []
            for ai, fwd in refs:
                s = arcs[ai]['s'] if fwd else arcs[ai]['s'][::-1]
                pts.extend(s if arcs[ai]['closed'] else s[:-1])
            R = M['regions'][r]
            col = (170, 170, 170) if R['kind'] == K_DISPUTED else (200, 196, 186) if R['kind'] == K_OTHER else \
                tones[R['tone'] % 8]
            if len(pts) >= 3:
                d.polygon([(x * k, y * k) for x, y in pts], fill=col)
        for ai in M['used']:
            A = arcs[ai]
            s = A['s'] + ([A['s'][0]] if A['closed'] else [])
            land = [x for x in A['sides'] if x >= 0]
            if -2 in A['sides'] and len(land) <= 1:
                continue
            col = (60, 40, 30) if len(land) == 2 else (90, 110, 130)
            d.line([(x * k, y * k) for x, y in s], fill=col, width=1)
        for R in M['regions']:
            if R['label'] and R['kind'] == K_ASK:
                x, y = R['label']
                d.ellipse([x * k - 2, y * k - 2, x * k + 2, y * k + 2], fill=(200, 40, 40))
        for f in M.get('frames', []):
            d.rectangle([f[0] * k, f[1] * k, f[2] * k, f[3] * k], outline=(80, 80, 80))
        im.save(os.path.join(folder, M['id'] + '.png'))


def write_data(app, maps, names, cont):
    L = []
    L.append("/* Map Quiz's maps: made by tools/mapquiz_gen.py from Natural Earth (public domain, naturalearthdata.com)."
             " Do not edit. */")
    L.append("#ifndef MAPQUIZ_DATA_H\n#define MAPQUIZ_DATA_H\n")
    counts = ["/* Map Quiz's counts: made by tools/mapquiz_gen.py. Do not edit. */",
              "#ifndef MAPQUIZ_COUNT_H\n#define MAPQUIZ_COUNT_H",
              "#define MQ_NAMES %d" % len(names), "#define MQ_MAPS %d" % len(maps)]
    total = 0
    for mi, M in enumerate(maps):
        regs = M['regions']
        used = M['used']
        remap = {a: i for i, a in enumerate(used)}
        # the arcs' points as bytes: each point's step from the one before (-127..127 a coordinate), or -128 then the
        # step as two 16-bit little-endian numbers; an arc's first point is a step from the last arc's last
        pts, starts, kinds = [], [], []
        px = py = 0
        M['npts'] = 0
        for a in used:
            A = M['arcs'][a]
            starts.append(len(A['s']))
            M['npts'] += len(A['s'])
            for x, y in A['s']:
                dx, dy = x - px, y - py
                if -127 <= dx <= 127 and -127 <= dy <= 127:
                    pts += [dx, dy]
                else:
                    pts += [-128] + [v - 256 if v > 127 else v for v in struct.pack('<hh', dx, dy)]
                px, py = x, y
            sides = [s for s in A['sides']]
            land = [s for s in sides if s >= 0]
            if -2 in sides and len(land) <= 1:
                k = 3                           # the window's edge: not drawn
            elif len(land) <= 1:
                k = 0                           # a coast
            else:
                c = {regs[s]['country'] for s in land}
                k = 2 if len(c) > 1 else 1      # between countries, or within one
            if any(regs[s].get("inset") for s in land):
                k |= 4                          # an inset's: drawn over its frame
            kinds.append(k)
        rd = []
        for r, refs, outer in sorted(M['rings'], key=lambda t: t[0]):
            rd += [r, 1 if outer else 0, len(refs)]
            for ai, fwd in refs:
                rd.append(remap[ai] if fwd else -1 - remap[ai])
        M['nbytes'] = len(pts)
        L.append(c_array("mq_pts_%s" % M['id'], "int8_t", pts, 32))
        L.append(c_array("mq_arc_%s" % M['id'], "uint16_t", starts))
        L.append(c_array("mq_arck_%s" % M['id'], "uint8_t", kinds, 48))
        L.append(c_array("mq_ring_%s" % M["id"], "int16_t", rd))
        L.append("static const mq_region_def mq_reg_%s[%d] = {" % (M['id'], len(regs)))
        for R in regs:
            lx, ly = R['label'] if R['label'] else (-1, -1)
            L.append("    {%s, %d, %d, %d, %d, %d, %d, %.1ff}," % (c_str(R['code']), R['nm'], R['kind'], R['tone'] % 8,
                                                                  1 if R.get("inset") else 0, int(round(lx)),
                                                                  int(round(ly)), R['area']))
        L.append("};")
        fr = M.get('frames', [])
        L.append(c_array("mq_frame_%s" % M['id'], "int16_t", [v for f in fr for v in f]))
        total += len(pts) + len(starts) * 2 + len(kinds) + len(rd) * 2 + len(regs) * 24
        M['nring'] = len(rd)
    L.append("static const mq_map_def mq_maps[MQ_MAPS] = {")
    for M in maps:
        i = M['id']
        L.append("    {%s, %d, %d, %d, mq_reg_%s, %d, %d, mq_pts_%s, %d, mq_arc_%s, mq_arck_%s, %d, mq_ring_%s, %d, "
                 "mq_frame_%s}," % (c_str(i), M['W'], M['H'], len(M['regions']), i, M['npts'], M['nbytes'], i,
                                    len(M['used']), i, i, M['nring'], i, len(M.get('frames', [])), i))
    L.append("};")
    # the continents' and countries' sets (the random tab, the "*" levels): codes between spaces
    L.append("static const char *const mq_sets[MQ_MAPS] = {")
    for M in maps:
        if M['kind'] == 'countries':
            s = sorted(cont[M['id']]) if M['id'] in cont else sorted(STATES)
            present = {R['code'] for R in M['regions']}
            miss = [c for c in s if c not in present]
            if miss:
                raise SystemExit('%s lacks %s' % (M['id'], miss))
            L.append("    " + c_str(" ".join(s)) + ",")
        else:
            L.append("    " + c_str("*") + ",")
    L.append("};")
    # the levels
    lv, chs = [], []
    ids = [M['id'] for M in maps]
    for ch, (icon, levels) in enumerate(LEVELS):
        chs.append(ids.index(icon) if icon in ids else 0)
        for mid, codes in levels:
            if mid not in ids:
                continue
            lv.append((ch, ids.index(mid), codes))
    counts.append("#define MQ_LEVELS %d\n#define MQ_CHAPTERS %d\n#endif" % (len(lv), len(LEVELS)))
    L.append("static const mq_level_def mq_levels[MQ_LEVELS] = {")
    for ch, mi, codes in lv:
        L.append("    {%d, %d, %s}," % (ch, mi, c_str(codes)))
    L.append("};")
    L.append(c_array("mq_chapter_maps", "uint8_t", chs))
    L.append("\n#endif")
    with open(os.path.join(app, "src", "games", "mapquiz_count.h"), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(counts) + "\n")
    path = os.path.join(app, 'src', 'games', 'mapquiz_data.h')
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write("\n".join(L) + "\n")
    print('data: about %d bytes compiled' % total, file=sys.stderr)


def write_words(app, names, maps):
    for lang in LANGS:
        lines = ["Map Quiz: the prompt (%s: the place), then every place's name, in the generator's order",
                 NM.FIND[lang]]
        for key, nm in names:
            lines.append(nm[lang].replace('\n', ' '))
        d = os.path.join(app, 'help', lang)
        with open(os.path.join(d, 'mapquiz_words.txt'), 'w', encoding='utf-8', newline='\n') as f:
            f.write("\n".join(lines) + "\n")
        vl = []
        for M in maps:
            if M['id'] in NM.MAP_NAMES:
                vl.append(NM.MAP_NAMES[M['id']][lang])
            else:
                key = 'C:' + M['country']
                vl.append(next(nm[lang] for k, nm in names if k == key))
        with open(os.path.join(d, 'mapquiz_variants.txt'), 'w', encoding='utf-8', newline='\n') as f:
            f.write("\n".join(vl) + "\n")


if __name__ == '__main__':
    main()
