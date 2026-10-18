/*
 * Twin Walkers' rules: the mirror move, a wall stopping one walker alone, the walkers meeting (the same tile, each
 * other's, one standing in the way), a crumbling tile, a plate and its door, ice, a move that moves neither; maps as
 * text both ways; every level read back, its par the search's (the levels with no crumbling tile also by a plain
 * search written here) and its shortest way played to the exits in par moves, each chapter's special tiles there; the
 * puzzles made at random (their par in the variant's range most of the time, the time they take); whole games from a
 * seed (random moves, then hints to the end, then undo back to the start).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "games/puzrand.h"
#include "games/twinwalk.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

/* the test's own search for maps with no crumbling tile: the two places only, a plain table */
static int own_par(const twk_map *m)
{
    int cells = m->w * m->h;
    static int dist[TWK_CELLS * TWK_CELLS], q[TWK_CELLS * TWK_CELLS];
    for (int i = 0; i < cells * cells; i++) dist[i] = -1;
    int head = 0, tail = 0, s0 = m->start[0] * cells + m->start[1];
    dist[s0] = 0;
    q[tail++] = s0;
    while (head < tail) {
        int k = q[head++];
        twk_state s = {{(uint8_t)(k / cells), (uint8_t)(k % cells)}, 0}, t;
        if (twk_solved(m, &s)) return dist[k];
        for (int d = 0; d < 4; d++) {
            if (!twk_try(m, &s, d, &t, NULL)) continue;
            int n = t.p[0] * cells + t.p[1];
            if (dist[n] < 0) { dist[n] = dist[k] + 1; q[tail++] = n; }
        }
    }
    return -1;
}

static int has(const char *s, const char *any) { return strpbrk(s, any) != NULL; }

static twk_state at(int a, int b) { twk_state s = {{(uint8_t)a, (uint8_t)b}, 0}; return s; }

