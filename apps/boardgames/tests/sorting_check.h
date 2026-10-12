/*
 * The checks Water Sort's and Ball Sort's tests share (their engine is one, games/sorting.h): the generator's
 * guarantees over hundreds of seeds (the colours all there, no tube already done, a way that solves it, the time
 * taken), the search against a plain breadth-first search (the shortest way), positions with no way, whole games of
 * random moves and undos from a seed, the hint played to the end, the notation.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#ifndef SORTING_CHECK_H
#define SORTING_CHECK_H

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/puzrand.h"
#include "games/sorting.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

typedef int (*sc_new_fn)(srt_game *g, int variant, uint32_t seed, srt_move *way);

static int sc_same_layers(const srt_game *g)   /* four of each colour, no more, none lost */
{
    int count[SRT_COLOURS + 1] = {0};
    for (int t = 0; t < g->tubes; t++) {
        if (g->p.h[t] > SRT_CAP) return 0;
        for (int i = 0; i < SRT_CAP; i++) {
            int c = g->p.c[t][i];
            if ((i < g->p.h[t]) != (c != 0) || c > g->colours) return 0;
            count[c]++;
        }
    }
    for (int c = 1; c <= g->colours; c++)
        if (count[c] != SRT_CAP) return 0;
    return 1;
}

static int sc_same_pos(const srt_pos *a, const srt_pos *b, int tubes)
{
    for (int t = 0; t < tubes; t++)
        if (a->h[t] != b->h[t] || memcmp(a->c[t], b->c[t], SRT_CAP)) return 0;
    return 1;
}

/* the generator: hundreds of seeds a variant */
static void sc_generator(sc_new_fn make, int one)
{
    static srt_move way[SRT_PATH];
    for (int v = 0; v < SRT_VARIANTS; v++) {
        int n = SRT_VARIANT_COLOURS[v], seeds = v < 2 ? 300 : 150, shortest = 999, longest = 0, total = 0;
        clock_t t0 = clock();
        double slowest = 0;
        for (int s = 1; s <= seeds; s++) {
            srt_game g;
            clock_t a = clock();
            int len = make(&g, v, (uint32_t)s * 7919u, way);
            double ms = (clock() - a) * 1000.0 / CLOCKS_PER_SEC;
            if (ms > slowest) slowest = ms;
            CHECK(g.colours == n && g.base == n + 2 && g.tubes == n + 2 && g.one == one);
            CHECK(sc_same_layers(&g));
            CHECK(!g.p.h[n] && !g.p.h[n + 1]);  /* the two empty tubes last */
            for (int t = 0; t < n; t++) CHECK(g.p.h[t] == SRT_CAP && !srt_complete(&g.p, t));
            CHECK(len > 0 && !srt_solved(&g));
            for (int i = 0; i < len; i++) CHECK(srt_move_do(&g, way[i].from, way[i].to) == way[i].n);
            CHECK(srt_solved(&g));          /* the way solves it: every puzzle can be solved */
            CHECK(g.moves == len);
            if (len < shortest) shortest = len;
            if (len > longest) longest = len;
            total += len;
            CHECK(len >= srt_bound(&g.start, n + 2, one));
        }
        double avg = (clock() - t0) * 1000.0 / CLOCKS_PER_SEC / seeds;
        printf("  %2d colours: shortest ways %d..%d (mean %.1f); made in %.1f ms (slowest %.1f)\n", n, shortest,
               longest, (double)total / seeds, avg, slowest);
        CHECK(slowest < 400);                   /* well under a second on a phone: a few times this machine */
        CHECK(shortest >= 3 * n);               /* never a trivial puzzle */
    }
    srt_game a, b;                              /* the same seed: the same puzzle */
    make(&a, 2, 42, NULL);
    make(&b, 2, 42, NULL);
    CHECK(sc_same_pos(&a.p, &b.p, a.tubes));
    make(&b, 2, 43, NULL);
    CHECK(!sc_same_pos(&a.p, &b.p, a.tubes));
}

