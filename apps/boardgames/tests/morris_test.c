/*
 * Nine men's morris: the rules' test (mills, removals, flying, losses, draws, undo). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/morris.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static mgame G;
static mmove L[MORRIS_MAX_MOVES];

/* a position of the moving phase: the ivory and ebony pieces ("a1 d1"), all placed */
static void setup(const char *ivory, const char *ebony, int side)
{
    morris_new(&G);
    mpos *p = &G.pos[0];
    p->hand[0] = p->hand[1] = 0;
    const char *s[2] = {ivory, ebony};
    for (int c = 0; c < 2; c++)
        for (const char *t = s[c]; *t; t++) {
            int pt = morris_point(t);
            if (pt < 0) continue;
            p->at[pt] = (int8_t)c;
            p->on[c]++;
            t++;
        }
    p->side = (uint8_t)side;
}

static const mpos *pos(void) { return morris_pos(&G); }
static int nlegal(void) { return morris_legal(pos(), L); }
static int at(const char *pt) { return pos()->at[morris_point(pt)]; }

/* the board: every point on two lines; 32 lines between neighbours (a lone piece moves to its neighbours) */
static void test_board(void)
{
    int on[MORRIS_POINTS] = {0}, moves = 0, ok = 1;
    for (int i = 0; i < MORRIS_MILLS; i++)
        for (int j = 0; j < 3; j++) on[morris_mill[i][j]]++;
    for (int i = 0; i < MORRIS_POINTS; i++) ok &= on[i] == 2;
    CHECK(ok);
    for (int i = 0; i < MORRIS_POINTS; i++) {
        char name[3] = {(char)('a' + morris_x[i]), (char)('1' + morris_y[i]), 0};
        CHECK(morris_point(name) == i);
        setup(name, "", 0);
        int n = nlegal();
        CHECK(n >= 2 && n <= 4);
        moves += n;
    }
    CHECK(moves == 64);
    morris_new(&G);
    CHECK(nlegal() == 24 && pos()->side == 0 && pos()->hand[0] == 9 && pos()->hand[1] == 9);
}

/* a mill while placing: one removal, then the other player */
static void test_placing_mill(void)
{
    morris_new(&G);
    CHECK(morris_script(&G, "a1 d6 d1 f4 g1"));
    CHECK(pos()->take && pos()->side == 0 && nlegal() == 2 && L[0].type == M_TAKE);
    CHECK(morris_status(&G) == MS_PLAY);
    CHECK(morris_script(&G, "xd6"));
    CHECK(!pos()->take && pos()->side == 1 && pos()->on[1] == 1 && pos()->hand[1] == 7 && at("d6") < 0);
    morris_new(&G);                             /* two mills at once: still one removal */
    CHECK(morris_script(&G, "d1 b2 g1 f2 a4 c3 a7 e3 a1"));
    CHECK(pos()->take && pos()->side == 0);
    CHECK(morris_script(&G, "xb2") && !pos()->take && pos()->side == 1);
    CHECK(morris_script(&G, "d2xa1") == 0);     /* b2 d2 f2 is no mill now: nothing to remove */
}

/* the removal: not from a mill, unless every piece is in one */
static void test_removal(void)
{
    morris_new(&G);
    CHECK(morris_script(&G, "d2 c5 b4 d5 a1 e5"));
    CHECK(pos()->take && pos()->side == 1 && nlegal() == 3);
    CHECK(morris_script(&G, "xd2 d1 f4 g1"));
    CHECK(pos()->take && nlegal() == 1 && L[0].from == morris_point("f4"));
    CHECK(!morris_script(&G, "xc5"));
    setup("a1 d1 g4 b2", "a7 d7 g7", 0);        /* every ebony piece in a mill: any of them */
    CHECK(morris_script(&G, "g4-g1") && pos()->take && nlegal() == 3);
    CHECK(morris_script(&G, "xd7") && morris_status(&G) == MS_FEW);   /* and ebony, down to 2, lost */
}

