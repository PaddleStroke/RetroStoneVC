/*
 * Flood's levels, drawn and chosen: src/games/flood_levels.h. Our own boards, drawn by this tool and judged by the
 * game's own solver (flood_rules.c). The boards are pictures, not noise:
 *   - the styles: patches (coloured countries round scattered sites), rings (square, diamond or round, round a
 *     centre), stripes (straight or slanted, a little wavy), waves, spirals, bricks and plaid;
 *   - a symmetry for most (mirrored on the diagonal through the starting corner, left to right, a half turn or a
 *     quarter turn), their edges a little ragged so the shapes look drawn by hand;
 *   - taste: every colour used fairly, few lone cells, the starting patch small;
 *   - each candidate's par: the shortest way a wide beam search finds, then an exact search (iterative deepening)
 *     that proves it the fewest when it ends within its budget (the comment says "proved", else "best found"); the
 *     small boards (6 x 6 and less) are always proved;
 *   - its work: the par, and how many moves the plain greedy way (the most cells each move) wastes beyond it; from
 *     the third chapter on a board where greedy play is as good as any is left out (no thinking needed);
 *   - near-duplicates dropped (the same board with its colours renamed, or mirrored on its diagonal: the same game).
 * Each chapter has its makes (a size, the colours, how many) and its levels are taken in turn from each style, evenly
 * along the ranked work; the chapter then goes from its easiest to its hardest, and ends on a showpiece: a board with
 * a quarter-turn or two-mirror symmetry, among the hardest of its biggest size. 5 x 5 with 3 colours first, up to
 * 14 x 14 with 6.
 * Deterministic (fixed seeds): run it again after a change to the solver and check the diff.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/fldgen apps/boardgames/tools/flood_gen.c \
 *       apps/boardgames/src/games/flood_rules.c -lm && /tmp/fldgen > apps/boardgames/src/games/flood_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/flood.h"
#include "games/puzrand.h"

#define WIDTH 1500                              /* the beam's width */
#define EXACT 3000000L                          /* the exact search's nodes */

enum { ST_PATCHES, ST_RINGS, ST_STRIPES, ST_WAVES, ST_SPIRAL, ST_BRICKS, ST_PLAID, NST };
static const char *const ST_NAME[NST] = {"patches", "rings", "stripes", "waves", "spiral", "bricks", "plaid"};
enum { SY_NONE, SY_DIAG, SY_MIRROR, SY_ROT2, SY_ROT4, SY_BOTH, NSY };   /* SY_BOTH: mirrored both ways */
static const char *const SY_NAME[NSY] = {"", ", diagonal", ", mirrored", ", half turn", ", quarter turn",
                                         ", two mirrors"};

typedef struct {
    fld_board b;
    uint8_t way[FLD_MOVES];
    int par, greedy, proved, style, sym;
    double work;
    char key[FLD_CELLS + 1];
} cand;

typedef struct { int n, k, count; float lo, hi; } make;

/* the last make of each chapter gives one level fewer: the showpiece takes its place */
static const make CHAPTER[FLD_CHAPTERS][3] = {
    {{5, 3, 10, 0.0f, 0.5f}, {6, 3, 15, 0.1f, 0.8f}},
    {{6, 4, 10, 0.2f, 0.8f}, {7, 4, 15, 0.2f, 0.9f}},
    {{8, 4, 10, 0.2f, 0.9f}, {9, 5, 15, 0.2f, 0.9f}},
    {{10, 5, 25, 0.1f, 0.95f}},
    {{11, 5, 10, 0.2f, 0.9f}, {12, 6, 15, 0.2f, 0.95f}},
    {{13, 6, 10, 0.3f, 0.95f}, {14, 6, 15, 0.3f, 1.0f}},
};

/* ---- drawing a board ---------------------------------------------------------------------------------------- */
static float frand(uint32_t *s) { return (pr_next(s) % 1000000u) / 1000000.0f; }

/* the colours of the bands: never the same twice running, rarely the one before when there are more than three */
static void sequence(uint32_t *s, int k, uint8_t *seq, int len)
{
    for (int i = 0; i < len; i++) {
        int c;
        do c = pr_below(s, k);
        while ((i > 0 && c == seq[i - 1]) || (k > 3 && i > 1 && c == seq[i - 2] && pr_below(s, 3)));
        seq[i] = (uint8_t)c;
    }
}

static int band_of(float v, int len)
{
    int b = (int)floorf(v) % len;
    return b < 0 ? b + len : b;
}

