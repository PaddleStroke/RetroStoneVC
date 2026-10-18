/*
 * Domino Chain's rules: dominoes fall along their axis and stay when hit from the side, turns, long dominoes reaching
 * over a square (or stopped by a piece standing in it), splitters, the bell, a piece hit twice in one step; pieces put,
 * turned and taken back with the stock, undo; boards as text both ways; every level: its par the search's, a way at
 * par checked by a push written here (its own steps), no way with fewer pieces (every placing tried, for the smaller
 * boards), the chapters' pieces, no two levels alike; the puzzles made at random (their par, the same from the same
 * seed, the time they take); the hints from random positions lead to the end.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "games/dominochain.h"
#include "games/puzrand.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

/* ---- the test's own push: each step a list of (square, way, reaching over) knocks ----------------------------- */
typedef struct { int sq, way, over; } knock;

static int own_next(const dmc_map *m, int c, int d)
{
    static const int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};
    int x = c % m->w + dx[d], y = c / m->w + dy[d];
    return x < 0 || y < 0 || x >= m->w || y >= m->h ? -1 : x + y * m->w;
}

static int own_push(const dmc_map *m, const dmc_state *s)
{
    int type[DMC_CELLS] = {0}, ori[DMC_CELLS] = {0}, down[DMC_CELLS] = {0};
    for (int c = 0; c < m->w * m->h; c++) {
        type[c] = s->t[c] != DMC_WOOD ? s->t[c] : m->t[c];
        ori[c] = s->t[c] != DMC_WOOD ? s->o[c] : m->o[c];
    }
    knock now[256], later[256];
    int n = 0, rang = 0;
    down[m->start] = 1;
    int a = own_next(m, m->start, ori[m->start]);
    if (a >= 0) now[n++] = (knock){a, ori[m->start], 0};
    while (n) {
        int nl = 0;
        for (int i = 0; i < n; i++) {
            int c = now[i].sq, d = now[i].way;
            if (type[c] < DMC_DOMINO || down[c]) {      /* nothing standing: a long domino reaches on */
                if (now[i].over) {
                    int b = own_next(m, c, d);
                    if (b >= 0) { now[i] = (knock){b, d, 0}; i--; }
                }
                continue;
            }
            int t = type[c], o = ori[c], outs[2], no = 0;
            if ((t == DMC_DOMINO || t == DMC_LONG) && (d % 2) == o) outs[no++] = d;
            if (t == DMC_TURN) {
                int from = (d + 2) % 4;                 /* the side it comes in by */
                if (from == o) outs[no++] = (o + 1) % 4;
                if (from == (o + 1) % 4) outs[no++] = o;
            }
            if (t == DMC_SPLIT) { outs[no++] = (d + 1) % 4; outs[no++] = (d + 3) % 4; }
            if (t == DMC_BELL) { rang = 1; down[c] = 1; continue; }
            if (!no) continue;
            down[c] = 1;
            for (int k = 0; k < no; k++) {
                int b = own_next(m, c, outs[k]);
                if (b >= 0 && nl < 256) later[nl++] = (knock){b, outs[k], t == DMC_LONG};
            }
        }
        memcpy(now, later, sizeof(knock) * (size_t)nl);
        n = nl;
    }
    for (int c = 0; c < m->w * m->h; c++)
        if (type[c] >= DMC_DOMINO && !down[c]) return 0;
    return rang;
}

/* every placing of k pieces from the stock on the free wood (squares from `from` on): one works */
static long tried;
static int own_any(const dmc_map *m, dmc_state *s, int k, int from)
{
    if (!k) { tried++; return own_push(m, s); }
    for (int c = from; c < m->w * m->h; c++) {
        if (m->t[c] != DMC_WOOD || s->t[c] != DMC_WOOD) continue;
        static const int T[DMC_KINDS] = {DMC_DOMINO, DMC_TURN, DMC_LONG, DMC_SPLIT}, NO[DMC_KINDS] = {2, 4, 2, 1};
        for (int kind = 0; kind < DMC_KINDS; kind++) {
            if (!s->left[kind]) continue;
            for (int o = 0; o < NO[kind]; o++) {
                s->t[c] = (uint8_t)T[kind];
                s->o[c] = (uint8_t)o;
                s->left[kind]--;
                int ok = own_any(m, s, k - 1, c + 1);
                s->left[kind]++;
                s->t[c] = DMC_WOOD;
                s->o[c] = 0;
                if (ok) return 1;
            }
        }
    }
    return 0;
}