/* moving: along the lines, a mill by moving, and flying with three */
static void test_moving(void)
{
    morris_new(&G);
    CHECK(morris_script(&G, "a1 d1 g1 b2 d2 f2 b4 a4 f4 g4 c5 d3 e3 d5 d7 f6 b6 a7"));
    CHECK(pos()->hand[0] == 0 && pos()->hand[1] == 0 && !pos()->take && pos()->side == 0);
    CHECK(!morris_script(&G, "e3-e5") && !morris_script(&G, "d1-e1") && !morris_script(&G, "d1-d2"));
    CHECK(!morris_script(&G, "c4"));            /* nothing left to place */
    CHECK(morris_script(&G, "e3-e4") && pos()->side == 1 && pos()->quiet == 1);
    setup("a1 d1 g4 b2 c3", "a7 d7 f6 e5", 0);
    G.pos[0].quiet = 40;
    CHECK(!morris_flying(pos(), 0) && morris_script(&G, "g4g1") && pos()->take && pos()->quiet == 0);
    setup("a1 d1 c4", "a7 d7 g7 f4", 0);
    CHECK(morris_flying(pos(), 0) && !morris_flying(pos(), 1));
    CHECK(nlegal() == 3 * 17);
    CHECK(morris_script(&G, "c4-g1") && pos()->take);
}

/* losing: fewer than three, or no move */
static void test_losses(void)
{
    setup("a4 d2 g4 b4", "a1 d1 g1 b2", 1);     /* ebony walled in */
    CHECK(morris_status(&G) == MS_BLOCKED);
    G.pos[0].side = 0;
    CHECK(morris_status(&G) == MS_PLAY);
    setup("a1 d1 g4 b2", "a7 d7 f6", 0);
    CHECK(morris_script(&G, "g4-g1 xf6") && pos()->side == 1 && morris_status(&G) == MS_FEW);
    morris_new(&G);                             /* while placing, pieces in hand count */
    CHECK(morris_script(&G, "a1 d6 d1 f4 g1 xd6") && morris_status(&G) == MS_PLAY);
}

/* draws: the third time the same position; 50 moves each without a mill */
static void test_draws(void)
{
    setup("a1 d2 f4 c5", "g7 b6 e3 d7", 0);
    CHECK(morris_script(&G, "a1-a4 g7-g4 a4-a1 g4-g7") && morris_status(&G) == MS_PLAY);
    CHECK(morris_script(&G, "a1-a4 g7-g4 a4-a1") && morris_status(&G) == MS_PLAY);
    CHECK(morris_script(&G, "g4-g7") && morris_status(&G) == MS_REPETITION);
    setup("a1 d2 f4 c5", "g7 b6 e3 d7", 0);
    G.pos[0].quiet = MORRIS_QUIET - 2;
    CHECK(morris_script(&G, "a1-a4") && morris_status(&G) == MS_PLAY);
    CHECK(morris_script(&G, "g7-g4") && morris_status(&G) == MS_QUIET);
    setup("a1 d1 g4 b2 c3", "a7 d7 f6 e5", 0);  /* a mill starts the count again */
    G.pos[0].quiet = MORRIS_QUIET - 1;
    CHECK(morris_script(&G, "g4-g1 xf6") && pos()->quiet == 0 && morris_status(&G) == MS_PLAY);
}

/* undo: one action at a time, through a removal, back to the start */
static void test_undo(void)
{
    morris_new(&G);
    mpos start = *pos();
    CHECK(morris_script(&G, "a1 d6 d1 f4 g1 xd6"));
    CHECK(morris_undo(&G) && pos()->take && at("d6") == 1 && pos()->on[1] == 2 && pos()->side == 0);
    CHECK(morris_undo(&G) && !pos()->take && at("g1") < 0 && pos()->hand[0] == 7 && pos()->side == 0);
    while (morris_undo(&G)) {}
    CHECK(G.ply == 0 && !memcmp(pos(), &start, sizeof start) && !morris_undo(&G));
    CHECK(!morris_script(&G, "a1 a1") && !morris_script(&G, "xa1") && !morris_script(&G, "h1"));
}

int main(void)
{
    test_board();
    test_placing_mill();
    test_removal();
    test_moving();
    test_losses();
    test_draws();
    test_undo();
    printf("morris: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