/* detail 0..1: the later chapters finer (thinner bands, more patches, more ragged edges) */
static void draw(fld_board *b, int n, int k, int style, int sym, float detail, uint32_t *s)
{
    uint8_t seq[64], seq2[64];
    sequence(s, k, seq, 64);
    sequence(s, k, seq2, 64);
    memset(b, 0, sizeof *b);
    b->n = n;
    b->k = k;
    int centred = sym != SY_NONE;
    float cx = centred ? (n - 1) / 2.0f : 1 + frand(s) * (n - 3), cy = centred ? (n - 1) / 2.0f : 1 + frand(s) * (n - 3);
    if (sym == SY_DIAG && pr_below(s, 2)) cx = cy = (n - 1) * (0.25f + 0.5f * frand(s));   /* on the diagonal */
    float w = n <= 6 ? 1 : 1 + frand(s) * (1 - detail) * (n >= 11 ? 1.2f : 0.8f), ph = frand(s) * 4;
    int metric = pr_below(s, 3);
    float fq = 0.4f + frand(s) * 0.5f, amp = 0.6f + frand(s) * 1.6f, ph2 = frand(s) * 6.28f;
    int dir = pr_below(s, 4), arms = 1 + pr_below(s, 2), bw = 2 + pr_below(s, n >= 9 ? 2 : 1), bh = 1 + pr_below(s, 2);
    /* the patches' sites */
    int ns = (int)(n * n / (n <= 6 ? 3 : 4 + pr_below(s, 3) - 2.2f * detail)), sx[FLD_CELLS], sy[FLD_CELLS];
    float sw[FLD_CELLS];
    uint8_t sc[FLD_CELLS];
    for (int i = 0; i < ns; i++) {
        sx[i] = pr_below(s, n), sy[i] = pr_below(s, n), sc[i] = (uint8_t)pr_below(s, k);
        sw[i] = 0.7f + 0.6f * frand(s);
    }
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++) {
            float dx = x - cx, dy = y - cy, d, v;
            int c = 0;
            switch (style) {
            case ST_PATCHES: {
                float best = 1e9f;
                for (int i = 0; i < ns; i++) {
                    float ex = (float)(x - sx[i]), ey = (float)(y - sy[i]);
                    float e = (fabsf(ex) + fabsf(ey) + sqrtf(ex * ex + ey * ey)) * sw[i];
                    if (e < best) best = e, c = sc[i];
                }
                break;
            }
            case ST_RINGS:
                d = metric == 0 ? fmaxf(fabsf(dx), fabsf(dy)) : metric == 1 ? fabsf(dx) + fabsf(dy)
                                                                            : sqrtf(dx * dx + dy * dy);
                c = seq[band_of(d / w + ph, 64)];
                break;
            case ST_STRIPES: {
                static const float DX[4] = {1, 1, 1, 2}, DY[4] = {0, 1, -1, 1};
                v = (x * DX[dir] + y * DY[dir]) / (dir == 3 ? 2.0f : 1.0f);
                v += 0.6f * sinf(fq * (x * DY[dir] - y * DX[dir]) + ph2);
                c = seq[band_of(v / w + ph, 64)];
                break;
            }
            case ST_WAVES:
                v = (dir & 1 ? x : y) + amp * sinf(fq * (dir & 1 ? y : x) + ph2);
                c = seq[band_of(v / (w + 0.4f) + ph, 64)];
                break;
            case ST_SPIRAL:
                d = sqrtf(dx * dx + dy * dy);
                v = d / (w + 0.3f) + arms * (atan2f(dy, dx) / 6.2832f + 0.5f) * 2;
                c = seq[band_of(v + ph, 64)];
                break;
            case ST_BRICKS: {
                int row = y / bh, col = (x + (row & 1) * (bw / 2 + 1)) / bw;
                c = seq[band_of((float)(row * 7 + col * 3), 64)];
                if (pr_below(s, 4) == 0) c = seq2[band_of((float)(row + col), 64)];
                break;
            }
            default: {                          /* ST_PLAID: the rows' bands crossing the columns' */
                int rb = band_of((y + ph) / (w + 0.6f), 64), cb = band_of((x + ph) / (w + 0.6f), 64);
                c = (rb + cb) & 1 ? seq[rb] : seq2[cb];
                break;
            }
            }
            b->c[y * n + x] = (uint8_t)c;
        }
    /* ragged edges: a few cells take a neighbour's colour */
    float rag = 0.03f + 0.08f * detail + frand(s) * 0.08f;
    uint8_t was[FLD_CELLS];
    memcpy(was, b->c, sizeof was);
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++) {
            if (frand(s) >= rag) continue;
            int d = pr_below(s, 4), xx = x + (d == 0) - (d == 1), yy = y + (d == 2) - (d == 3);
            if (xx >= 0 && yy >= 0 && xx < n && yy < n) b->c[y * n + x] = was[yy * n + xx];
        }
    /* the symmetry: each cell copies the first of its images */
    memcpy(was, b->c, sizeof was);
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++) {
            int ix[8], iy[8], m = 0;
            ix[m] = x, iy[m++] = y;
            if (sym == SY_DIAG) ix[m] = y, iy[m++] = x;
            if (sym == SY_MIRROR || sym == SY_BOTH) ix[m] = n - 1 - x, iy[m++] = y;
            if (sym == SY_BOTH) ix[m] = x, iy[m++] = n - 1 - y, ix[m] = n - 1 - x, iy[m++] = n - 1 - y;
            if (sym == SY_ROT2) ix[m] = n - 1 - x, iy[m++] = n - 1 - y;
            if (sym == SY_ROT4)
                ix[m] = n - 1 - y, iy[m++] = x, ix[m] = n - 1 - x, iy[m++] = n - 1 - y, ix[m] = y, iy[m++] = n - 1 - x;
            int best = 0;
            for (int i = 1; i < m; i++)
                if (iy[i] * n + ix[i] < iy[best] * n + ix[best]) best = i;
            b->c[y * n + x] = was[iy[best] * n + ix[best]];
        }
}

