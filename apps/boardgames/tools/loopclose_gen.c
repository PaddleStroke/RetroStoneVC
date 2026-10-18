/*
 * Close the Loops' levels: src/games/loopclose_levels.h. Our own boards: no published or commercial set is used.
 * Each chapter has its idea (small boards of plain loops; tees and crosses; ends; bigger boards; quarter-turn
 * mandalas on square boards; all of it on the largest boards) and makes many with the rules' maker (lcl_make, from
 * fixed seeds: a symmetric design of closed shapes, its tiles turned at random). A board is kept when it has the
 * chapter's feature and its design was not met before (turned or mirrored either). The difficulty is measured
 * (measure()): the tiles that can turn into more than one shape, the fewest taps (the par, by the rules' exact
 * search), how many rounds of the plainest logic (dropping the turns no neighbour can meet) it takes to settle the
 * tiles and how many it leaves, less when many ways close it. Each chapter keeps 25: up to two easy ones first
 * (chapter 1's first, the fewest taps of all) that show its new tiles, then boards no chore (16 tiles to turn or
 * more, and more than two rounds of that logic), spread from the chapter's floor to its hardest; of the boards near
 * each place on the curve the one that looks best is kept (the board well used, shapes of several kinds, another
 * size than the level before); the chapter's last level, its showpiece, the best looking of its hardest few. The
 * boards grow sooner than they did. Chapter 1 leaves some tiles as they are, so its first boards take a few taps.
 * Deterministic (fixed seeds).
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/lclgen apps/boardgames/tools/loopclose_gen.c -lm && \
 *       /tmp/lclgen > apps/boardgames/src/games/loopclose_levels.h
 * The levels' difficulty table: the same with -DLCL_TABLE, run with no output file.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#define LCL_GEN
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/loopclose_rules.c"

#define PER (LCL_LEVELS / LCL_CHAPTERS)
#define MAKES 500                               /* boards kept a chapter, at most */

typedef struct { int active, par, depth; long ways; double prop, score; } metric;   /* measure() */
#define TRIVIAL(mt) ((mt)->active < 16 || ((mt)->prop >= 0.999 && (mt)->depth < 3))

typedef struct {
    char text[LCL_TEXT];
    int par, cells, w, h;
    double diff, look;
    metric m;
} cand;

/* a chapter: the sizes (w .. w + gw, h .. h + gh; square: h = w), the symmetries (bits), fill, walls, stubs; need:
 * 0 plain loops only, 1 tees or crosses, 2 ends, 3 any; intro: its first levels, easy ones that show its new tiles
 * (chapter 1 from the very easiest); floor: the least score (measure()) of the others */
typedef struct {
    int w, gw, h, gh, square, syms, fill_lo, fill_hi, walls, stubs, need, intro, from_easiest;
    double floor;
} chapter;

static const chapter CH[LCL_CHAPTERS] = {
    {3, 3, 3, 4, 0, 3, 45, 70, 0, 0, 0, 2, 1, 0},
    {5, 2, 5, 3, 0, 11, 45, 65, 30, 0, 1, 1, 0, 0},
    {6, 2, 6, 3, 0, 11, 40, 60, 10, 30, 2, 1, 0, 0},
    {7, 2, 8, 3, 0, 11, 40, 60, 15, 15, 3, 0, 0, 0},
    {8, 2, 8, 2, 1, 4, 40, 60, 15, 10, 3, 0, 0, 0},
    {9, 1, 11, 2, 0, 11, 40, 60, 15, 15, 3, 0, 0, 0},
};

static int by_diff(const void *a, const void *b)
{
    double d = ((const cand *)a)->diff - ((const cand *)b)->diff;
    return d < 0 ? -1 : d > 0 ? 1 : strcmp(((const cand *)a)->text, ((const cand *)b)->text);
}

/* ---- near-duplicates: the design (each tile's kind, however turned) under the eight turns and mirrors ---------- */
static char (*kept)[LCL_TEXT];
static int nkept;