/* the search: at weight 4 (A*) its ways are as short as a breadth-first search's (weight 0) */
static void sc_shortest(sc_new_fn make, int one)
{
    static srt_move w1[SRT_PATH], w2[SRT_PATH];
    for (int s = 1; s <= 40; s++) {
        srt_game g;
        make(&g, s % 2, (uint32_t)s, NULL);
        int full1, full2, a = srt_solve(&g.p, g.tubes, one, 400000, 4, w1, NULL, &full1);
        int b = srt_solve(&g.p, g.tubes, one, 2000000, 0, w2, NULL, &full2);
        if (full2) continue;                    /* the plain search ran out of room: no verdict */
        CHECK(a == b);
    }
}

/* positions with no way: the search says so, having looked at every position */
static void sc_no_way(int one)
{
    srt_pos p;
    memset(&p, 0, sizeof p);
    static const uint8_t t0[4] = {1, 2, 1, 2}, t1[4] = {2, 1, 2, 1};
    memcpy(p.c[0], t0, 4);
    memcpy(p.c[1], t1, 4);
    p.h[0] = p.h[1] = 4;
    srt_move way[SRT_PATH];
    int full, len = srt_solve(&p, 2, one, 10000, 4, way, NULL, &full);
    CHECK(len == -1 && !full);
    srt_game g;
    srt_load(&g, 2, 2, one, &p);
    CHECK(srt_stuck(&g));
    int f, t, extra;
    CHECK(srt_hint(&g, &f, &t, &extra) < 0 && extra == 1);   /* the extra tube would help */
    CHECK(srt_extra(&g));
    CHECK(!srt_extra(&g));                      /* once only */
    CHECK(srt_hint(&g, &f, &t, &extra) > 0 && t == 2);
    CHECK(srt_undo(&g) == 2 && g.tubes == 2 && srt_extra_ok(&g));
    /* shuffles with one empty tube only, many of them with no way: the search and a breadth-first search agree on
     * which, and on the shortest way of the others */
    static srt_move w2[SRT_PATH];
    int none = 0;
    uint32_t r = pr_seed(99u + (uint32_t)one);
    for (int s = 0; s < 60; s++) {
        int colours = 3 + s % 2, full2;
        srt_fill(colours, &r, &p);
        len = srt_solve(&p, colours + 1, one, 400000, 4, way, NULL, &full);
        int b = srt_solve(&p, colours + 1, one, 400000, 0, w2, NULL, &full2);
        CHECK(!full && !full2);
        CHECK(len == b);
        none += len < 0;
    }
    printf("  one empty tube: %d shuffles of 60 with no way\n", none);
    CHECK(none > 0 && none < 60);
}

/* whole games from a seed: random moves (and undos now and then), the layers never lost, undo back to the start; then
 * the hints played to the end */
static void sc_games(sc_new_fn make, int one)
{
    for (int s = 1; s <= 60; s++) {
        uint32_t r = pr_seed((uint32_t)s);
        srt_game g;
        make(&g, s % 3, (uint32_t)s * 31u, NULL);
        if (s % 4 == 0) CHECK(srt_extra(&g));
        for (int k = 0; k < 120; k++) {
            if (pr_below(&r, 6) == 0) { srt_undo(&g); continue; }
            int from = pr_below(&r, g.tubes), to = pr_below(&r, g.tubes);
            srt_pos before = g.p;
            int n = srt_can(&g, from, to);
            int moved = srt_move_do(&g, from, to);
            CHECK(moved == n);
            if (!moved) { CHECK(sc_same_pos(&before, &g.p, g.tubes)); continue; }
            CHECK(one ? moved == 1 : moved >= 1);
            CHECK(g.p.h[to] <= SRT_CAP && srt_top(&g.p, to) == srt_top(&before, from));
            CHECK(sc_same_layers(&g));
        }
        while (srt_undo(&g)) {}
        CHECK(g.moves == 0 && g.tubes == g.base && sc_same_pos(&g.p, &g.start, g.tubes));
        /* the hints: from the start, then from a few random moves in, played until solved */
        for (int k = 0; k < (s % 5) * 2; k++) srt_move_do(&g, pr_below(&r, g.tubes), pr_below(&r, g.tubes));
        int steps = 0, from, to, extra;
        while (!srt_solved(&g) && steps < 300) {
            int len = srt_hint(&g, &from, &to, &extra);
            if (len < 0) {
                if (extra) { CHECK(srt_extra(&g)); continue; }
                CHECK(srt_undo(&g));            /* a dead end: undo, as the help says */
                continue;
            }
            CHECK(srt_move_do(&g, from, to) > 0);
            steps++;
        }
        CHECK(srt_solved(&g));
    }
}

