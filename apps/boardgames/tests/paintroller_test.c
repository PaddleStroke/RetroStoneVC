/*
 * Paint Roller's rules: a push rolls to the wall and paints, a push that moves nothing refused, the start painted,
 * double tiles (two coats), portals (out of the other, on the same way, no paint), arrows (along them only), undo;
 * maps as text both ways; every level's par found again by a search written here (its own rolling and paint), its
 * shortest way played at par, the chapters' tiles, no two levels alike; the puzzles made at random (their par, the
 * same from the same seed, the time they take); whole games from a seed (random pushes, a hint, the shortest way to
 * the end, undo back to the start).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "games/paintroller.h"
#include "games/puzrand.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

/* ---- the test's own search: a state as text (the ball's cell, then each tile's coats), kept in a hash table ---- */
#define OWN_CAP 400000
typedef struct { unsigned char pos, coats[PNR_CELLS]; } own;

static int own_need(const pnr_map *m, int c)
{
    int t = m->t[c];
    return t == PNR_DOUBLE ? 2 : t == PNR_TILE || t == PNR_ARROW ? 1 : 0;
}

/* a push the test's way: 0 nothing moved */
static int own_push(const pnr_map *m, const own *s, int dir, own *out)
{
    static const int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};
    *out = *s;
    int cur = s->pos, moved = 0;
    if (m->t[cur] == PNR_ARROW && m->k[cur] != dir) return 0;
    int guard = 0;
    for (; guard < 1000; guard++) {
        int x = cur % m->w + dx[dir], y = cur / m->w + dy[dir];
        if (x < 0 || y < 0 || x >= m->w || y >= m->h) break;
        int c = x + y * m->w;
        if (m->t[c] == PNR_WALL || (m->t[c] == PNR_ARROW && m->k[c] != dir)) break;
        moved = 1;
        if (m->t[c] == PNR_PORTAL) { cur = m->other[c]; continue; }
        if (out->coats[c] < own_need(m, c)) out->coats[c]++;
        cur = c;
    }
    if (guard == 1000) return 0;               /* round and round through the portals: refused */
    out->pos = (unsigned char)cur;
    return moved;
}

static int own_done(const pnr_map *m, const own *s)
{
    for (int c = 0; c < m->w * m->h; c++)
        if (s->coats[c] < own_need(m, c)) return 0;
    return 1;
}

static int own_par(const pnr_map *m)
{
    static own q[OWN_CAP];
    static int dist[OWN_CAP], tab[1 << 20];
    int n = m->w * m->h, tail = 0;
    memset(tab, -1, sizeof tab);
    memset(&q[0], 0, sizeof q[0]);
    q[0].pos = (unsigned char)m->start;
    q[0].coats[m->start] = 1;
    dist[0] = 0;
    tail = 1;
    for (int head = 0; head < tail; head++) {
        if (own_done(m, &q[head])) return dist[head];
        for (int d = 0; d < 4; d++) {
            own nx;
            if (!own_push(m, &q[head], d, &nx)) continue;
            unsigned h = 2166136261u ^ nx.pos;
            for (int c = 0; c < n; c++) h = (h ^ nx.coats[c]) * 16777619u;
            h &= (1 << 20) - 1;
            int found = 0;
            while (tab[h] >= 0) {
                own *o = &q[tab[h]];
                if (o->pos == nx.pos && !memcmp(o->coats, nx.coats, (size_t)n)) { found = 1; break; }
                h = (h + 1) & ((1 << 20) - 1);
            }
            if (found || tail >= OWN_CAP) continue;
            tab[h] = tail;
            q[tail] = nx;
            dist[tail++] = dist[head] + 1;
        }
    }
    return -1;
}

static int has(const char *s, const char *any) { return strpbrk(s, any) != NULL; }

