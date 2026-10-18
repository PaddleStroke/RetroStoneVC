/*
 * Train Switches' levels: src/games/trains_levels.h. Our own levels: no published or commercial set is used.
 * Each chapter has its idea (one switch at a time; two portals; toggles; crossings and loops; trains together;
 * tunnels) and two sources of networks:
 * - drawn by hand (TPL below: a fork, sidings, a comb, a fan, an anchor, a trident, a crossover, a station square, a
 *   figure of eight, a mountain line, tunnels on the sides or crossed...), each as drawn and mirrored; the rules'
 *   finisher sets their stems (mostly facing the portals), toggles and trains;
 * - laid by the rules' maker from fixed seeds, most of them mirrored left to right (a symmetric yard; a tunnel
 *   mirrored too, its image the second tunnel).
 * A network is kept when every switch is run over, the trains go to stations of two colours or more, and it was not
 * met before (nor its mirror). The difficulty is measured (measure()): the par (the fewest flips), the share of
 * first flips that mislead, how rare the settings that work are, the trains, and how many naive players solve it
 * (each sends the trains and flips a switch the first wrong train ran over). The look: symmetry, the track spread
 * over the board, few jogs and curves. Each chapter opens with its opening drawings (chapter 1: a fork, sidings, a
 * comb); then only networks the naive players mostly fail, spread from the chapter's floor score to its hardest, the
 * best-looking near each place, a drawing used twice at most and never twice in a row; each chapter ends on its
 * showpiece (a drawing's hardest puzzle, else the best-looking of the hardest few; in chapters 3 and 6 one of the
 * chapter's hardest). Deterministic.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/trsgen apps/boardgames/tools/trains_gen.c -lm && \
 *       /tmp/trsgen > apps/boardgames/src/games/trains_levels.h
 * The levels' difficulty table: the same with -DTRS_TABLE, run with no output file.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#define TRS_GEN
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/trains_rules.c"

#define PER (TRS_LEVELS / TRS_CHAPTERS)
#define MAXC 6000
/* a level past the chapter's openings: at most NAIVE[ch] of the naive players solve it (measure()), its score at least
 * FLOOR[ch] */
static const double NAIVE[TRS_CHAPTERS] = {0.25, 0.25, 0.25, 0.25, 0.2, 0.15};
static const double FLOOR[TRS_CHAPTERS] = {4, 5.5, 6.5, 7.5, 9, 11};
static double naive_max, floor_min;

/* ---- the drawings: '#' 'o' track (a line of each joins its own kind; '%' joins any), '+' a crossing, 'P' a portal,
 * 'S' a station, '1' '2' a tunnel's two mouths ('3' '4' a second tunnel's); '.' grass. Each with its chapter, the
 * puzzle it is made for (TSP), and its part: 1 an opening level (in their order), 2 the chapter's showpiece, 0 any place ---------------------- */
typedef struct { const char *rows[TRS_MAXH + 1]; int ch, part, sp; } tpl;