/* the moves themselves: one ball, or the whole run as far as it fits */
static void sc_moves(int one)
{
    srt_pos p;
    memset(&p, 0, sizeof p);
    static const uint8_t a[4] = {1, 2, 2, 2}, b[4] = {2, 1, 0, 0}, c[4] = {1, 2, 1, 0};
    memcpy(p.c[0], a, 4);
    memcpy(p.c[1], b, 4);
    memcpy(p.c[2], c, 4);
    p.h[0] = 4, p.h[1] = 2, p.h[2] = 3;
    srt_game g;
    srt_load(&g, 2, 4, one, &p);
    CHECK(!srt_can(&g, 0, 1));                  /* 2 onto 1 */
    CHECK(!srt_can(&g, 3, 0));                  /* from an empty tube */
    CHECK(!srt_can(&g, 0, 0));
    CHECK(srt_can(&g, 0, 3) == (one ? 1 : 3));  /* into the empty tube: the run of three */
    CHECK(srt_can(&g, 1, 2) == 1);              /* 1 onto 1, one place left */
    CHECK(!srt_can(&g, 2, 0));                  /* into a full tube */
    srt_game h = g;
    CHECK(srt_move_do(&h, 1, 2) == 1);          /* tube c full: 1 2 1 1 */
    CHECK(h.p.h[2] == 4 && srt_top(&h.p, 2) == 1 && h.p.h[1] == 1);
    CHECK(srt_move_do(&h, 0, 1) == (one ? 1 : 3));   /* the 2s onto the 2: the room is three */
    CHECK(srt_undo(&h) == 1 && srt_undo(&h) == 1 && sc_same_pos(&h.p, &g.p, 4) && h.moves == 0);
    CHECK(!srt_undo(&h));
    /* a pour stopped by the room left: three 2s, room for one */
    memset(&p, 0, sizeof p);
    static const uint8_t d[4] = {1, 2, 2, 2}, e[4] = {1, 1, 2, 0};
    memcpy(p.c[0], d, 4);
    memcpy(p.c[1], e, 4);
    p.h[0] = 4, p.h[1] = 3;
    srt_load(&g, 2, 2, one, &p);
    CHECK(srt_move_do(&g, 0, 1) == 1 && g.p.h[0] == 3 && srt_run(&g.p, 0) == 2 && srt_complete(&g.p, 1) == 0);
    /* solved: each colour in one full tube; a colour split in two tubes is not */
    memset(&p, 0, sizeof p);
    static const uint8_t f1[4] = {1, 1, 1, 1}, f2[4] = {2, 2, 0, 0};
    memcpy(p.c[0], f1, 4);
    memcpy(p.c[1], f2, 4);
    memcpy(p.c[2], f2, 4);
    p.h[0] = 4, p.h[1] = p.h[2] = 2;
    srt_load(&g, 2, 4, one, &p);
    CHECK(!srt_solved(&g));
    CHECK(srt_move_do(&g, 1, 2) == (one ? 1 : 2));
    if (one) CHECK(srt_move_do(&g, 1, 2) == 1);
    CHECK(srt_solved(&g));
}

/* the notation */
static void sc_script(sc_new_fn make, int one)
{
    srt_game g;
    int len = make(&g, 0, 5, NULL);
    srt_game h = g;
    CHECK(srt_script(&h, "=3") && h.moves == 3);
    char buf[16];
    snprintf(buf, sizeof buf, "=%d", len - 3);
    CHECK(srt_script(&h, buf) && srt_solved(&h) && h.moves == len);
    h = g;
    CHECK(srt_script(&h, "=") && srt_solved(&h) && h.moves == len);
    h = g;
    CHECK(srt_script(&h, "+") && h.tubes == g.tubes + 1 && h.moves == 0);
    CHECK(!srt_script(&h, "+"));
    h = g;
    CHECK(!srt_script(&h, "ea"));               /* from an empty tube */
    CHECK(!srt_script(&h, "az"));
    h = g;
    CHECK(srt_script(&h, "ae") && h.p.h[4] == (one ? 1 : srt_run(&g.p, 0)));
}

#endif
