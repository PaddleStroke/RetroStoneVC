/*
 * The men's morris games: the rules' test (the three boards, mills, removals, flying, losses, draws, undo; twelve men's
 * diagonals and full board, six men's lines between the squares and no flying). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/morris.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static mgame G;
static mmove L[MORRIS_MAX_MOVES];

static const mpos *pos(void) { return morris_pos(&G); }
static int nlegal(void) { return morris_legal(pos(), L); }
static int at(const char *pt) { return pos()->at[morris_point(pos()->variant, pt)]; }

/* a position of the moving phase on a variant's board: the ivory and ebony pieces ("a1 d1"), all placed */
static void setup_v(int variant, const char *ivory, const char *ebony, int side)
{
    morris_new(&G, variant);
    mpos *p = &G.pos[0];
    p->hand[0] = p->hand[1] = 0;
    const char *s[2] = {ivory, ebony};
    for (int c = 0; c < 2; c++)
        for (const char *t = s[c]; *t; t++) {
            int pt = morris_point(variant, t);
            if (pt < 0) continue;
            p->at[pt] = (int8_t)c;
            p->on[c]++;
            t++;
        }
    p->side = (uint8_t)side;
}

static void setup(const char *ivory, const char *ebony, int side) { setup_v(MORRIS_NINE, ivory, ebony, side); }

/* the boards: the lines through each point, the neighbours (a lone piece moves to them): nine men's every point on
 * two mills, 32 lines between neighbours; twelve men's 4 diagonals more (12 points on three lines); six men's 8 mills
 * and 4 lines of two, every point on two lines */
static void test_board(void)
{
    static const int lines3[MORRIS_VARIANTS] = {0, 12, 0}, all_moves[MORRIS_VARIANTS] = {64, 80, 40},
                     mills[MORRIS_VARIANTS] = {16, 20, 8};
    for (int v = 0; v < MORRIS_VARIANTS; v++) {
        const mboard *b = morris_board(v);
        int on[MORRIS_POINTS] = {0}, moves = 0, three = 0, ok = 1, nmills = 0;
        for (int i = 0; i < b->lines; i++) {
            nmills += b->line[i][2] >= 0;
            for (int j = 0; j < 3; j++)
                if (b->line[i][j] >= 0) on[b->line[i][j]]++;
        }
        for (int i = 0; i < b->points; i++) {
            ok &= on[i] == 2 || on[i] == 3;
            three += on[i] == 3;
        }
        CHECK(ok && three == lines3[v] && nmills == mills[v]);
        for (int i = 0; i < b->points; i++) {
            char name[3] = {(char)('a' + b->x[i]), (char)('1' + b->y[i]), 0};
            CHECK(morris_point(v, name) == i);
            setup_v(v, name, "", 0);
            int n = nlegal();
            CHECK(n >= 2 && n <= 4);
            moves += n;
        }
        CHECK(moves == all_moves[v]);
        morris_new(&G, v);
        CHECK(nlegal() == b->points && pos()->side == 0 && pos()->hand[0] == b->pieces &&
              pos()->hand[1] == b->pieces);
    }
    CHECK(morris_board(MORRIS_NINE)->pieces == 9 && morris_board(MORRIS_TWELVE)->pieces == 12 &&
          morris_board(MORRIS_SIX)->pieces == 6 && morris_board(MORRIS_SIX)->points == 16);
    CHECK(morris_point(MORRIS_SIX, "c3") < 0 && morris_point(MORRIS_SIX, "f2") < 0 &&
          morris_point(MORRIS_SIX, "c1") == 1);
}

/* a mill while placing: one removal, then the other player */
static void test_placing_mill(void)
{
    morris_new(&G, MORRIS_NINE);
    CHECK(morris_script(&G, "a1 d6 d1 f4 g1"));
    CHECK(pos()->take && pos()->side == 0 && nlegal() == 2 && L[0].type == M_TAKE);
    CHECK(morris_status(&G) == MS_PLAY);
    CHECK(morris_script(&G, "xd6"));
    CHECK(!pos()->take && pos()->side == 1 && pos()->on[1] == 1 && pos()->hand[1] == 7 && at("d6") < 0);
    morris_new(&G, MORRIS_NINE);                             /* two mills at once: still one removal */
    CHECK(morris_script(&G, "d1 b2 g1 f2 a4 c3 a7 e3 a1"));
    CHECK(pos()->take && pos()->side == 0);
    CHECK(morris_script(&G, "xb2") && !pos()->take && pos()->side == 1);
    CHECK(morris_script(&G, "d2xa1") == 0);     /* b2 d2 f2 is no mill now: nothing to remove */
}

/* the removal: not from a mill, unless every piece is in one */
static void test_removal(void)
{
    morris_new(&G, MORRIS_NINE);
    CHECK(morris_script(&G, "d2 c5 b4 d5 a1 e5"));
    CHECK(pos()->take && pos()->side == 1 && nlegal() == 3);
    CHECK(morris_script(&G, "xd2 d1 f4 g1"));
    CHECK(pos()->take && nlegal() == 1 && L[0].from == morris_point(pos()->variant, "f4"));
    CHECK(!morris_script(&G, "xc5"));
    setup("a1 d1 g4 b2", "a7 d7 g7", 0);        /* every ebony piece in a mill: any of them */
    CHECK(morris_script(&G, "g4-g1") && pos()->take && nlegal() == 3);
    CHECK(morris_script(&G, "xd7") && morris_status(&G) == MS_FEW);   /* and ebony, down to 2, lost */
}