static const tpl TPL[] = {
    /* chapter 1: one train, one portal */
    {{"..P..", "..#..", "..#..", ".###.", ".#.#.", ".S.S.", NULL}, 0, 1, 0},                       /* a fork */
    {{"..P..", "..#..", "S##..", "..#..", "..##S", "..#..", "..S..", NULL}, 0, 1, 1},              /* sidings */
    {{".P....", ".#....", ".###S.", ".#....", ".###S.", ".#....", ".#....", ".S....", NULL}, 0, 1, 1},   /* comb */
    {{"..P..", "..#..", ".###.", ".#.#.", ".#.#.", ".#.#.", ".S.S.", NULL}, 0, 0, 0},
    {{"...P...", "...#...", ".#####.", ".#...#.", ".#..###", ".#..#.#", ".S..S.S", NULL}, 0, 0, 1},
    {{".......", "P###...", "...#...", ".#####.", ".#...#.", ".S...#.", ".....S.", NULL}, 0, 0, 0},
    {{"...P...", "...#...", ".#####.", ".#...#.", "###.###", "#.#.#.#", "S.S.S.S", NULL}, 0, 0, 2},   /* a tree */
    {{"...P....", "...#....", ".######.", ".#....#.", ".#.S..#.", ".#.#..#.", ".###.###", ".#...#.#",
      ".S...S.S", NULL}, 0, 2, 2},                                                                 /* a fan */
    {{"...P...", "...#...", "S#####S", ".#...#.", ".#...#.", ".S...S.", NULL}, 0, 0, 2},       /* a candelabra */
    {{"...P...", "...#...", ".#####.", ".#...#.", "S#...#S", ".#...#.", ".S...S.", NULL}, 0, 0, 2},   /* an anchor */
    {{"..P..", "..#..", ".###.", ".#.#.", "S#.#S", ".#.#.", ".S.S.", NULL}, 0, 0, 2},          /* a narrow anchor */
    {{"...P...", "...#...", "..###..", "..#.#..", "S##.##S", "..#.#..", "..S.S..", NULL}, 0, 0, 2},   /* a trident */
    /* chapter 2: two portals */
    {{".P...P.", ".#...#.", ".#...#.", ".#####.", ".#...#.", ".#...#.", ".S...S.", NULL}, 1, 1, 3},   /* crossover */
    {{".P...P.", ".#...#.", "###.###", "#.#.#.#", "#.###.#", "#..#..#", "S..S..S", NULL}, 1, 0, 3},
    {{".P...P.", ".#...#.", ".#####.", ".#...#.", ".#.S.#.", ".#.#.#.", ".#####.", ".#...#.", ".S...S.", NULL},
     1, 2, 4},                                                                                     /* a square */
    /* chapter 3: toggles */
    {{"..P..", "..#..", "..#..", ".###.", ".#.#.", ".S.S.", NULL}, 2, 1, 5},
    {{".P...P.", ".#...#.", ".##.##.", "..#.#..", "..###..", "...#...", "..###..", "..#.#..", "..S.S..", NULL},
     2, 1, 13},                                                                                    /* an X */
    {{"...P...", "...#...", ".#####.", ".#...#.", "###.###", "#.#.#.#", "S.S.S.S", NULL}, 2, 0, 13},
    {{".P...P.", ".#...#.", "###.###", "#.#.#.#", "#.###.#", "#..#..#", "S..S..S", NULL}, 2, 2, 6},
    /* chapter 4: crossings and loops */
    {{"...P...", "...#...", ".###...", ".#.#...", "S##+##.", "...#.#S", "...###.", "...#...", "...S...", NULL},
     3, 2, 8},                                                                                     /* figure 8 */
    /* chapter 5: together */
    {{".P...P.", ".#...#.", "S##.##S", "..#.#..", "..#%#..", "...#...", "..###..", "..#.#..", "..S.S..", NULL},
     4, 1, 15},                                                                                    /* a meeting */
    /* chapter 6: tunnels */
    {{"...P...", "...#...", ".#####.", ".#...#.", ".1...3.", ".......", ".4...2.", ".##.##.", ".SS.SS.", NULL},
     5, 0, 14},                                                                                    /* a mountain */
    {{".P...P.", ".#...#.", ".##.##.", ".1S.S3.", ".......", ".4...2.", ".#...#.", ".##%##.", "...#...",
      ".S###S.", NULL}, 5, 0, 12},                                                                  /* under the hill */
    {{"...P...", "...#...", ".###...", ".#.#...", ".1.#...", ".S####.", ".....#.", ".2...#.", ".#...S.",
      ".S.....", NULL}, 5, 0, 11},
    {{".P...P.", ".#...#.", ".#...#.", ".##%##.", "...#...", ".1.#.2.", ".#.#.#.", "S##%##S", NULL}, 5, 0, 12},
    {{".P...P.", ".#...#.", "###.###", "#.#.#.#", "1.#.#.3", "..#.#..", "2.###.4", "#..#..#", "##.#.##",
      ".S.S.S.", NULL}, 5, 0, 16},                                                                  /* side tunnels */
    {{".P...P.", ".#...#.", ".#...#.", ".1...3.", ".......", "..4.2..", "..#.#..", ".#####.", ".#.#.#.",
      ".S.S.S.", NULL}, 5, 0, 16},                                                                  /* crossed below */
    {{".P...P.", ".#...#.", "###.###", "1.#.#.3", "..#.#..", "..###..", "4..#..2", "#..#..#", "##.#.##",
      ".S.S.S.", NULL}, 5, 2, 17},                                                                  /* the pass */
    {{NULL}, -1, 0, 0},
};

