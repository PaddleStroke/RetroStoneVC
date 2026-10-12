/*
 * The Knight's Tour's rules: the moves and undo, the solver (on 5 x 5 checked square by square against a full search:
 * a tour from each of the 13 squares of the corners' colour, none from the 12 others), the starts drawn from hundreds
 * of seeds (each one with a tour, found and played), whole tours played by following the hint alone, the hint never
 * leading into a dead end (and saying so when no tour can be finished), the closed tours, and the time it all takes.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/knightstour.h"
#include "games/puzrand.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

static int knight_step(int n, int a, int b)
{
    int df = a % n - b % n, dr = a / n - b / n;
    return df * df + dr * dr == 5;
}

/* every square visited once, each step a knight's move */
static int valid_tour(const kt_game *g)
{
    int seen[KT_MAX] = {0};
    for (int i = 0; i < g->len; i++) {
        if (seen[g->path[i]]++) return 0;
        if (i && !knight_step(g->n, g->path[i - 1], g->path[i])) return 0;
        if (g->order[g->path[i]] != i + 1) return 0;
    }
    return g->len == g->n * g->n;
}

int main(void)
{
    kt_game g;
    int8_t out[KT_MAX];

    /* the moves, undo */
    kt_new_at(&g, 5, 0);
    CHECK(kt_can(&g, 7) && kt_can(&g, 11) && !kt_can(&g, 1) && !kt_can(&g, 6) && !kt_can(&g, 0));
    CHECK(kt_move(&g, 7) && g.len == 2 && g.order[7] == 2);
    CHECK(!kt_move(&g, 0));                    /* visited */
    CHECK(!kt_move(&g, 8));                    /* not a knight's move */
    CHECK(kt_undo(&g) && g.len == 1 && !g.order[7] && !kt_undo(&g));
    CHECK(kt_script(&g, "a1 b3 c5") && g.len == 3 && g.path[2] == 22);
    CHECK(!kt_script(&g, "e5"));               /* c5 to e5: no */
    kt_new_at(&g, 5, 0);
    CHECK(kt_script(&g, "c3 d5") && g.len == 2 && g.path[0] == 12);   /* the first square names the start */
    kt_new_at(&g, 6, 0);
    CHECK(kt_script(&g, "? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ?") && kt_done(&g));

    /* 5 x 5: a tour exactly from the corners' colour (a full search, no budget) */
    for (int s = 0; s < 25; s++) {
        kt_new_at(&g, 5, s);
        int r = kt_solve(&g, out, 1L << 40);
        CHECK(r == ((s % 5 + s / 5) % 2 == 0));
        if (r == 1) {
            for (int i = 0; i < 24; i++) CHECK(kt_move(&g, out[i]));
            CHECK(kt_done(&g) && valid_tour(&g));
        }
    }

    /* the starts from many seeds: each with a tour, played; then whole tours by the hint alone */
    for (int v = 0; v < KT_VARIANTS; v++) {
        int n = kt_size(v), closed = 0;
        clock_t t0 = clock();
        double worst = 0;
        for (uint32_t seed = 1; seed <= 300; seed++) {
            clock_t a = clock();
            kt_new(&g, v, seed);
            double dt = (double)(clock() - a) / CLOCKS_PER_SEC;
            if (dt > worst) worst = dt;
            CHECK(g.n == n && g.len == 1);
            if (n == 5) CHECK((g.path[0] % 5 + g.path[0] / 5) % 2 == 0);
            CHECK(kt_solve(&g, out, 1L << 30) == 1);
            kt_game h = g;
            for (int i = 0; i < n * n - 1; i++) CHECK(kt_move(&h, out[i]));
            CHECK(valid_tour(&h));
            CHECK(kt_closed(&h) == knight_step(n, h.path[0], h.path[n * n - 1]));
            closed += kt_closed(&h);
            if (seed <= 60) {                   /* the hint alone */
                while (!kt_done(&g)) {
                    int s = kt_hint(&g);
                    CHECK(s >= 0);
                    if (s < 0 || !kt_move(&g, s)) break;
                }
                CHECK(valid_tour(&g) && g.hints == n * n - 1);
                CHECK(kt_hint(&g) == -2);
            }
        }
        printf("%d x %d: 300 starts, the slowest %.3f s; 60 tours by the hint, all %.2f s; %d closed\n", n, n,
               worst, (double)(clock() - t0) / CLOCKS_PER_SEC, closed);
        CHECK(worst < 0.25);
    }

    /* random walks, then the hint: never into a dead end; -1 only when no tour can be finished (a full search on the
     * small boards) */
    int said_no = 0, followed = 0;
    for (uint32_t seed = 1; seed <= 400; seed++) {
        uint32_t rs = pr_seed(seed);
        int v = pr_below(&rs, 2);               /* 5 x 5 and 6 x 6: a full search is quick */
        kt_new(&g, v, seed);
        int steps = 3 + pr_below(&rs, 12);
        for (int i = 0; i < steps; i++) {
            int8_t to[8];
            int k = kt_moves(&g, to);
            if (!k) break;
            kt_move(&g, to[pr_below(&rs, k)]);
        }
        if (kt_stuck(&g)) {
            CHECK(kt_hint(&g) == -2);
            continue;
        }
        int s = kt_hint(&g);
        int exists = kt_solve(&g, NULL, 1L << 40);
        if (s == -1) {
            CHECK(exists == 0);
            said_no++;
            continue;
        }
        CHECK(exists == 1 && s >= 0);
        followed++;
        while (s >= 0 && kt_move(&g, s)) s = kt_hint(&g);
        CHECK(kt_done(&g) && valid_tour(&g));
    }
    printf("random walks: the hint finished %d, said no tour %d times\n", followed, said_no);
    CHECK(followed > 50 && said_no > 10);

    /* a closed tour on 6 x 6 found by searching the starts' tours, and an open one */
    {
        int seen_closed = 0, seen_open = 0;
        for (uint32_t seed = 1; seed <= 300 && !(seen_closed && seen_open); seed++) {
            kt_new(&g, KT_6, seed);
            kt_solve(&g, out, 1L << 30);
            for (int i = 0; i < 35; i++) kt_move(&g, out[i]);
            if (kt_closed(&g)) seen_closed = 1;
            else seen_open = 1;
            CHECK(kt_done(&g));
        }
        CHECK(seen_open);
        printf("closed tour seen on 6 x 6: %d\n", seen_closed);
    }

    if (fails) {
        printf("knightstour: %d failures\n", fails);
        return 1;
    }
    printf("knightstour: ok\n");
    return 0;
}
