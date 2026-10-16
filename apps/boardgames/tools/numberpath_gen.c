/*
 * Number Path's levels, made by chance and chosen: src/games/numberpath_levels.h. Our own puzzles, from the game's own
 * maker (a path drawn by chance, its numbers taken away while the reasoning still solves it, some put back for the
 * easier ones; numberpath_rules.c). Each candidate:
 *   - checked by the search to have one solution;
 *   - its work: the rounds of reasoning it takes (each round all the numbers and cells found at once), and how many
 *     of them needed a cell with one number left (the medium way);
 *   - near-duplicates dropped (the same givens turned, mirrored, or numbered from the other end).
 * Six chapters of 50: in each, 25 where numbers touch all around and 25 where they touch side by side, in turn. Each
 * way's chapter has its makes (a size, the part of the cells given, how many, and the part of the make's candidates,
 * ranked by that work, they are picked from evenly): 5 x 5 with nearly half the numbers given first, up to 9 x 9 with
 * as few as the reasoning allows; it goes from its easiest to its hardest.
 * Deterministic (fixed seeds): run it again after a change to the maker and check the diff.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/nphgen apps/boardgames/tools/numberpath_gen.c \
 *       apps/boardgames/src/games/numberpath_rules.c && /tmp/nphgen > apps/boardgames/src/games/numberpath_levels.h
 * "/tmp/nphgen survey W H WAY KEEP": the givens, the work and the time for a few puzzles (choosing the makes).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "games/numberpath.h"

typedef struct {
    nph_puzzle p;
    uint8_t sol[NPH_CELLS];
    int givens, steps, mediums;
    long nodes;
    double work;
    char key[2 * NPH_CELLS + 1];
} cand;

typedef struct { int w, h, count; float keep, lo, hi; } make;

static const make CHAPTER[6][2] = {
    {{5, 5, 25, 0.46f, 0.0f, 0.7f}},
    {{5, 6, 10, 0.40f, 0.2f, 0.8f}, {6, 6, 15, 0.38f, 0.2f, 0.9f}},
    {{6, 7, 10, 0.34f, 0.2f, 0.9f}, {7, 7, 15, 0.32f, 0.2f, 1.0f}},
    {{7, 8, 10, 0.30f, 0.2f, 1.0f}, {8, 8, 15, 0.28f, 0.2f, 1.0f}},
    {{8, 9, 10, 0.26f, 0.3f, 1.0f}, {9, 9, 15, 0.24f, 0.3f, 1.0f}},
    {{9, 9, 25, 0.0f, 0.2f, 1.0f}},
};

/* the givens' key: the smallest of the grid's pictures turned and mirrored (keeping its shape), each also numbered
 * from the other end */
static void key_of(const nph_puzzle *p, char *out)
{
    int w = p->w, h = p->h, n = w * h;
    out[0] = 0;
    for (int t = 0; t < 8; t++) {
        if ((t & 4) && w != h) continue;        /* a quarter turn: square grids only */
        for (int rev = 0; rev < 2; rev++) {
            char k[2 * NPH_CELLS + 1];
            for (int y = 0; y < h; y++)
                for (int x = 0; x < w; x++) {
                    int sx = t & 1 ? w - 1 - x : x, sy = t & 2 ? h - 1 - y : y;
                    if (t & 4) {
                        int s = sx;
                        sx = sy, sy = s;
                    }
                    int v = p->given[sy * w + sx];
                    if (v && rev) v = n + 1 - v;
                    k[2 * (y * w + x)] = (char)('0' + v / 10);
                    k[2 * (y * w + x) + 1] = (char)('0' + v % 10);
                }
            k[2 * n] = 0;
            if (!out[0] || strcmp(k, out) < 0) strcpy(out, k);
        }
    }
}

static void judge(cand *c)
{
    uint8_t val[NPH_CELLS], sol[NPH_CELLS];
    int n = nph_cells(&c->p);
    memcpy(val, c->p.given, sizeof val);
    if (nph_reason(&c->p, val, &c->steps, &c->mediums) != 1 || memcmp(val, c->sol, (size_t)n)) {
        fprintf(stderr, "a puzzle the reasoning does not solve\n");
        exit(1);
    }
    if (nph_count(&c->p, 2, sol, 0, &c->nodes) != 1 || memcmp(sol, c->sol, (size_t)n)) {
        fprintf(stderr, "a puzzle without one solution\n");
        exit(1);
    }
    c->givens = 0;
    for (int i = 0; i < n; i++) c->givens += c->p.given[i] != 0;
    c->work = c->steps + 2.0 * c->mediums + 6.0 * (1 - (double)c->givens / n);
    key_of(&c->p, c->key);
}