/* the drawings' puzzles: w h (unused) portals (unused) stations trains switches loops toggles crossings tunnels
 * together lo hi tries sym natural */
static const trs_spec TSP[] = {
    {0, 0, 0, 2, 1, 1, 0, 0, 0, 0, 0, 1, 2, 1, 0, 100},   /* 0: chapter 1, the way to go */
    {0, 0, 0, 3, 1, 2, 0, 0, 0, 0, 0, 1, 3, 1, 0, 90},    /* 1 */
    {0, 0, 0, 4, 1, 3, 0, 0, 0, 0, 0, 2, 3, 1, 0, 80},    /* 2 */
    {0, 0, 0, 2, 2, 2, 0, 0, 0, 0, 0, 1, 3, 1, 0, 80},    /* 3: two portals */
    {0, 0, 0, 3, 2, 4, 0, 0, 0, 0, 0, 3, 4, 1, 0, 60},    /* 4 */
    {0, 0, 0, 2, 2, 1, 0, 1, 0, 0, 0, 1, 1, 1, 0, 100},   /* 5: a toggle */
    {0, 0, 0, 3, 4, 2, 0, 1, 0, 0, 0, 2, 4, 1, 0, 70},    /* 6 */
    {0, 0, 0, 3, 3, 2, 0, 1, 1, 0, 0, 2, 4, 1, 0, 50},    /* 7: crossings */
    {0, 0, 0, 3, 2, 2, 0, 1, 1, 0, 0, 2, 4, 1, 0, 60},    /* 8 */
    {0, 0, 0, 3, 2, 2, 0, 0, 1, 0, 1, 2, 3, 1, 0, 60},    /* 9: together */
    {0, 0, 0, 3, 1, 2, 0, 0, 1, 0, 1, 2, 4, 1, 0, 50},    /* 10 */
    {0, 0, 0, 2, 2, 2, 0, 1, 0, 1, 0, 2, 4, 1, 0, 60},    /* 11: tunnels */
    {0, 0, 0, 2, 3, 3, 0, 1, 0, 1, 0, 3, 5, 1, 0, 50},    /* 12 */
    {0, 0, 0, 2, 2, 2, 0, 1, 0, 0, 0, 1, 3, 1, 0, 70},    /* 13: the X, its toggle */
    {0, 0, 0, 4, 2, 3, 0, 1, 0, 0, 0, 2, 4, 1, 0, 60},    /* 14: a mountain */
    {0, 0, 0, 4, 2, 3, 0, 0, 0, 0, 1, 1, 3, 1, 0, 70},    /* 15: a meeting, together */
    {0, 0, 0, 3, 3, 3, 0, 1, 0, 1, 0, 2, 5, 1, 0, 50},    /* 16: tunnels, three trains */
    {0, 0, 0, 3, 4, 3, 0, 1, 0, 1, 0, 2, 6, 1, 0, 50},    /* 17: tunnels, four trains (a showpiece) */
};

/* the maker's networks, each chapter's: w h portals stations trains switches loops toggles crossings tunnels together
 * lo hi tries sym natural */
