/*
 * Dice Roll's rules: the die's 24 ways (checked against a die of the test's own: three faces turned by hand), rolls
 * over the void refused, the number tiles, the odd and even tiles, the arrows, the goal, undo; maps as text both ways;
 * every level's par found again by a search written here (its own die), its shortest way played at par, the chapters'
 * tiles, no two levels alike; the puzzles made at random (their par, the same from the same seed, the time they
 * take), and whole games from a seed (random rolls, a hint, the shortest way to the end, undo back to the start).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "games/diceroll.h"
#include "games/puzrand.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

/* ---- the test's own die: its top, south and east faces, turned by hand ------------------------------------------ */
typedef struct { int top, south, east; } die;

static die turn(die d, int dir)
{
    die n = d;
    switch (dir) {
    case DCR_RIGHT: n.top = 7 - d.east; n.east = d.top; break;
    case DCR_LEFT: n.top = d.east; n.east = 7 - d.top; break;
    case DCR_DOWN: n.top = 7 - d.south; n.south = d.top; break;
    default: n.top = d.south; n.south = 7 - d.top; break;
    }
    return n;
}

/* the fewest rolls by the test's own search (its die, its own reading of the tiles) */
static int own_par(const dcr_map *m)
{
    static int dist[DCR_CELLS][7][7][7];
    static int q[DCR_CELLS * 343][4];
    static const int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};
    memset(dist, -1, sizeof dist);
    die d0 = {dcr_top(m->o0), dcr_south(m->o0), dcr_east(m->o0)};
    int head = 0, tail = 0;
    q[tail][0] = m->start; q[tail][1] = d0.top; q[tail][2] = d0.south; q[tail++][3] = d0.east;
    dist[m->start][d0.top][d0.south][d0.east] = 0;
    while (head < tail) {
        int c = q[head][0];
        die d = {q[head][1], q[head][2], q[head][3]};
        head++;
        int k = dist[c][d.top][d.south][d.east];
        if (c == m->goal && d.top == m->face) return k;
        for (int dir = 0; dir < 4; dir++) {
            if (m->t[c] == DCR_ARROW && m->k[c] != dir) continue;
            int x = c % m->w + dx[dir], y = c / m->w + dy[dir];
            if (x < 0 || y < 0 || x >= m->w || y >= m->h) continue;
            int n = x + y * m->w, t = m->t[n];
            die e = turn(d, dir);
            if (t == DCR_VOID || (t == DCR_NUM && e.top != m->k[n]) || (t == DCR_ODD && e.top % 2 == 0) ||
                (t == DCR_EVEN && e.top % 2 == 1) || (t == DCR_ARROW && m->k[n] != dir))
                continue;
            if (dist[n][e.top][e.south][e.east] >= 0) continue;
            dist[n][e.top][e.south][e.east] = k + 1;
            q[tail][0] = n; q[tail][1] = e.top; q[tail][2] = e.south; q[tail++][3] = e.east;
        }
    }
    return -1;
}

static int has(const char *s, const char *any) { return strpbrk(s + 4, any) != NULL; }

