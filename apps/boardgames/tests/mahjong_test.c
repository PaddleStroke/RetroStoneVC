/*
 * Mahjong Solitaire's rules: the layouts, the free-tile rule, matching (the flowers, the seasons), the deals made
 * backwards (many seeds, each cleared by following its own order), the shuffle, undo and the end.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/mahjong.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

static unsigned rng = 1;
static int rnd(int n)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return n > 0 ? (int)(rng % (unsigned)n) : 0;
}

static mjgame g, h;

static int find(const mjgame *m, int x, int y, int z)
{
    for (int i = 0; i < m->n; i++)
        if (m->pos[i].x == x && m->pos[i].y == y && m->pos[i].z == z) return i;
    return -1;
}

int main(void)
{
    /* the layouts: 144 and 80 places, no two on one layer overlapping, every upper tile resting on tiles */
    static const int sizes[MJ_VARIANTS] = {144, 80};
    for (int v = 0; v < MJ_VARIANTS; v++) {
        mjpos p[MJ_MAX];
        int n = mj_layout(v, p), overlap = 0, floating = 0;
        CHECK(n == sizes[v]);
        for (int i = 0; i < n; i++) {
            int under = 0;
            for (int j = 0; j < n; j++) {
                int ox = p[i].x - p[j].x, oy = p[i].y - p[j].y;
                int over = ox > -2 && ox < 2 && oy > -2 && oy < 2;
                if (i != j && p[i].z == p[j].z && over) overlap++;
                if (p[j].z == p[i].z - 1 && over) under++;
            }
            if (p[i].z > 0 && !under) floating++;
        }
        CHECK(!overlap && !floating);
    }

    /* the faces: the matches */
    CHECK(mj_match(MJ_DOTS + 4, MJ_DOTS + 4) && !mj_match(MJ_DOTS + 4, MJ_BAMBOO + 4));
    CHECK(!mj_match(MJ_CHARS + 0, MJ_CHARS + 1) && mj_match(MJ_WINDS + 2, MJ_WINDS + 2));
    CHECK(mj_match(MJ_DRAGONS + 2, MJ_DRAGONS + 2) && !mj_match(MJ_DRAGONS, MJ_DRAGONS + 1));
    for (int a = 0; a < 4; a++)
        for (int b = 0; b < 4; b++) {
            CHECK(mj_match(MJ_FLOWERS + a, MJ_FLOWERS + b));   /* any flower, any flower */
            CHECK(mj_match(MJ_SEASONS + a, MJ_SEASONS + b));
            CHECK(!mj_match(MJ_FLOWERS + a, MJ_SEASONS + b) && !mj_match(MJ_SEASONS + a, MJ_FLOWERS + b));
        }
    CHECK(!mj_match(MJ_FLOWERS, MJ_DOTS) && !mj_match(MJ_NONE, MJ_NONE));

    /* a deal of the Turtle: each face four times, each flower and season once */
    rng = 7;
    CHECK(mj_deal(&g, 0, rnd));
    int count[MJ_FACES] = {0};
    for (int i = 0; i < g.n; i++) count[g.face[i]]++;
    for (int f = 0; f < MJ_FACES; f++) CHECK(count[f] == (f < MJ_FLOWERS ? 4 : 1));
    CHECK(g.tiles == 144 && mj_status(&g) == MJ_PLAY);

    /* the free-tile rule on the Turtle */
    int top = find(&g, 13, 7, 4), head = find(&g, 0, 7, 0), tail = find(&g, 28, 7, 0), t26 = find(&g, 26, 7, 0);
    int r0c1 = find(&g, 2, 0, 0), r0c2 = find(&g, 4, 0, 0), r3c1 = find(&g, 2, 6, 0), under_top = find(&g, 12, 6, 3);
    CHECK(top >= 0 && head >= 0 && tail >= 0 && t26 >= 0 && r0c1 >= 0 && r3c1 >= 0 && under_top >= 0);
    CHECK(mj_free(&g, top));                    /* the top: nothing on it, both sides open */
    CHECK(mj_free(&g, head) && mj_free(&g, tail) && !mj_free(&g, t26));   /* the tail's inner tile: both sides */
    CHECK(mj_free(&g, r0c1) && !mj_free(&g, r0c2));  /* a row's end is free, its next tile not */
    CHECK(!mj_free(&g, r3c1));                  /* row 3's first tile: the head on its left, a tile on its right */
    CHECK(!mj_free(&g, under_top));             /* under the top tile */
    int free0 = 0;
    for (int i = 0; i < g.n; i++) free0 += mj_free(&g, i);
    CHECK(free0 == 35);                         /* the row ends of each layer, the head, the tail, the top */
    {   /* the side rule by itself: take the head away (by hand), row 3's first tile opens */
        h = g;
        h.face[head] = MJ_NONE;
        CHECK(mj_free(&h, r3c1));
        h.face[top] = MJ_NONE;
        CHECK(mj_free(&h, under_top));          /* the top gone: its left side is open (row's end on layer 3) */
    }

    /* taking a pair: only free matching tiles; undo puts them back */
    {
        int a = -1, b = -1;
        CHECK(mj_hint(&g, &a, &b) || mj_status(&g) != MJ_PLAY);
        if (a >= 0) {
            CHECK(mj_match(g.face[a], g.face[b]) && mj_free(&g, a) && mj_free(&g, b));
            int fa = g.face[a];
            CHECK(!mj_take(&g, a, a));
            CHECK(mj_take(&g, a, b) && g.tiles == 142 && g.face[a] == MJ_NONE);
            CHECK(!mj_take(&g, a, b));          /* gone */
            CHECK(mj_undo(&g) && g.tiles == 144 && g.face[a] == fa);
            CHECK(!mj_undo(&g));
        }
        int blocked = r0c2, other = -1;         /* a blocked tile, though its match is free: refused */
        for (int i = 0; i < g.n; i++)
            if (i != blocked && mj_free(&g, i) && mj_match(g.face[i], g.face[blocked])) other = i;
        if (other >= 0) CHECK(!mj_take(&g, blocked, other));
        int x = -1, y = -1;                     /* two free tiles that do not match: refused */
        for (int i = 0; i < g.n && x < 0; i++)
            for (int j = i + 1; j < g.n; j++)
                if (mj_free(&g, i) && mj_free(&g, j) && !mj_match(g.face[i], g.face[j])) { x = i; y = j; break; }
        CHECK(x >= 0 && !mj_take(&g, x, y));
    }

    /* the deals: many seeds, both layouts, each cleared by following its own order */
    int dealt = 0, cleared = 0;
    for (int v = 0; v < MJ_VARIANTS; v++)
        for (unsigned seed = 1; seed <= 300; seed++) {
            rng = seed * 2654435761u;
            if (!rng) rng = 1;
            if (!mj_deal(&g, v, rnd)) continue;
            dealt++;
            int ok = 1;
            for (int k = 0; k + 1 < g.nsol && ok; k += 2) ok = mj_take(&g, g.sol[k], g.sol[k + 1]);
            if (ok && g.tiles == 0 && mj_status(&g) == MJ_WON) cleared++;
        }
    CHECK(dealt == 600);
    CHECK(cleared == 600);
    printf("mahjong: %d deals made, %d cleared by their own order\n", dealt, cleared);

    /* the Pyramid's deal: 40 pairs of the full set */
    rng = 99;
    CHECK(mj_deal(&g, 1, rnd) && g.n == 80 && g.tiles == 80);
    memset(count, 0, sizeof count);
    for (int i = 0; i < g.n; i++) count[g.face[i]]++;
    int fl = 0, se = 0;
    for (int f = 0; f < MJ_FACES; f++) {
        if (f >= MJ_SEASONS) se += count[f];
        else if (f >= MJ_FLOWERS) fl += count[f];
        else CHECK(count[f] % 2 == 0 && count[f] <= 4);
    }
    CHECK(fl % 2 == 0 && se % 2 == 0);

    /* the script: "=n" follows the deal's order, "+n" the first pairs found, "a-b" a pair by its places */
    rng = 5;
    CHECK(mj_deal(&g, 0, rnd));
    CHECK(mj_script(&g, "=10", rnd) && g.tiles == 124);
    {
        int a, b;
        CHECK(mj_hint(&g, &a, &b));
        char m[16];
        snprintf(m, sizeof m, "%d-%d", a, b);
        CHECK(mj_script(&g, m, rnd) && g.tiles == 122);
        CHECK(!mj_script(&g, m, rnd));
        CHECK(!mj_script(&g, "3-", rnd) && !mj_script(&g, "x", rnd));
    }

    /* no pair left: a shuffle deals the tiles left again (still clearable); undo takes the shuffle back */
    int stuck_seen = 0, saved = 0;
    for (unsigned seed = 1; seed <= 400 && !stuck_seen; seed++) {
        rng = seed;
        if (!mj_deal(&g, 0, rnd)) continue;
        CHECK(mj_script(&g, "+*", rnd) && mj_status(&g) != MJ_PLAY);   /* the first pair found, blindly */
        if (mj_status(&g) == MJ_LOST) CHECK(!mj_shuffle(&g, rnd) && mj_pairs(&g) == 0);
        if (mj_status(&g) != MJ_STUCK) continue;
        stuck_seen = 1;
        CHECK(mj_pairs(&g) == 0);
        int left = g.tiles, nh = g.nh;
        uint8_t before[MJ_MAX];
        memcpy(before, g.face, sizeof before);
        CHECK(mj_shuffle(&g, rnd));
        CHECK(g.tiles == left && g.nh == nh + 1);
        for (int i = 0; i < g.n; i++) CHECK((before[i] == MJ_NONE) == (g.face[i] == MJ_NONE));   /* same places */
        int cb[MJ_FACES] = {0}, ca[MJ_FACES] = {0};
        for (int i = 0; i < g.n; i++) {
            if (before[i] != MJ_NONE) cb[before[i]]++;
            if (g.face[i] != MJ_NONE) ca[g.face[i]]++;
        }
        for (int f = 0; f < MJ_FLOWERS; f++) CHECK(ca[f] == cb[f]);   /* the same tiles */
        h = g;                                  /* the new deal clears by its own order */
        int ok = 1;
        for (int k = 0; k + 1 < h.nsol && ok; k += 2) ok = mj_take(&h, h.sol[k], h.sol[k + 1]);
        CHECK(ok && mj_status(&h) == MJ_WON);
        saved = 1;
        CHECK(mj_undo(&g) && !memcmp(before, g.face, sizeof before) && mj_status(&g) != MJ_PLAY);
    }
    CHECK(stuck_seen && saved);

    /* the end that no shuffle can save: two tiles left, one on the other */
    rng = 3;
    CHECK(mj_deal(&g, 0, rnd));
    {
        int a = find(&g, 13, 7, 4), b = find(&g, 12, 6, 3);
        for (int i = 0; i < g.n; i++)
            if (i != a && i != b) g.face[i] = MJ_NONE;
        g.face[a] = g.face[b] = MJ_DOTS;
        g.tiles = 2;
        g.nh = 0;
        CHECK(mj_pairs(&g) == 0);
        g.face[a] = MJ_NONE;                    /* refresh through a move: put the top back by undo */
        g.tiles = 1;
        memcpy(g.hist[0], g.face, MJ_MAX);
        g.hist[0][a] = MJ_DOTS;
        g.nh = 1;
        CHECK(mj_undo(&g) && g.tiles == 2);
        CHECK(mj_status(&g) == MJ_LOST && !mj_shuffle(&g, rnd));
    }

    /* the win */
    rng = 11;
    CHECK(mj_deal(&g, 1, rnd));
    CHECK(mj_script(&g, "=40", rnd) && g.tiles == 0 && mj_status(&g) == MJ_WON);
    CHECK(!mj_script(&g, "+", rnd));

    if (fails) { printf("mahjong: %d failed\n", fails); return 1; }
    printf("mahjong: all passed\n");
    return 0;
}