typedef struct { trs_spec sp[4]; int nsp, makes; } chapter;
static const chapter CH[TRS_CHAPTERS] = {
    {{{5, 7, 1, 3, 1, 3, 0, 0, 0, 0, 0, 2, 3, 1, 1, 80}, {7, 7, 1, 4, 1, 3, 0, 0, 0, 0, 0, 2, 3, 1, 1, 70},
      {7, 8, 1, 4, 1, 4, 0, 0, 0, 0, 0, 2, 4, 1, 1, 60}, {6, 7, 1, 4, 1, 3, 0, 0, 0, 0, 0, 2, 3, 1, 0, 60}}, 4, 40000},
    {{{7, 7, 2, 2, 2, 2, 0, 0, 0, 0, 0, 2, 3, 1, 1, 70}, {7, 8, 2, 3, 2, 3, 0, 0, 0, 0, 0, 2, 3, 1, 1, 70},
      {7, 8, 2, 4, 2, 3, 0, 0, 0, 0, 0, 2, 4, 1, 1, 60}, {6, 7, 2, 3, 3, 3, 0, 0, 0, 0, 0, 2, 3, 1, 0, 60}}, 4, 40000},
    {{{7, 7, 1, 3, 3, 2, 0, 1, 0, 0, 0, 1, 3, 1, 1, 70}, {7, 8, 2, 3, 4, 3, 0, 1, 0, 0, 0, 2, 4, 1, 1, 60},
      {7, 8, 2, 4, 4, 3, 0, 1, 0, 0, 0, 2, 4, 1, 1, 60}, {6, 8, 2, 4, 4, 3, 0, 2, 0, 0, 0, 2, 4, 1, 0, 50}}, 4, 40000},
    {{{7, 8, 2, 3, 3, 3, 1, 0, 1, 0, 0, 2, 4, 1, 1, 60}, {7, 8, 2, 4, 4, 4, 1, 1, 1, 0, 0, 3, 5, 1, 1, 50},
      {6, 8, 3, 4, 4, 4, 0, 1, 1, 0, 0, 3, 5, 1, 0, 50}, {7, 8, 1, 3, 3, 3, 1, 1, 1, 0, 0, 3, 5, 1, 1, 50}}, 4, 40000},
    {{{7, 8, 2, 3, 2, 2, 0, 0, 1, 0, 1, 2, 4, 1, 1, 60}, {7, 8, 3, 3, 3, 3, 0, 0, 1, 0, 1, 2, 4, 1, 1, 50},
      {6, 8, 3, 4, 3, 4, 1, 1, 0, 0, 1, 3, 5, 1, 0, 50}, {7, 8, 2, 4, 2, 3, 1, 0, 1, 0, 1, 3, 5, 1, 1, 50}}, 4, 40000},
    {{{7, 9, 2, 4, 4, 4, 1, 1, 1, 1, 0, 3, 5, 1, 1, 50}, {7, 9, 3, 4, 3, 4, 0, 0, 1, 1, 1, 3, 5, 1, 1, 50},
      {7, 9, 2, 5, 5, 5, 1, 2, 0, 1, 0, 3, 6, 1, 1, 50}, {7, 9, 2, 4, 4, 4, 1, 1, 1, 0, 0, 3, 5, 1, 1, 50}}, 4, 40000},
};

/* ---- how hard a puzzle is, measured -----------------------------------------------------------------------------
 * first: the share of the switches whose flip is a mistake at the start (it does not bring a working setting a flip
 * nearer); rare: how rare the working settings are (log2 of the settings over the working ones); naive: the share of
 * 300 naive players who solve it within its par and half again: each sends the trains, watches the first one that
 * goes wrong and flips a switch it ran over (one in four flips: any switch at random) */
typedef struct { int par, ns, nt, sols; double first, rare, naive, score; } metric;

