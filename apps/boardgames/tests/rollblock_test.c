/*
 * Roll the Block's rules: the rolls (standing, lying either way, over the void refused), a cracked tile, a round
 * button and a cross with their bridge, a bridge shut under the block, the split tile and the cubes joining again,
 * undo; the pars of the plain levels found again by a search written here, every level's par and its shortest way
 * played, the chapters' special tiles, maps as text both ways, the puzzles made at random (their par, the time they
 * take), and whole games from a seed (random rolls, then hints to the end, then undo back to the start).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "games/puzrand.h"
#include "games/rollblock.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

/* ---- the test's own search, plain tiles only: the block as the two cells it covers (the same twice: standing) */
static int own_par(const rlb_map *m)
{
    static int dist[RLB_CELLS][RLB_CELLS];
    static int qa[RLB_CELLS * RLB_CELLS], qb[RLB_CELLS * RLB_CELLS];
    int w = m->w, h = m->h;
    memset(dist, -1, sizeof dist);
    int head = 0, tail = 0;
    qa[tail] = qb[tail] = m->start;
    tail++;
    dist[m->start][m->start] = 0;
    while (head < tail) {
        int a = qa[head], b = qb[head++], d = dist[a][b];
        if (a == b && a == m->hole) return d;
        int ax = a % w, ay = a / w, bx = b % w, by = b / w;
        for (int k = 0; k < 4; k++) {
            int dx = k == 1 ? 1 : k == 3 ? -1 : 0, dy = k == 2 ? 1 : k == 0 ? -1 : 0, nax, nay, nbx, nby;
            if (a == b) {                       /* standing: down onto the two cells that way */
                nax = ax + dx; nay = ay + dy; nbx = ax + 2 * dx; nby = ay + 2 * dy;
            } else if ((bx - ax) * dx + (by - ay) * dy != 0) {   /* lying along the roll: up on the cell past its end */
                int ahead = (bx - ax) * dx + (by - ay) * dy > 0, fx = ahead ? bx : ax, fy = ahead ? by : ay;
                nax = nbx = fx + dx; nay = nby = fy + dy;
            } else {                            /* lying across it: alongside */
                nax = ax + dx; nay = ay + dy; nbx = bx + dx; nby = by + dy;
            }
            if (nax < 0 || nay < 0 || nbx < 0 || nby < 0 || nax >= w || nbx >= w || nay >= h || nby >= h) continue;
            int na = nax + nay * w, nb = nbx + nby * w;
            if (!m->t[na] || !m->t[nb]) continue;
            if (na > nb) { int t = na; na = nb; nb = t; }
            if (dist[na][nb] >= 0) continue;
            dist[na][nb] = d + 1;
            qa[tail] = na;
            qb[tail++] = nb;
        }
    }
    return -1;
}

static int has(const char *s, const char *any) { return strpbrk(s, any) != NULL; }