/* taste: every colour fairly used, few lone cells, a small starting patch */
static int tasteful(const fld_board *b)
{
    int n = b->n, cells = n * n, used[FLD_COLOURS] = {0}, lone = 0;
    for (int i = 0; i < cells; i++) used[b->c[i]]++;
    for (int c = 0; c < b->k; c++)
        if (used[c] < cells / (2 * b->k)) return 0;
    for (int i = 0; i < cells; i++) {
        int x = i % n, y = i / n, same = 0;
        same += x > 0 && b->c[i - 1] == b->c[i];
        same += x < n - 1 && b->c[i + 1] == b->c[i];
        same += y > 0 && b->c[i - n] == b->c[i];
        same += y < n - 1 && b->c[i + n] == b->c[i];
        lone += !same;
    }
    if (lone * 100 > cells * (n <= 6 ? 30 : 14)) return 0;
    uint8_t in[FLD_CELLS];
    return fld_region(b, in) * 100 <= cells * 12;
}

/* ---- choosing ----------------------------------------------------------------------------------------------- */
/* the board's key: its colours renamed in order of first sight; the smaller of it and its mirror's */
static void key_of(const fld_board *b, char *out)
{
    char k[2][FLD_CELLS + 1];
    int n = b->n;
    for (int m = 0; m < 2; m++) {
        int name[FLD_COLOURS], next = 0;
        for (int c = 0; c < FLD_COLOURS; c++) name[c] = -1;
        for (int i = 0; i < n * n; i++) {
            int c = m ? b->c[i % n * n + i / n] : b->c[i];
            if (name[c] < 0) name[c] = next++;
            k[m][i] = (char)('a' + name[c]);
        }
        k[m][n * n] = 0;
    }
    strcpy(out, strcmp(k[0], k[1]) <= 0 ? k[0] : k[1]);
}

static void judge(cand *c)
{
    c->par = fld_solve(&c->b, c->way, FLD_MOVES, WIDTH, EXACT, &c->proved);
    c->greedy = fld_greedy(&c->b);
    c->work = c->par + 1.5 * (c->greedy - c->par);
}

static int by_work(const void *a, const void *b)
{
    const cand *x = a, *y = b;
    return x->work < y->work ? -1 : x->work > y->work ? 1 : strcmp(x->key, y->key);
}

static cand chosen[FLD_LEVELS];
static int nchosen;

static int known(const char *key)
{
    for (int i = 0; i < nchosen; i++)
        if (!strcmp(chosen[i].key, key)) return 1;
    return 0;
}

static uint32_t seed = 20261017u;

/* a candidate of a size in a style (sym -1: one by chance); 0 when it fails the taste or the chapter's need */
static int candidate(cand *c, int n, int k, int style, int sym, int chapter)
{
    uint32_t s = pr_seed(seed++);
    if (sym < 0) {
        static const int SYMS[10] = {SY_NONE, SY_NONE, SY_DIAG, SY_DIAG, SY_DIAG, SY_MIRROR, SY_ROT2, SY_ROT2,
                                     SY_ROT4, SY_BOTH};
        sym = SYMS[pr_below(&s, 10)];
    }
    draw(&c->b, n, k, style, sym, chapter / (FLD_CHAPTERS - 1.0f), &s);
    if (!tasteful(&c->b)) return 0;
    key_of(&c->b, c->key);
    if (known(c->key)) return 0;
    c->style = style;
    c->sym = sym;
    judge(c);
    if (n <= 6 && !c->proved) return 0;
    if (chapter >= 2 && c->greedy == c->par) return 0;
    return c->par >= 3;
}

