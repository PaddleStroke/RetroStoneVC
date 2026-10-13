/*
 * Tents' rules: the solver and the grader on fields drawn by hand (the counts, the tents touching, a tree's own
 * tent), the fields made from many seeds (one solution, the level they were made for, well under a second each), the
 * player's tents, grass, drags, the faults shown, hints and undo, and whole games played from a seed.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/puzrand.h"
#include "games/tents.h"
#include "games/tents_levels.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

/* a field from rows of text ('T' a tree, '.' else) and its counts (rows, then columns; -1 not told) */
static void field_of(tn_puzzle *p, int n, const char *const *rows, const int *counts)
{
    memset(p, 0, sizeof *p);
    p->n = n;
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++) p->tree[y * n + x] = rows[y][x] == 'T';
    for (int i = 0; i < n; i++) {
        p->count[0][i] = (int8_t)counts[i];
        p->count[1][i] = (int8_t)counts[n + i];
    }
}

static int keeps_rules(const tn_puzzle *p, const uint8_t *tents)
{
    tn_game g;
    memset(&g, 0, sizeof g);
    g.p = *p;
    memcpy(g.sol, tents, (size_t)p->n * p->n);
    for (int c = 0; c < p->n * p->n; c++) g.st[c] = tents[c] ? TN_TENT : TN_EMPTY;
    return tn_solved(&g);
}

