/*
 * Clear the Arrows' rules: an arrow leaves when every square from its head straight to the edge is free, its body
 * needing no room (it follows the head's path); it bumps the first arrow in its way (a mistake) when not; undo; boards
 * as text both ways (winding bodies, a neck right behind its head, a body never on its own way out, no square
 * shared); every level: read back as written, its arrows apart and each on the board, every head pointing the way
 * its body travels, cleared by the test's own search (any order of free arrows, arrow by arrow), not all free at the
 * start, every arrow leaving at some point, past chapter 1 at most 15% of its arrows straight or
 * single squares, the chapters' lengths, no two levels alike; the boards made at random (the same checks, the same
 * from the same seed, the time they take); whole games from a seed (random taps, hints, undo back to the start).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "games/arrowescape.h"
#include "games/puzrand.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

/* ---- the test's own board: a grid of arrow numbers, the arrows' squares walked from their heads ----------------- */
typedef struct { int w, h, n, g[ARX_CELLS], hx[ARX_MAXP], hy[ARX_MAXP], d[ARX_MAXP], left, overlap, off; } own;

static void own_from(const arx_map *m, own *o)
{
    static const int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};
    o->w = m->w;
    o->h = m->h;
    o->n = o->left = m->n;
    o->overlap = o->off = 0;
    for (int c = 0; c < ARX_CELLS; c++) o->g[c] = -1;
    for (int i = 0; i < m->n; i++) {
        int x = m->p[i].x, y = m->p[i].y;
        o->hx[i] = x;
        o->hy[i] = y;
        o->d[i] = m->p[i].dir;
        for (int k = 0; k < m->p[i].len; k++) {
            if (x < 0 || y < 0 || x >= m->w || y >= m->h) { o->off++; break; }
            if (o->g[x + y * m->w] >= 0) o->overlap++;
            o->g[x + y * m->w] = i;
            if (k + 1 < m->p[i].len) {
                x += dx[(int)m->p[i].step[k]];
                y += dy[(int)m->p[i].step[k]];
            }
        }
    }
}

static int own_go(own *o, int i)                /* 1: arrow i left (the squares ahead of its head all free) */
{
    static const int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};
    int d = o->d[i];
    for (int x = o->hx[i] + dx[d], y = o->hy[i] + dy[d]; x >= 0 && y >= 0 && x < o->w && y < o->h; x += dx[d], y += dy[d])
        if (o->g[x + y * o->w] >= 0) return 0;
    for (int c = 0; c < o->w * o->h; c++)
        if (o->g[c] == i) o->g[c] = -1;
    o->left--;
    return 1;
}

/* cleared arrow by arrow, each time a random free one; *left: each arrow left at some point */
static int own_clears(const arx_map *m, uint32_t seed)
{
    own o;
    own_from(m, &o);
    uint32_t r = pr_seed(seed);
    uint8_t gone[ARX_MAXP] = {0};
    while (o.left) {
        int list[ARX_MAXP], n = 0;
        for (int i = 0; i < o.n; i++) {
            if (gone[i]) continue;
            own t = o;
            if (own_go(&t, i)) list[n++] = i;
        }
        if (!n) return 0;
        int i = list[pr_below(&r, n)];
        own_go(&o, i);
        gone[i] = 1;
    }
    for (int i = 0; i < m->n; i++)
        if (!gone[i]) return 0;
    return 1;
}

/* every arrow's head points the way its body travels: its neck right behind it */
static int necks_behind(const arx_map *m)
{
    for (int i = 0; i < m->n; i++)
        if (m->p[i].len > 1 && m->p[i].step[0] != ((m->p[i].dir + 2) & 3)) return 0;
    return 1;
}

