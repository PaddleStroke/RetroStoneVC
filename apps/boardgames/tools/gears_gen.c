/*
 * Gears' levels: src/games/gears_levels.h. Our own boards: no published or commercial set is used. Each chapter has
 * its idea (small gears only: which way a target turns; two sizes; three sizes, where a loop of an odd number jams;
 * several targets; output shafts asking for a speed; big boards with all of it) and makes many boards with the
 * rules' maker (gea_make, from fixed seeds: chains grown from the motor, empty pegs beside them, maybe mirrored), each
 * solved for its par (the fewest gears). A board is kept when its par is in the chapter's range, when it shows the
 * chapter's idea, and when it was not met before (turned round or mirrored either). Each is measured: the par, the
 * ways that reach it, the trains the search found jammed and its work, and the share of naive players (gea_naive:
 * building at random wherever a gear meshes, a winning gear taken at once) that solve it. Only a chapter's opening
 * levels may be that easy: the first chapter is only those (pars 1, 2, 3: with one size of gear any train that reaches
 * the target turns it the one way it can, so it cannot ask for more), the second opens with one, the others with two.
 * The rest (gea_chapter_of: 29 or 30 a chapter) are boards most naive players fail and of a par of 3 or more, spread
 * over their difficulties; of the boards near each place on the curve the one that looks best is kept (mirrored, the
 * board well used); the chapter's last level, its showpiece, the best looking of its hardest few. The per-level table
 * and the chapters' summary go to stderr (-DREPORT: the same for the levels in gears_levels.h). Deterministic.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/geagen apps/boardgames/tools/gears_gen.c -lm && \
 *       /tmp/geagen > apps/boardgames/src/games/gears_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#define GEA_GEN
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/gears_rules.c"

#define MAKES 700                               /* boards kept a chapter, at most */
#define SPECS 4

typedef struct {
    char text[GEA_TEXT], canon[GEA_TEXT];
    int par, ways, sym;
    long nodes, jams;
    double diff, look, naive, near;
} cand;

/* a chapter: its kinds of board (w, h, sizes, motor, tsizes, targets, shafts, chain, decoys, sym, lo, hi) and what it
 * needs: 1 three sizes in the solution, 2 jams met, 4 two ends or more, 8 a shaft */
typedef struct {
    gea_spec s[SPECS];
    int need, tries;
    double nmax;                                /* past the opening levels: the share of naive players solving it */
    int pmin;                                   /* and the par at least */
} chapter;

static const chapter CH[GEA_CHAPTERS] = {
    {{{6, 8, 2, 2, 2, 1, 0, 1, 3, 0, 1, 2}, {7, 9, 2, 2, 2, 1, 0, 2, 4, 1, 2, 4},
      {8, 10, 2, 2, 2, 1, 0, 2, 5, 0, 2, 5}, {8, 10, 2, 2, 2, 2, 0, 1, 4, 1, 2, 5}}, 0, 600, 1.0, 1},
    {{{8, 10, 6, 6, 6, 1, 0, 1, 4, 0, 2, 5}, {9, 11, 6, 6, 6, 1, 0, 2, 5, 1, 3, 6},
      {10, 12, 6, 6, 6, 2, 0, 1, 5, 0, 3, 6}, {10, 12, 6, 4, 6, 1, 0, 2, 6, 1, 3, 7}}, 0, 4000, 0.3, 3},
    {{{10, 12, 14, 14, 14, 1, 0, 2, 5, 0, 3, 6}, {11, 13, 14, 14, 14, 1, 0, 2, 6, 1, 4, 7},
      {11, 13, 14, 8, 14, 2, 0, 1, 6, 0, 4, 7}, {12, 14, 14, 14, 14, 2, 0, 2, 6, 1, 4, 8}}, 3, 8000, 0.25, 4},
    {{{11, 13, 14, 14, 14, 2, 0, 1, 5, 0, 4, 7}, {12, 14, 14, 14, 14, 2, 0, 2, 6, 1, 4, 8},
      {12, 14, 14, 14, 14, 3, 0, 1, 6, 0, 5, 8}, {12, 15, 14, 14, 14, 3, 0, 2, 5, 1, 5, 9}}, 4, 5000, 0.2, 4},
    {{{10, 12, 14, 14, 14, 0, 1, 2, 5, 0, 4, 7}, {11, 13, 14, 14, 14, 1, 1, 1, 6, 0, 4, 8},
      {12, 14, 14, 14, 14, 0, 2, 1, 6, 1, 5, 8}, {12, 14, 14, 14, 14, 1, 1, 2, 6, 1, 5, 9}}, 8, 5000, 0.15, 4},
    {{{14, 17, 14, 14, 14, 2, 1, 2, 7, 0, 6, 10}, {14, 18, 14, 14, 14, 2, 0, 3, 8, 1, 7, 11},
      {15, 19, 14, 14, 14, 3, 1, 2, 8, 0, 7, 11}, {16, 20, 14, 14, 14, 2, 2, 2, 8, 1, 8, 12}}, 4, 4000, 0.1, 6},
};