static int kind(int m)                          /* the smallest of its turns */
{
    int k = m;
    for (int r = 0; r < 4; r++, m = lcl_rot(m))
        if (m < k) k = m;
    return k;
}

static void form_of(const lcl_map *m, int tr, char *out)
{
    int w = tr & 4 ? m->h : m->w, h = tr & 4 ? m->w : m->h, n = 0;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int sx = tr & 4 ? y : x, sy = tr & 4 ? x : y;
            if (tr & 1) sx = m->w - 1 - sx;
            if (tr & 2) sy = m->h - 1 - sy;
            out[n++] = "0123456789abcdef"[kind(m->start[sx + sy * m->w])];
        }
        out[n++] = '|';
    }
    out[n] = 0;
}

static int met(const lcl_map *m)
{
    char l[8][LCL_TEXT];
    for (int tr = 0; tr < 8; tr++) form_of(m, tr, l[tr]);
    for (int i = 0; i < nkept; i++)
        for (int tr = 0; tr < 8; tr++)
            if (!strcmp(kept[i], l[tr])) return 1;
    strcpy(kept[nkept++], l[0]);
    return 0;
}

static int degree(int m) { return (m & 1) + (m >> 1 & 1) + (m >> 2 & 1) + (m >> 3 & 1); }

static int has(const lcl_map *m, int need, double *look)
{
    int n[5] = {0};
    for (int c = 0; c < m->w * m->h; c++) n[degree(m->start[c])]++;
    int kinds = (n[1] > 0) + (n[2] > 0) + (n[3] > 0) + (n[4] > 0);
    *look = 1.0 - (double)n[0] / (m->w * m->h) + 0.15 * kinds;
    if (need == 0) return !n[1] && !n[3] && !n[4];
    if (need == 1) return n[3] + n[4] >= 2 && !n[1];
    if (need == 2) return n[1] >= 2;
    return n[1] + n[3] + n[4] >= 3;
}

/* ---- how hard a board is, measured ------------------------------------------------------------------------------
 * active: the tiles that can be turned into more than one shape; prop: the share of them the plainest logic settles
 * (a turn dropped when some arm of it, or some side without one, can meet nothing that the neighbour, or the edge,
 * still allows; again and again); ways: the closing ways. A board the plainest logic settles is a chore, not a
 * puzzle, unless it is large; one where that logic stalls with few closing ways asks for real deduction. */
static int arms_at(const lcl_map *m, int c, int r)
{
    int a = m->start[c];
    while (r-- > 0) a = lcl_rot(a);
    return a;
}

static void measure(const lcl_map *m, metric *mt)
{
    static const int DXL[4] = {0, 1, 0, -1}, DYL[4] = {-1, 0, 1, 0};
    int n = m->w * m->h;
    uint8_t opt[LCL_CELLS];
    mt->active = 0;
    for (int c = 0; c < n; c++) {
        opt[c] = 0;
        for (int r = 0; r < 4; r++) {
            int dup = 0;
            for (int q = 0; q < r; q++) dup |= (opt[c] >> q & 1) && arms_at(m, c, q) == arms_at(m, c, r);
            if (!dup) opt[c] |= (uint8_t)(1 << r);
        }
        mt->active += __builtin_popcount(opt[c]) > 1;
    }
    mt->depth = 0;
    for (int changed = 1; changed; mt->depth += changed) {   /* in rounds: each from what the last one left */
        changed = 0;
        uint8_t was[LCL_CELLS];
        memcpy(was, opt, sizeof was);
        for (int c = 0; c < n; c++)
            for (int r = 0; r < 4; r++) {
                if (!(was[c] >> r & 1)) continue;
                int a = arms_at(m, c, r), ok = 1;
                for (int d = 0; d < 4 && ok; d++) {
                    int need = a >> d & 1, x = c % m->w + DXL[d], y = c / m->w + DYL[d];
                    if (x < 0 || y < 0 || x >= m->w || y >= m->h) { ok = !need; continue; }
                    int b = x + y * m->w, meet = 0;
                    for (int q = 0; q < 4 && !meet; q++)
                        meet = (was[b] >> q & 1) && (arms_at(m, b, q) >> ((d + 2) & 3) & 1) == need;
                    ok = meet;
                }
                if (!ok) { opt[c] &= (uint8_t)~(1 << r); changed = 1; }
            }
    }
    int settled = 0;
    for (int c = 0; c < n; c++) {
        int k = 0;
        for (int r = 0; r < 4; r++) k += opt[c] >> r & 1;
        settled += k == 1;
    }
    settled -= n - mt->active;                  /* (the tiles of one shape only were settled from the start) */
    mt->prop = mt->active ? (double)settled / mt->active : 1;
    long ways;
    mt->par = lcl_solve(m->w, m->h, m->start, NULL, &ways);
    mt->ways = ways;
    /* the score: the tiles to turn, the taps, the logic that stalls, the closing ways few */
    mt->score = 0.25 * mt->active + 0.3 * mt->par + 8 * (1 - mt->prop) + 1.5 * mt->depth -
                1.5 * log10((double)(ways > 0 ? ways : 1));
}

