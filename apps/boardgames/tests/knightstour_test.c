/*
 * The Knight's Tour's rules: the moves and undo, the solver (on 5 x 5 checked square by square against a full search:
 * a tour from each of the 13 squares of the corners' colour, none from the 12 others), the starts drawn from hundreds
 * of seeds (each one with a tour, found and played), whole tours played by following the hint alone, the hint never
 * leading into a dead end (and saying so when no tour can be finished), the closed tours, holes and dead ends, every
 * level toured by the solver and by the hint alone, and the time it all takes.
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
        if (i && !knight_step(g->w, g->path[i - 1], g->path[i])) return 0;
        if (g->order[g->path[i]] != i + 1) return 0;
    }
    return g->len == g->cells;
}

int main(void)
{
    kt_game g;
    uint8_t out[KT_MAX];

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
            CHECK(g.w == n && g.h == n && g.cells == n * n && g.len == 1);
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
            uint8_t to[8];
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

    /* holes: never visited, never a move; a board with holes done when every other square is */
    {
        static const uint8_t ring[9] = {0, 0, 0, 0, 1, 0, 0, 0, 0};   /* 3 x 3 without its middle: a ring of 8 */
        CHECK(kt_setup(&g, 3, 3, ring, 0) && g.cells == 8 && !kt_setup(&g, 3, 3, ring, 4));
        kt_setup(&g, 3, 3, ring, 0);
        CHECK(!kt_can(&g, 4) && kt_script(&g, "b3 c1 a2 c3 b1 a3 c2") && kt_done(&g) && kt_closed(&g));
        CHECK(valid_tour(&g) && kt_stars(&g) == 3);
        CHECK(kt_load(&g, "N.#|#..|...") && g.w == 3 && g.h == 3 && g.cells == 7 && g.path[0] == 6 && g.hole[8]);
        CHECK(!kt_load(&g, "N..|..") && !kt_load(&g, "...|...") && !kt_load(&g, "N.N|...") && !kt_load(&g, "N.x"));
        /* a dead end counted (and not by the hint's look ahead); the stars */
        kt_new_at(&g, 4, 0);                    /* 4 x 4: no tour, any walk ends stuck */
        int dead = 0;
        for (int i = 0; i < 30 && !dead; i++) {     /* the first square each time, until stuck */
            uint8_t to[8];
            if (!kt_moves(&g, to)) break;
            kt_move(&g, to[0]);
            dead = kt_stuck(&g);
        }
        CHECK(dead && g.dead_ends == 1 && kt_stars(&g) == 2);
        kt_new_at(&g, 6, 0);
        int h1 = kt_hint(&g);
        CHECK(g.dead_ends == 0 && g.hints == 1 && kt_stars(&g) == 2);
        CHECK(kt_hint(&g) == h1 && g.hints == 1);   /* asked again in the same position: counted once */
        CHECK(kt_move(&g, h1) && kt_undo(&g) && kt_hint(&g) == h1 && g.hints == 1);
        g.hints = 3;
        CHECK(kt_stars(&g) == 1);
    }

    /* every level: a board within the bounds, a tour from its start found by the solver and by the hint alone (in a
     * moment), the chapters' sizes growing, no level twice */
    {
        static const int lo[KT_CHAPTERS] = {8, 20, 30, 40, 52, 68}, hi[KT_CHAPTERS] = {20, 30, 40, 52, 68, 101};
        clock_t t0 = clock();
        double worst = 0;
        int holes = 0, shapes = 0;
        for (int l = 0; l < KT_LEVELS; l++) {
            if (!kt_levels[l] || !kt_level(&g, l)) {
                printf("FAIL level %d: not loaded\n", l + 1);
                fails++;
                continue;
            }
            int c = l / (KT_LEVELS / KT_CHAPTERS);
            CHECK(g.cells >= lo[c] && g.cells < hi[c]);
            CHECK(g.w >= 3 && g.h >= 3 && g.w <= KT_MAXW && g.h <= KT_MAXW && g.len == 1);
            holes += g.cells < g.w * g.h;
            shapes += g.w != g.h;
            for (int m = 0; m < l; m++) CHECK(!kt_levels[m] || strcmp(kt_levels[m], kt_levels[l]) != 0);
            kt_game h = g;
            CHECK(kt_solve(&h, out, 1L << 30) == 1);
            clock_t a = clock();
            while (!kt_done(&h)) {
                int s = kt_hint(&h);
                if (s < 0 || !kt_move(&h, s)) break;
            }
            double dt = (double)(clock() - a) / CLOCKS_PER_SEC;
            if (dt > worst) worst = dt;
            CHECK(kt_done(&h) && valid_tour(&h) && h.dead_ends == 0);
            if (!kt_done(&h)) printf("FAIL level %d: the hint does not get round\n", l + 1);
        }
        printf("%d levels: %d with holes, %d not square; the slowest tour by the hint %.3f s, all %.2f s\n", KT_LEVELS,
               holes, shapes, worst, (double)(clock() - t0) / CLOCKS_PER_SEC);
        CHECK(worst < 1.0 && holes > 50 && shapes > 30);
    }

    if (fails) {
        printf("knightstour: %d failures\n", fails);
        return 1;
    }
    printf("knightstour: ok\n");
    return 0;
}