static int by_work(const void *a, const void *b)
{
    const cand *x = a, *y = b;
    if (x->work != y->work) return x->work < y->work ? -1 : 1;
    return strcmp(x->key, y->key);
}

static cand chosen[NPH_LEVELS];
static int nchosen;

static int known(const char *key)
{
    for (int i = 0; i < nchosen; i++)
        if (!strcmp(chosen[i].key, key)) return 1;
    return 0;
}

static void survey(int w, int h, int way, float keep)
{
    for (uint32_t s = 1; s <= 12; s++) {
        cand c;
        clock_t t0 = clock();
        nph_make(w, h, way, keep, s * 7919u, &c.p, c.sol);
        double t = (double)(clock() - t0) / CLOCKS_PER_SEC;
        clock_t t1 = clock();
        judge(&c);
        printf("%dx%d %s keep %.2f seed %u: %d given, %d steps, %d mediums, %ld nodes; made in %.3f s, judged in "
               "%.3f s\n", w, h, way ? "side" : "all", keep, s, c.givens, c.steps, c.mediums, c.nodes, t,
               (double)(clock() - t1) / CLOCKS_PER_SEC);
        fflush(stdout);
    }
}

int main(int argc, char **argv)
{
    if (argc == 6 && !strcmp(argv[1], "survey")) {
        survey(atoi(argv[2]), atoi(argv[3]), atoi(argv[4]), (float)atof(argv[5]));
        return 0;
    }
    uint32_t seed = 20261017u;
    for (int way = 0; way < 2; way++)
        for (int ch = 0; ch < 6; ch++) {
            int first = nchosen;
            for (int m = 0; m < 2 && CHAPTER[ch][m].count; m++) {
                const make *mk = &CHAPTER[ch][m];
                int nc = 3 * mk->count;
                cand *cs = calloc((size_t)nc, sizeof(cand));
                if (!cs) return 1;
                for (int i = 0; i < nc; i++) {
                    do {
                        nph_make(mk->w, mk->h, way, mk->keep, seed++, &cs[i].p, cs[i].sol);
                        judge(&cs[i]);
                    } while (known(cs[i].key));
                    fprintf(stderr, "\rway %d chapter %d make %d: %d/%d   ", way, ch + 1, m + 1, i + 1, nc);
                }
                qsort(cs, (size_t)nc, sizeof(cand), by_work);
                int lo = (int)(mk->lo * (nc - 1)), hi = (int)(mk->hi * (nc - 1));
                for (int i = 0; i < mk->count; i++) {
                    int at = lo + (hi - lo) * i / (mk->count - 1);
                    while (at < nc && known(cs[at].key)) at++;
                    if (at >= nc) return 1;
                    chosen[nchosen++] = cs[at];
                }
                free(cs);
            }
            qsort(chosen + first, (size_t)(nchosen - first), sizeof(cand), by_work);
        }
    fprintf(stderr, "\n");
    if (nchosen != NPH_LEVELS) return 1;
    printf("/*\n"
           " * Number Path's levels: our own puzzles, made by chance and chosen by tools/numberpath_gen.c (the\n"
           " * game's own maker; each checked to have one solution), six chapters of 50 from the easiest, a puzzle\n"
           " * where numbers touch all around and one where they touch side by side in turn. Each: \"<w><h><a|s>:\"\n"
           " * the cell of 1 (two digits), \":\" a step a number (0..7: right, then round clockwise), \":\" the\n"
           " * numbers given (hex: bit i number i + 1). In the comment: its size, the numbers given, the rounds of\n"
           " * reasoning (the medium ones).\n"
           " * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n */\n");
    printf("const char *const nph_levels[NPH_LEVELS] = {\n");
    for (int i = 0; i < nchosen; i++) {         /* chosen: all around's 150, then side by side's; shown in turn */
        char s[3 * NPH_CELLS];
        cand *c = &chosen[i % 2 * (NPH_LEVELS / 2) + i / 2];
        nph_encode(&c->p, c->sol, s, sizeof s);
        printf("    \"%s\", /* %d: %dx%d %s, %d given, %d rounds (%d medium) */\n", s, i + 1, c->p.w, c->p.h,
               c->p.way ? "side" : "all", c->givens, c->steps, c->mediums);
    }
    printf("};\n");
    return 0;
}