static int by_diff(const void *a, const void *b)
{
    double d = ((const cand *)a)->diff - ((const cand *)b)->diff;
    return d < 0 ? -1 : d > 0 ? 1 : strcmp(((const cand *)a)->text, ((const cand *)b)->text);
}

/* ---- near-duplicates: the board under its four turns and mirrors (the ways asked stay: they only say how a target turns
 * against the motor) ------------- */
static void form_of(const gea_map *m, int tr, char *out)
{
    gea_map t = *m;
    for (int i = 0; i < t.n; i++) {
        if (tr & 1) t.p[i].x = (uint8_t)(m->w - m->p[i].x);
        if (tr & 2) t.p[i].y = (uint8_t)(m->h - m->p[i].y);
    }
    for (int i = 0; i < t.n; i++)               /* in a set order */
        for (int j = i + 1; j < t.n; j++) {
            gea_peg *a = &t.p[i], *b = &t.p[j];
            int ka = a->kind * 10000 + a->y * 100 + a->x, kb = b->kind * 10000 + b->y * 100 + b->x;
            if (kb < ka) { gea_peg c = *a; *a = *b; *b = c; }
        }
    gea_text(&t, out);
}

static void canon_of(const gea_map *m, char *out)
{
    char f[GEA_TEXT];
    form_of(m, 0, out);
    for (int tr = 1; tr < 4; tr++) {
        form_of(m, tr, f);
        if (strcmp(f, out) < 0) strcpy(out, f);
    }
}

static int symmetric(const gea_map *m)          /* the pegs mirrored left to right */
{
    for (int i = 0; i < m->n; i++) {
        int found = 0;
        for (int j = 0; j < m->n && !found; j++)
            found = m->p[j].x == m->w - m->p[i].x && m->p[j].y == m->p[i].y && m->p[j].kind == m->p[i].kind;
        if (!found) return 0;
    }
    return 1;
}

static double look_of(const gea_map *m, int sym)
{
    int x0 = 99, x1 = -1, y0 = 99, y1 = -1;
    for (int i = 0; i < m->n; i++) {
        x0 = m->p[i].x < x0 ? m->p[i].x : x0;
        x1 = m->p[i].x > x1 ? m->p[i].x : x1;
        y0 = m->p[i].y < y0 ? m->p[i].y : y0;
        y1 = m->p[i].y > y1 ? m->p[i].y : y1;
    }
    double used = (double)(x1 - x0 + 2) * (y1 - y0 + 2) / ((double)m->w * m->h);
    return used + (sym ? 0.6 : 0);
}

/* ---- the difficulty metrics: the naive players (gea_naive), the par, the optimal ways, the search's work */
static void naive(const gea_map *m, uint32_t seed, double *won, double *near, int par)
{
    *won = gea_naive(m, par, seed, near);
}

static void table_head(void) { fprintf(stderr, "lvl par ways  jams   nodes naive near  diff\n"); }

static void table_row(int l, int par, int ways, long jams, long nodes, double nv, double nr, double diff)
{
    fprintf(stderr, "%4d %3d %4d %5ld %7ld %5.2f %4.2f %5.2f\n", l + 1, par, ways, jams, nodes, nv, nr, diff);
}