static double binom(int n, int k)
{
    double r = 1;
    for (int i = 0; i < k; i++) r = r * (n - i) / (i + 1);
    return r;
}

static int has(const char *s, const char *any) { return strpbrk(s, any) != NULL; }

int main(void)
{
    static dmc_game g, t;
    dmc_trace tr;
    char text[DMC_TEXT];

    /* a straight chain: the start, a domino, a gap, a domino, the bell */
    CHECK(dmc_parse(&g.m, "Rh.hB", "1000") && g.m.w == 5 && g.m.h == 1 && g.m.start == 0 && g.m.bell == 4);
    dmc_begin(&g);
    CHECK(!dmc_run(&g.m, &g.s, &tr) && tr.fallen[1] && !tr.fallen[3] && tr.bell < 0);
    CHECK(!dmc_put(&g, 1, 0, 1) && !dmc_put(&g, 2, DMC_K_TURN, 0));   /* a given piece; none in the stock */
    CHECK(dmc_put(&g, 2, DMC_K_DOMINO, 0) && g.s.left[0] == 0 && g.s.placed == 1);
    CHECK(!dmc_run(&g.m, &g.s, &tr) && tr.nwob == 1 && tr.wob[0].c == 2);   /* across the push: it stays */
    CHECK(dmc_turn_piece(&g, 2, 1) && dmc_run(&g.m, &g.s, &tr) && tr.bell == 4 && tr.n == 5);
    CHECK(dmc_undo(&g) && g.s.o[2] == 0 && dmc_take(&g, 2) && g.s.left[0] == 1 && g.s.placed == 0);
    CHECK(dmc_script(&g, "dca1 p") && g.done && g.s.placed == 1 && !dmc_put(&g, 2, 0, 1));
    CHECK(dmc_undo(&g) && !g.done);
    dmc_text(&g.m, NULL, text);
    CHECK(!strcmp(text, "Rh.hB"));
    dmc_text(&g.m, &g.s, text);
    CHECK(!strcmp(text, "Rh.hB"));
    /* a turn: right then down; the wrong corner stays */
    CHECK(dmc_parse(&g.m, "R3|#v|#B", "0000"));
    dmc_begin(&g);
    CHECK(dmc_run(&g.m, &g.s, NULL));
    CHECK(dmc_parse(&g.m, "R4|#v|#B", "0000") && !dmc_run(&g.m, &g.s, NULL));
    /* a long domino reaches over the felt; a domino standing in the square it reaches over is hit instead */
    CHECK(dmc_parse(&g.m, "RH#hB", "0000"));
    dmc_begin(&g);
    CHECK(dmc_run(&g.m, &g.s, NULL));
    CHECK(dmc_parse(&g.m, "RH.hB", "1000"));
    dmc_begin(&g);
    CHECK(dmc_run(&g.m, &g.s, NULL) && dmc_put(&g, 2, 0, 0) && !dmc_run(&g.m, &g.s, &tr) && !tr.fallen[3]);
    /* a splitter: both ways across; a piece left standing fails the push */
    CHECK(dmc_parse(&g.m, "B#v|v#v|1+4|#v#|#U#", "0000"));
    dmc_begin(&g);
    CHECK(dmc_run(&g.m, &g.s, NULL));
    CHECK(dmc_parse(&g.m, "B#v|v#v|1+3|#v#|#U#", "0000") && !dmc_run(&g.m, &g.s, NULL));
    CHECK(!dmc_parse(&g.m, "Rh.h", "1000") && !dmc_parse(&g.m, "Rh.hB", "100") && !dmc_parse(&g.m, "RRhB", "0000") &&
          !dmc_parse(&g.m, "Rh|hB.", "0000") && !dmc_parse(&g.m, "Rx.hB", "0000"));

    /* every level: read back, its par the search's, a way at par the test's own push accepts, none with fewer */
    int per = DMC_LEVELS / DMC_CHAPTERS, brute = 0;
    double slow = 0;
    for (int lv = 0; lv < DMC_LEVELS; lv++) {
        const dmc_level_t *L = &dmc_levels[lv];
        CHECK(dmc_level(&g, lv));
        dmc_text(&g.m, NULL, text);
        CHECK(!strcmp(text, L->board));
        CHECK(g.par == L->par && g.par > 0 && !dmc_run(&g.m, &g.s, NULL));
        dmc_state sol;
        clock_t t0 = clock();
        int r = dmc_solve(&g.m, &g.s, 12, 50000000L, &sol, NULL, NULL);
        double ms = (clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
        if (ms > slow) slow = ms;
        CHECK(r == g.par);
        if (r != g.par) printf("dominochain: level %d, the search %d, par %d\n", lv + 1, r, g.par);
        if (r >= 0) CHECK(own_push(&g.m, &sol) && sol.placed == g.par && dmc_run(&g.m, &sol, NULL));
        t = g;
        CHECK(dmc_script(&t, "=") && t.done && t.s.placed == g.par);
        int wood = 0;
        for (int c = 0; c < g.m.w * g.m.h; c++) wood += g.m.t[c] == DMC_WOOD;
        double opts = binom(wood, g.par - 1);
        for (int k = 0; k < g.par - 1; k++) opts *= 9;
        if (opts < 3e6) {                       /* every placing with one piece fewer: none works */
            dmc_state e = g.s;
            int none = 1;
            for (int k = 0; k < g.par && none; k++) none = !own_any(&g.m, &e, k, 0);
            CHECK(none);
            brute++;
        }
        int ch = lv / per;
        if (ch == 0) CHECK(L->stock[1] == '0' && L->stock[2] == '0' && L->stock[3] == '0' && !has(L->board, "HV+"));
        if (ch == 1) CHECK(L->stock[1] != '0' && !has(L->board, "HV+") && L->stock[2] == '0' && L->stock[3] == '0');
        if (ch == 2) CHECK((has(L->board, "HV") || L->stock[2] != '0') && L->stock[3] == '0' && !has(L->board, "+"));
        if (ch == 3) CHECK((has(L->board, "+") || L->stock[3] != '0') && L->stock[2] == '0' && !has(L->board, "HV"));
        if (lv && lv % per) CHECK(L->par + 1 >= dmc_levels[lv - 1].par);   /* a smooth curve */
        for (int k = 0; k < lv; k++) CHECK(strcmp(dmc_levels[k].board, L->board) != 0);
    }
    CHECK(dmc_levels[0].par == 1);
    CHECK(!dmc_level(&g, DMC_LEVELS) && !dmc_level(&g, -1));
    printf("dominochain: every level at its par, %d of them tried by every placing (%ld pushes), the slowest search "
           "%.0f ms\n", brute, tried, slow);

    /* the puzzles made at random: solvable in their par, the same from the same seed, the time it takes */
    for (int v = 0; v < DMC_VARIANTS; v++) {
        double worst = 0;
        int lo = 99, hi = 0;
        for (uint32_t seed = 1; seed <= 20; seed++) {
            clock_t t0 = clock();
            dmc_random(&g, v, seed);
            double ms = (clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
            if (ms > worst) worst = ms;
            if (g.par < lo) lo = g.par;
            if (g.par > hi) hi = g.par;
            CHECK(g.par >= 1);
            t = g;
            CHECK(dmc_script(&t, "=") && t.done && t.s.placed == g.par && own_push(&t.m, &t.s));
            dmc_random(&t, v, seed);
            CHECK(!memcmp(&t.m, &g.m, sizeof g.m));
        }
        CHECK(worst < 400);
        printf("dominochain: random variant %d, pars %d .. %d, the slowest %.0f ms\n", v, lo, hi, worst);
    }

    /* the hints from random positions: pieces put at random, then hints only, reach the end at the par */
    int ok = 1;
    for (uint32_t seed = 1; seed <= 40; seed++) {
        uint32_t r = pr_seed(seed);
        dmc_level(&g, (int)(seed * 11 % DMC_LEVELS));
        for (int k = 0; k < 3; k++) {
            int c = pr_below(&r, g.m.w * g.m.h), kind = pr_below(&r, DMC_KINDS);
            dmc_put(&g, c, kind, pr_below(&r, kind == DMC_K_TURN ? 4 : kind == DMC_K_SPLIT ? 1 : 2));
        }
        int steps = 0;
        while (!dmc_run(&g.m, &g.s, NULL) && steps < 40) {
            int c, kind, o, h = dmc_hint(&g.m, &g.s, g.par, &c, &kind, &o);
            if (h == 1) { if (!dmc_put(&g, c, kind, o)) ok = 0; }
            else if (h == 2) { if (!dmc_take(&g, c)) ok = 0; }
            else { ok = 0; break; }
            steps++;
        }
        if (!dmc_push(&g, NULL) || g.s.placed != g.par) ok = 0;
        while (dmc_undo(&g)) {}
        if (g.edits || g.s.placed) ok = 0;
    }
    CHECK(ok);

    if (fails) { printf("dominochain: %d failures\n", fails); return 1; }
    printf("dominochain: all passed\n");
    return 0;
}
