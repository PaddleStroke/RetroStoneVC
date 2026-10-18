/*
 * Clear the Arrows' rules: a tile leaves when its way to the edge is clear, bumps the first tile in its way (a
 * mistake) when not, bars move whole, a turnstile that cannot go turns a quarter, undo; boards as text both ways;
 * every level cleared by the test's own search (any order of free tiles, played tile by tile), not all free at the
 * start, deeper by the chapters, each chapter's feature, its shape symmetric, no two levels alike; the boards made at
 * random (cleared, the same from the same seed, the time they take); whole games from a seed (random taps, hints,
 * undo back to the start).
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

/* ---- the test's own board: a grid of tile numbers; a tile goes when the squares ahead of its head are empty ----- */
typedef struct { int w, h, n, g[ARX_CELLS], hx[ARX_MAXP], hy[ARX_MAXP], d[ARX_MAXP], turn[ARX_MAXP], left; } own;

static void own_from(const arx_map *m, own *o)
{
    o->w = m->w;
    o->h = m->h;
    o->n = o->left = m->n;
    for (int c = 0; c < ARX_CELLS; c++) o->g[c] = -1;
    for (int i = 0; i < m->n; i++) {
        static const int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};
        o->hx[i] = m->p[i].x;
        o->hy[i] = m->p[i].y;
        o->d[i] = m->p[i].dir;
        o->turn[i] = m->p[i].turn;
        for (int k = 0; k < m->p[i].len; k++) {
            int x = m->p[i].x - k * dx[o->d[i]], y = m->p[i].y - k * dy[o->d[i]];
            if (x >= 0 && y >= 0 && x < m->w && y < m->h) o->g[x + y * m->w] = i;
        }
    }
}

static int own_clear(const own *o, int i, int d)
{
    static const int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};
    for (int x = o->hx[i] + dx[d], y = o->hy[i] + dy[d]; x >= 0 && y >= 0 && x < o->w && y < o->h; x += dx[d], y += dy[d])
        if (o->g[x + y * o->w] >= 0) return 0;
    return 1;
}

static int own_go(own *o, int i)                /* 1: tile i left (a turnstile turned to its first clear way) */
{
    int d = -1;
    for (int k = 0; k < (o->turn[i] ? 4 : 1); k++)
        if (own_clear(o, i, (o->d[i] + k) & 3)) { d = (o->d[i] + k) & 3; break; }
    if (d < 0) return 0;
    for (int c = 0; c < o->w * o->h; c++)
        if (o->g[c] == i) o->g[c] = -1;
    o->left--;
    return 1;
}

/* cleared tile by tile, each time a random free tile */
static int own_clears(const arx_map *m, uint32_t seed)
{
    own o;
    own_from(m, &o);
    uint32_t r = pr_seed(seed);
    while (o.left) {
        int list[ARX_MAXP], n = 0;
        for (int i = 0; i < o.n; i++) {
            int on = 0;
            for (int c = 0; c < o.w * o.h; c++) on |= o.g[c] == i;
            if (!on) continue;
            own t = o;
            if (own_go(&t, i)) list[n++] = i;
        }
        if (!n) return 0;
        own_go(&o, list[pr_below(&r, n)]);
    }
    return 1;
}

static int mirrored(const arx_map *m)           /* the shape the same mirrored across, or turned half round */
{
    int a = 1, b = 1;
    for (int y = 0; y < m->h; y++)
        for (int x = 0; x < m->w; x++) {
            a &= m->shape[x + y * m->w] == m->shape[m->w - 1 - x + y * m->w];
            b &= m->shape[x + y * m->w] == m->shape[m->w - 1 - x + (m->h - 1 - y) * m->w];
        }
    if (!a && !b) return 0;
    /* the tiles too: mirrored across (a way left or right swapped), or turned half round (a tile turned onto its own
     * squares aside: the middle) */
    static const char *const MIR = "URDLurdl0123", *const MIRA = "ULDRuldr0321", *const HALF = "DLURdlur2301";
    char s[ARX_TEXT];
    arx_text(m, s);
    int ma = 1, mb = 1, w = m->w + 1;
    own own_g;                                  /* (a tile on its own image: the middle, laid alone) */
    own_from(m, &own_g);
    for (int y = 0; y < m->h; y++)
        for (int x = 0; x < m->w; x++) {
            char c = s[x + y * w], o = s[m->w - 1 - x + y * w], q = s[m->w - 1 - x + (m->h - 1 - y) * w];
            int mid = own_g.g[x + y * m->w] >= 0 && own_g.g[x + y * m->w] == own_g.g[m->w - 1 - x + (m->h - 1 - y) * m->w];
            const char *pc = c == '.' ? NULL : strchr(MIR, c);
            if (!pc) { ma &= o == '.'; mb &= q == '.' || mid; continue; }
            ma &= o == MIRA[pc - MIR];
            mb &= q == HALF[pc - MIR] || mid;
        }
    return ma || mb;
}