int main(void)
{
    static dcr_game g, t, first;
    char text[DCR_TEXT];

    /* the die: 24 ways, all different, opposite faces 7 apart, each roll the hand-turned die's, undone by the other way */
    CHECK(dcr_top(0) == 1 && dcr_south(0) == 2 && dcr_east(0) == 3);
    for (int o = 0; o < 24; o++) {
        int a = dcr_top(o), b = dcr_south(o), c = dcr_east(o);
        CHECK(a != b && a != c && b != c && a + b != 7 && a + c != 7 && b + c != 7);
        CHECK(dcr_way_of(a, b) == o);
        for (int p = 0; p < o; p++) CHECK(dcr_top(p) != a || dcr_south(p) != b);
        for (int d = 0; d < 4; d++) {
            int n = dcr_roll(o, d);
            die e = turn((die){a, b, c}, d);
            CHECK(dcr_top(n) == e.top && dcr_south(n) == e.south && dcr_east(n) == e.east);
            CHECK(dcr_roll(n, d ^ 2) == o);
            CHECK(dcr_roll(dcr_roll(dcr_roll(n, d), d), d) == o);
        }
        const int8_t *R = dcr_rot(o);           /* a turn: its rows are unit vectors, its determinant 1 */
        int det = R[0] * (R[4] * R[8] - R[5] * R[7]) - R[1] * (R[3] * R[8] - R[5] * R[6]) +
                  R[2] * (R[3] * R[7] - R[4] * R[6]);
        CHECK(det == 1);
    }

    /* rolls on a plain board: 1 up, rolled right shows 4 up, then down shows 5 (the die: 1 up, 2 south, 3 east) */
    CHECK(dcr_parse(&g.m, "512:Soo|ooo|.oG"));
    dcr_begin(&g);
    CHECK(g.m.w == 3 && g.m.h == 3 && g.m.start == 0 && g.m.goal == 8 && g.m.face == 5 && g.m.o0 == 0);
    CHECK(!dcr_move(&g, DCR_UP) && !dcr_move(&g, DCR_LEFT) && g.moves == 0);
    CHECK(dcr_move(&g, DCR_RIGHT) && dcr_top(g.s.o) == 4 && g.s.x == 1);
    CHECK(dcr_move(&g, DCR_DOWN) && dcr_top(g.s.o) == 5 && g.s.y == 1);
    CHECK(dcr_undo(&g) && g.s.y == 0 && dcr_top(g.s.o) == 4 && g.moves == 1);
    CHECK(dcr_undo(&g) && !dcr_undo(&g) && g.s.o == 0);
    dcr_text(&g.m, text);
    CHECK(!strcmp(text, "512:Soo|ooo|.oG"));
    CHECK(!dcr_parse(&g.m, "512:ooo|ooG") && !dcr_parse(&g.m, "512:Soo|oG") &&
          !dcr_parse(&g.m, "512:SoZ|ooG") && !dcr_parse(&g.m, "5:SoG"));
    /* the goal: passed with another number on top, it is a plain tile; with its number: solved */
    CHECK(dcr_parse(&g.m, "412:SGo"));
    dcr_begin(&g);
    CHECK(dcr_move(&g, DCR_RIGHT) && dcr_solved(&g.m, &g.s) && !dcr_move(&g, DCR_RIGHT));
    CHECK(dcr_parse(&g.m, "612:SGo"));
    dcr_begin(&g);
    CHECK(dcr_move(&g, DCR_RIGHT) && !dcr_solved(&g.m, &g.s) && dcr_move(&g, DCR_RIGHT));
    /* a number tile: only with its number coming up */
    CHECK(dcr_parse(&g.m, "112:S3G|ooo"));
    dcr_begin(&g);
    CHECK(!dcr_move(&g, DCR_RIGHT));            /* 4 would come up */
    CHECK(dcr_parse(&g.m, "112:S4G|ooo"));
    dcr_begin(&g);
    CHECK(dcr_move(&g, DCR_RIGHT));
    /* odd and even tiles */
    CHECK(dcr_parse(&g.m, "112:Si|eG"));
    dcr_begin(&g);
    CHECK(!dcr_move(&g, DCR_RIGHT) && !dcr_move(&g, DCR_DOWN));   /* 4 then 5 would come up: even, odd */
    CHECK(dcr_parse(&g.m, "112:Se|iG"));
    dcr_begin(&g);
    CHECK(dcr_move(&g, DCR_RIGHT) && dcr_undo(&g) && dcr_move(&g, DCR_DOWN));
    /* an arrow: onto it and off it along it only */
    CHECK(dcr_parse(&g.m, "112:S>o|ooo|ooG"));
    dcr_begin(&g);
    CHECK(dcr_move(&g, DCR_RIGHT) && !dcr_move(&g, DCR_DOWN) && !dcr_move(&g, DCR_LEFT) && dcr_move(&g, DCR_RIGHT));
    CHECK(!dcr_move(&g, DCR_LEFT));             /* back onto it against it */
    CHECK(dcr_parse(&g.m, "112:Svo|ooo|ooG"));
    dcr_begin(&g);
    CHECK(!dcr_move(&g, DCR_RIGHT));
    t = g;
    CHECK(dcr_script(&t, "=") && dcr_solved(&t.m, &t.s));

    /* every level: read back as written, its par the search's and the test's own, its shortest way that long; the
     * chapters' tiles; no two alike; how many have one shortest way only */
    int per = DCR_LEVELS / DCR_CHAPTERS, unique = 0;
    for (int lv = 0; lv < DCR_LEVELS; lv++) {
        CHECK(dcr_level(&g, lv));
        dcr_text(&g.m, text);
        CHECK(!strcmp(text, dcr_levels[lv]));
        int ways;
        CHECK(!dcr_solved(&g.m, &g.s) && g.par == dcr_level_par[lv] && g.par > 0);
        CHECK(dcr_solve(&g.m, &g.s, NULL, NULL, &ways) == g.par && own_par(&g.m) == g.par);
        unique += ways == 1;
        t = g;
        CHECK(dcr_script(&t, "=") && dcr_solved(&t.m, &t.s) && t.moves == g.par);
        int ch = lv / per;
        const char *s = dcr_levels[lv];
        if (ch == 0) CHECK(!has(s, "123456ie^>v<"));
        if (ch == 1 || ch == 2) CHECK(has(s, "123456") && !has(s, "ie^>v<"));
        if (ch == 3) CHECK(has(s, "ie") && !has(s, "^>v<"));
        if (ch == 4) CHECK(has(s, "^>v<") && !has(s, "ie"));
        if (lv && lv % per) CHECK(dcr_level_par[lv] + 3 >= dcr_level_par[lv - 1]);   /* a smooth curve */
        for (int k = 0; k < lv; k++) CHECK(strcmp(dcr_levels[k], s) != 0);
    }
    CHECK(!dcr_level(&g, DCR_LEVELS) && !dcr_level(&g, -1));
    printf("diceroll: %d of %d levels have a single shortest way\n", unique, DCR_LEVELS);

    /* the puzzles made at random: solvable in their par, the same from the same seed, the time it takes */
    for (int v = 0; v < DCR_VARIANTS; v++) {
        double worst = 0;
        int lo = 999, hi = 0;
        for (uint32_t seed = 1; seed <= 30; seed++) {
            clock_t t0 = clock();
            dcr_random(&g, v, seed);
            double ms = (clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
            if (ms > worst) worst = ms;
            if (g.par < lo) lo = g.par;
            if (g.par > hi) hi = g.par;
            CHECK(g.par >= 5 && dcr_solve(&g.m, &g.s, NULL, NULL, NULL) == g.par && own_par(&g.m) == g.par);
            dcr_random(&t, v, seed);
            CHECK(!memcmp(&t.m, &g.m, sizeof g.m));
            dcr_text(&g.m, text);
            CHECK(dcr_parse(&t.m, text) && !memcmp(&t.m, &g.m, sizeof g.m));
        }
        CHECK(worst < 250);
        printf("diceroll: random variant %d, pars %d .. %d, the slowest %.0f ms\n", v, lo, hi, worst);
    }

    /* the hint from positions along random games: one roll fewer after it, its way ends solved; undo to the start */
    int ok = 1;
    for (uint32_t seed = 1; seed <= 100; seed++) {
        uint32_t r = pr_seed(seed);
        dcr_level(&g, (int)(seed * 7 % DCR_LEVELS));
        first = g;
        int steps = pr_below(&r, 30);
        for (int k = 0; k < steps && !dcr_solved(&g.m, &g.s); k++) dcr_move(&g, pr_below(&r, 4));
        if (!dcr_solved(&g.m, &g.s)) {
            int d, left = dcr_solve(&g.m, &g.s, &d, NULL, NULL);
            if (left > 0) {                     /* (an arrow may have led to a dead end) */
                if (!dcr_move(&g, d)) ok = 0;
                if (dcr_solve(&g.m, &g.s, NULL, NULL, NULL) != left - 1) ok = 0;
                int before = g.moves;
                dcr_script(&g, "=");
                if (!dcr_solved(&g.m, &g.s) || g.moves - before != left - 1) ok = 0;
            }
        }
        while (dcr_undo(&g)) {}
        if (memcmp(&first.s, &g.s, sizeof g.s) || g.moves) ok = 0;
    }
    CHECK(ok);

    if (fails) { printf("diceroll: %d failures\n", fails); return 1; }
    printf("diceroll: all passed\n");
    return 0;
}