#ifdef LCL_TABLE                                /* the shipped levels' table: -DLCL_TABLE */
#include "games/loopclose_levels.h"
int main(void)
{
    (void)CH, (void)by_diff, (void)met, (void)has;
    double sum[LCL_CHAPTERS][5] = {{0}};
    int trivial = 0;
    printf("lvl squares active par ways prop depth score  board\n");
    for (int lv = 0; lv < LCL_LEVELS; lv++) {
        lcl_map m;
        metric mt;
        lcl_parse(&m, lcl_levels[lv].board);
        measure(&m, &mt);
        printf("%4d %4d %4d %4d %6ld %5.2f %3d %6.2f  %s\n", lv + 1, m.w * m.h, mt.active, mt.par, mt.ways, mt.prop,
               mt.depth, mt.score, lcl_levels[lv].board);
        int triv = TRIVIAL(&mt);
        double *s = sum[lv / PER];
        s[0] += mt.active; s[1] += mt.par; s[2] += mt.prop; s[3] += mt.score; s[4] += triv;
        trivial += lv >= 3 && triv;
    }
    printf("\nchapter active  par  prop score trivial\n");
    for (int c = 0; c < LCL_CHAPTERS; c++)
        printf("%7d %5.1f %5.1f %5.2f %5.1f %4.0f\n", c + 1, sum[c][0] / PER, sum[c][1] / PER, sum[c][2] / PER,
               sum[c][3] / PER, sum[c][4]);
    printf("trivial levels from level 4 (under 16 tiles to turn, or all settled by two rounds of the plainest "
           "logic): %d\n", trivial);
    return 0;
}
#else
/* a chapter's levels from its pool (sorted by score): its first `intro` from the easy end (the chapter's idea shown
 * plainly), the others spread from `floor` to the hardest over the boards that are no chore (TRIVIAL), the last
 * of the hardest few; of the boards near each place on the curve the best looking, another size than the last
 * level's when it can */
static int pick(const cand *pool, int n, const chapter *C, int *chosen)
{
    static int hard[MAKES];
    int nh = 0, used[MAKES] = {0};
    for (int i = 0; i < n; i++)
        if (!TRIVIAL(&pool[i].m) && pool[i].m.score >= C->floor) hard[nh++] = i;
    fprintf(stderr, "  %d hard boards\n", nh);
    if (nh < PER - C->intro + 2) return 0;
    int lw = 0, lh = 0, last = -1;
    for (int k = 0; k < C->intro; k++) {
        int span = n / 5 > C->intro ? n / 5 : C->intro, i = -1;
        int a = k * span / C->intro, b = (k + 1) * span / C->intro - 1;
        if (a <= last) a = last + 1;
        if (b < a) b = a;
        for (int j = a; j <= b && j < n; j++)       /* the very first: the fewest taps of all */
            if (i < 0 || (C->from_easiest && k == 0 ? pool[j].par < pool[i].par : pool[j].look > pool[i].look)) i = j;
        used[i] = 1;
        last = i;
        chosen[k] = i;
        lw = pool[i].w, lh = pool[i].h;
    }
    double step = (double)(nh - 1) / (PER - C->intro - 1);
    last = -1;
    for (int k = 0; k < PER - C->intro; k++) {
        int mid = (int)(k * step + 0.5), a = mid - (int)(step / 2), b = mid + (int)(step / 2), i = -1;
        if (k == PER - C->intro - 1) a = nh - 1 - nh / 25 - 2, b = nh - 1;   /* the showpiece: of the hardest few */
        if (a <= last) a = last + 1;
        if (b > nh - 1) b = nh - 1;
        if (b < a) b = a;
        double best = -1e9;
        for (int j = a; j <= b; j++) {
            const cand *c = &pool[hard[j]];
            double v = c->look - (c->w == lw && c->h == lh ? 0.3 : 0);
            if (!used[hard[j]] && (i < 0 || v > best)) { best = v; i = j; }
        }
        if (i < 0) i = a;
        last = i;
        used[hard[i]] = 1;
        chosen[C->intro + k] = hard[i];
        lw = pool[hard[i]].w, lh = pool[hard[i]].h;
    }
    return 1;
}


