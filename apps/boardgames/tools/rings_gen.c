/*
 * Ring Puzzle's levels: src/games/rings_levels.h. Our own levels: no published or commercial set is used. Each
 * chapter has its rings and colours (two small rings of ivory and ebony; a third colour on their far arcs; larger
 * rings, four colours; three rings in a row; three in a triangle, one bead shared by all three; all of them, scrambled
 * deeper) and makes many scrambles from fixed seeds: a target of the chapter (each ring its colour, rg_pattern), random
 * turns from it, the par searched (the fewest moves, rg_solve). A scramble is kept when its par is in the chapter's
 * range, when its shortest way turns more than one ring (from a par of three: not one ring turned back), and when it
 * was not met before. Each chapter keeps 25 spread over its pars (the first level of all: one move), its last level,
 * the showpiece, one of the deepest. Deterministic (fixed seeds).
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/rggen apps/boardgames/tools/rings_gen.c -lm && \
 *       /tmp/rggen > apps/boardgames/src/games/rings_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#define RG_GEN
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/rings_rules.c"

#define PER (RG_LEVELS / RG_CHAPTERS)
#define POOL 400

typedef struct {
    int kind, n, colours, par, rings_used, pattern;
    char target[RG_PLACES + 1], start[RG_PLACES + 1], way[RG_DEEP + 1];
    double key;
} cand;

/* a chapter: its kinds of rings (kind, n, pattern), the pars wanted */
typedef struct { int kinds[4][3], nk, lo, hi; } chapter;

static const chapter CH[RG_CHAPTERS] = {
    {{{RG_TWO, 8, 0}}, 1, 1, 5},
    {{{RG_TWO, 8, 1}, {RG_TWO, 12, 0}}, 2, 2, 6},
    {{{RG_TWO, 8, 2}, {RG_TWO, 12, 1}, {RG_TWO, 12, 2}}, 3, 3, 7},
    {{{RG_CHAIN, 8, 0}, {RG_CHAIN, 8, 1}}, 2, 3, 8},
    {{{RG_TRIANGLE, 12, 0}, {RG_TRIANGLE, 12, 1}}, 2, 4, 9},
    {{{RG_TWO, 12, 2}, {RG_CHAIN, 8, 1}, {RG_TRIANGLE, 12, 1}, {RG_TRIANGLE, 12, 0}}, 4, 6, 11},
};

static int by_key(const void *a, const void *b)
{
    double d = ((const cand *)a)->key - ((const cand *)b)->key;
    return d < 0 ? -1 : d > 0 ? 1 : strcmp(((const cand *)a)->start, ((const cand *)b)->start);
}

int main(void)
{
    static cand pool[POOL], out[RG_LEVELS];
    static const char *const KIND[] = {"RG_TWO", "RG_CHAIN", "RG_TRIANGLE"};
    printf("/* Ring Puzzle's levels: made by tools/rings_gen.c (our own scrambles, each par found by the search; do not\n"
           " * edit). Each: the rings, their beads, the colours, the par, the target's and the start's colours (a digit a\n"
           " * place), a shortest way (a letter a move) (rings.h). */\n");
    for (int ch = 0; ch < RG_CHAPTERS; ch++) {
        const chapter *C = &CH[ch];
        uint32_t r = pr_seed(5300u + (uint32_t)ch);
        int n = 0;
        for (int t = 0; t < 30 * POOL && n < POOL; t++) {
            cand *c = &pool[n];
            const int *k = C->kinds[pr_below(&r, C->nk)];
            rg_map m;
            rg_build(&m, k[0], k[1]);
            rg_pattern(&m, k[2], 0);
            int want = C->lo + pr_below(&r, C->hi - C->lo + 1), steps = want + pr_below(&r, 3), last = -1;
            rg_pos p = m.target;
            for (int i = 0; i < steps; i++) {
                int mv;
                do mv = pr_below(&r, rg_moves(&m));
                while (last >= 0 && mv == rg_inverse(last));
                p = rg_apply(&m, p, mv);
                last = mv;
            }
            int par = rg_solve(&m, p, NULL);
            if (par < C->lo || par > C->hi) continue;
            /* its shortest way, the rings it turns */
            rg_pos q = p;
            int used = 0, ok = 1;
            for (int i = 0; i < par; i++) {
                int f;
                if (rg_solve(&m, q, &f) != par - i || f < 0) { ok = 0; break; }
                c->way[i] = (char)('a' + f);
                used |= 1 << (f >> 1);
                q = rg_apply(&m, q, f);
            }
            if (!ok || q != m.target) continue;
            c->way[par] = 0;
            int rings = __builtin_popcount((unsigned)used);
            if (par >= 3 && rings < 2) continue;
            rg_text(&m, m.target, c->target);
            rg_text(&m, p, c->start);
            int dup = 0;
            for (int i = 0; i < n && !dup; i++)
                dup = pool[i].kind == k[0] && pool[i].n == k[1] && !strcmp(pool[i].start, c->start) &&
                      !strcmp(pool[i].target, c->target);
            if (dup) continue;
            c->kind = k[0], c->n = k[1], c->pattern = k[2], c->colours = m.colours, c->par = par, c->rings_used = rings;
            c->key = par + 0.1 * rings + 0.05 * m.colours + 0.01 * k[1] + 0.001 * pr_below(&r, 100);
            n++;
        }
        if (n < PER) { fprintf(stderr, "chapter %d: only %d\n", ch + 1, n); return 1; }
        qsort(pool, (size_t)n, sizeof *pool, by_key);
        static char used[POOL];
        memset(used, 0, sizeof used);
        for (int k = 0; k < PER - 1; k++) {     /* the pars climbing evenly over the chapter */
            int want = C->lo + (int)((C->hi - C->lo) * k / (double)(PER - 2) + 0.5), pick = -1;
            for (int i = 0; i < n - 1; i++)
                if (!used[i] && (pick < 0 || abs(pool[i].par - want) < abs(pool[pick].par - want))) pick = i;
            used[pick] = 1;
            out[ch * PER + k] = pool[pick];
        }
        for (int a = 0; a < PER - 1; a++)       /* (in order of the key: the pars never going down) */
            for (int b = a + 1; b < PER - 1; b++)
                if (by_key(&out[ch * PER + a], &out[ch * PER + b]) > 0) {
                    cand t = out[ch * PER + a];
                    out[ch * PER + a] = out[ch * PER + b];
                    out[ch * PER + b] = t;
                }
        int show = n - 1;                       /* the showpiece: the deepest, the most colours */
        for (int i = n - 1; i >= n - 8 && i > 0; i--)
            if (pool[i].par == pool[n - 1].par && pool[i].colours * 10 + pool[i].rings_used >
                                                      pool[show].colours * 10 + pool[show].rings_used)
                show = i;
        out[ch * PER + PER - 1] = pool[show];
        fprintf(stderr, "chapter %d: %d scrambles, pars %d .. %d\n", ch + 1, n, out[ch * PER].par,
                out[ch * PER + PER - 1].par);
    }
    printf("const rg_level_def rg_levels[RG_LEVELS] = {\n");
    for (int l = 0; l < RG_LEVELS; l++) {
        const cand *c = &out[l];
        printf("    {%s, %d, %d, %d, \"%s\", \"%s\", \"%s\"}, /* %d */\n", KIND[c->kind], c->n, c->colours, c->par,
               c->target, c->start, c->way, l + 1);
    }
    printf("};\n");
    return 0;
}