static void measure(const trs_map *m, metric *mt)
{
    mt->par = trs_solve(m, m->set0, NULL, &mt->sols);
    mt->ns = m->ns;
    mt->nt = m->nt;
    mt->rare = log2((double)(1 << m->ns) / (mt->sols > 0 ? mt->sols : 1));
    int bad = 0;
    for (int i = 0; i < m->ns; i++) bad += trs_solve(m, (uint16_t)(m->set0 ^ 1u << i), NULL, NULL) != mt->par - 1;
    mt->first = m->ns ? (double)bad / m->ns : 0;
    uint32_t r = pr_seed(77);
    int wins = 0;
    static trs_run run;
    for (int t = 0; t < 300; t++) {
        uint16_t set = m->set0;
        int ok = 0;
        for (int k = 0; k <= mt->par + mt->par / 2 && !ok; k++) {
            if (trs_simulate(m, set, &run) == TRS_OK) { ok = 1; break; }
            if (k == mt->par + mt->par / 2) break;
            int on[TRS_PATH], non = 0, bt = run.bad_t >= 0 && run.bad_t < m->nt ? run.bad_t : 0;
            for (int j = 0; j < run.len[bt]; j++) {
                int s = trs_switch_at(m, run.cell[bt][j]);
                if (s >= 0) on[non++] = s;
            }
            int s = non && pr_below(&r, 4) ? on[pr_below(&r, non)] : pr_below(&r, m->ns);
            set ^= (uint16_t)(1u << s);
        }
        wins += ok;
    }
    mt->naive = wins / 300.0;
    /* the score: the flips, worth more where the first flips mislead and the naive players fail, the working
     * settings rare, the trains many */
    mt->score = mt->par * (0.6 + mt->first) * (1.2 - mt->naive) * 2 + 0.5 * mt->rare + 0.3 * mt->nt;
}

typedef struct {
    char text[TRS_TEXT], shape[2 * TRS_CELLS + 1];
    int par, src, part, used;                   /* src: the drawing (-1 the maker's) */
    double diff, look;
    metric m;
} cand;

static cand C[MAXC];
static int nc;

static void shape_of(const trs_map *m, int mirror, char *out)   /* the network's squares (kinds and sides) */
{
    static const char HEX[] = "0123456789abcdef";
    for (int y = 0; y < m->h; y++)
        for (int x = 0; x < m->w; x++) {
            int c = (mirror ? m->w - 1 - x : x) + y * m->w, o = m->open[c];
            if (mirror) o = (o & 5) | (o & 2) << 2 | (o & 8) >> 2;
            *out++ = (char)('a' + m->kind[c]);
            *out++ = HEX[o];
        }
    *out = 0;
}

static int sym_shape(const trs_map *m)
{
    char a[2 * TRS_CELLS + 1], b[2 * TRS_CELLS + 1];
    shape_of(m, 0, a);
    shape_of(m, 1, b);
    return !strcmp(a, b) && (m->w & 1);
}

static double look_of(const trs_map *m)
{
    int W = m->w * m->h, used = 0, x0 = m->w, x1 = -1, y0 = m->h, y1 = -1, curves = 0, jog = 0;
    for (int c = 0; c < W; c++) {
        if (m->kind[c] == TRS_EMPTY) continue;
        used++;
        int x = c % m->w, y = c / m->w;
        if (x < x0) x0 = x;
        if (x > x1) x1 = x;
        if (y < y0) y0 = y;
        if (y > y1) y1 = y;
        if (m->kind[c] == TRS_TRACK && m->open[c] != 5 && m->open[c] != 10) {
            curves++;
            for (int d = 0; d < 4; d++) {      /* a curve into a curve both leaving the same way: a jog */
                int b = trs_neighbour(m, c, d);
                if (!(m->open[c] >> d & 1) || b <= c || m->kind[b] != TRS_TRACK || m->open[b] == 5 || m->open[b] == 10)
                    continue;
                if ((m->open[c] & ~(1 << d)) == (m->open[b] & ~(1 << ((d + 2) & 3)))) jog++;
            }
        }
    }
    double cov = (double)used / W, spread = (double)(x1 - x0 + 1) * (y1 - y0 + 1) / W;
    return -4 * fabs(cov - 0.5) + 1.2 * spread - 4.0 * jog / (double)(used + 1) - 0.6 * curves / (double)(used + 1) +
           2.0 * sym_shape(m);
}

/* a drawing as a network (mirrored: flipped left to right); 0: too big */
static int conn(char a, char b)
{
    const char *trk = "#o%+", *end = "PS1234";
    if (a == '.' || b == '.') return 0;
    int ta = strchr(trk, a) != NULL, tb = strchr(trk, b) != NULL;
    if (strchr(end, a) || strchr(end, b)) return ta || tb;
    return a == '%' || b == '%' || a == '+' || b == '+' || a == b;
}