/* moving: along the lines, a mill by moving, and flying with three */
static void test_moving(void)
{
    morris_new(&G, MORRIS_NINE);
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
    morris_new(&G, MORRIS_NINE);                             /* while placing, pieces in hand count */
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
    morris_new(&G, MORRIS_NINE);
    mpos start = *pos();
    CHECK(morris_script(&G, "a1 d6 d1 f4 g1 xd6"));
    CHECK(morris_undo(&G) && pos()->take && at("d6") == 1 && pos()->on[1] == 2 && pos()->side == 0);
    CHECK(morris_undo(&G) && !pos()->take && at("g1") < 0 && pos()->hand[0] == 7 && pos()->side == 0);
    while (morris_undo(&G)) {}
    CHECK(G.ply == 0 && !memcmp(pos(), &start, sizeof start) && !morris_undo(&G));
    CHECK(!morris_script(&G, "a1 a1") && !morris_script(&G, "xa1") && !morris_script(&G, "h1"));
}


/* twelve men's: the diagonals make mills and lines to move along; a board filled while placing is a draw */
static int colour[MORRIS_POINTS];

static int no_mill_fill(const mboard *b, int i, int left0, int left1)   /* backtracking: 12 each, no line alike */
{
    if (i == b->points) return 1;
    for (int c = 0; c < 2; c++) {
        if (!(c ? left1 : left0)) continue;
        colour[i] = c;
        int ok = 1;
        for (int k = 0; k < b->lines && ok; k++) {
            const int8_t *l = b->line[k];
            if (l[2] < 0 || (l[0] != i && l[1] != i && l[2] != i) || l[0] > i || l[1] > i || l[2] > i) continue;
            ok = !(colour[l[0]] == colour[l[1]] && colour[l[1]] == colour[l[2]]);
        }
        if (ok && no_mill_fill(b, i + 1, left0 - !c, left1 - c)) return 1;
    }
    return 0;
}

static void test_twelve(void)
{
    morris_new(&G, MORRIS_TWELVE);
    CHECK(morris_script(&G, "a1 d6 b2 f4 c3") && pos()->take && pos()->side == 0);
    CHECK(morris_script(&G, "xd6") && pos()->hand[0] == 9 && pos()->hand[1] == 10);
    setup_v(MORRIS_TWELVE, "b2", "", 0);
    CHECK(nlegal() == 4 && morris_script(&G, "b2-c3"));
    setup_v(MORRIS_TWELVE, "a7 b6 c4 d2", "g1 f2 a4 d7", 0);
    CHECK(morris_script(&G, "c4-c5") && pos()->take);   /* a7 b6 c5: a diagonal mill by moving */
    setup_v(MORRIS_TWELVE, "a1 d1 c4", "a7 d7 g7 f4", 0);
    CHECK(morris_flying(pos(), 0) && nlegal() == 3 * 17);   /* twelve men's flies too */

    const mboard *b = morris_board(MORRIS_TWELVE);
    CHECK(no_mill_fill(b, 0, 12, 12));
    morris_new(&G, MORRIS_TWELVE);
    int k[2] = {0, 0}, ok = 1;
    for (int ply = 0; ply < 24 && ok; ply++) {
        int c = ply & 1;
        while (colour[k[c]] != c) k[c]++;
        mmove m = {M_PLACE, -1, (int8_t)k[c]++};
        ok = morris_play(&G, m) && (ply == 23 || morris_status(&G) == MS_PLAY);
    }
    CHECK(ok && pos()->hand[0] == 0 && pos()->hand[1] == 0 && morris_status(&G) == MS_FULL);
    CHECK(morris_undo(&G) && morris_status(&G) == MS_PLAY && nlegal() == 1);
}

/* six men's: two squares, lines of two between them, six pieces, no flying */
static void test_six(void)
{
    setup_v(MORRIS_SIX, "c1", "", 0);
    CHECK(nlegal() == 3 && morris_script(&G, "c1-c2"));
    setup_v(MORRIS_SIX, "a3", "", 0);
    CHECK(nlegal() == 3 && morris_script(&G, "a3-b3") && !morris_script(&G, "b3-d3"));
    setup_v(MORRIS_SIX, "a1 c1 c4", "a5 e5 e1 d4", 0);
    CHECK(!morris_flying(pos(), 0) && nlegal() == 4);   /* a1-a3 c1-c2 c4-b4 c4-c5 */
    setup_v(MORRIS_SIX, "a1 c1 e3", "a5 c5 b4", 0);
    CHECK(morris_script(&G, "e3-e1") && pos()->take && morris_script(&G, "xb4") && morris_status(&G) == MS_FEW);
    morris_new(&G, MORRIS_SIX);
    CHECK(morris_script(&G, "a1 a5 c1 c5 b2 d4 d2 b4 a3 e3 d3 e1"));
    CHECK(pos()->hand[0] == 0 && pos()->hand[1] == 0 && !pos()->take && morris_status(&G) == MS_PLAY);
    CHECK(!morris_script(&G, "g7") && !morris_script(&G, "d1"));
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
    test_twelve();
    test_six();
    printf("morris: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