int main(void)
{
    tn_puzzle p;
    uint8_t sol[TN_CELLS];

    /* drawn by hand: two trees, the counts leave each one place */
    static const char *const two[] = {"T...", "....", "....", "...T"};
    static const int two_counts[] = {0, 1, 1, 0, 1, 0, 0, 1};
    field_of(&p, 4, two, two_counts);
    CHECK(tn_grade(&p, sol) == BENCH_EASY && sol[4] && sol[11] && keeps_rules(&p, sol));
    CHECK(tn_count(&p, 3) == 1);
    /* no counts: two ways; counts of nothing: none; two tents touching are no way */
    static const int unknown[] = {-1, -1, -1, -1, -1, -1, -1, -1};
    field_of(&p, 4, two, unknown);
    CHECK(tn_count(&p, 10) > 1 && tn_grade(&p, NULL) == BENCH_GUESS);
    static const int zero[] = {0, 0, 0, 0, 0, 0, 0, 0};
    field_of(&p, 4, two, zero);
    CHECK(tn_count(&p, 2) == 0 && tn_grade(&p, NULL) == BENCH_NONE);
    static const char *const pair[] = {"T...", "T...", "....", "...."};
    static const int touch_counts[] = {1, 1, 0, 0, 0, 2, 0, 0};   /* both tents in column b: they would touch */
    field_of(&p, 4, pair, touch_counts);
    CHECK(tn_count(&p, 2) == 0);
    /* one tent a tree: two trees side by side with one cell between them both can use is not enough */
    static const char *const share[] = {"T.T.", "....", "....", "...."};
    static const int share_counts[] = {1, 0, 0, 0, 0, 1, 0, 0};
    field_of(&p, 4, share, share_counts);
    CHECK(tn_count(&p, 2) == 0 && tn_grade(&p, NULL) == BENCH_NONE);

    /* the fields from many seeds: one solution, of their level, quick */
    clock_t t0 = clock();
    double slowest[TN_VARIANTS] = {0};
    int trees[TN_VARIANTS] = {0};
    for (int v = 0; v < TN_VARIANTS; v++)
        for (uint32_t seed = 1; seed <= 40; seed++) {
            clock_t t1 = clock();
            tn_make(tn_size(v), tn_level(v), seed * 7919u + (uint32_t)v, &p, sol);
            double took = (double)(clock() - t1) / CLOCKS_PER_SEC;
            if (took > slowest[v]) slowest[v] = took;
            int n = p.n, tents = 0;
            CHECK(n == tn_size(v) && keeps_rules(&p, sol));
            uint8_t found[TN_CELLS];
            int lv = tn_grade(&p, found);
            if (lv != tn_level(v)) printf("variant %d seed %u: graded %d\n", v, seed, lv);
            CHECK(lv == tn_level(v));
            CHECK(!memcmp(found, sol, (size_t)n * n));
            CHECK(tn_count(&p, 2) == 1);
            for (int c = 0; c < n * n; c++) {
                trees[v] += p.tree[c];
                tents += sol[c];
                CHECK(!(sol[c] && p.tree[c]));
            }
            for (int i = 0; i < n; i++) CHECK(p.count[0][i] >= 0 && p.count[1][i] >= 0);   /* every count told */
            CHECK(tents * 8 >= n * n);              /* about a cell in five: never much fewer */
        }
    printf("tents: 40 fields a variant in %.1f s; the slowest", (double)(clock() - t0) / CLOCKS_PER_SEC);
    for (int v = 0; v < TN_VARIANTS; v++) printf(" %.3f", slowest[v]);
    printf(" s; trees");
    for (int v = 0; v < TN_VARIANTS; v++) printf(" %d", trees[v] / 40);
    printf("\n");
    for (int v = 0; v < TN_VARIANTS; v++) CHECK(slowest[v] < 0.5);

    /* the same seed: the same field */
    tn_game a, b;
    tn_new(&a, 10, 42);
    tn_new(&b, 10, 42);
    CHECK(!memcmp(&a.p, &b.p, sizeof a.p) && !memcmp(a.sol, b.sol, sizeof a.sol));

    /* the player's cells: a tap a tent or none, a hold grass; a tree takes nothing; undo */
    tn_game G;
    tn_new(&G, 0, 7);
    int n = G.p.n, f = -1, tree = -1;
    for (int c = 0; c < n * n; c++) {
        if (tn_free(&G, c) && f < 0) f = c;
        if (!tn_free(&G, c) && tree < 0) tree = c;
    }
    CHECK(tn_tap_state(&G, f) == TN_TENT && tn_hold_state(&G, f) == TN_GRASS);
    CHECK(tn_paint(&G, &f, 1, TN_TENT) && tn_tap_state(&G, f) == TN_EMPTY && tn_hold_state(&G, f) == TN_GRASS);
    CHECK(tn_paint(&G, &f, 1, TN_GRASS) && tn_tap_state(&G, f) == TN_TENT && tn_hold_state(&G, f) == TN_EMPTY);
    CHECK(!tn_paint(&G, &tree, 1, TN_TENT));
    CHECK(G.hist.moves == 2 && tn_undo(&G) == f && G.st[f] == TN_TENT && tn_undo(&G) == f && !G.st[f]);
    CHECK(tn_undo(&G) < 0);
    /* the faults: tents touching (a corner too), a tree with no room left, a line's tents */
    tn_game F;
    memset(&F, 0, sizeof F);
    field_of(&F.p, 4, pair, touch_counts);
    F.st[1] = F.st[6] = TN_TENT;
    CHECK(tn_touching(&F, 1) && tn_touching(&F, 6) && tn_line_tents(&F, 1, 1) == 1 && tn_line_tents(&F, 1, 2) == 1);
    F.st[6] = TN_EMPTY;
    CHECK(!tn_touching(&F, 1) && !tn_tree_lost(&F, 4));
    F.st[5] = F.st[8] = TN_GRASS;
    CHECK(tn_tree_lost(&F, 4) && !tn_tree_lost(&F, 0));
    /* the notation: a hold dragged down a column puts grass in every empty cell of it, and passes the trees */
    tn_new(&G, 7, 3);
    n = G.p.n;
    int col = -1;
    for (int x = 0; x < n && col < 0; x++)
        if (tn_free(&G, x)) col = x;
    char moves[32];
    snprintf(moves, sizeof moves, ".%c1-%c%d", (char)('a' + col), (char)('a' + col), n % 100);
    CHECK(tn_script(&G, moves) && G.hist.moves == 1);
    for (int y = 0; y < n; y++) {
        int c = y * n + col;
        CHECK(G.p.tree[c] ? G.st[c] == TN_EMPTY : G.st[c] == TN_GRASS);
    }
    CHECK(tn_undo(&G) >= 0);
    for (int y = 0; y < n; y++) CHECK(G.st[y * n + col] == TN_EMPTY);
    CHECK(!tn_script(&G, "z9") && !tn_script(&G, "a1-c3"));

    /* a hint: a wrong cell put right first, else the next cell found; hints alone solve a field */
    for (int v = 0; v < TN_VARIANTS; v++) {
        tn_new(&G, v, 100u + (uint32_t)v);
        n = G.p.n;
        int wrong = -1;
        for (int c = 0; c < n * n && wrong < 0; c++)
            if (tn_free(&G, c) && !G.sol[c]) wrong = c;
        CHECK(tn_paint(&G, &wrong, 1, TN_TENT) && tn_hint(&G) == wrong && G.st[wrong] == TN_GRASS);
        int guard = 0;
        while (!tn_solved(&G) && guard++ < n * n) {
            int c = tn_hint(&G);
            CHECK(c >= 0 && (G.st[c] == TN_TENT) == G.sol[c]);
            if (c < 0) break;
        }
        CHECK(tn_solved(&G));
    }

    /* whole games from a seed: random taps, holds and drags, then hints to the end; undo all the way back */
    for (uint32_t seed = 1; seed <= 30; seed++) {
        uint32_t s = pr_seed(seed);
        tn_new(&G, (int)(seed % TN_VARIANTS), seed);
        n = G.p.n;
        tn_game start = G;
        for (int i = 0; i < 50; i++) {
            int c = pr_below(&s, n * n), kind = pr_below(&s, 3);
            if (!tn_free(&G, c)) continue;
            if (kind < 2) {
                tn_paint(&G, &c, 1, kind ? tn_hold_state(&G, c) : tn_tap_state(&G, c));
            } else {
                int cells[TN_MAX], k = 0, was = G.st[c];
                for (int x = c; x < (c / n + 1) * n; x++)
                    if (tn_free(&G, x) && G.st[x] == was) cells[k++] = x;
                tn_paint(&G, cells, k, tn_hold_state(&G, c));
            }
        }
        int guard = 0;
        while (!tn_solved(&G) && guard++ < 2 * n * n) CHECK(tn_hint(&G) >= 0);
        CHECK(tn_solved(&G));
        while (tn_undo(&G) >= 0) {}
        CHECK(!memcmp(G.st, start.st, sizeof G.st) && G.hist.moves == 0);
    }

    /* the levels: each decodes and encodes back, has one solution found by reasoning alone (easy in chapter 1, hard
     * at the end), and the sizes never shrink */
    {
        int last_n = 0, grades[3] = {0, 0, 0};
        clock_t t1 = clock();
        for (int l = 0; l < TN_LEVELS; l++) {
            tn_puzzle q;
            static tn_game L;
            char back[TN_CELLS + 64];
            int ok = tn_decode(tn_levels[l], &q) && tn_load(&L, &q) && tn_count(&q, 2) == 1;
            for (int c = 0; ok && c < q.n * q.n; c++) L.st[c] = L.sol[c] ? TN_TENT : TN_EMPTY;
            ok = ok && tn_solved(&L);           /* its solution keeps the rules */
            if (!ok) printf("tents level %d: not one solution\n", l + 1);
            CHECK(ok);
            CHECK(tn_encode(&q, back, sizeof back) && !strcmp(back, tn_levels[l]));
            CHECK(q.n >= last_n);
            last_n = q.n;
            if (ok) grades[L.level]++;
            if (l < TN_LEVELS / TN_CHAPTERS) CHECK(L.level == BENCH_EASY);
        }
        tn_puzzle q;
        CHECK(tn_decode(tn_levels[TN_LEVELS - 1], &q) && tn_grade(&q, NULL) == BENCH_HARD);
        printf("tents: %d levels, one solution each (easy %d, medium %d, hard %d) in %.1f s\n", TN_LEVELS,
               grades[0], grades[1], grades[2], (double)(clock() - t1) / CLOCKS_PER_SEC);
    }


    if (fails) printf("tents: %d checks failed\n", fails);
    else printf("tents: all checks passed\n");
    return fails != 0;
}