static int drawn(trs_map *m, const tpl *t, int mirror)
{
    memset(m, 0, sizeof *m);
    int h = 0;
    while (t->rows[h]) h++;
    m->w = (int)strlen(t->rows[0]);
    m->h = h;
    if (m->w > TRS_MAXW || h > TRS_MAXH) return 0;
#define AT(x, y) t->rows[y][mirror ? m->w - 1 - (x) : (x)]
    for (int y = 0; y < h; y++)
        for (int x = 0; x < m->w; x++) {
            char ch = AT(x, y);
            int c = x + y * m->w;
            m->kind[c] = ch == '.' ? TRS_EMPTY : ch == 'P' ? TRS_PORTAL : ch == 'S' ? TRS_STATION
                       : ch >= '1' && ch <= '4' ? TRS_TUNNEL : ch == '+' ? TRS_CROSS : TRS_TRACK;
            if (ch == '3' || ch == '4') m->k[c] = 1;   /* the second tunnel */
            if (x + 1 < m->w && conn(ch, AT(x + 1, y))) { m->open[c] |= 2; m->open[c + 1] |= 8; }
            if (y + 1 < h && conn(ch, AT(x, y + 1))) { m->open[c] |= 4; m->open[c + m->w] |= 1; }
        }
#undef AT
    return 1;
}

static int seen(const char *shape)
{
    for (int i = 0; i < nc; i++)
        if (!strcmp(C[i].shape, shape)) return 1;
    return 0;
}

static void add(const trs_map *m, const trs_quality *q, int src, int part)
{
    if (nc >= MAXC) return;
    char sh[2 * TRS_CELLS + 1], mi[2 * TRS_CELLS + 1], text[TRS_TEXT];
    shape_of(m, 0, sh);
    shape_of(m, 1, mi);
    trs_text(m, text);
    if (src < 0 && (seen(sh) || seen(mi))) return;   /* a laid network met before (a drawing's are drawn twice) */
    for (int i = 0; i < nc; i++)
        if (!strcmp(C[i].text, text)) return;
    cand *c = &C[nc++];
    strcpy(c->text, text);
    strcpy(c->shape, sh);
    c->par = q->par;
    c->src = src;
    c->part = part;
    c->used = 0;
    measure(m, &c->m);
    c->diff = c->m.score;
    c->look = look_of(m) + (src >= 0 ? 0.8 : 0);
}

static int by_diff(const void *a, const void *b)
{
    double d = ((const cand *)a)->diff - ((const cand *)b)->diff;
    return d < 0 ? -1 : d > 0;
}

/* the best-looking free network from..to: a drawing twice at most, six levels apart at least, its showpiece and
 * openings kept for their places */
static int last_at[64], slot;
static int pick(int from, int to, int prev_src, const int *uses)
{
    int best = -1;
    (void)prev_src;
    for (int j = from; j <= to; j++) {
        if (j < 0 || j >= nc || C[j].used || C[j].part) continue;
        if (C[j].m.naive > naive_max || C[j].m.score < floor_min) continue;
        if (C[j].src >= 0 && (uses[C[j].src] >= 2 || slot - last_at[C[j].src] < 6)) continue;
        if (best < 0 || C[j].look > C[best].look) best = j;
    }
    return best;
}