int main(void)
{
    static pnr_game g, t, first;
    char text[PNR_TEXT];
    int path[PNR_PATH], np;

    /* a push rolls to the wall, painting: S at the top left, a wall in the middle */
    CHECK(pnr_parse(&g.m, "S..|.#.|..."));
    pnr_begin(&g);
    CHECK(g.m.w == 3 && g.m.h == 3 && g.m.nbits == 8 && pnr_coats(&g.m, &g.s, 0) == 1 && pnr_coats(&g.m, &g.s, 1) == 0);
    CHECK(!pnr_move(&g, PNR_UP) && !pnr_move(&g, PNR_LEFT) && g.moves == 0);
    CHECK(pnr_try(&g.m, &g.s, PNR_RIGHT, NULL, path, &np) && np == 3 && path[0] == 0 && path[2] == 2);
    CHECK(pnr_move(&g, PNR_RIGHT) && g.s.pos == 2 && pnr_coats(&g.m, &g.s, 1) == 1 && pnr_coats(&g.m, &g.s, 2) == 1);
    CHECK(pnr_move(&g, PNR_DOWN) && g.s.pos == 8 && !pnr_solved(&g.m, &g.s));
    CHECK(pnr_move(&g, PNR_LEFT) && pnr_move(&g, PNR_UP) && pnr_solved(&g.m, &g.s) && g.moves == 4);
    CHECK(!pnr_move(&g, PNR_RIGHT));
    CHECK(pnr_undo(&g) && !pnr_solved(&g.m, &g.s) && g.s.pos == 6 && g.moves == 3);
    pnr_text(&g.m, text);
    CHECK(!strcmp(text, "S..|.#.|..."));
    CHECK(!pnr_parse(&g.m, "...|.#.") && !pnr_parse(&g.m, "S..|.#") && !pnr_parse(&g.m, "SA.|...") &&
          !pnr_parse(&g.m, "S.x|...") && !pnr_parse(&g.m, "SS.|..."));
    /* a double tile: two coats */
    CHECK(pnr_parse(&g.m, "S2."));
    pnr_begin(&g);
    CHECK(pnr_move(&g, PNR_RIGHT) && pnr_coats(&g.m, &g.s, 1) == 1 && !pnr_solved(&g.m, &g.s));
    CHECK(pnr_move(&g, PNR_LEFT) && pnr_coats(&g.m, &g.s, 1) == 2 && pnr_solved(&g.m, &g.s));
    /* portals: in at one, out at the other, rolling on the same way; no paint on them */
    CHECK(pnr_parse(&g.m, "SA#|##.|.A."));
    pnr_begin(&g);
    CHECK(pnr_need(&g.m, 1) == 0 && g.m.other[1] == 7 && g.m.other[7] == 1);
    CHECK(pnr_try(&g.m, &g.s, PNR_RIGHT, NULL, path, &np) && np == 4 && path[1] == 1 && path[2] == 7 && path[3] == 8);
    CHECK(pnr_move(&g, PNR_RIGHT) && g.s.pos == 8 && pnr_coats(&g.m, &g.s, 8) == 1);
    /* an arrow: onto it and off it along it only */
    CHECK(pnr_parse(&g.m, "S>.|...|<.."));
    pnr_begin(&g);
    CHECK(pnr_move(&g, PNR_RIGHT) && g.s.pos == 2);
    CHECK(pnr_move(&g, PNR_DOWN) && pnr_move(&g, PNR_LEFT) && g.s.pos == 6);   /* along the arrow: onto it */
    /* off it along it only: here a dead end */
    CHECK(!pnr_move(&g, PNR_RIGHT) && !pnr_move(&g, PNR_UP) && pnr_solve(&g.m, &g.s, 1000, NULL, NULL) < 0);
    CHECK(pnr_parse(&g.m, ".<.|S.."));
    pnr_begin(&g);
    CHECK(pnr_move(&g, PNR_UP) && g.s.pos == 0 && !pnr_move(&g, PNR_RIGHT));   /* against the arrow: stopped */

    /* every level: read back as written, its par the search's and the test's own, its shortest way at par */
    int per = PNR_LEVELS / PNR_CHAPTERS;
    for (int lv = 0; lv < PNR_LEVELS; lv++) {
        CHECK(pnr_level(&g, lv));
        pnr_text(&g.m, text);
        CHECK(!strcmp(text, pnr_levels[lv]));
        CHECK(!pnr_solved(&g.m, &g.s) && g.par == pnr_level_par[lv] && g.par > 0);
        CHECK(pnr_solve(&g.m, &g.s, 400000, NULL, NULL) == g.par);
        int op = own_par(&g.m);
        CHECK(op == g.par);
        if (op != g.par) printf("paintroller: level %d, the test's own search %d, par %d\n", lv + 1, op, g.par);
        t = g;
        CHECK(pnr_script(&t, "=") && pnr_solved(&t.m, &t.s) && t.moves == g.par);
        int ch = lv / per;
        const char *s = pnr_levels[lv];
        if (ch <= 1) CHECK(!has(s, "2ABC^>v<"));
        if (ch == 2) CHECK(has(s, "2") && !has(s, "ABC^>v<"));
        if (ch == 3) CHECK(has(s, "ABC") && !has(s, "2^>v<"));
        if (ch == 4) CHECK(has(s, "^>v<") && !has(s, "2ABC"));
        if (lv && lv % per) CHECK(pnr_level_par[lv] + 3 >= pnr_level_par[lv - 1]);   /* a smooth curve */
        for (int k = 0; k < lv; k++) CHECK(strcmp(pnr_levels[k], s) != 0);
    }
    CHECK(!pnr_level(&g, PNR_LEVELS) && !pnr_level(&g, -1));

    /* the puzzles made at random: solvable in their par, the same from the same seed, the time it takes */
    for (int v = 0; v < PNR_VARIANTS; v++) {
        double worst = 0;
        int lo = 999, hi = 0;
        for (uint32_t seed = 1; seed <= 20; seed++) {
            clock_t t0 = clock();
            pnr_random(&g, v, seed);
            double ms = (clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
            if (ms > worst) worst = ms;
            if (g.par < lo) lo = g.par;
            if (g.par > hi) hi = g.par;
            CHECK(g.par >= 3 && own_par(&g.m) == g.par);
            pnr_random(&t, v, seed);
            CHECK(!memcmp(&t.m, &g.m, sizeof g.m));
            pnr_text(&g.m, text);
            CHECK(pnr_parse(&t.m, text) && !memcmp(&t.m, &g.m, sizeof g.m));
        }
        CHECK(worst < 400);
        printf("paintroller: random variant %d, pars %d .. %d, the slowest %.0f ms\n", v, lo, hi, worst);
    }

    /* the hint from positions along random games: one push fewer after it, its way ends solved; undo to the start */
    int ok = 1;
    for (uint32_t seed = 1; seed <= 60; seed++) {
        uint32_t r = pr_seed(seed);
        pnr_level(&g, (int)(seed * 7 % PNR_LEVELS));
        first = g;
        int steps = pr_below(&r, 12);
        for (int k = 0; k < steps && !pnr_solved(&g.m, &g.s); k++) pnr_move(&g, pr_below(&r, 4));
        if (!pnr_solved(&g.m, &g.s)) {
            int d, left = pnr_solve(&g.m, &g.s, 400000, &d, NULL);
            if (left > 0) {                     /* (an arrow may have led to a dead end) */
                if (!pnr_move(&g, d)) ok = 0;
                if (pnr_solve(&g.m, &g.s, 400000, NULL, NULL) != left - 1) ok = 0;
                int before = g.moves;
                pnr_script(&g, "=");
                if (!pnr_solved(&g.m, &g.s) || g.moves - before != left - 1) ok = 0;
            }
        }
        while (pnr_undo(&g)) {}
        if (memcmp(&first.s, &g.s, sizeof g.s) || g.moves) ok = 0;
    }
    CHECK(ok);

    if (fails) { printf("paintroller: %d failures\n", fails); return 1; }
    printf("paintroller: all passed\n");
    return 0;
}