int main(void)
{
    static rlb_game g, t, first;
    char text[RLB_CELLS + RLB_MAXH + 1];

    /* rolls on a rectangle: standing on a1, lying on b1..c1, standing on d1, lying on d2..d3, rolled along to e2..e3 */
    CHECK(rlb_parse(&g.m, "Sooooo|oooooo|oooooH"));
    rlb_begin(&g);
    CHECK(g.m.w == 6 && g.m.h == 3 && g.m.start == 0 && g.m.hole == 17);
    CHECK(!rlb_move(&g, RLB_LEFT) && !rlb_move(&g, RLB_UP) && g.moves == 0);   /* over the void */
    CHECK(rlb_move(&g, RLB_RIGHT) && g.s.o == RLB_LIE_X && g.s.x == 1 && g.s.y == 0);
    CHECK(rlb_move(&g, RLB_RIGHT) && g.s.o == RLB_STAND && g.s.x == 3);
    CHECK(rlb_move(&g, RLB_DOWN) && g.s.o == RLB_LIE_Y && g.s.y == 1 && !rlb_move(&g, RLB_DOWN));
    CHECK(rlb_move(&g, RLB_RIGHT) && g.s.o == RLB_LIE_Y && g.s.x == 4 && g.moves == 4);
    CHECK(rlb_undo(&g) && g.s.x == 3 && g.moves == 3);
    CHECK(rlb_solve(&g.m, &g.s, NULL, NULL, NULL) > 0);
    t = g;
    CHECK(rlb_script(&t, "=") && rlb_solved(&t.s, &t.m) && !rlb_move(&t, RLB_UP));
    rlb_text(&g.m, text);
    CHECK(!strcmp(text, "Sooooo|oooooo|oooooH"));
    CHECK(!rlb_parse(&g.m, "ooo|oHo") && !rlb_parse(&g.m, "Soo|oH") && !rlb_parse(&g.m, "SoZ|ooH") &&
          !rlb_parse(&g.m, "Sox|ooH"));

    /* a cracked tile: lying on it, never standing */
    CHECK(rlb_parse(&g.m, "Sfo|ooH"));
    rlb_begin(&g);
    CHECK(rlb_move(&g, RLB_RIGHT) && g.s.o == RLB_LIE_X && rlb_move(&g, RLB_LEFT));
    CHECK(rlb_parse(&g.m, "Soofoo|oooooH"));
    rlb_begin(&g);
    CHECK(rlb_move(&g, RLB_RIGHT) && !rlb_move(&g, RLB_RIGHT));   /* up on the crack */

    /* a round button opens the bridge (1), shuts it again when rolled on once more */
    CHECK(rlb_parse(&g.m, "Saoo1oH"));
    rlb_begin(&g);
    CHECK(rlb_move(&g, RLB_RIGHT) == 16 + RLB_EV_SWITCH && (g.s.open & 1));   /* lying on the button and b1 */
    CHECK(rlb_move(&g, RLB_RIGHT) == 16 && g.s.x == 3 && g.s.o == RLB_STAND);  /* on d1, before the bridge */
    CHECK(rlb_move(&g, RLB_RIGHT) && g.s.o == RLB_LIE_X && g.s.x == 4);        /* on the bridge and f1 */
    CHECK(rlb_move(&g, RLB_RIGHT) == 16 + RLB_EV_WIN && rlb_solved(&g.s, &g.m));
    rlb_begin(&g);
    CHECK(rlb_script(&g, "rr") && rlb_move(&g, RLB_LEFT) == 16 + RLB_EV_SWITCH && !(g.s.open & 1));
    /* a cross: only under the block standing */
    CHECK(rlb_parse(&g.m, "SAooH"));
    rlb_begin(&g);
    CHECK(rlb_move(&g, RLB_RIGHT) == 16 && !g.s.open);
    CHECK(rlb_parse(&g.m, "SooA1oH"));
    rlb_begin(&g);
    CHECK(rlb_move(&g, RLB_RIGHT) && rlb_move(&g, RLB_RIGHT) == 16 + RLB_EV_SWITCH && (g.s.open & 1));
    CHECK(rlb_script(&g, "rr") && rlb_solved(&g.s, &g.m) && g.moves == 4);
    /* a bridge open at the start, shut under the block by its button: refused */
    CHECK(rlb_parse(&g.m, "S5a|ooH"));
    rlb_begin(&g);
    CHECK((g.s.open & 1) && !rlb_move(&g, RLB_RIGHT) && g.moves == 0);

    /* the split tile: the block standing on it splits onto the rings; a cube at a time; side by side they join */
    CHECK(rlb_parse(&g.m, "Sooxo|.....|yoooy|ooooo|....H"));
    rlb_begin(&g);
    CHECK(rlb_move(&g, RLB_RIGHT) && rlb_move(&g, RLB_RIGHT) == 16 + RLB_EV_SPLIT);
    CHECK(g.s.o == RLB_TWO && g.s.x == 0 && g.s.y == 2 && g.s.x2 == 4 && g.s.y2 == 2 && g.s.act == 0);
    CHECK(rlb_move(&g, RLB_RIGHT) && g.s.x == 1 && rlb_choose(&g) && g.s.act == 1);
    CHECK(rlb_move(&g, RLB_LEFT) && g.s.x2 == 3 && g.s.o == RLB_TWO);
    CHECK(rlb_move(&g, RLB_LEFT) == 16 + RLB_EV_JOIN && g.s.o == RLB_LIE_X && g.s.x == 1 && g.s.y == 2);
    CHECK(rlb_undo(&g) && g.s.o == RLB_TWO && g.s.x2 == 3);
    t = g;
    CHECK(rlb_script(&t, "=") && rlb_solved(&t.s, &t.m));   /* the cubes join in a column on e3..e4 */
    CHECK(!rlb_choose(&t));

    /* every level: read back as it was written, its par the search's (the plain ones by the test's search too), its
     * shortest way that long; no two alike; the chapters' tiles */
    int per = RLB_LEVELS / RLB_CHAPTERS;
    for (int lv = 0; lv < RLB_LEVELS; lv++) {
        CHECK(rlb_level(&g, lv));
        rlb_text(&g.m, text);
        CHECK(!strcmp(text, rlb_levels[lv]));
        CHECK(!rlb_solved(&g.s, &g.m) && g.par == rlb_level_par[lv] && g.par > 0);
        CHECK(rlb_solve(&g.m, &g.s, NULL, NULL, NULL) == g.par);
        if (lv < 2 * per) CHECK(!has(rlb_levels[lv], "fabcdABCD12345678xy") && own_par(&g.m) == g.par);
        t = g;
        CHECK(rlb_script(&t, "=") && rlb_solved(&t.s, &t.m) && t.moves == g.par);
        int ch = lv / per;
        if (ch == 2) CHECK(has(rlb_levels[lv], "f"));
        if (ch == 3) CHECK(has(rlb_levels[lv], "abcdABCD") && has(rlb_levels[lv], "12345678"));
        if (ch >= 4) CHECK(has(rlb_levels[lv], "x"));
        if (ch == 5) CHECK(has(rlb_levels[lv], "abcdABCD"));
        if (lv >= per && lv % per == 0) CHECK(rlb_level_par[lv + per - 1] >= rlb_level_par[lv - 1]);   /* the hardest */
        for (int k = 0; k < lv; k++) CHECK(strcmp(rlb_levels[k], rlb_levels[lv]) != 0);
    }
    CHECK(!rlb_level(&g, RLB_LEVELS));

    /* the puzzles made at random: solvable in their par, the same from the same seed, the time it takes */
    for (int v = 0; v < RLB_VARIANTS; v++) {
        double worst = 0;
        int lo = 999, hi = 0;
        for (uint32_t seed = 1; seed <= 10; seed++) {
            clock_t t0 = clock();
            rlb_random(&g, v, seed);
            double ms = (clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
            if (ms > worst) worst = ms;
            if (g.par < lo) lo = g.par;
            if (g.par > hi) hi = g.par;
            CHECK(g.par >= 4 && rlb_solve(&g.m, &g.s, NULL, NULL, NULL) == g.par);
            rlb_random(&t, v, seed);
            CHECK(!memcmp(&t.m, &g.m, sizeof g.m));
            rlb_text(&g.m, text);
            CHECK(rlb_parse(&t.m, text) && !memcmp(&t.m, &g.m, sizeof g.m));
            if (v == 2) CHECK(g.m.split >= 0);
        }
        printf("rollblock: random variant %d, pars %d .. %d, the slowest %.0f ms\n", v, lo, hi, worst);
    }

    /* the hint from positions along random games: its first move leaves one move fewer, its way ends solved; undo
     * all the way back gives the start again */
    int ok = 1;
    for (uint32_t seed = 1; seed <= 80; seed++) {
        uint32_t r = pr_seed(seed);
        rlb_level(&g, (int)(seed * 7 % RLB_LEVELS));
        first = g;
        int steps = pr_below(&r, 30);
        for (int k = 0; k < steps && !rlb_solved(&g.s, &g.m); k++) {
            if (pr_below(&r, 5) == 0) rlb_choose(&g);
            rlb_move(&g, pr_below(&r, 4));
        }
        if (!rlb_solved(&g.s, &g.m)) {
            int d, cube, left = rlb_solve(&g.m, &g.s, &d, &cube, NULL);
            if (left > 0) {                     /* (a cube may have stranded the other one: no way left) */
                if (g.s.o == RLB_TWO) g.s.act = (int8_t)cube;
                if (!rlb_move(&g, d)) ok = 0;
                if (rlb_solve(&g.m, &g.s, NULL, NULL, NULL) != left - 1) ok = 0;
                int before = g.moves;
                rlb_script(&g, "=");
                if (!rlb_solved(&g.s, &g.m) || g.moves - before != left - 1) ok = 0;
            }
        }
        while (rlb_undo(&g)) {}
        if (memcmp(&first.s, &g.s, sizeof g.s) || g.moves) ok = 0;
    }
    CHECK(ok);

    if (fails) { printf("rollblock: %d failures\n", fails); return 1; }
    printf("rollblock: all passed\n");
    return 0;
}