int main(void)
{
    static arx_game g, t, first;
    char text[ARX_TEXT];

    /* a row: R at the right edge goes; the L behind... */
    CHECK(arx_parse(&g.m, "LrR|...") && g.m.w == 3 && g.m.h == 2 && g.m.n == 2);
    arx_begin(&g);
    CHECK(g.m.p[0].len == 1 && g.m.p[1].len == 2 && g.m.p[1].x == 2 && g.m.p[1].dir == ARX_RIGHT);
    CHECK(arx_free(&g, 0) && arx_free(&g, 1));
    CHECK(arx_parse(&g.m, "RrL|...") == 0);   /* a bar's square behind no head */
    CHECK(arx_parse(&g.m, "RL.|.U.|.D."));
    arx_begin(&g);
    int b = -9, gap = -1;
    CHECK(arx_blocker(&g, 0, &gap) == 1 && gap == 0);   /* R bumps L */
    CHECK(arx_tap(&g, 0, &b) == ARX_BUMP && b == 1 && g.mistakes == 1 && g.left == 4);
    CHECK(arx_tap(&g, 1, NULL) == ARX_BUMP);              /* L bumps R */
    CHECK(arx_blocker(&g, 2, &gap) == 1 && gap == 0);     /* U at b2 runs into L at b1 */
    CHECK(arx_blocker(&g, 3, &gap) == -1 && gap == 0);    /* D at the bottom */
    CHECK(arx_tap(&g, 3, NULL) == ARX_GONE && g.left == 3 && g.at[1 + 2 * 3] == ARX_NONE);
    CHECK(arx_undo(&g) && g.left == 4 && g.at[1 + 2 * 3] == 3 && g.mistakes == 2);
    CHECK(arx_script(&g, "b3") && !arx_script(&g, "b3") && g.left == 3);
    CHECK(arx_hint(&g) == -1 || arx_free(&g, arx_hint(&g)));
    CHECK(!arx_free(&g, 0) && !arx_free(&g, 1));
    CHECK(arx_rounds(&g.m, NULL) == -1);                  /* R and L face each other: never */
    /* a turnstile: turns a quarter until a way is clear, then goes */
    CHECK(arx_parse(&g.m, ".D.|R1L|.U."));
    arx_begin(&g);
    CHECK(g.m.p[2].turn && arx_free(&g, 2) == 0);
    CHECK(arx_parse(&g.m, "R2.|.U."));
    arx_begin(&g);
    int ts = g.at[1];
    CHECK(ts == 1 && g.m.p[ts].turn && arx_free(&g, ts) && !arx_free(&g, 0) && !arx_free(&g, 2));
    CHECK(arx_tap(&g, ts, NULL) == ARX_TURNED && g.dir[ts] == ARX_LEFT && g.mistakes == 0);
    CHECK(arx_tap(&g, ts, NULL) == ARX_TURNED && g.dir[ts] == ARX_UP);
    CHECK(arx_tap(&g, ts, NULL) == ARX_GONE && g.left == 2 && arx_free(&g, 0) && arx_free(&g, 2));
    CHECK(arx_undo(&g) && g.dir[ts] == ARX_UP && arx_undo(&g) && g.dir[ts] == ARX_LEFT && arx_undo(&g) &&
          g.dir[ts] == ARX_DOWN && !arx_undo(&g));
    arx_text(&g.m, text);
    CHECK(!strcmp(text, "R2.|.U."));
    CHECK(arx_rounds(&g.m, NULL) == 2);                   /* the turnstile, then the arrows it stood before */
    /* a bar moves whole, and stands in the way with any of its squares */
    CHECK(arx_parse(&g.m, "Ll.|R.U|..u") && g.m.p[0].len == 2 && g.m.p[2].len == 2 && g.m.p[2].dir == ARX_UP);
    arx_begin(&g);
    CHECK(arx_blocker(&g, 1, &gap) == 2 && gap == 1 && arx_tap(&g, 1, NULL) == ARX_BUMP);
    CHECK(arx_blocker(&g, 2, NULL) == -1 && arx_tap(&g, 0, NULL) == ARX_GONE && g.at[0] == ARX_NONE && g.at[1] == ARX_NONE);
    CHECK(arx_tap(&g, 2, NULL) == ARX_GONE && arx_tap(&g, 1, NULL) == ARX_GONE && arx_solved(&g));
    CHECK(!arx_parse(&g.m, "Lx.|..."));

    /* every level */
    int per = ARX_LEVELS / ARX_CHAPTERS, depth[ARX_CHAPTERS] = {0};
    for (int lv = 0; lv < ARX_LEVELS; lv++) {
        CHECK(arx_level(&g, lv));
        arx_text(&g.m, text);
        CHECK(!strcmp(text, arx_levels[lv]));
        int f0, rounds = arx_rounds(&g.m, &f0);
        CHECK(rounds >= 2 && f0 < g.m.n);
        if (rounds < 2) printf("arrowescape: level %d, depth %d\n", lv + 1, rounds);
        CHECK(own_clears(&g.m, (uint32_t)lv + 1) && own_clears(&g.m, (uint32_t)lv + 1000));
        CHECK(mirrored(&g.m));
        if (!mirrored(&g.m)) printf("arrowescape: level %d not symmetric: %s\n", lv + 1, arx_levels[lv]);
        t = g;
        CHECK(arx_script(&t, "=") && arx_solved(&t) && t.mistakes == 0);
        int bars = 0, turns = 0;
        for (int i = 0; i < g.m.n; i++) {
            bars += g.m.p[i].len > 1;
            turns += g.m.p[i].turn;
        }
        int ch = lv / per;
        if (ch == 0) CHECK(!bars && !turns);
        if (ch == 1 || ch == 3 || ch == 4) CHECK(bars >= 2 && !turns);
        if (ch == 2) CHECK(turns >= 2);
        depth[ch] += rounds;
        for (int k = 0; k < lv; k++) CHECK(strcmp(arx_levels[k], arx_levels[lv]) != 0);
    }
    for (int ch = 1; ch < ARX_CHAPTERS; ch++) if (ch != 2) CHECK(depth[ch] >= depth[ch - 1]);
    printf("arrowescape: depth by chapter (sum) %d %d %d %d %d %d\n", depth[0], depth[1], depth[2], depth[3], depth[4],
           depth[5]);
    CHECK(!arx_level(&g, ARX_LEVELS) && !arx_level(&g, -1));

    /* the boards made at random */
    for (int v = 0; v < ARX_VARIANTS; v++) {
        double worst = 0;
        int lo = 99, hi = 0;
        for (uint32_t seed = 1; seed <= 30; seed++) {
            clock_t t0 = clock();
            arx_random(&g, v, seed);
            double ms = (clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
            if (ms > worst) worst = ms;
            int f0, rounds = arx_rounds(&g.m, &f0);
            if (rounds < lo) lo = rounds;
            if (rounds > hi) hi = rounds;
            CHECK(rounds >= 3 && f0 * 2 <= g.m.n && own_clears(&g.m, seed));
            arx_random(&t, v, seed);
            CHECK(!memcmp(&t.m, &g.m, sizeof g.m));
        }
        CHECK(worst < 300);
        printf("arrowescape: random variant %d, depth %d .. %d, the slowest %.1f ms\n", v, lo, hi, worst);
    }

    /* whole games: random taps (mistakes counted, turnstiles turning), a hint now and then, undo to the start */
    int ok = 1;
    for (uint32_t seed = 1; seed <= 80; seed++) {
        uint32_t r = pr_seed(seed);
        arx_level(&g, (int)(seed * 13 % ARX_LEVELS));
        first = g;
        int bumps = 0;
        for (int k = 0; k < 2000 && !arx_solved(&g); k++) {
            int i = pr_below(&r, 5) == 0 ? arx_hint(&g) : pr_below(&r, g.m.n);
            if (i < 0) { ok = 0; break; }
            int was = g.left, res = arx_tap(&g, i, NULL);
            if (res == ARX_BUMP) bumps++;
            if (res == ARX_GONE && g.left != was - 1) ok = 0;
            if (res == ARX_BUMP && g.m.p[i].turn) ok = 0;
        }
        if (!arx_solved(&g) || g.mistakes != bumps) ok = 0;
        while (arx_undo(&g)) {}
        if (g.nh == 0 && g.moves == 0 && memcmp(g.at, first.at, sizeof g.at)) ok = 0;
    }
    CHECK(ok);

    if (fails) { printf("arrowescape: %d failures\n", fails); return 1; }
    printf("arrowescape: all passed\n");
    return 0;
}