int main(void)
{
    twk_map m;
    twk_state s, t;
    twk_trace tr;
    char text[TWK_CELLS + TWK_MAXH + 1];

    /* the mirror: the first walker right, the second left; up and down alike */
    CHECK(twk_parse(&m, ".....|.A.B.|.....|a...b"));
    CHECK(m.w == 5 && m.h == 4 && m.start[0] == 6 && m.start[1] == 8 && m.exit[0] == 15 && m.exit[1] == 19);
    CHECK(twk_mirror_way(TWK_RIGHT) == TWK_LEFT && twk_mirror_way(TWK_UP) == TWK_UP);
    s = at(6, 8);
    CHECK(twk_try(&m, &s, TWK_LEFT, &t, &tr) && t.p[0] == 5 && t.p[1] == 9 && tr.n[0] == 1 && tr.n[1] == 1);
    CHECK(twk_try(&m, &s, TWK_UP, &t, NULL) && t.p[0] == 1 && t.p[1] == 3);
    /* meeting: both for the middle tile: both stay */
    CHECK(!twk_try(&m, &s, TWK_RIGHT, &t, &tr) && tr.bump[0] && tr.bump[1]);
    /* side by side, each for the other's tile: both stay */
    s = at(7, 8);
    CHECK(!twk_try(&m, &s, TWK_RIGHT, &t, NULL));
    /* one in the way of the other that stays (against the edge): it stays too; the other way, both go */
    s = at(3, 4);
    CHECK(!twk_try(&m, &s, TWK_RIGHT, &t, NULL));
    s = at(1, 4);                               /* apart: both walk */
    CHECK(twk_try(&m, &s, TWK_RIGHT, &t, NULL) && t.p[0] == 2 && t.p[1] == 3);
    /* following: the first walks into the tile the second leaves */
    s = at(3, 4);
    CHECK(twk_try(&m, &s, TWK_LEFT, &t, NULL) && t.p[0] == 2 && t.p[1] == 4);   /* the second against the edge */
    s = at(6, 1);                               /* the second just above the first, both going down */
    CHECK(twk_try(&m, &s, TWK_DOWN, &t, NULL) && t.p[0] == 11 && t.p[1] == 6);
    CHECK(!twk_try(&m, &s, TWK_UP, &t, NULL)); /* up: the second against the edge, the first under it */
    /* a wall stops one alone */
    CHECK(twk_parse(&m, "A#.B.|.....|a...b"));
    s = at(0, 3);
    CHECK(twk_try(&m, &s, TWK_RIGHT, &t, &tr) && t.p[0] == 0 && t.p[1] == 2 && tr.bump[0] && !tr.bump[1]);
    /* the text both ways */
    twk_text(&m, text);
    CHECK(!strcmp(text, "A#.B.|.....|a...b"));

    /* a crumbling tile: stepped off, gone */
    CHECK(twk_parse(&m, "A%..|....|...B|a..b"));
    CHECK(m.ncr == 1);
    s = at(0, 11);
    CHECK(twk_try(&m, &s, TWK_RIGHT, &t, NULL) && t.p[0] == 1 && !t.gone);
    s = t;
    CHECK(twk_try(&m, &s, TWK_RIGHT, &t, NULL) && t.p[0] == 2 && t.gone == 1);
    s = t;
    CHECK(twk_try(&m, &s, TWK_LEFT, &t, NULL) && t.p[0] == 2);   /* the gap: it stays, the second walks */
    CHECK(!twk_passable(&m, &s, 1));

    /* a plate and its door: shut, then open while a walker stands on the plate */
    CHECK(twk_parse(&m, "AP.|...|p.B|a.b"));
    s = at(0, 8);
    CHECK(!twk_door_open(&m, &s, 1));
    CHECK(twk_try(&m, &s, TWK_RIGHT, &t, &tr) && t.p[0] == 0 && t.p[1] == 7);   /* shut: the first stays */
    s = at(0, 6);                               /* the second on the plate */
    CHECK(twk_door_open(&m, &s, 1));
    CHECK(twk_try(&m, &s, TWK_RIGHT, &t, NULL) && t.p[0] == 1 && t.p[1] == 6);   /* the second against the edge */
    CHECK(twk_door_open(&m, &t, 1));            /* in the door: open */

    /* ice: a slide until the ice ends or something stops it */
    CHECK(twk_parse(&m, "A~~~.#|......|B.....|a....b"));
    s = at(0, 12);
    CHECK(twk_try(&m, &s, TWK_RIGHT, &t, &tr) && t.p[0] == 4 && tr.n[0] == 4 && tr.n[1] == 0);
    CHECK(tr.at[0][0] == 1 && tr.at[0][3] == 4);
    CHECK(twk_parse(&m, "A~~#..|......|B.....|a....b"));
    s = at(0, 12);
    CHECK(twk_try(&m, &s, TWK_RIGHT, &t, &tr) && t.p[0] == 2);   /* the wall at the ice's end */

    /* every level */
    twk_game g;
    int per = TWK_LEVELS / TWK_CHAPTERS, plain = 0;
    for (int lv = 0; lv < TWK_LEVELS; lv++) {
        CHECK(twk_level(&g, lv));
        twk_text(&g.m, text);
        CHECK(!strcmp(text, twk_levels[lv]));
        CHECK(!twk_solved(&g.m, &g.s) && g.par > 0 && g.par == twk_level_par[lv]);
        int d, n = twk_solve(&g.m, &g.s, &d, NULL);
        CHECK(n == g.par);
        if (!has(twk_levels[lv], "%")) {
            CHECK(own_par(&g.m) == g.par);
            plain++;
        }
        CHECK(twk_script(&g, "=") && twk_solved(&g.m, &g.s) && g.moves == g.par);
        CHECK(g.m.w <= 8 && g.m.h <= 10);
        int ch = lv / per;
        if (ch < 2) CHECK(!has(twk_levels[lv], "%pqPQ~"));
        if (ch == 2) CHECK(has(twk_levels[lv], "%"));
        if (ch == 3) CHECK(has(twk_levels[lv], "pq") && has(twk_levels[lv], "PQ"));
        if (ch == 4) CHECK(has(twk_levels[lv], "~"));
        if (lv) CHECK(strcmp(twk_levels[lv], twk_levels[lv - 1]));
    }
    CHECK(plain >= 2 * per);
    for (int i = 0; i < TWK_LEVELS; i++)        /* no two alike */
        for (int j = i + 1; j < TWK_LEVELS; j++) CHECK(strcmp(twk_levels[i], twk_levels[j]));
    CHECK(twk_level_par[0] <= 3);               /* a gentle first level */

    /* the puzzles made at random: solvable, the par in range most of the time, quick */
    for (int v = 0; v < TWK_VARIANTS; v++) {
        twk_spec sp;
        twk_variant(v, &sp);
        int in = 0, runs = 40;
        double worst = 0;
        for (int k = 0; k < runs; k++) {
            clock_t c0 = clock();
            twk_random(&g, v, 1000u + (uint32_t)k * 77u);
            double sec = (double)(clock() - c0) / CLOCKS_PER_SEC;
            if (sec > worst) worst = sec;
            CHECK(g.par > 0 && g.par <= sp.hi);
            if (g.par >= sp.lo) in++;
            twk_state s0 = g.s;
            CHECK(twk_solve(&g.m, &s0, NULL, NULL) == g.par);
            CHECK(twk_script(&g, "=") && g.moves == g.par);
        }
        printf("twinwalk variant %d: %d/%d in range, slowest %.0f ms\n", v, in, runs, worst * 1000);
        CHECK(in >= runs * 3 / 4);
        CHECK(worst < 0.25);
    }

    /* whole games from a seed: random moves, hints to the end, undo back to the start */
    for (int k = 0; k < 30; k++) {
        uint32_t rs = pr_seed(500u + (uint32_t)k);
        twk_level(&g, (int)(pr_next(&rs) % TWK_LEVELS));
        twk_state s0 = g.s;
        for (int i = 0; i < 25; i++) twk_move(&g, pr_below(&rs, 4));
        int made = g.moves;
        while (!twk_solved(&g.m, &g.s)) {
            int d = twk_hint(&g);
            if (d < 0) { CHECK(twk_undo(&g)); continue; }   /* a dead end (a tile fallen): back first */
            CHECK(twk_move(&g, d));
        }
        CHECK(g.moves >= 0 && made >= 0);
        CHECK(!twk_move(&g, TWK_UP));          /* solved: no more moves */
        while (twk_undo(&g)) {}
        CHECK(g.moves == 0 && g.s.p[0] == s0.p[0] && g.s.p[1] == s0.p[1] && g.s.gone == 0);
    }

    if (fails) printf("twinwalk: %d failures\n", fails);
    else printf("twinwalk: all good\n");
    return fails != 0;
}
