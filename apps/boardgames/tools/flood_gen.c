/*
 * Flood's levels, dealt by chance and chosen: src/games/flood_levels.h. Our own boards, from the game's own dealer and
 * solver (flood_rules.c). Each candidate:
 *   - its par: the shortest way a wide beam search finds, then an exact search (iterative deepening) that proves it
 *     the fewest when it ends within its budget (the comment says "proved", else "best found");
 *   - its work: the par, and how many moves the plain greedy way (the most cells each move) wastes beyond it;
 *   - near-duplicates dropped (the same board with its colours renamed, or mirrored on its diagonal: the same game).
 * Each chapter has its makes (a size, the colours, how many, and the part of the make's candidates, ranked by that
 * work, they are picked from evenly): 5 x 5 with 3 colours first, up to 14 x 14 with 6. Each chapter then goes from
 * its easiest to its hardest.
 * Deterministic (fixed seeds): run it again after a change to the solver and check the diff.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/fldgen apps/boardgames/tools/flood_gen.c \
 *       apps/boardgames/src/games/flood_rules.c && /tmp/fldgen > apps/boardgames/src/games/flood_levels.h
 * "/tmp/fldgen survey N K": the par, the greedy way and the time for a few boards of that size (choosing the makes).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "games/flood.h"

#define WIDTH 1500                              /* the beam's width */
#define EXACT 3000000L                          /* the exact search's nodes */

typedef struct {
    fld_board b;
    uint8_t way[FLD_MOVES];
    int par, greedy, proved;
    double work;
    char key[FLD_CELLS + 1];
} cand;

typedef struct { int n, k, count; float lo, hi; } make;

static const make CHAPTER[FLD_CHAPTERS][3] = {
    {{5, 3, 10, 0.0f, 0.5f}, {6, 3, 15, 0.1f, 0.8f}},
    {{6, 4, 10, 0.2f, 0.9f}, {7, 4, 15, 0.2f, 1.0f}},
    {{8, 4, 10, 0.3f, 1.0f}, {9, 5, 15, 0.2f, 1.0f}},
    {{10, 5, 25, 0.2f, 1.0f}},
    {{11, 5, 10, 0.3f, 1.0f}, {12, 6, 15, 0.2f, 1.0f}},
    {{13, 6, 10, 0.3f, 1.0f}, {14, 6, 15, 0.3f, 1.0f}},
};

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

static void survey(int n, int k)
{
    for (uint32_t s = 1; s <= 12; s++) {
        cand c;
        fld_deal(&c.b, n, k, s * 7919u + 17u);
        clock_t t0 = clock();
        judge(&c);
        double t = (double)(clock() - t0) / CLOCKS_PER_SEC;
        int bw[3] = {24, 60, 200};
        printf("%dx%d k%d seed %u: par %d (%s) greedy %d, %.2f s;", n, n, k, s, c.par, c.proved ? "proved" : "best",
               c.greedy, t);
        for (int i = 0; i < 3; i++) {
            uint8_t w[FLD_MOVES];
            clock_t t1 = clock();
            int l = fld_solve(&c.b, w, FLD_MOVES, bw[i], 0, NULL);
            printf(" beam %d: %d %.3f s;", bw[i], l, (double)(clock() - t1) / CLOCKS_PER_SEC);
        }
        printf("\n");
        fflush(stdout);
    }
}

int main(int argc, char **argv)
{
    if (argc == 4 && !strcmp(argv[1], "survey")) {
        survey(atoi(argv[2]), atoi(argv[3]));
        return 0;
    }
    uint32_t seed = 20261017u;
    int level = 0;
    for (int ch = 0; ch < FLD_CHAPTERS; ch++) {
        int first = nchosen;
        for (int m = 0; m < 3 && CHAPTER[ch][m].count; m++) {
            const make *mk = &CHAPTER[ch][m];
            int nc = 3 * mk->count;
            cand *cs = calloc((size_t)nc, sizeof(cand));
            if (!cs) return 1;
            for (int i = 0; i < nc; i++) {
                do {
                    fld_deal(&cs[i].b, mk->n, mk->k, seed++);
                    key_of(&cs[i].b, cs[i].key);
                } while (known(cs[i].key));
                judge(&cs[i]);
                fprintf(stderr, "\rchapter %d make %d: %d/%d", ch + 1, m + 1, i + 1, nc);
            }
            qsort(cs, (size_t)nc, sizeof(cand), by_work);
            int lo = (int)(mk->lo * (nc - 1)), hi = (int)(mk->hi * (nc - 1));
            for (int i = 0; i < mk->count; i++) {
                int at = mk->count > 1 ? lo + (hi - lo) * i / (mk->count - 1) : lo;
                while (at < nc && known(cs[at].key)) at++;
                if (at >= nc) return 1;
                chosen[nchosen++] = cs[at];
            }
            free(cs);
        }
        qsort(chosen + first, (size_t)(nchosen - first), sizeof(cand), by_work);
        level = nchosen;
    }
    fprintf(stderr, "\n");
    if (level != FLD_LEVELS) return 1;
    printf("/*\n"
           " * Flood's levels: our own boards, dealt by chance and chosen by tools/flood_gen.c (the game's own dealer\n"
           " * and solver), six chapters of 25 from the easiest. Each: \"<n>:<k>:\" the cells (a colour 0..k-1 each,\n"
           " * row by row), \":\" the best way found (a colour a move: its length is the par). In the comment: its\n"
           " * size and colours, the par (proved the fewest by the exact search, or the best found), the greedy way.\n"
           " * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n */\n");
    printf("const char *const fld_levels[FLD_LEVELS] = {\n");
    for (int i = 0; i < nchosen; i++) {
        char s[FLD_CELLS + FLD_MOVES + 16];
        cand *c = &chosen[i];
        fld_encode(&c->b, c->way, c->par, s, sizeof s);
        printf("    \"%s\", /* %d: %dx%d %d colours, par %d %s, greedy %d */\n", s, i + 1, c->b.n, c->b.n, c->b.k,
               c->par, c->proved ? "proved" : "best found", c->greedy);
    }
    printf("};\n");
    return 0;
}
