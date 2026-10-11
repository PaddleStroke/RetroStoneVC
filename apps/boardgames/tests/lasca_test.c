/*
 * Lasca: the rules' test (the board, the columns and their order, the top piece taken, soldiers and officers, captures
 * compulsory and their chains, no square jumped twice, the crowning ending a chain, no move: lost; the draws, undo,
 * a whole game keeping every piece). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/lasca.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static lsgame G;
static lsmove L[LS_MAX_MOVES];

static void setup(const char *columns, int side)
{
    ls_new(&G);
    if (!ls_setup(&G.pos[0], side, columns)) printf("bad setup %s\n", columns);
}

static const lspos *pos(void) { return ls_pos(&G); }
static int nlegal(void) { return ls_legal(pos(), L); }
static lscol col(const char *sq) { return pos()->col[ls_square(sq)]; }

/* the column on a square, written from the top as ls_setup does ("wB"; "" empty) */
static int is(const char *sq, const char *want)
{
    lscol c = col(sq);
    if (c.h != strlen(want)) return 0;
    for (int i = 0; i < c.h; i++)
        if ("wbWB"[ls_piece(c, i)] != want[i]) return 0;
    return 1;
}

static int all_captures(void)
{
    int n = nlegal();
    for (int i = 0; i < n; i++)
        if (L[i].over < 0) return 0;
    return n > 0;
}

/* the board: 25 squares (the corners among them), 11 soldiers each, the middle row empty */
static void test_board(void)
{
    int used = 0;
    for (int s = 0; s < LS_SQUARES; s++) used += ls_used(s);
    CHECK(used == 25 && ls_used(0) && ls_used(6) && ls_used(42) && ls_used(48) && !ls_used(1));
    CHECK(ls_square("a1") == 0 && ls_square("g7") == 48 && ls_square("b1") < 0 && ls_square("h1") < 0);
    ls_new(&G);
    CHECK(pos()->side == 0 && pos()->chain < 0);
    CHECK(ls_columns(pos(), 0) == 11 && ls_columns(pos(), 1) == 11 && ls_prisoners(pos(), 0) == 0);
    CHECK(is("a1", "w") && is("c3", "w") && is("g3", "w") && is("b4", "") && is("d4", "") && is("a5", "b"));
    CHECK(is("g7", "b") && is("b6", "b"));
    CHECK(nlegal() == 6);                       /* a3, c3, e3, g3 into the middle row */
    CHECK(ls_status(&G) == LS_PLAY);
}

/* the columns: the top piece taken goes under the capturing column; what is beneath stays */
static void test_stacking(void)
{
    setup("c3:w d4:b", 0);
    CHECK(ls_script(&G, "c3xe5") && is("e5", "wb") && is("d4", "") && is("c3", ""));
    CHECK(ls_prisoners(pos(), 0) == 1 && ls_columns(pos(), 1) == 0);
    setup("c3:wB d4:bW g7:b", 0);               /* a column taking from a column: only its top piece, to the bottom */
    CHECK(ls_script(&G, "c3xe5") && is("e5", "wBb") && is("d4", "W"));
    CHECK(ls_columns(pos(), 0) == 2 && ls_prisoners(pos(), 0) == 2 && ls_prisoners(pos(), 1) == 0);
    setup("c3:w d4:bww g7:b", 0);               /* the jumped column freed: commanded now by player 0 */
    CHECK(ls_script(&G, "c3xe5") && is("d4", "ww") && is("e5", "wb") && ls_columns(pos(), 0) == 2);
    setup("e5:b d4:wbw a1:w", 1);               /* player 1 too, downwards */
    CHECK(ls_script(&G, "e5xc3") && is("c3", "bw") && is("d4", "bw"));
    setup("c3:wbbbbbbbbbb d4:bwwwwwwwwww", 0);  /* tall columns: 11 and 11, then 12 and 10 */
    CHECK(ls_script(&G, "c3xe5") && col("e5").h == 12 && col("d4").h == 10 && is("e5", "wbbbbbbbbbbb"));
}