#ifdef TRS_TABLE                                /* the shipped levels' table: -DTRS_TABLE */
#include "games/trains_levels.h"
int main(void)
{
    (void)TPL, (void)TSP, (void)CH, (void)by_diff, (void)look_of, (void)drawn, (void)add, (void)pick, (void)NAIVE,
        (void)FLOOR;
    double sum[TRS_CHAPTERS][5] = {{0}};
    int trivial = 0;
    printf("lvl par sw trains first rare naive score  network\n");
    for (int lv = 0; lv < TRS_LEVELS; lv++) {
        trs_map m;
        metric mt;
        trs_parse(&m, trs_levels[lv]);
        measure(&m, &mt);
        printf("%4d %3d %2d %2d %5.2f %5.2f %5.2f %6.2f  %s\n", lv + 1, mt.par, mt.ns, mt.nt, mt.first, mt.rare,
               mt.naive, mt.score, trs_levels[lv]);
        int triv = mt.naive > 0.25 || mt.par <= 1;
        double *s = sum[lv / PER];
        s[0] += mt.par; s[1] += mt.first; s[2] += mt.naive; s[3] += mt.score; s[4] += triv;
        trivial += lv >= 3 && triv;
    }
    printf("\nchapter  par first naive score trivial\n");
    for (int c = 0; c < TRS_CHAPTERS; c++)
        printf("%7d %5.1f %5.2f %5.2f %5.1f %4.0f\n", c + 1, sum[c][0] / PER, sum[c][1] / PER, sum[c][2] / PER,
               sum[c][3] / PER, sum[c][4]);
    printf("trivial levels from level 4 (naive > 0.25 or par <= 1): %d\n", trivial);
    return 0;
}
#else
int main(void)
{
    static char out[TRS_LEVELS][TRS_TEXT];
    int par[TRS_LEVELS], k = 0, uses[64] = {0};
    for (int i = 0; i < 64; i++) last_at[i] = -100;
    for (int ch = 0; ch < TRS_CHAPTERS; ch++) {
        nc = 0;
        naive_max = NAIVE[ch];
        floor_min = FLOOR[ch];
        uint32_t rs = pr_seed(1000u + 7919u * (uint32_t)ch);
        for (int t = 0; TPL[t].ch >= 0; t++) {  /* the drawings, as drawn and mirrored */
            if (TPL[t].ch != ch) continue;
            int made = 0, fin = 0, col = 0;
            for (int mirror = 0; mirror < 2; mirror++)
                for (int a = 0; a < 600; a++) {
                    trs_map m;
                    trs_quality q;
                    trs_spec sp = TSP[TPL[t].sp];
                    lay_ctx L;
                    memset(&L, 0, sizeof L);
                    L.rs = rs;
                    if (!drawn(&m, &TPL[t], mirror)) { fprintf(stderr, "drawing %d: too big\n", t); return 1; }
                    sp.w = m.w;
                    sp.h = m.h;
                    L.sym = sym_shape(&m);
                    int ok = finish(&m, &sp, &L);
                    if (!ok && a == 0 && mirror == 0) { fprintf(stderr, "drawing %d: not a network\n", t); return 1; }
                    fin += ok;
                    ok = ok && colour_and_set(&m, &sp, &L, &q);
                    col += ok;
                    rs = L.rs;
                    if (ok && q.par >= sp.lo && q.par <= sp.hi) { add(&m, &q, t, TPL[t].part); made++; }
                }
            if (!made) fprintf(stderr, "drawing %d: no puzzle (%d finished, %d coloured)\n", t, fin, col);
        }
        int drawn_n = nc, nsym = 0;
        for (int t = 0; t < CH[ch].makes && nc < MAXC; t++) {
            const trs_spec *sp = &CH[ch].sp[t % CH[ch].nsp];
            trs_map m;
            trs_quality q;
            if (!make_one(&m, sp, &rs, &q) || q.par < sp->lo || q.par > sp->hi) continue;
            add(&m, &q, -1, 0);
            nsym += sym_shape(&m);
        }
        if (nc < PER) { fprintf(stderr, "chapter %d: only %d networks\n", ch + 1, nc); return 1; }
        qsort(C, (size_t)nc, sizeof C[0], by_diff);
        fprintf(stderr, "chapter %d (%d mirrored): %d drawn, %d laid, difficulty %.1f .. %.1f\n", ch + 1, nsym, drawn_n, nc - drawn_n,
                C[0].diff, C[nc - 1].diff);
        int prev = -2, chosen[PER], open = 0;
        /* the openings: the opening drawings in their order, each its easiest puzzle */
        for (int t = 0; TPL[t].ch >= 0; t++) {
            if (TPL[t].ch != ch || TPL[t].part != 1) continue;
            int best = -1;
            for (int j = 0; j < nc; j++)
                if (C[j].src == t && !C[j].used && (best < 0 || C[j].diff < C[best].diff)) best = j;
            if (best < 0) continue;
            C[best].used = 1;
            uses[t]++;
            chosen[open++] = best;
            prev = t;
        }
        /* the showpiece: the hardest puzzle of the chapter's showpiece drawing; in chapters 3 and 6 only if among the
         * hardest sixteenth, else the best-looking there (a showpiece drawing's first, then the harder) */
        int show = -1;
        for (int j = 0; j < nc; j++)
            if (C[j].part == 2 && !C[j].used && (show < 0 || C[j].diff > C[show].diff)) show = j;
        double hard = C[nc - 1 - nc / 16].diff;
        if ((ch == 2 || ch == 5) && show >= 0 && C[show].diff < hard) {   /* chapters 3 and 6 */
            int best = -1;
            for (int j = nc - 1; j >= 0 && C[j].diff >= hard; j--)
                if (!C[j].used && C[j].part != 1 &&
                    (best < 0 || C[j].look + (C[j].part == 2) + 0.1 * C[j].diff >
                                     C[best].look + (C[best].part == 2) + 0.1 * C[best].diff))
                    best = j;
            if (best >= 0) show = best;
        }
        if (show < 0) show = pick(nc - nc / 12 - 2, nc - 1, -2, uses);
        C[show].used = 1;
        if (C[show].src >= 0) uses[C[show].src]++;
        static int H[MAXC];                     /* the hard ones, by score */
        int nh = 0, laid = 0;
        for (int j = 0; j < nc; j++)
            if (!C[j].part && C[j].m.naive <= naive_max && C[j].m.score >= floor_min) {
                H[nh++] = j;
                laid += C[j].src < 0;
            }
        fprintf(stderr, "  %d hard networks, %d laid\n", nh, laid);
        if (nh < PER) { fprintf(stderr, "chapter %d: too few hard networks\n", ch + 1); return 1; }
        for (int i = open; i < PER - 1; i++) {  /* along the curve: the best-looking near its place */
            slot = k + i;
            double f = (double)(i - open) / (PER - 1 - open);
            int p = (int)(f * (nh - 1) + 0.5), w = nh / (PER - open) + 1;   /* its place among the hard ones */
            int at = H[p], lo = H[p - w < 0 ? 0 : p - w], hi = H[p + w > nh - 1 ? nh - 1 : p + w];
            int best = pick(lo, hi, prev, uses);
            for (int w2 = hi - lo + 1; best < 0 && w2 < 2 * nc; w2 *= 2) best = pick(at - w2, at + w2, prev, uses);
            if (best < 0) { fprintf(stderr, "chapter %d: ran out\n", ch + 1); return 1; }
            C[best].used = 1;
            if (C[best].src >= 0) { uses[C[best].src]++; last_at[C[best].src] = slot; }
            prev = C[best].src;
            chosen[i] = best;
        }
        chosen[PER - 1] = show;
        for (int i = 0; i < PER; i++) {
            strcpy(out[k], C[chosen[i]].text);
            par[k++] = C[chosen[i]].par;
            fprintf(stderr, "  %3d: par %d diff %4.1f look %5.2f %s%d\n", k, C[chosen[i]].par, C[chosen[i]].diff,
                    C[chosen[i]].look, C[chosen[i]].src >= 0 ? "drawn " : "laid", C[chosen[i]].src >= 0 ? C[chosen[i]].src : 0);
        }
    }
    printf("/* Train Switches' levels: made by tools/trains_gen.c (our own networks, drawn and found by its search; do\n"
           " * not edit). Each network as text (trains.h), and its par: the fewest flips. */\n");
    printf("const char *const trs_levels[TRS_LEVELS] = {\n");
    for (int i = 0; i < TRS_LEVELS; i++) printf("    \"%s\",\n", out[i]);
    printf("};\n\nconst uint8_t trs_level_par[TRS_LEVELS] = {");
    for (int i = 0; i < TRS_LEVELS; i++) printf("%s%d,", i % 25 ? " " : "\n    ", par[i]);
    printf("\n};\n");
    return 0;
}
#endif