typedef struct { double par, naive, near, ways; int trivial, n, lo, hi; } chsum;
static void sum_add(chsum *c, int par, int ways, double nv, double nr)
{
    c->par += par, c->naive += nv, c->near += nr, c->ways += ways, c->n++;
    c->trivial += nv >= 0.5 || par <= 1;
    if (!c->lo || par < c->lo) c->lo = par;
    if (par > c->hi) c->hi = par;
}
static void sum_print(int ch, const chsum *c)
{
    fprintf(stderr, "  %d: par %4.1f (%d..%d)  ways %4.1f  naive %.2f  naive within par+1 %.2f  trivial %d/%d\n",
            ch + 1, c->par / c->n, c->lo, c->hi, c->ways / c->n, c->naive / c->n, c->near / c->n, c->trivial, c->n);
}

#ifdef REPORT                                   /* -DREPORT: the metrics of the levels in gears_levels.h */
#include "games/gears_levels.h"
static int report(void)
{
    static chsum S[GEA_CHAPTERS];
    table_head();
    for (int l = 0; l < GEA_LEVELS; l++) {
        gea_map m;
        gea_parse(&m, gea_levels[l].text);
        long nodes = 0, jams = 0;
        int par = gea_solve(&m, NULL, 4000000, &nodes, &jams), ways = gea_ways(&m, par, 20);
        double nv, nr;
        naive(&m, 5u + (uint32_t)l, &nv, &nr, par);
        table_row(l, par, ways, jams, nodes, nv, nr, 0);
        sum_add(&S[gea_chapter_of(l)], par, ways, nv, nr);
    }
    fprintf(stderr, "chapter: par mean (min..max), optimal ways mean, naive share, naive within par+1; trivial (naive "
                    ">= 0.5 or par 1)\n");
    for (int c = 0; c < GEA_CHAPTERS; c++) sum_print(c, &S[c]);
    return 0;
}
#endif

static cand *pool;
static int npool;

static int seen(const char *canon)
{
    for (int i = 0; i < npool; i++)
        if (!strcmp(pool[i].canon, canon)) return 1;
    return 0;
}

