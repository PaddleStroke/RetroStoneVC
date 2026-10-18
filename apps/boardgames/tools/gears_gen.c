/*
 * Gears' levels: src/games/gears_levels.h. Our own boards: no published or commercial set is used. Each chapter has
 * its idea (small gears only: which way a target turns; two sizes; three sizes, where a loop of an odd number jams;
 * several targets; output shafts asking for a speed; big boards with all of it) and makes many boards with the
 * rules' maker (gea_make, from fixed seeds: chains grown from the motor, empty pegs beside them, maybe mirrored), each
 * solved for its par (the fewest gears). A board is kept when its par is in the chapter's range, when it shows the
 * chapter's idea, and when it was not met before (turned round or mirrored either). The
 * difficulty: the par, more when few ways reach it, more for the trains the search found jammed and the work it did.
 * Each chapter keeps 25 spread over its difficulties (chapter 1 from the very easiest: one gear); of the boards near
 * each place on the curve the one that looks best is kept (mirrored, the board well used); the chapter's last level,
 * its showpiece, the best looking of its hardest few. Deterministic (fixed seeds).
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

#define PER (GEA_LEVELS / GEA_CHAPTERS)
#define MAKES 260                               /* boards kept a chapter, at most */
#define SPECS 4

typedef struct {
    char text[GEA_TEXT], canon[GEA_TEXT];
    int par, ways, sym;
    long nodes, jams;
    double diff, look;
} cand;

/* a chapter: its kinds of board (w, h, sizes, motor, tsizes, targets, shafts, chain, decoys, sym, lo, hi) and what it
 * needs: 1 three sizes in the solution, 2 jams met, 4 two ends or more, 8 a shaft */
typedef struct { gea_spec s[SPECS]; int need, tries; } chapter;

static const chapter CH[GEA_CHAPTERS] = {
    {{{6, 8, 2, 2, 2, 1, 0, 1, 1, 0, 1, 2}, {7, 9, 2, 2, 2, 1, 0, 1, 2, 1, 1, 3},
      {8, 10, 2, 2, 2, 1, 0, 2, 3, 0, 2, 4}, {8, 10, 2, 2, 2, 2, 0, 1, 2, 1, 2, 4}}, 0, 900},
    {{{8, 10, 6, 6, 6, 1, 0, 1, 2, 0, 2, 4}, {9, 11, 6, 6, 6, 1, 0, 2, 3, 1, 2, 5},
      {10, 12, 6, 6, 6, 2, 0, 1, 3, 0, 3, 5}, {10, 12, 6, 4, 6, 1, 0, 2, 4, 1, 3, 6}}, 0, 1200},
    {{{10, 12, 14, 14, 14, 1, 0, 2, 3, 0, 3, 5}, {11, 13, 14, 14, 14, 1, 0, 2, 4, 1, 3, 6},
      {11, 13, 14, 8, 14, 2, 0, 1, 4, 0, 3, 6}, {12, 14, 14, 14, 14, 2, 0, 2, 4, 1, 4, 7}}, 3, 5000},
    {{{11, 13, 14, 14, 14, 2, 0, 1, 3, 0, 3, 6}, {12, 14, 14, 14, 14, 2, 0, 2, 4, 1, 4, 7},
      {12, 14, 14, 14, 14, 3, 0, 1, 4, 0, 4, 7}, {12, 15, 14, 14, 14, 3, 0, 2, 3, 1, 5, 8}}, 4, 1600},
    {{{10, 12, 14, 14, 14, 0, 1, 2, 3, 0, 3, 6}, {11, 13, 14, 14, 14, 1, 1, 1, 4, 0, 3, 7},
      {12, 14, 14, 14, 14, 0, 2, 1, 4, 1, 4, 7}, {12, 14, 14, 14, 14, 1, 1, 2, 4, 1, 5, 8}}, 8, 1600},
    {{{14, 17, 14, 14, 14, 2, 1, 2, 5, 0, 6, 9}, {14, 18, 14, 14, 14, 2, 0, 3, 6, 1, 6, 10},
      {15, 19, 14, 14, 14, 3, 1, 2, 6, 0, 7, 10}, {16, 20, 14, 14, 14, 2, 2, 2, 6, 1, 7, 11}}, 4, 1200},
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
    static const char *const IDEA[GEA_CHAPTERS] = {
        "small gears: which way", "two sizes", "three sizes, jams", "more targets", "shafts at a speed", "big boards",
    };
    pool = malloc(sizeof *pool * MAKES);
    make_limit = 400000;
    printf("/* Gears' levels: made by tools/gears_gen.c (our own boards, solved by its search; do not edit).\n"
           " * Each level: its board (gears.h's text: the size, the tray's sizes, the pegs), the par: the fewest gears. */\n"
           "const gea_level_t gea_levels[GEA_LEVELS] = {\n");
    for (int c = 0; c < GEA_CHAPTERS; c++) {
        const chapter *ch = &CH[c];
        npool = 0;
        for (int k = 0; k < ch->tries && npool < MAKES; k++) {
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
            d->diff = par + 0.9 / d->ways + 0.25 * log10(1.0 + nodes) + 0.15 * log10(1.0 + jams) + 0.04 * m.n;
            d->look = look_of(&m, d->sym);
            npool++;
        }
        qsort(pool, npool, sizeof *pool, by_diff);
        fprintf(stderr, "chapter %d (%s): %d boards, diff %.2f .. %.2f\n", c + 1, IDEA[c], npool,
                npool ? pool[0].diff : 0, npool ? pool[npool - 1].diff : 0);
        if (npool < PER) { fprintf(stderr, "too few\n"); return 1; }
        int used[MAKES] = {0};
        for (int i = 0; i < PER; i++) {
            /* place i on the curve: chapter 1 from the very easiest, the others from a fifth in; the last one the
             * best looking of the hardest few */
            double f = (double)i / (PER - 1), lo = c ? 0.12 : 0.0, at = lo + (0.97 - lo) * f;
            int centre = (int)(at * (npool - 1)), span = npool / 30 + 1, pick = -1;
            if (i == PER - 1) { centre = npool - 4; span = 4; }
            for (int j = centre - span; j <= centre + span; j++) {
                if (j < 0 || j >= npool || used[j]) continue;
                if (pick < 0 || pool[j].look > pool[pick].look) pick = j;
            }
            for (int j = 0; pick < 0 && j < npool; j++)
                if (!used[(centre + j) % npool]) pick = (centre + j) % npool;
            used[pick] = 1;
            cand *d = &pool[pick];
            printf("    {\"%s\", %d},\n", d->text, d->par);
            fprintf(stderr, "  %3d par %2d ways %2d jams %5ld nodes %7ld sym %d diff %.2f\n", c * PER + i + 1, d->par,
                    d->ways, d->jams, d->nodes, d->sym, d->diff);
        }
    }
    printf("};\n");
    free(pool);
    return 0;
}
