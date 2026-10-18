/*
 * Twist Five: the rules' test (the quarters and their turns, rows of five after the marble or after the twist, five for
 * both at once, the full board, the notation, undo, whole random games). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/twistfive.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static tvgame G;

static tvpos *pos(void) { return &G.pos[G.ply]; }
static int h(const char *s) { return tv_square(s); }

/* a position: player 0's marbles, player 1's ("a1 b2"), who is to twist (stage TV_TWIST) */
static void setup(const char *mine, const char *theirs, int side)
{
    tv_new(&G);
    tvpos *p = pos();
    const char *list[2] = {mine, theirs};
    for (int k = 0; k < 2; k++)
        for (const char *t = list[k]; *t;) {
            int c = tv_square(t);
            if (c >= 0) p->cell[c] = (uint8_t)(k + 1), p->count++;
            while (*t && *t != ' ') t++;
            while (*t == ' ') t++;
        }
    p->side = (uint8_t)side;
    p->stage = TV_TWIST;
}

static void test_quarters(void)
{
    CHECK(h("a1") == 0 && h("f6") == 35 && h("g1") < 0 && h("a7") < 0);
    CHECK(tv_quarter_of(h("a6")) == 0 && tv_quarter_of(h("c4")) == 0 && tv_quarter_of(h("d4")) == 1);
    CHECK(tv_quarter_of(h("f6")) == 1 && tv_quarter_of(h("a1")) == 2 && tv_quarter_of(h("c3")) == 2);
    CHECK(tv_quarter_of(h("d3")) == 3 && tv_quarter_of(h("f1")) == 3);
    /* clockwise: a corner goes to the next corner round, an edge to the next edge, the middle stays */
    CHECK(tv_turned(h("a6"), 0, TV_CW) == h("c6") && tv_turned(h("c6"), 0, TV_CW) == h("c4"));
    CHECK(tv_turned(h("c4"), 0, TV_CW) == h("a4") && tv_turned(h("a4"), 0, TV_CW) == h("a6"));
    CHECK(tv_turned(h("b6"), 0, TV_CW) == h("c5") && tv_turned(h("b5"), 0, TV_CW) == h("b5"));
    CHECK(tv_turned(h("a6"), 0, TV_CCW) == h("a4") && tv_turned(h("d3"), 3, TV_CW) == h("f3"));
    CHECK(tv_turned(h("f1"), 3, TV_CCW) == h("f3") && tv_turned(h("a1"), 3, TV_CW) == h("a1"));
    CHECK(tv_turned(h("d6"), 1, TV_CW) == h("f6") && tv_turned(h("a1"), 2, TV_CW) == h("a3"));
    /* four turns, or one each way: the same board */
    srand(3);
    for (int k = 0; k < 50; k++) {
        uint8_t c[TV_CELLS], d[TV_CELLS];
        for (int i = 0; i < TV_CELLS; i++) c[i] = (uint8_t)(rand() % 3);
        memcpy(d, c, sizeof d);
        int q = rand() % 4;
        for (int t = 0; t < 4; t++) tv_rotate(d, q, TV_CW);
        CHECK(!memcmp(c, d, sizeof c));
        tv_rotate(d, q, TV_CW);
        tv_rotate(d, q, TV_CCW);
        CHECK(!memcmp(c, d, sizeof c));
        tv_rotate(d, q, TV_CCW);
        for (int i = 0; i < TV_CELLS; i++)      /* the other quarters untouched */
            if (tv_quarter_of(i) != q) CHECK(d[i] == c[i]);
    }
    CHECK(TV_LINES == 32);
}

static void test_wins(void)
{
    /* five after the marble: won at once, no twist */
    tv_new(&G);
    CHECK(tv_script(&G, "a1/4+ d6/4+ a2/4+ e6/4+ a3/4+ f5/4+ a4/4+ d4/4+"));
    CHECK(pos()->winner == -1 && pos()->stage == TV_PLACE && pos()->side == 0 && pos()->count == 8);
    CHECK(tv_script(&G, "a5") && pos()->winner == 0 && pos()->count == 9);
    CHECK(!tv_play_twist(&G, 3, TV_CW) && !tv_play_place(&G, h("b1")));
    /* five made by the twist */
    setup("a1 a2 a3 b4 c4", "e5 f5", 0);
    tv_twist(pos(), 0, TV_CW);                  /* c4 to a4, b4 to a5 */
    CHECK(pos()->winner == 0);
    setup("a1 a2 a3 b4 c4", "e5 f5", 0);
    tv_twist(pos(), 0, TV_CCW);                 /* the other way: nothing */
    CHECK(pos()->winner == -1 && pos()->side == 1 && pos()->stage == TV_PLACE);
    /* five for both at once: a draw */
    setup("a1 a2 a3 b4 c4", "a4 a5 a6 d6 e6", 0);
    tv_twist(pos(), 0, TV_CW);                  /* ivory a1..a5, ebony a6..e6 */
    CHECK(pos()->winner == TV_DRAW);
    /* the twist makes five for the other player only: the other player wins */
    setup("a1 a2 a3 b4", "a4 a5 a6 d6 e6", 0);
    tv_twist(pos(), 0, TV_CW);
    CHECK(pos()->winner == 1);
    /* five of the other's already there before the marble? it can only come from a twist: the twist decides; a
     * diagonal and a column across two quarters */
    setup("b2 c3 d4 e5", "", 1);
    pos()->stage = TV_PLACE;
    pos()->side = 0;
    CHECK(tv_play_place(&G, h("f6")) && pos()->winner == 0);
    uint8_t mark[TV_CELLS];
    CHECK(tv_marks(pos()->cell, mark) == 5 && mark[h("b2")] && mark[h("f6")] && !mark[h("a1")]);
}