int main(void)
{
#ifdef REPORT
    return report();
#endif
    static const char *const IDEA[GEA_CHAPTERS] = {
        "small gears: which way", "two sizes", "three sizes, jams", "more targets", "shafts at a speed", "big boards",
    };
    pool = malloc(sizeof *pool * MAKES);
    make_limit = 400000;
    printf("/* Gears' levels: made by tools/gears_gen.c (our own boards, solved by its search; do not edit).\n"
           " * Each level: its board (gears.h's text: the size, the tray's sizes, the pegs), the par: the fewest gears. */\n"
           "const gea_level_t gea_levels[GEA_LEVELS] = {\n");
    static chsum S[GEA_CHAPTERS];
    table_head();
    for (int c = 0; c < GEA_CHAPTERS; c++) {
        const chapter *ch = &CH[c];
        npool = 0;
        int strict = 0;
        for (int k = 0; k < ch->tries && strict < MAKES / 2 && npool < MAKES; k++) {
            const gea_spec *sp = &ch->s[k % SPECS];
            gea_map m;
            int par;
            gea_make(&m, sp, (uint32_t)(1000003u * (c + 1) + 7919u * k), &par);
            if (par < sp->lo) continue;
            long nodes = 0, jams = 0;
            gea_state sol;
            if (gea_solve(&m, &sol, 4000000, &nodes, &jams) != par) continue;
            int sizes = 0, ends = 0, shafts = 0;
            for (int i = 0; i < m.n; i++) {
                if (!gea_fixed(&m, i) && sol.g[i]) sizes |= 1 << sol.g[i];
                if (m.p[i].kind == GEA_TARGET || m.p[i].kind == GEA_SHAFT) ends++;
                if (m.p[i].kind == GEA_SHAFT) shafts++;
                if (m.p[i].kind == GEA_TARGET) sizes |= 1 << m.p[i].size;
            }
            if (ch->need & 1 && sizes != 14) continue;
            if (ch->need & 2 && jams < 2) continue;
            if (ch->need & 4 && ends < 2) continue;
            if (ch->need & 8 && !shafts) continue;
            cand *d = &pool[npool];
            canon_of(&m, d->canon);
            if (seen(d->canon)) continue;
            gea_text(&m, d->text);
            d->par = par;
            d->ways = gea_ways(&m, par, 20);
            d->nodes = nodes;
            d->jams = jams;
            d->sym = symmetric(&m);
            naive(&m, 5u + (uint32_t)k, &d->naive, &d->near, par);
            d->diff = par + 0.9 / d->ways + 0.25 * log10(1.0 + nodes) + 0.15 * log10(1.0 + jams) + 0.04 * m.n +
                      3 * (1 - d->naive) + 2 * (1 - d->near);
            d->look = look_of(&m, d->sym);
            strict += d->naive <= ch->nmax && par >= ch->pmin;
            npool++;
        }
        qsort(pool, npool, sizeof *pool, by_diff);
        fprintf(stderr, "chapter %d (%s): %d boards, %d hard enough\n", c + 1, IDEA[c], npool, strict);
        {
            int hist[5] = {0}, ph[16] = {0};
            for (int j = 0; j < npool; j++) hist[(int)(pool[j].naive * 4)]++, ph[pool[j].par < 15 ? pool[j].par : 15]++;
            fprintf(stderr, "  naive by quarters %d %d %d %d %d; pars", hist[0], hist[1], hist[2], hist[3], hist[4]);
            for (int p = 0; p < 16; p++) fprintf(stderr, " %d", ph[p]);
            fprintf(stderr, "\n");
        }
        int used[MAKES] = {0}, at = 0, nopen = c == 0 ? 3 : c == 1 ? 1 : 2;
        int cnt = gea_chapter_start(c + 1) - gea_chapter_start(c);
        cand *out[GEA_LEVELS];
        /* the opening levels (the chapter's new idea, easy): the first chapter's pars 1, 2, 3; the others' easiest two
         * that are not one gear */
        for (int o = 0; o < nopen; o++) {
            int pick = -1;
            for (int j = 0; j < npool && pick < 0; j++)
                if (!used[j] && (c ? pool[j].par >= 2 : pool[j].par == o + 1)) pick = j;
            for (int j = 0; j < npool && pick < 0; j++)
                if (!used[j]) pick = j;
            used[pick] = 1;
            out[at++] = &pool[pick];
        }
        int hard[MAKES], nh = 0;                /* the rest: few naive players solve them */
        for (int j = 0; j < npool; j++)
            if (!used[j] && pool[j].naive <= ch->nmax && pool[j].par >= ch->pmin) hard[nh++] = j;
        if (nh < cnt - nopen) { fprintf(stderr, "too few\n"); return 1; }
        int taken[MAKES] = {0};
        for (int i = 0; i < cnt - nopen; i++) {
            /* place i on the curve; of the boards near it the best looking; the last one the best looking of the
             * hardest few */
            double f = cnt - nopen > 1 ? (double)i / (cnt - nopen - 1) : 0, at_ = 0.97 * f;
            int centre = (int)(at_ * (nh - 1)), span = nh / 30 + 1, pick = -1;
            if (i == cnt - nopen - 1) { centre = nh - 4; span = 4; }
            for (int j = centre - span; j <= centre + span; j++) {
                if (j < 0 || j >= nh || taken[j]) continue;
                if (pick < 0 || pool[hard[j]].look > pool[hard[pick]].look) pick = j;
            }
            for (int j = 0; pick < 0 && j < nh; j++)
                if (!taken[(centre + j) % nh]) pick = (centre + j) % nh;
            taken[pick] = 1;
            out[at++] = &pool[hard[pick]];
        }
        for (int i = 0; i < cnt; i++) {
            cand *d = out[i];
            printf("    {\"%s\", %d},\n", d->text, d->par);
            table_row(gea_chapter_start(c) + i, d->par, d->ways, d->jams, d->nodes, d->naive, d->near, d->diff);
            sum_add(&S[c], d->par, d->ways, d->naive, d->near);
        }
    }
    printf("};\n");
    fprintf(stderr, "chapter: par mean (min..max), optimal ways mean, naive share, naive within par+1; trivial (naive "
                    ">= 0.5 or par 1)\n");
    for (int c = 0; c < GEA_CHAPTERS; c++) sum_print(c, &S[c]);
    free(pool);
    return 0;
}