/* soldiers forwards only, officers both ways; a column moves as its top piece */
static void test_moves(void)
{
    setup("d4:w g7:b", 0);
    CHECK(nlegal() == 2 && !ls_script(&G, "d4-c3") && ls_script(&G, "d4-e5"));
    setup("d4:b a1:w", 1);
    CHECK(nlegal() == 2 && !ls_script(&G, "d4-e5") && ls_script(&G, "d4-c3"));
    setup("d4:W g7:b", 0);
    CHECK(nlegal() == 4 && ls_script(&G, "d4-c3"));
    setup("d4:Wb g7:b", 0);                     /* an officer on top: the column moves as an officer */
    CHECK(nlegal() == 4);
    setup("d4:wB g7:b", 0);                     /* a soldier on top of an officer: as a soldier */
    CHECK(nlegal() == 2);
    setup("a1:w g7:b", 0);                      /* the edge */
    CHECK(nlegal() == 1);
    setup("d4:w c3:b g7:b", 0);                 /* a soldier does not capture backwards */
    CHECK(nlegal() == 2 && !all_captures());
    setup("d4:W c3:b g7:b", 0);                 /* an officer does */
    CHECK(all_captures() && nlegal() == 1 && ls_script(&G, "d4xb2") && is("b2", "Wb"));
    setup("c3:w d4:w g7:b", 0);                 /* nor over one's own column, nor onto a column */
    CHECK(!all_captures());
    setup("c3:w d4:b e5:w g7:b", 0);
    CHECK(!all_captures());
}

/* captures are compulsory; a chain goes on, the same column only, a jump at a time, never over a square twice */
static void test_chains(void)
{
    setup("a1:w g1:w b2:b d4:b a7:b", 0);       /* a1 must take; then on from c3 over d4 */
    CHECK(all_captures() && nlegal() == 1);
    CHECK(!ls_script(&G, "g1-f2") && ls_script(&G, "a1xc3"));
    CHECK(pos()->side == 0 && pos()->chain == ls_square("c3") && ls_status(&G) == LS_PLAY);
    CHECK(nlegal() == 1 && !ls_script(&G, "g1-f2") && ls_script(&G, "c3xe5"));
    CHECK(pos()->side == 1 && pos()->chain < 0 && pos()->jumped == 0 && is("e5", "wbb"));
    setup("a1:w g1:w b2:b d4:b a7:b", 0);       /* written at once */
    CHECK(ls_script(&G, "a1xc3xe5") && G.ply == 2 && pos()->side == 1);
    setup("a1:W b2:bb g7:b", 0);                /* b2 still ebony on top after the jump: not jumped back over */
    CHECK(ls_script(&G, "a1xc3") && pos()->side == 1 && is("b2", "b") && is("c3", "Wb"));
    setup("c3:W d4:bb f4:b f2:b g7:b", 0);      /* over d4, f4, f2: an officer turning, d4 never twice */
    CHECK(ls_script(&G, "c3xe5xg3xe1") && pos()->side == 1 && is("e1", "Wbbb") && is("d4", "b"));
    setup("b2:W c3:b e3:b c5:b e5:b a7:b", 0);  /* a ring of four: on from the middle, any way */
    CHECK(ls_script(&G, "b2xd4") && pos()->chain == ls_square("d4"));
    CHECK(ls_script(&G, "d4xf6") || ls_script(&G, "d4xb6") || ls_script(&G, "d4xf2"));
}

