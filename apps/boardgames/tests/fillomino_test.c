/*
 * Fillomino's rules: the solver and the grader on grids drawn by hand (a grid of one region, numbers that leave no
 * way, numbers that leave two), the puzzles made from many seeds (one solution, the level they were made for, well
 * under a second each), the player's numbers, notes, the clashes shown, hints and undo, whole games played from a
 * seed, and every level.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/puzrand.h"
#include "games/fillomino.h"
#include "games/fillomino_levels.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

/* the player's grid holding the numbers of v[]: solved by the rules (each region of its number's size)? */
static int keeps_rules(const flm_puzzle *p, const uint8_t *v)
{
    static flm_game g;
    memset(&g, 0, sizeof g);
    g.p = *p;
    memcpy(g.val, v, sizeof g.val);
    for (int c = 0; c < p->n * p->n; c++) {
        if (!v[c] || g.val[c] > FLM_TOP) return 0;
        if (flm_region(&g, c) != v[c]) return 0;
    }
    return 1;
}

int main(void)
{
    flm_puzzle p;
    uint8_t sol[FLM_CELLS];

    /* drawn by hand: a 3 x 3 with one 9: all nines */
    CHECK(flm_decode("3:....9.....", &p) == 0);   /* too long */
    CHECK(flm_decode("3:....9....", &p) && flm_count(&p, 5) == 1);
    CHECK(flm_grade(&p, sol) == BENCH_MEDIUM && keeps_rules(&p, sol));   /* every cell needed by the 9 */
    for (int c = 0; c < 9; c++) CHECK(sol[c] == 9);
    /* two ones side by side: no way */
    CHECK(flm_decode("3:11.......", &p) && flm_count(&p, 2) == 0 && flm_grade(&p, NULL) == BENCH_NONE);
    /* a 3 x 3 with a 3 in each corner of the top row and nothing else: more than one way */
    CHECK(flm_decode("3:3.3......", &p) && flm_count(&p, 2) == 2);
    /* a region too big is a clash; one of its size is not */
    {
        static flm_game F;
        memset(&F, 0, sizeof F);
        flm_decode("3:.........", &F.p);
        F.val[0] = F.val[1] = 2;
        CHECK(!flm_clash(&F, 0) && flm_region(&F, 1) == 2);
        F.val[2] = 2;
        CHECK(flm_clash(&F, 0) && flm_clash(&F, 2) && flm_region(&F, 0) == 3);
    }

    /* the puzzles from many seeds: one solution, of their level, quick */
    clock_t t0 = clock();
    double slowest[FLM_VARIANTS] = {0};
    int at_level[FLM_VARIANTS] = {0};
    for (int v = 0; v < FLM_VARIANTS; v++)
        for (uint32_t seed = 1; seed <= 10; seed++) {
            clock_t t1 = clock();
            flm_make(flm_size(v), flm_level(v), seed * 7919u + (uint32_t)v, &p, sol);
            double took = (double)(clock() - t1) / CLOCKS_PER_SEC;
            if (took > slowest[v]) slowest[v] = took;
            CHECK(p.n == flm_size(v) && keeps_rules(&p, sol));
            uint8_t found[FLM_CELLS];
            int lv = flm_grade(&p, found);
            CHECK(lv >= BENCH_EASY && lv <= flm_level(v));
            at_level[v] += lv == flm_level(v);
            CHECK(!memcmp(found, sol, (size_t)p.n * p.n));
            CHECK(flm_count(&p, 2) == 1);
            for (int c = 0; c < p.n * p.n; c++) CHECK(!p.given[c] || p.given[c] == sol[c]);
        }
    printf("fillomino: 10 puzzles a variant in %.1f s; the slowest", (double)(clock() - t0) / CLOCKS_PER_SEC);
    for (int v = 0; v < FLM_VARIANTS; v++) printf(" %.3f", slowest[v]);
    printf(" s; at their level");
    for (int v = 0; v < FLM_VARIANTS; v++) printf(" %d", at_level[v]);
    printf("\n");
    for (int v = 0; v < FLM_VARIANTS; v++) CHECK(slowest[v] < 0.6 && at_level[v] >= 8);

    /* the same seed: the same puzzle */
    static flm_game a, b;
    flm_new(&a, 7, 42);
    flm_new(&b, 7, 42);
    CHECK(!memcmp(&a.p, &b.p, sizeof a.p) && !memcmp(a.sol, b.sol, sizeof a.sol));

    /* the player's numbers: put, the same again, a note, the eraser; a given number stays; undo */
    static flm_game G;
    flm_new(&G, 0, 7);
    int n = G.p.n, f = -1, gv = -1;
    for (int c = 0; c < n * n; c++) {
        if (!G.p.given[c] && f < 0) f = c;
        if (G.p.given[c] && gv < 0) gv = c;
    }
    CHECK(G.val[gv] == G.p.given[gv] && !flm_put(&G, gv, 1) && !flm_erase(&G, gv));
    CHECK(flm_put(&G, f, 4) && G.val[f] == 4 && !flm_put(&G, f, 4) && !flm_note(&G, f, 2));
    CHECK(flm_erase(&G, f) && !G.val[f] && flm_note(&G, f, 2) && flm_note(&G, f, 5));
    CHECK(G.notes[f] == (1u << 2 | 1u << 5));
    CHECK(flm_put(&G, f, 3) && !G.notes[f] && flm_undo(&G) == f && G.notes[f] == (1u << 2 | 1u << 5) && !G.val[f]);
    while (flm_undo(&G) >= 0) {}
    CHECK(!G.val[f] && !G.notes[f] && G.hist.moves == 0);
    /* the notation */
    {
        char moves[16];
        snprintf(moves, sizeof moves, "%c%d=%d", 'a' + f % n, f / n + 1, 7);
        CHECK(flm_script(&G, moves) && G.val[f] == 7);
        snprintf(moves, sizeof moves, "%c%d'%d", 'a' + f % n, f / n + 1, 7);
        CHECK(!flm_script(&G, moves));          /* a note where a number is */
        CHECK(!flm_script(&G, "z9=1") && !flm_script(&G, "a1"));
        while (flm_undo(&G) >= 0) {}
    }

    /* a hint: a wrong number put right first, else the next cell found; hints alone solve a puzzle */
    for (int v = 0; v < FLM_VARIANTS; v++) {
        flm_new(&G, v, 100u + (uint32_t)v);
        n = G.p.n;
        int wrong = -1;
        for (int c = 0; c < n * n && wrong < 0; c++)
            if (!G.p.given[c]) wrong = c;
        CHECK(flm_put(&G, wrong, G.sol[wrong] % 9 + 1) && flm_hint(&G) == wrong && G.val[wrong] == G.sol[wrong]);
        int guard = 0;
        while (!flm_solved(&G) && guard++ < n * n) {
            int c = flm_hint(&G);
            CHECK(c >= 0 && G.val[c] == G.sol[c]);
            if (c < 0) break;
        }
        CHECK(flm_solved(&G) && G.hist.hints > 0);
    }

    /* whole games from a seed: random numbers, notes and erasing, then hints to the end; undo all the way back */
    for (uint32_t seed = 1; seed <= 20; seed++) {
        uint32_t s = pr_seed(seed);
        flm_new(&G, (int)(seed % FLM_VARIANTS), seed);
        n = G.p.n;
        static flm_game first;
        first = G;
        for (int i = 0; i < 40; i++) {
            int c = pr_below(&s, n * n), kind = pr_below(&s, 4), d = 1 + pr_below(&s, FLM_TOP);
            if (kind < 2) flm_put(&G, c, d);
            else if (kind == 2) flm_note(&G, c, d);
            else flm_erase(&G, c);
        }
        int guard = 0;
        while (!flm_solved(&G) && guard++ < 2 * n * n) CHECK(flm_hint(&G) >= 0);
        CHECK(flm_solved(&G) && keeps_rules(&G.p, G.val));
        while (flm_undo(&G) >= 0) {}
        CHECK(!memcmp(G.val, first.val, sizeof G.val) && !memcmp(G.notes, first.notes, sizeof G.notes));
        CHECK(G.hist.moves == 0);
    }
    /* the stars' mistakes: a wrong number taken back with undo */
    flm_new(&G, 0, 11);
    {
        int c = -1;
        for (int i = 0; i < G.p.n * G.p.n && c < 0; i++)
            if (!G.p.given[i]) c = i;
        flm_put(&G, c, G.sol[c] % 9 + 1);
        flm_undo(&G);
        CHECK(G.hist.mistakes == 1 && G.hist.hints == 0);
    }

    /* the levels: each decodes and encodes back, has one solution found by reasoning alone (easy in chapter 1, hard
     * at the end), and the sizes never shrink */
    {
        int last_n = 0, grades[3] = {0, 0, 0};
        clock_t t1 = clock();
        for (int l = 0; l < FLM_LEVELS; l++) {
            flm_puzzle q;
            static flm_game L;
            char back[FLM_CELLS + 64];
            int ok = flm_decode(flm_levels[l], &q) && flm_load(&L, &q) && flm_count(&q, 2) == 1;
            ok = ok && keeps_rules(&q, L.sol);
            if (!ok) printf("fillomino level %d: not one solution\n", l + 1);
            CHECK(ok);
            CHECK(flm_encode(&q, back, sizeof back) && !strcmp(back, flm_levels[l]));
            CHECK(q.n >= last_n);
            last_n = q.n;
            if (ok) grades[L.level]++;
            if (l < FLM_LEVELS / FLM_CHAPTERS) CHECK(L.level == BENCH_EASY);
        }
        flm_puzzle q;
        CHECK(flm_decode(flm_levels[FLM_LEVELS - 1], &q) && flm_grade(&q, NULL) == BENCH_HARD);
        printf("fillomino: %d levels, one solution each (easy %d, medium %d, hard %d) in %.1f s\n", FLM_LEVELS,
               grades[0], grades[1], grades[2], (double)(clock() - t1) / CLOCKS_PER_SEC);
    }

    if (fails) printf("fillomino: %d checks failed\n", fails);
    else printf("fillomino: all checks passed\n");
    return fails != 0;
}