int main(void)
{
    kept = malloc((size_t)LCL_CHAPTERS * MAKES * 4 * sizeof *kept);
    static cand pool[MAKES];
    static cand out[LCL_LEVELS];
    printf("/* Close the Loops' levels: made by tools/loopclose_gen.c (our own designs; do not edit). Each level: its "
           "board\n * (rows from the top split by '|', a tile's arms at the start in hexadecimal: loopclose.h), its "
           "par: the fewest taps. */\n");
    for (int ch = 0; ch < LCL_CHAPTERS; ch++) {
        const chapter *C = &CH[ch];
        uint32_t r = pr_seed(5300u + (uint32_t)ch);
        int n = 0;
        for (int k = 0; k < 80 * MAKES && n < MAKES; k++) {
            int syms[4], ns = 0;
            for (int s = 0; s < 4; s++)
                if (C->syms >> s & 1) syms[ns++] = s;
            lcl_spec sp = {C->w + pr_below(&r, C->gw + 1), C->h + pr_below(&r, C->gh + 1), syms[pr_below(&r, ns)],
                           C->fill_lo + pr_below(&r, C->fill_hi - C->fill_lo + 1), C->walls, C->stubs, 0};
            if (C->square) sp.h = sp.w;
            sp.mix = ch == 0 ? 35 + pr_below(&r, 66) : 0;   /* (the first boards: some tiles left as they are) */
            lcl_map m;
            if (!lcl_make(&m, &sp, pr_next(&r))) continue;
            double look;
            if (!has(&m, C->need, &look) || met(&m)) continue;
            long ways;
            int par = lcl_solve(m.w, m.h, m.start, NULL, &ways);
            lcl_text(&m, pool[n].text);
            pool[n].par = par;
            pool[n].cells = m.w * m.h;
            measure(&m, &pool[n].m);
            pool[n].diff = pool[n].m.score;
            pool[n].w = m.w;
            pool[n].h = m.h;
            pool[n].look = look;
            n++;
        }
        if (n < PER) { fprintf(stderr, "chapter %d: only %d boards\n", ch + 1, n); return 1; }
        qsort(pool, (size_t)n, sizeof *pool, by_diff);
        int chosen[PER];
        if (!pick(pool, n, C, chosen)) { fprintf(stderr, "chapter %d: too few hard boards\n", ch + 1); return 1; }
        for (int k = 0; k < PER; k++) out[ch * PER + k] = pool[chosen[k]];
        fprintf(stderr, "chapter %d: %d boards, par %d .. %d, squares %d .. %d\n", ch + 1, n, out[ch * PER].par,
                out[ch * PER + PER - 1].par, out[ch * PER].cells, out[ch * PER + PER - 1].cells);
    }
    printf("const lcl_level_t lcl_levels[LCL_LEVELS] = {\n");
    for (int l = 0; l < LCL_LEVELS; l++) printf("    {\"%s\", %d},\n", out[l].text, out[l].par);
    printf("};\n");
    return 0;
}
#endif
