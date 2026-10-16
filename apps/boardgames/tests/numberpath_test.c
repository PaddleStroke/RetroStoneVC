/*
 * Number Path's rules: the two ways of touching, the paths drawn by chance, the reasoning and the search on puzzles
 * drawn by hand, every level (one solution: by the reasoning, by the search, and on the smaller grids by a plain search
 * of every path written here), the random puzzles (one solution, the time on the biggest), the player's runs, erasing,
 * undo, the mistakes and the hints, the notation, and random games played from seeds.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "games/numberpath.h"
#include "games/puzrand.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

/* 1: sol holds 1..n once each, every number touching the next */
static int a_path(const nph_puzzle *p, const uint8_t *sol)
{
    int n = nph_cells(p), at[NPH_CELLS + 1];
    for (int v = 0; v <= n; v++) at[v] = -1;
    for (int c = 0; c < n; c++) {
        if (sol[c] < 1 || sol[c] > n || at[sol[c]] >= 0) return 0;
        at[sol[c]] = c;
    }
    for (int v = 1; v < n; v++)
        if (!nph_touch(p, at[v], at[v + 1])) return 0;
    return 1;
}

/* ---- a plain search of every path, written apart from the rules: from 1, number after number, each given number
 * where it is; a cell is free when it is not given; pruned only by the next given number's distance and by the cells
 * left being joined to the path's end -------------------------------------------------------------------------- */
static const nph_puzzle *P;
static int N, at_given[NPH_CELLS + 2], next_given[NPH_CELLS + 2], found;
static uint8_t used[NPH_CELLS];
static long steps, step_cap;

static int far_from(int a, int b)
{
    int dx = abs(a % P->w - b % P->w), dy = abs(a / P->w - b / P->w);
    return P->way == NPH_SIDE ? dx + dy : dx > dy ? dx : dy;
}

static int joined(int head)
{
    int q[NPH_CELLS], h = 0, t = 0, left = 0;
    uint8_t seen[NPH_CELLS] = {0};
    for (int c = 0; c < N; c++) left += !used[c];
    q[t++] = head;
    seen[head] = 1;
    while (h < t) {
        int c = q[h++];
        for (int d = 0; d < N; d++)
            if (!seen[d] && !used[d] && nph_touch(P, c, d)) seen[d] = 1, q[t++] = d, left--;
    }
    return left == 0;
}

static void walk(int v, int c)
{
    if (found > 1 || ++steps > step_cap) return;
    if (v == N) {
        found++;
        return;
    }
    int nv = v + 1, g = at_given[nv], tg = next_given[nv];
    for (int e = 0; e < N; e++) {
        if (used[e] || !nph_touch(P, c, e)) continue;
        if (g >= 0 ? e != g : P->given[e] != 0) continue;
        if (g < 0 && tg <= N && far_from(e, at_given[tg]) > tg - nv) continue;
        used[e] = 1;
        if (joined(e)) walk(nv, e);
        used[e] = 0;
    }
}

/* the solutions by the plain search, up to 2; -1: past the cap */
static int plain(const nph_puzzle *p, long cap)
{
    P = p;
    N = nph_cells(p);
    for (int v = 0; v <= N + 1; v++) at_given[v] = -1;
    for (int c = 0; c < N; c++)
        if (p->given[c]) at_given[p->given[c]] = c;
    next_given[N + 1] = N + 1;
    for (int v = N; v >= 1; v--) next_given[v] = at_given[v] >= 0 ? v : next_given[v + 1];
    memset(used, 0, sizeof used);
    found = 0;
    steps = 0;
    step_cap = cap;
    used[at_given[1]] = 1;
    walk(1, at_given[1]);
    return steps > cap ? -1 : found;
}

static void puzzle_of(nph_puzzle *p, int w, int h, int way, const char *const *rows)
{
    memset(p, 0, sizeof *p);
    p->w = w, p->h = h, p->way = way;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {           /* three characters a cell */
            char f[4] = {rows[y][3 * x], rows[y][3 * x + 1], rows[y][3 * x + 2], 0};
            p->given[y * w + x] = (uint8_t)atoi(f);
        }
}