int main(void)
{
    static arx_game g, t, first;
    char text[ARX_TEXT];
    int gap;

    /* an L: its head R at the right of the middle row, its body to the left then down */
    CHECK(arx_parse(&g.m, "...|rR.|u..") && g.m.w == 3 && g.m.h == 3 && g.m.n == 1);
    CHECK(g.m.p[0].len == 3 && g.m.p[0].x == 1 && g.m.p[0].y == 1 && g.m.p[0].dir == ARX_RIGHT);
    CHECK(g.m.p[0].step[0] == ARX_LEFT && g.m.p[0].step[1] == ARX_DOWN && arx_bends(&g.m, 0) == 1);
    arx_text(&g.m, text);
    CHECK(!strcmp(text, "...|rR.|u.."));
    CHECK(!arx_parse(&g.m, "Rr.|...") && !arx_parse(&g.m, "lR.|...") && !arx_parse(&g.m, "Lx.|..."));
    /* its body never on its own way out: a head pointing at its own body */
    CHECK(!arx_parse(&g.m, "dl|Ru") && arx_parse(&g.m, "Ll.|..."));
    /* the body needs no room: a U-turn snake leaves when its head's way is clear */
    CHECK(arx_parse(&g.m, "Ud.|ul.|D.."));
    arx_begin(&g);
    CHECK(g.m.n == 2 && g.m.p[0].len == 4 && arx_free(&g, 0));
    CHECK(arx_blocker(&g, 1, &gap) == -1 && gap == 0);
    CHECK(arx_parse(&g.m, "R.L|...") && g.m.n == 2);
    arx_begin(&g);
    int b = -9;
    CHECK(arx_blocker(&g, 0, &gap) == 1 && gap == 1);
    CHECK(arx_tap(&g, 0, &b) == ARX_BUMP && b == 1 && g.mistakes == 1 && g.left == 2 && g.moves == 1);
    CHECK(arx_rounds(&g.m, NULL) == -1);         /* face to face: never */
    /* a snake blocked by another's body; cleared in the right order; undo */
    CHECK(!arx_parse(&g.m, ".D.|LrU|..."));   /* (a neck at the side: no) */
    CHECK(arx_parse(&g.m, ".D.|L.U|.ru"));
    arx_begin(&g);
    CHECK(g.m.n == 3);
    int dn = g.at[1], lf = g.at[3], up = g.at[5];
    CHECK(g.m.p[up].len == 3 && arx_free(&g, up) && !arx_free(&g, dn) && arx_free(&g, lf));
    CHECK(arx_tap(&g, dn, NULL) == ARX_BUMP);  /* D runs into the body of the arrow below it */
    CHECK(arx_tap(&g, up, NULL) == ARX_GONE && arx_free(&g, dn) && g.at[7] == ARX_NONE);
    CHECK(arx_undo(&g) && g.at[7] == up && g.left == 3 && g.mistakes == 1);
    CHECK(arx_script(&g, "=") && arx_solved(&g) && !arx_script(&g, "b2"));
    CHECK(arx_rounds(&g.m, NULL) == 2);

    /* every level */
    int per = ARX_LEVELS / ARX_CHAPTERS;
    double cells_ch[ARX_CHAPTERS] = {0}, arrows_ch[ARX_CHAPTERS] = {0}, single_ch[ARX_CHAPTERS] = {0};
    for (int lv = 0; lv < ARX_LEVELS; lv++) {
        CHECK(arx_level(&g, lv));
        arx_text(&g.m, text);
        CHECK(!strcmp(text, arx_levels[lv]));
        own o;
        own_from(&g.m, &o);
        CHECK(!o.overlap && !o.off && necks_behind(&g.m));
        int f0, rounds = arx_rounds(&g.m, &f0);
        CHECK(rounds >= (lv < 3 ? 2 : 3) && f0 < g.m.n);   /* (past level 3: three rounds deep at least) */
        if (lv / per == ARX_CHAPTERS - 1) CHECK(g.m.w * g.m.h >= 380 && g.m.n >= 45);   /* the last: big */
        CHECK(own_clears(&g.m, (uint32_t)lv + 1) && own_clears(&g.m, (uint32_t)lv + 1000));
        t = g;
        CHECK(arx_script(&t, "=") && arx_solved(&t) && t.mistakes == 0);
        int straight = 0, ch = lv / per;
        for (int i = 0; i < g.m.n; i++) {
            straight += arx_bends(&g.m, i) == 0;
            cells_ch[ch] += g.m.p[i].len;
            single_ch[ch] += g.m.p[i].len == 1;
        }
        arrows_ch[ch] += g.m.n;
        if (ch > 0) CHECK(straight * 100 <= 15 * g.m.n);
        if (ch > 0 && straight * 100 > 15 * g.m.n) printf("arrowescape: level %d, %d straight of %d\n", lv + 1, straight, g.m.n);
        for (int k = 0; k < lv; k++) CHECK(strcmp(arx_levels[k], arx_levels[lv]) != 0);
    }
    for (int ch = 0; ch < ARX_CHAPTERS; ch++)
        printf("arrowescape: chapter %d, arrows %.1f squares long on average, %.0f%% single squares\n", ch + 1,
               cells_ch[ch] / arrows_ch[ch], 100 * single_ch[ch] / arrows_ch[ch]);
    CHECK(cells_ch[1] / arrows_ch[1] > cells_ch[0] / arrows_ch[0] && cells_ch[2] / arrows_ch[2] >= 5);
    CHECK(arrows_ch[ARX_CHAPTERS - 1] / per >= 60);   /* the last chapter: dozens of arrows a board */
    CHECK(!arx_level(&g, ARX_LEVELS) && !arx_level(&g, -1));

    /* the boards made at random */
    for (int v = 0; v < ARX_VARIANTS; v++) {
        double worst = 0;
        int lo = 99, hi = 0;
        for (uint32_t seed = 1; seed <= 20; seed++) {
            clock_t t0 = clock();
            arx_random(&g, v, seed);
            double ms = (clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
            if (ms > worst) worst = ms;
            int f0, rounds = arx_rounds(&g.m, &f0);
            if (rounds < lo) lo = rounds;
            if (rounds > hi) hi = rounds;
            own o;
            own_from(&g.m, &o);
            CHECK(rounds >= 3 && f0 * 2 <= g.m.n && own_clears(&g.m, seed) && !o.overlap && !o.off && necks_behind(&g.m));
            arx_random(&t, v, seed);
            CHECK(!memcmp(&t.m, &g.m, sizeof g.m));
        }
        CHECK(worst < 400);
        printf("arrowescape: random variant %d, depth %d .. %d, the slowest %.1f ms\n", v, lo, hi, worst);
    }

    /* whole games: random taps (mistakes counted), a hint now and then, undo to the start */
    int ok = 1;
    for (uint32_t seed = 1; seed <= 80; seed++) {
        uint32_t r = pr_seed(seed);
        arx_level(&g, (int)(seed * 13 % ARX_LEVELS));
        first = g;
        int bumps = 0;
        for (int k = 0; k < 4000 && !arx_solved(&g); k++) {
            int i = pr_below(&r, 5) == 0 ? arx_hint(&g) : pr_below(&r, g.m.n);
            if (i < 0) { ok = 0; break; }
            int was = g.left, res = arx_tap(&g, i, NULL);
            if (res == ARX_BUMP) bumps++;
            if (res == ARX_GONE && g.left != was - 1) ok = 0;
        }
        if (!arx_solved(&g) || g.mistakes != bumps) ok = 0;
        while (arx_undo(&g)) {}
        if (memcmp(g.at, first.at, sizeof g.at) || g.left != g.m.n) ok = 0;
    }
    CHECK(ok);

    if (fails) { printf("arrowescape: %d failures\n", fails); return 1; }
    printf("arrowescape: all passed\n");
    return 0;
}
