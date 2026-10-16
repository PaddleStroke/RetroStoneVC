/*
 * Flood's rules: the region and a move on boards drawn by hand, the solver against a plain search of every way on
 * small boards (its par the fewest), every level (its way floods it in par moves), the random boards (their par, the
 * time on the biggest), the move limit, undo, the hint followed to the end, the notation, and random games.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/flood.h"
#include "games/puzrand.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

static void board_of(fld_board *b, int n, int k, const char *cells)
{
    memset(b, 0, sizeof *b);
    b->n = n;
    b->k = k;
    for (int i = 0; i < n * n; i++) b->c[i] = (uint8_t)(cells[i] - '0');
}

/* 1: flooded within depth moves, trying every way (a colour that floods something) */
static int within(const fld_board *b, const uint8_t *in, int depth)
{
    if (fld_flooded(b)) return 1;
    if (depth == 0) return 0;
    for (int c = 0; c < b->k; c++) {
        fld_board t = *b;
        uint8_t tin[FLD_CELLS];
        memcpy(tin, in, sizeof tin);
        if (fld_fill(&t, tin, c) && within(&t, tin, depth - 1)) return 1;
    }
    return 0;
}

static int fewest(const fld_board *b)
{
    uint8_t in[FLD_CELLS];
    fld_region(b, in);
    for (int d = 0; d < 30; d++)
        if (within(b, in, d)) return d;
    return -1;
}

static int floods(const fld_board *start, const uint8_t *way, int len)
{
    fld_board b = *start;
    uint8_t in[FLD_CELLS];
    fld_region(&b, in);
    for (int i = 0; i < len; i++)
        if (!fld_fill(&b, in, way[i])) return 0;
    return fld_flooded(&b);
}