/* the crowning: on the far row a soldier on top becomes an officer; in a capture that ends the move */
static void test_crowning(void)
{
    setup("b6:w g1:b", 0);
    CHECK(ls_script(&G, "b6-c7") && is("c7", "W") && pos()->side == 1);
    setup("b6:wb g1:b", 0);                     /* only the top piece */
    CHECK(ls_script(&G, "b6-a7") && is("a7", "Wb"));
    setup("b2:b a7:w", 1);
    CHECK(ls_script(&G, "b2-c1") && is("c1", "B"));
    setup("c5:w d6:b f6:b a1:b", 0);            /* c5xe7 crowns: f6 could be taken by the new officer, but not now */
    CHECK(ls_script(&G, "c5xe7") && pos()->side == 1 && pos()->chain < 0 && is("e7", "Wb") && is("f6", "b"));
    setup("c5:W d6:b f6:b a1:b", 0);            /* an officer on the far row goes on */
    CHECK(ls_script(&G, "c5xe7") && pos()->side == 0 && ls_script(&G, "e7xg5") && is("g5", "Wbb"));
}

/* the end: no move (no column, or every one blocked) loses; the draws */
static void test_ends(void)
{
    setup("a1:w b2:b c3:b", 0);                 /* hemmed in */
    CHECK(nlegal() == 0 && ls_status(&G) == LS_STUCK);
    setup("c3:bw g7:b", 0);                     /* every piece of player 0 a prisoner: no column */
    CHECK(ls_columns(pos(), 0) == 0 && ls_status(&G) == LS_STUCK);
    setup("c3:w d4:b", 0);                      /* the last column taken */
    CHECK(ls_script(&G, "c3xe5") && ls_status(&G) == LS_STUCK && pos()->side == 1);
    setup("a1:W g7:B", 0);                      /* the same position a third time */
    CHECK(ls_script(&G, "a1-b2 g7-f6 b2-a1 f6-g7 a1-b2 g7-f6 b2-a1"));
    CHECK(ls_status(&G) == LS_PLAY);
    CHECK(ls_script(&G, "f6-g7") && ls_status(&G) == LS_REPETITION);
    setup("a1:W g7:B", 0);                      /* 40 moves each without a capture or a soldier moving */
    G.pos[0].quiet = LS_QUIET - 1;
    CHECK(ls_status(&G) == LS_PLAY && ls_script(&G, "a1-b2") && ls_status(&G) == LS_QUIET_DRAW);
    setup("a1:w g7:B", 0);                      /* a soldier's move starts the count again */
    G.pos[0].quiet = LS_QUIET - 1;
    CHECK(ls_script(&G, "a1-b2") && pos()->quiet == 0 && ls_status(&G) == LS_PLAY);
}

/* a whole game: every piece stays on the board (22 in all), undo all the way back */
static void test_game(void)
{
    ls_new(&G);
    lspos start = *pos();
    CHECK(!ls_undo(&G));
    unsigned seed = 11;
    int ok = 1, plies = 0, tallest = 0;
    while (ls_status(&G) == LS_PLAY && plies < 1500) {
        int n = nlegal();
        seed = seed * 1103515245u + 12345u;
        ls_play(&G, L[(seed >> 16) % (unsigned)n]);
        plies++;
        int total = 0;
        for (int s = 0; s < LS_SQUARES; s++) {
            total += pos()->col[s].h;
            ok &= ls_used(s) || !pos()->col[s].h;
            if (pos()->col[s].h > tallest) tallest = pos()->col[s].h;
        }
        ok &= total == 2 * LS_PIECES;
    }
    CHECK(ok && tallest >= 3 && ls_status(&G) != LS_PLAY);
    int n = 0;
    while (ls_undo(&G)) n++;
    CHECK(n == plies && !memcmp(&pos()->col, &start.col, sizeof start.col) && pos()->side == 0);
    CHECK(!ls_script(&G, "b1-c2") && !ls_script(&G, "c3") && !ls_script(&G, "c3-c4"));
    CHECK(ls_script(&G, "c3-d4 e5xc3 b2xd4") && is("d4", "wb") && is("c3", "w") && pos()->side == 1);
}

int main(void)
{
    test_board();
    test_stacking();
    test_moves();
    test_chains();
    test_crowning();
    test_ends();
    test_game();
    printf("lasca: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