/* the full board after a twist with no five: a draw */
static void test_full(void)
{
    srand(11);
    int found = 0;
    for (int k = 0; k < 200000 && !found; k++) {
        uint8_t c[TV_CELLS];
        int n0 = 0;
        for (int i = 0; i < TV_CELLS; i++) c[i] = (uint8_t)(1 + rand() % 2), n0 += c[i] == 1;
        if (n0 != 18 || tv_five(c, 0) || tv_five(c, 1)) continue;
        uint8_t d[TV_CELLS];
        memcpy(d, c, sizeof d);
        tv_rotate(d, 2, TV_CW);
        if (tv_five(d, 0) || tv_five(d, 1)) continue;
        int last = -1;                          /* ebony's last marble (ebony places the 36th) */
        for (int i = 0; i < TV_CELLS && last < 0; i++)
            if (c[i] == 2) last = i;
        tv_new(&G);
        memcpy(pos()->cell, c, sizeof c);
        pos()->cell[last] = 0;
        pos()->count = 35;
        pos()->side = 1;
        CHECK(tv_play_place(&G, last) && pos()->winner == -1 && pos()->stage == TV_TWIST);
        CHECK(tv_play_twist(&G, 2, TV_CW) && pos()->winner == TV_DRAW);
        found = 1;
    }
    CHECK(found);
}

static void test_misc(void)
{
    tv_new(&G);
    CHECK(!tv_script(&G, "/1+"));               /* a twist before a marble */
    CHECK(!tv_script(&G, "c4/2+ c4"));          /* c4 is quarter 1's: still taken after quarter 2 turned */
    CHECK(G.ply == 2 && pos()->stage == TV_PLACE);
    tv_new(&G);
    CHECK(tv_script(&G, "c4/1+ c4"));           /* turned away to a4: c4 empty again */
    CHECK(pos()->cell[h("a4")] == 1 && pos()->cell[h("c4")] == 2 && pos()->stage == TV_TWIST);
    CHECK(!tv_script(&G, "d1") && !tv_script(&G, "/5+") && !tv_script(&G, "/1*"));
    CHECK(tv_undo(&G) && pos()->stage == TV_PLACE && pos()->side == 1 && pos()->cell[h("c4")] == 0);
    CHECK(tv_undo(&G) && pos()->cell[h("a4")] == 0 && pos()->cell[h("c4")] == 1 && pos()->stage == TV_TWIST);
    while (tv_undo(&G)) {}
    CHECK(G.ply == 0 && pos()->count == 0);
}

/* whole random games: every step legal, the marbles counted, an end, the winner's row there */
static void test_games(void)
{
    srand(5);
    int ends[3] = {0, 0, 0};
    for (int game = 0; game < 2000; game++) {
        tv_new(&G);
        while (pos()->winner < 0) {
            if (pos()->stage == TV_PLACE) {
                int h0 = rand() % TV_CELLS;
                while (pos()->cell[h0]) h0 = (h0 + 1) % TV_CELLS;
                if (!tv_play_place(&G, h0)) { CHECK(0); break; }
            } else if (!tv_play_twist(&G, rand() % 4, rand() % 2)) { CHECK(0); break; }
            int n = 0;
            for (int i = 0; i < TV_CELLS; i++) n += pos()->cell[i] != 0;
            CHECK(n == pos()->count);
        }
        int w = pos()->winner;
        ends[w]++;
        CHECK(G.ply <= TV_MAX_PLY);
        if (w < 2) CHECK(tv_five(pos()->cell, w) && !tv_five(pos()->cell, !w));
        if (w == 2) CHECK((tv_five(pos()->cell, 0) && tv_five(pos()->cell, 1)) || pos()->count == TV_CELLS);
    }
    CHECK(ends[0] > 200 && ends[1] > 200 && ends[2] > 0);
    printf("random games: ivory %d, ebony %d, draws %d\n", ends[0], ends[1], ends[2]);
}

int main(void)
{
    test_quarters();
    test_wins();
    test_full();
    test_misc();
    test_games();
    printf("twistfive: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