int main(void)
{
    fld_board b;
    uint8_t in[FLD_CELLS], way[FLD_MOVES];

    /* by hand: the region, a move absorbing what touches it, a colour touching nothing */
    board_of(&b, 3, 3, "001"
                       "021"
                       "222");
    CHECK(fld_region(&b, in) == 3 && in[0] && in[1] && in[3] && !in[2]);
    CHECK(fld_fill(&b, in, 0) == 0);              /* its own colour: nothing */
    CHECK(fld_fill(&b, in, 2) == 4 && b.c[0] == 2 && in[8] && !in[2]);
    CHECK(fld_fill(&b, in, 1) == 2 && fld_flooded(&b));
    board_of(&b, 3, 3, "001"
                       "021"
                       "222");
    CHECK(fld_solve(&b, way, FLD_MOVES, 10, 1000, NULL) == 2 && floods(&b, way, 2));
    CHECK(fld_greedy(&b) == 2);

    /* the solver against every way, on small boards: the fewest, proved */
    int tried = 0, proved_all = 1;
    for (uint32_t s = 1; s <= 300; s++) {
        int n = 4 + (int)(s % 2), k = 3 + (int)(s % 3 == 0);
        fld_deal(&b, n, k, s * 31u);
        int proved = 0, len = fld_solve(&b, way, FLD_MOVES, 4, 200000, &proved);
        int best = fewest(&b);
        if (len != best) printf("seed %u: solver %d, every way %d\n", s, len, best);
        CHECK(len == best && floods(&b, way, len));
        proved_all &= proved;
        tried++;
    }
    CHECK(proved_all);
    /* a narrow beam alone is never shorter than the fewest */
    for (uint32_t s = 1; s <= 100; s++) {
        fld_deal(&b, 5, 4, s * 77u);
        int len = fld_solve(&b, way, FLD_MOVES, 2, 0, NULL);
        CHECK(len >= fewest(&b) && floods(&b, way, len));
    }

    /* every level: its way floods it in par moves; sizes grow with the chapters; no two alike */
    fld_game G;
    int last_n = 0, last_k = 0, proved_levels = 0;
    for (int l = 0; l < FLD_LEVELS; l++) {
        int len;
        CHECK(fld_levels[l] && fld_decode(fld_levels[l], &b, way, &len));
        CHECK(fld_level(&G, l) && G.par == len && G.limit == fld_limit(len) && G.limit > G.par);
        CHECK(floods(&b, way, len) && len <= fld_greedy(&b));
        CHECK(b.n >= 5 && b.n <= FLD_MAX && b.k >= 3 && b.k <= FLD_COLOURS);
        if (l % (FLD_LEVELS / FLD_CHAPTERS) == 0) {
            CHECK(b.n >= last_n && b.k >= last_k);
            last_n = b.n, last_k = b.k;
        }
        char s[FLD_CELLS + FLD_MOVES + 16];
        CHECK(fld_encode(&b, way, len, s, sizeof s) && !strcmp(s, fld_levels[l]));
        for (int m = 0; m < l; m++) CHECK(strcmp(fld_levels[m], fld_levels[l]));
        if (b.n <= 6) {                         /* the small ones: no way shorter */
            CHECK(fewest(&b) == len);
            proved_levels++;
        }
    }
    CHECK(!fld_level(&G, -1) && !fld_level(&G, FLD_LEVELS));

    /* the random boards: a way of par moves, never worse than greedy; quick on the biggest */
    double slowest[FLD_VARIANTS] = {0};
    int pars[FLD_VARIANTS] = {0};
    for (int v = 0; v < FLD_VARIANTS; v++)
        for (uint32_t s = 1; s <= 40; s++) {
            clock_t t0 = clock();
            fld_new(&G, v, s * 7919u + (uint32_t)v);
            double t = (double)(clock() - t0) / CLOCKS_PER_SEC;
            if (t > slowest[v]) slowest[v] = t;
            int n, k;
            fld_variant(v, &n, &k);
            CHECK(G.start.n == n && G.start.k == k && G.nmoves == 0 && !fld_done(&G));
            CHECK(G.par > 0 && floods(&G.start, G.way, G.par) && G.par <= fld_greedy(&G.start));
            pars[v] += G.par;
        }
    printf("flood: random boards, the slowest %.3f %.3f %.3f s, par %.1f %.1f %.1f; %d small boards against every "
           "way; %d small levels checked\n", slowest[0], slowest[1], slowest[2], pars[0] / 40.0, pars[1] / 40.0,
           pars[2] / 40.0, tried, proved_levels);
    for (int v = 0; v < FLD_VARIANTS; v++) CHECK(slowest[v] < 0.25);
    fld_game A, B;
    fld_new(&A, 1, 42);
    fld_new(&B, 1, 42);
    CHECK(!memcmp(&A.start, &B.start, sizeof A.start) && A.par == B.par);

    /* a move: not its own colour, not one touching nothing; undo all the way; the limit */
    fld_new(&G, FLD_SMALL, 5);
    int own = G.b.c[0];
    CHECK(!fld_can(&G, own) && !fld_play(&G, own) && !fld_can(&G, -1) && !fld_can(&G, G.b.k));
    for (int c = 0; c < G.b.k; c++) {
        fld_board t = G.b;
        uint8_t tin[FLD_CELLS];
        memcpy(tin, G.in, sizeof tin);
        CHECK(fld_can(&G, c) == (fld_fill(&t, tin, c) > 0));
    }
    fld_board before = G.b;
    int c0 = -1;
    for (int c = 0; c < G.b.k && c0 < 0; c++)
        if (fld_can(&G, c)) c0 = c;
    CHECK(fld_play(&G, c0) > 0 && G.nmoves == 1 && G.b.c[0] == c0);
    CHECK(fld_undo(&G) && G.nmoves == 0 && !memcmp(&G.b, &before, sizeof before) && !fld_undo(&G));
    /* wasting moves until the limit: then nothing more, undo opens it again */
    int guard = 0;
    while (!fld_out(&G) && !fld_done(&G) && guard++ < 200) {
        int pick = -1, least = 1 << 30;
        for (int c = 0; c < G.b.k; c++) {       /* the colour flooding the fewest cells */
            if (!fld_can(&G, c)) continue;
            fld_board t = G.b;
            uint8_t tin[FLD_CELLS];
            memcpy(tin, G.in, sizeof tin);
            int got = fld_fill(&t, tin, c);
            if (got < least) least = got, pick = c;
        }
        CHECK(pick >= 0 && fld_play(&G, pick));
    }
    if (fld_out(&G)) {
        CHECK(G.nmoves == G.limit && fld_hint(&G) < 0);
        for (int c = 0; c < G.b.k; c++) CHECK(!fld_can(&G, c));
        CHECK(fld_undo(&G) && !fld_out(&G));
    }

    /* the hint followed from the start: flooded within the limit; from the middle too */
    int at_par = 0;
    for (uint32_t s = 1; s <= 30; s++) {
        fld_new(&G, (int)(s % FLD_VARIANTS), s * 13u);
        CHECK(fld_hint(&G) == G.way[0]);
        int steps = 0;
        while (!fld_done(&G) && steps < 100) {
            int h = fld_hint(&G);
            CHECK(h >= 0 && fld_play(&G, h));
            if (h < 0) break;
            steps++;
        }
        CHECK(fld_done(&G) && G.nmoves <= G.limit);
        at_par += G.nmoves <= G.par;
    }
    printf("flood: the hints alone at par %d times of 30\n", at_par);
    CHECK(at_par >= 20);
    for (int l = 0; l < FLD_LEVELS; l += 7) {   /* a level begun by chance, then the hints */
        uint32_t r = pr_seed((uint32_t)l + 1);
        fld_level(&G, l);
        for (int i = 0; i < 3; i++) {
            int c = pr_below(&r, G.b.k);
            if (fld_can(&G, c)) fld_play(&G, c);
        }
        while (!fld_done(&G) && !fld_out(&G)) {
            int h = fld_hint(&G);
            CHECK(h >= 0);
            if (h < 0 || !fld_play(&G, h)) break;
        }
        CHECK(fld_done(&G) || G.nmoves == G.limit);
    }

    /* the notation: colours 1..k, a hint, an undo */
    fld_level(&G, 0);
    char moves[FLD_MOVES + 1];
    for (int i = 0; i < G.par; i++) moves[i] = (char)('1' + G.way[i]);
    moves[G.par] = 0;
    CHECK(fld_script(&G, moves) && fld_done(&G) && G.nmoves == G.par);
    fld_level(&G, 0);
    CHECK(fld_script(&G, "h u h") && G.nmoves == 1 && G.hints == 2);
    CHECK(!fld_script(&G, "x"));

    /* random games from seeds: legal colours by chance, undo now and then; every move floods something */
    for (uint32_t s = 1; s <= 200; s++) {
        uint32_t r = pr_seed(s);
        fld_new(&G, (int)(s % FLD_VARIANTS), s);
        int total = G.b.n * G.b.n;
        for (int step = 0; step < 300 && !fld_done(&G) && !fld_out(&G); step++) {
            if (G.nmoves && pr_below(&r, 6) == 0) {
                CHECK(fld_undo(&G));
                continue;
            }
            int c = pr_below(&r, G.b.k), had = fld_region(&G.b, in);
            if (!fld_can(&G, c)) {
                CHECK(!fld_play(&G, c));
                continue;
            }
            int got = fld_play(&G, c);
            CHECK(got > 0 && fld_region(&G.b, in) == had + got && had + got <= total);
        }
        CHECK(fld_done(&G) || fld_out(&G) || G.nmoves <= G.limit);
    }

    if (fails) printf("flood: %d FAILED\n", fails);
    else printf("flood: all passed\n");
    return fails != 0;
}