int main(void)
{
    nph_puzzle p;
    uint8_t sol[NPH_CELLS], val[NPH_CELLS];

    /* touching: all around, or side by side */
    p = (nph_puzzle){.w = 4, .h = 4, .way = NPH_ALL};
    CHECK(nph_touch(&p, 5, 0) && nph_touch(&p, 5, 10) && nph_touch(&p, 5, 6) && !nph_touch(&p, 5, 7));
    CHECK(!nph_touch(&p, 3, 4) && !nph_touch(&p, 5, 5));   /* a row's end and the next row's start; itself */
    p.way = NPH_SIDE;
    CHECK(!nph_touch(&p, 5, 0) && !nph_touch(&p, 5, 10) && nph_touch(&p, 5, 6) && nph_touch(&p, 5, 9));

    /* by hand: a 3 x 3 grid side by side, 1 and 9 in opposite corners and 3 in the top row's end: one way (the snake
     * along the rows); without the 3: two (along the rows or the columns) */
    static const char *const snake[] = {" 1     3 ", "         ", "        9"};
    puzzle_of(&p, 3, 3, NPH_SIDE, snake);
    memcpy(val, p.given, sizeof val);
    CHECK(nph_reason(&p, val, NULL, NULL) == 1 && val[3] == 6 && val[4] == 5);
    CHECK(nph_count(&p, 3, sol, 0, NULL) == 1 && plain(&p, 1000000) == 1 && !memcmp(sol, val, 9));
    p.given[2] = 0;
    CHECK(nph_count(&p, 3, NULL, 0, NULL) == 2 && plain(&p, 1000000) == 2);
    memcpy(val, p.given, sizeof val);
    CHECK(nph_reason(&p, val, NULL, NULL) == 0);   /* stuck: two ways */
    /* a contradiction: 2 given away from 1 */
    static const char *const bad[] = {" 1     2 ", "         ", "        9"};
    puzzle_of(&p, 3, 3, NPH_ALL, bad);
    memcpy(val, p.given, sizeof val);
    CHECK(nph_reason(&p, val, NULL, NULL) == -1 && nph_count(&p, 2, NULL, 0, NULL) == 0 && plain(&p, 100000) == 0);

    /* the paths drawn by chance: every cell once, each number touching the next */
    for (uint32_t s = 1; s <= 200; s++) {
        int w = 2 + (int)(s % 8), h = 2 + (int)(s / 8 % 8), way = (int)(s % 2);
        p = (nph_puzzle){.w = w, .h = h, .way = way};
        nph_path(w, h, way, s, sol);
        CHECK(a_path(&p, sol));
    }

    /* every level: decoded, 1 and the last given, one solution (the reasoning, the search, the plain search) */
    nph_game G;
    int plain_checked = 0, sizes[2] = {0, 0};
    clock_t t0 = clock();
    for (int l = 0; l < NPH_LEVELS; l++) {
        CHECK(nph_levels[l] && nph_decode(nph_levels[l], &p, sol));
        int n = nph_cells(&p);
        CHECK(a_path(&p, sol) && p.way == (l % 2 ? NPH_SIDE : NPH_ALL));   /* the two ways in turn */
        CHECK(p.w <= p.h);                      /* upright: a phone held so */
        memcpy(val, p.given, sizeof val);
        CHECK(nph_reason(&p, val, NULL, NULL) == 1 && !memcmp(val, sol, (size_t)n));
        uint8_t found_sol[NPH_CELLS];
        CHECK(nph_count(&p, 2, found_sol, 0, NULL) == 1 && !memcmp(found_sol, sol, (size_t)n));
        if (n <= 36) {
            CHECK(plain(&p, 4000000) == 1);
            plain_checked++;
        }
        char s[3 * NPH_CELLS];
        CHECK(nph_encode(&p, sol, s, sizeof s) && !strcmp(s, nph_levels[l]));
        for (int m = 0; m < l; m++) CHECK(strcmp(nph_levels[m], nph_levels[l]));
        CHECK(nph_level(&G, l) && !nph_solved(&G) && G.hist.moves == 0);
        int ch = l / (NPH_LEVELS / NPH_CHAPTERS);
        if (ch == 0) CHECK(n == 25);
        if (ch == 5) CHECK(n == 81);
        sizes[p.way] += n;
    }
    printf("numberpath: %d levels in %.2f s (%d also by the plain search), cells %d all around, %d side by side\n",
           NPH_LEVELS, (double)(clock() - t0) / CLOCKS_PER_SEC, plain_checked, sizes[0], sizes[1]);
    CHECK(!nph_level(&G, -1) && !nph_level(&G, NPH_LEVELS));

    /* the random puzzles: one solution, quick on the biggest */
    double slowest[NPH_VARIANTS] = {0};
    int givens[NPH_VARIANTS] = {0};
    for (int v = 0; v < NPH_VARIANTS; v++)
        for (uint32_t s = 1; s <= 30; s++) {
            clock_t t1 = clock();
            nph_new(&G, v, s * 7919u + (uint32_t)v);
            double t = (double)(clock() - t1) / CLOCKS_PER_SEC;
            if (t > slowest[v]) slowest[v] = t;
            int w, h, way, n = nph_cells(&G.p);
            nph_variant(v, &w, &h, &way);
            CHECK(G.p.w == w && G.p.h == h && G.p.way == way && a_path(&G.p, G.sol));
            memcpy(val, G.p.given, sizeof val);
            CHECK(nph_reason(&G.p, val, NULL, NULL) == 1 && !memcmp(val, G.sol, (size_t)n));
            for (int c = 0; c < n; c++) {
                givens[v] += G.p.given[c] != 0;
                CHECK(!G.p.given[c] || G.p.given[c] == G.sol[c]);
                if (G.sol[c] == 1 || G.sol[c] == n) CHECK(G.p.given[c]);
            }
        }
    printf("numberpath: random puzzles, the slowest");
    for (int v = 0; v < NPH_VARIANTS; v++) printf(" %.3f", slowest[v]);
    printf(" s; givens");
    for (int v = 0; v < NPH_VARIANTS; v++) printf(" %.1f", givens[v] / 30.0);
    printf("\n");
    for (int v = 0; v < NPH_VARIANTS; v++) CHECK(slowest[v] < 0.25);
    nph_game A, B;
    nph_new(&A, 4, 99);
    nph_new(&B, 4, 99);
    CHECK(!memcmp(&A.p, &B.p, sizeof A.p));

    /* the player: a run from a number, the run's end found, erasing, undo, a mistake */
    nph_level(&G, 0);
    int n = nph_cells(&G.p), one = nph_where(&G, 1), dir = 0;
    CHECK(one >= 0 && nph_given(&G, one) && nph_head(&G, one, &dir) >= 0);
    int head = nph_head(&G, one, &dir);
    CHECK(dir == 1);
    int nv = G.val[head] + 1, right = -1, wrong = -1;
    for (int c = 0; c < n; c++) {
        if (G.sol[c] == nv) right = c;
        else if (!G.val[c] && nph_touch(&G.p, head, c) && wrong < 0) wrong = c;
    }
    CHECK(right >= 0 && nph_place(&G, one, &right, 1) && G.val[right] == nv && G.hist.moves == 1);
    CHECK(nph_undo(&G) == right && !G.val[right] && G.hist.mistakes == 0);
    if (wrong >= 0) {                           /* a wrong number taken back: a mistake */
        CHECK(nph_place(&G, one, &wrong, 1) && nph_wrongs(&G) == 1);
        CHECK(nph_undo(&G) == wrong && G.hist.mistakes == 1 && nph_wrongs(&G) == 0);
        CHECK(nph_place(&G, one, &wrong, 1) && nph_erase(&G, &wrong, 1) && G.hist.mistakes == 1);   /* erased: not */
    }
    int far = -1;                               /* not touching the run's end: refused */
    for (int c = 0; c < n && far < 0; c++)
        if (!G.val[c] && !nph_touch(&G.p, nph_head(&G, one, &dir), c)) far = c;
    int moves = G.hist.moves;
    CHECK(far < 0 || (!nph_place(&G, one, &far, 1) && G.hist.moves == moves));
    CHECK(!nph_erase(&G, &one, 1) && G.val[one] == 1);   /* a given stays */
    /* the hint: a wrong number out first, then the reasoning's next; hints alone solve it */
    nph_level(&G, 30);
    n = nph_cells(&G.p);
    one = nph_where(&G, 1);
    head = nph_head(&G, one, &dir);
    wrong = -1;
    for (int c = 0; c < n && wrong < 0; c++)
        if (!G.val[c] && nph_touch(&G.p, head, c) && G.sol[c] != G.val[head] + dir) wrong = c;
    int put;
    if (wrong >= 0) {
        CHECK(nph_place(&G, one, &wrong, 1) && nph_clash(&G, wrong) == nph_clash(&G, wrong));
        CHECK(nph_hint(&G, &put) == wrong && !put && !G.val[wrong] && G.hist.hints == 1);
    }
    int guard = 0;
    while (!nph_solved(&G) && guard++ < 200) {
        int c = nph_hint(&G, &put);
        CHECK(c >= 0 && put && G.val[c] == G.sol[c]);
        if (c < 0) break;
    }
    CHECK(nph_solved(&G) && nph_wrongs(&G) == 0 && nph_hint(&G, &put) < 0);
    for (int c = 0; c < n; c++) CHECK(!nph_clash(&G, c));
    /* undo all the way back */
    while (nph_undo(&G) >= 0) {}
    CHECK(G.hist.moves == 0);
    for (int c = 0; c < n; c++) CHECK(G.val[c] == G.p.given[c]);

    /* a run going down: from a number whose next is there, the one before goes next */
    nph_level(&G, 160);
    n = nph_cells(&G.p);
    for (int v = 2; v < n; v++) {
        int c = nph_where(&G, v), up = nph_where(&G, v + 1);
        if (c < 0 || up < 0 || nph_where(&G, v - 1) >= 0 || !nph_touch(&G.p, c, up)) continue;
        int h2 = nph_head(&G, c, &dir);
        if (h2 == c) {
            CHECK(dir == -1);
            int below = -1;
            for (int x = 0; x < n; x++)
                if (G.sol[x] == v - 1) below = x;
            CHECK(nph_place(&G, c, &below, 1) && G.val[below] == v - 1);
        }
        break;
    }

    /* the notation: a run, an erase, a hint, the solution's next numbers */
    nph_level(&G, 0);
    n = nph_cells(&G.p);
    one = nph_where(&G, 1);
    char moves_s[64];
    int c2 = -1;
    for (int c = 0; c < n; c++)
        if (G.sol[c] == 2) c2 = c;
    if (!G.val[c2]) {
        snprintf(moves_s, sizeof moves_s, "%c%d-%c%d x%c%d h", 'a' + one % G.p.w, one / G.p.w + 1, 'a' + c2 % G.p.w,
                 c2 / G.p.w + 1, 'a' + c2 % G.p.w, c2 / G.p.w + 1);
        CHECK(nph_script(&G, moves_s) && G.hist.moves == 3 && G.hist.hints == 1);
    }
    CHECK(nph_script(&G, "+99") && nph_solved(&G));
    CHECK(!nph_script(&G, "z9"));

    /* random games from seeds: runs, erases, undos and hints by chance; the givens never move, no number twice */
    for (uint32_t s = 1; s <= 150; s++) {
        uint32_t r = pr_seed(s);
        if (s % 3) nph_level(&G, (int)(s * 7 % NPH_LEVELS));
        else nph_new(&G, (int)(s % NPH_VARIANTS), s);
        n = nph_cells(&G.p);
        for (int step = 0; step < 120 && !nph_solved(&G); step++) {
            int kind = pr_below(&r, 10), c = pr_below(&r, n);
            if (kind == 0) nph_undo(&G);
            else if (kind == 1) nph_hint(&G, &put);
            else if (kind == 2) nph_erase(&G, &c, 1);
            else if (G.val[c]) {
                int h3 = nph_head(&G, c, &dir), to[3], k = 0;
                for (int i = 0; i < 3 && h3 >= 0; i++) {
                    int e = pr_below(&r, n);
                    if (!G.val[e] && nph_touch(&G.p, k ? to[k - 1] : h3, e)) to[k++] = e;
                }
                if (k) nph_place(&G, c, to, k);
            }
            int seen[NPH_CELLS + 1] = {0}, ok = 1;
            for (int x = 0; x < n; x++) {
                if (G.p.given[x] && G.val[x] != G.p.given[x]) ok = 0;
                if (G.val[x] && seen[G.val[x]]++) ok = 0;
            }
            CHECK(ok);
            if (!ok) break;
        }
        while (!nph_solved(&G))
            if (nph_hint(&G, &put) < 0) break;
        CHECK(nph_solved(&G));
    }

    if (fails) printf("numberpath: %d FAILED\n", fails);
    else printf("numberpath: all passed\n");
    return fails != 0;
}