int main(void)
{
    static cand show;
    for (int ch = 0; ch < FLD_CHAPTERS; ch++) {
        int first = nchosen, nm = 0;
        while (nm < 3 && CHAPTER[ch][nm].count) nm++;
        for (int m = 0; m < nm; m++) {
            const make *mk = &CHAPTER[ch][m];
            int count = mk->count - (m == nm - 1), per = 4 * count / NST + 2, nst[NST] = {0};
            cand *cs = calloc((size_t)per * NST, sizeof(cand));
            if (!cs) return 1;
            for (int st = 0; st < NST; st++) {   /* each style's candidates, ranked */
                cand *own = cs + st * per;
                for (int tries = 0; nst[st] < per && tries < 80 * per; tries++) {
                    if (candidate(&own[nst[st]], mk->n, mk->k, st, -1, ch)) {
                        int dup = 0;
                        for (int j = 0; j < nst[st] && !dup; j++) dup = !strcmp(own[j].key, own[nst[st]].key);
                        if (!dup) nst[st]++;
                    }
                    fprintf(stderr, "\rchapter %d make %d style %d: %d/%d   ", ch + 1, m + 1, st, nst[st], per);
                }
                qsort(own, (size_t)nst[st], sizeof(cand), by_work);
            }
            /* the levels in turn from each style, each at its place along the work */
            for (int i = 0; i < count; i++) {
                int st = (i + ch) % NST, at = -1;
                for (int t = 0; t < NST && at < 0; t++, st = (st + 1) % NST) {
                    cand *own = cs + st * per;
                    if (!nst[st]) continue;
                    float q = mk->lo + (mk->hi - mk->lo) * (count > 1 ? (float)i / (count - 1) : 0);
                    int want = (int)(q * (nst[st] - 1) + 0.5f);
                    for (int d = 0; d < nst[st] && at < 0; d++) {
                        if (want + d < nst[st] && !known(own[want + d].key)) at = want + d;
                        else if (want - d >= 0 && !known(own[want - d].key)) at = want - d;
                    }
                    if (at >= 0) chosen[nchosen++] = own[at];
                }
                if (at < 0) return 1;
            }
            free(cs);
        }
        qsort(chosen + first, (size_t)(nchosen - first), sizeof(cand), by_work);
        /* the showpiece: of the biggest size, a quarter turn or two mirrors, the nearest the chapter's hardest */
        const make *big = &CHAPTER[ch][nm - 1];
        double top = chosen[nchosen - 1].work;
        int have = 0;
        for (int t = 0; t < 600 && have < 12; t++) {
            static const int SHOW[4] = {ST_RINGS, ST_PATCHES, ST_SPIRAL, ST_PLAID};
            cand c;
            if (!candidate(&c, big->n, big->k, SHOW[t % 4], t % 3 ? SY_ROT4 : SY_BOTH, ch)) continue;
            if (!have++ || fabs(c.work - top) < fabs(show.work - top)) show = c;
            fprintf(stderr, "\rchapter %d showpiece: %d   ", ch + 1, have);
        }
        if (!have) return 1;
        chosen[nchosen++] = show;
    }
    fprintf(stderr, "\n");
    if (nchosen != FLD_LEVELS) return 1;
    printf("/*\n"
           " * Flood's levels: our own boards, drawn by tools/flood_gen.c (patches, rings, stripes, waves, spirals,\n"
           " * bricks and plaid, most of them symmetric) and judged by the game's own solver, six chapters of 25 from\n"
           " * the easiest, each ending on a showpiece. Each: \"<n>:<k>:\" the cells (a colour 0..k-1 each, row by\n"
           " * row), \":\" the best way found (a colour a move: its length is the par). In the comment: its size and\n"
           " * colours, its style, the par (proved the fewest by the exact search, or the best found), the greedy way.\n"
           " * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n */\n");
    printf("const char *const fld_levels[FLD_LEVELS] = {\n");
    for (int i = 0; i < nchosen; i++) {
        char s[FLD_CELLS + FLD_MOVES + 16];
        cand *c = &chosen[i];
        fld_encode(&c->b, c->way, c->par, s, sizeof s);
        printf("    \"%s\", /* %d: %dx%d %d colours, %s%s, par %d %s, greedy %d */\n", s, i + 1, c->b.n, c->b.n,
               c->b.k, ST_NAME[c->style], SY_NAME[c->sym], c->par, c->proved ? "proved" : "best found", c->greedy);
    }
    printf("};\n");
    return 0;
}
