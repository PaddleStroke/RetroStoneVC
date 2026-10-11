/*
 * Pachisi: the rules' test (the cowries' table and the graces, the board's paths and castles, leaving the charkoni,
 * captures and their extra throw, the castles, pieces sharing squares, the exact throw home, declining, the
 * partnership's win, undo, whole games). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/pachisi.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static pgame G;
static const ppos *P = &G.pos;

/* a position: every army's pieces (4 places each, 0 the charkoni), all armies out already, the player to throw */
static void setup(const int at[4][4], int turn)
{
    pa_new(&G, 2);
    for (int a = 0; a < PA_ARMIES; a++)
        for (int i = 0; i < PA_PIECES; i++) G.pos.at[a][i] = (uint8_t)at[a][i];
    G.pos.out = 0xf;
    G.pos.turn = (uint8_t)turn;
}

/* the cowries: mouths up 0..6 give 25, 10, 2, 3, 4, 5, 6; 6, 10 and 25 are graces */
static void test_table(void)
{
    static const int want[7] = {25, 10, 2, 3, 4, 5, 6};
    for (int m = 0; m <= 6; m++) {
        CHECK(pa_value(m) == want[m] && pa_mouths(want[m]) == m);
        CHECK(pa_grace(want[m]) == (m == 0 || m == 1 || m == 6));
    }
    CHECK(pa_mouths(1) < 0 && pa_mouths(7) < 0 && pa_mouths(0) < 0 && !pa_value(7));
}

/* the board: 96 squares; each army's path a chain of neighbouring cells from the charkoni round to it; 12 castles */
static void test_board(void)
{
    int used[19][19] = {{0}}, x, y, px = 9, py = 9;
    for (int s = 0; s < PA_SQUARES; s++) {
        pa_cell(s, &x, &y);
        CHECK(x >= 0 && y >= 0 && x < 19 && y < 19 && !(x >= 8 && x <= 10 && y >= 8 && y <= 10));
        CHECK((x >= 8 && x <= 10) != (y >= 8 && y <= 10));   /* on an arm */
        used[y][x]++;
    }
    int cells = 0;
    for (y = 0; y < 19; y++)
        for (x = 0; x < 19; x++) cells += used[y][x] == 1;
    CHECK(cells == 96);
    for (int a = 0; a < PA_ARMIES; a++) {
        int ok = 1, seen[PA_SQUARES] = {0};
        for (int s = 1; s < PA_HOME; s++) {
            int sq = pa_square(a, s);
            pa_cell(sq, &x, &y);
            if (s == 1) ok &= abs(x - 9) + abs(y - 9) == 2;    /* next to the charkoni, on its own arm */
            else ok &= abs(x - px) <= 1 && abs(y - py) <= 1 && (x != px || y != py);
            px = x, py = y;
            seen[sq]++;
        }
        ok &= abs(x - 9) + abs(y - 9) == 2;     /* and back next to it */
        int all = 1;                            /* the whole track, and its own column twice (out and home) */
        for (int sq = 0; sq < PA_SQUARES; sq++) {
            int own = sq >= PA_LAP && (sq - PA_LAP) / 7 == a;
            all &= seen[sq] == (sq == pa_square(a, PA_END) || own ? 2 : sq < PA_LAP ? 1 : 0);
        }
        CHECK(ok && all);
        CHECK(pa_square(a, PA_END) == pa_square(a, PA_UP) && pa_castle(pa_square(a, PA_END)));
        CHECK(pa_castle(pa_square(a, PA_HOME - 25)));   /* a castle 25 from home: the arm before's end */
        CHECK(pa_square(a, 0) == -1 && pa_square(a, PA_HOME) == -1);
    }
    /* the arms: army 0 goes down the bottom arm's middle, then right and up (anticlockwise) */
    pa_cell(pa_square(0, 1), &x, &y);
    CHECK(x == 9 && y == 11);
    pa_cell(pa_square(0, PA_END), &x, &y);
    CHECK(x == 9 && y == 18);
    pa_cell(pa_square(0, PA_END + 1), &x, &y);
    CHECK(x == 10 && y == 18);
    pa_cell(pa_square(1, PA_END), &x, &y);
    CHECK(x == 18 && y == 9);
    /* castles: 12, the ends and the fourth square from each outer column's end */
    int castles = 0, cx[19][19] = {{0}};
    for (int s = 0; s < PA_SQUARES; s++)
        if (pa_castle(s)) {
            castles++;
            pa_cell(s, &x, &y);
            cx[y][x] = 1;
        }
    CHECK(castles == 12 && cx[18][9] && cx[15][8] && cx[15][10] && cx[0][9] && cx[3][8] && cx[3][10]);
    CHECK(cx[9][0] && cx[8][3] && cx[10][3] && cx[9][18] && cx[8][15] && cx[10][15] && !cx[14][8]);
}

/* leaving the charkoni: each army's first piece with any throw, the others with a grace; a grace throws again */
static void test_enter(void)
{
    pa_new(&G, 2);
    CHECK(P->turn == 0 && P->phase == PA_THROW);
    CHECK(!pa_move(&G, 0));
    CHECK(pa_throw(&G, 3) == PT_MOVE && pa_movable(P) == 0xff && pa_dest(P, 0) == 3);
    CHECK(pa_move(&G, 0) == 1 && P->at[0][0] == 3 && P->turn == 1 && P->phase == PA_THROW);
    CHECK(pa_throw(&G, 4) == PT_MOVE && pa_move(&G, 5) == 1 && P->at[3][1] == 4 && P->turn == 0);   /* army 3 */
    /* army 0 is out: its other pieces need a grace; army 2's first goes with anything */
    CHECK(pa_throw(&G, 2) == PT_MOVE && pa_movable(P) == (1 | 0xf0));
    CHECK(pa_move(&G, 4) == 1 && P->at[2][0] == 2 && P->turn == 1);
    CHECK(pa_throw(&G, 5) == PT_MOVE && pa_movable(P) == (0x0f | 1 << 5));   /* army 1's first, army 3's piece out */
    CHECK(pa_move(&G, 5) == 1 && P->at[3][1] == 9 && P->turn == 0);
    CHECK(pa_throw(&G, 1) == PT_MOVE && pa_dest(P, 1) == 10);   /* a 10: a grace, another piece out */
    CHECK(pa_move(&G, 1) == 1 && P->at[0][1] == 10 && P->turn == 0 && P->phase == PA_THROW);
    CHECK(pa_throw(&G, 0) == PT_MOVE && pa_dest(P, 2) == 25 && pa_dest(P, 0) == 28);   /* a 25 */
    CHECK(pa_move(&G, 0) == 1 && P->turn == 0);
    CHECK(pa_throw(&G, 6) == PT_MOVE && pa_dest(P, 3) == 6 && pa_move(&G, 3) == 1 && P->turn == 0);   /* a 6 */
    CHECK(!pa_script(&G, "7") && !pa_script(&G, "1") && !pa_script(&G, "3 i"));
}

/* captures: on an ordinary square, all the other player's pieces there (any army), and another throw */
static void test_capture(void)
{
    static const int s[4][4] = {{20, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}};
    int at[4][4];
    memcpy(at, s, sizeof at);
    int sq = pa_square(0, 23);                  /* three ahead of army 0's piece */
    at[1][0] = (sq - 17 + PA_LAP) % PA_LAP + PA_END;   /* army 1's piece on that square */
    at[3][2] = (sq - 51 + PA_LAP) % PA_LAP + PA_END;   /* and army 3's */
    CHECK(pa_square(1, at[1][0]) == sq && pa_square(3, at[3][2]) == sq && !pa_castle(sq));
    setup(at, 0);
    CHECK(pa_throw(&G, 3) == PT_MOVE && pa_takes(P, 0) == 2 && pa_move(&G, 0) == 3);
    CHECK(P->at[1][0] == 0 && P->at[3][2] == 0 && P->at[0][0] == 23);
    CHECK(P->turn == 0 && P->phase == PA_THROW);  /* the capture's extra throw */
    /* a piece sent back needs a grace to come out again */
    CHECK(pa_throw(&G, 2) == PT_MOVE && pa_move(&G, 0) == 1 && P->turn == 1);
    CHECK(pa_throw(&G, 4) == PT_PASS && P->turn == 0);   /* army 1 and 3 all in the charkoni, no grace */
    /* the partner's pieces are friends: army 2 lands on army 0's square, nothing happens */
    memcpy(at, s, sizeof at);
    at[2][0] = (pa_square(0, 20) - 34 + PA_LAP) % PA_LAP + PA_END - 4;
    setup(at, 0);
    CHECK(pa_throw(&G, 4) == PT_MOVE && pa_takes(P, 4) == 0 && pa_move(&G, 4) == 1);
    CHECK(pa_square(2, P->at[2][0]) == pa_square(0, 20) && P->at[0][0] == 20 && P->turn == 1);
}

/* castles: safe; a piece may not land where the other's pieces stand on one; its own and its partner's may share */
static void test_castles(void)
{
    int at[4][4] = {{0}}, c = pa_square(0, PA_END + 4);     /* the right column's castle */
    CHECK(pa_castle(c));
    at[0][0] = PA_END + 1;                      /* army 0's piece three short of it */
    at[1][0] = (c - 17 + PA_LAP) % PA_LAP + PA_END;         /* army 1's piece on it */
    CHECK(pa_square(1, at[1][0]) == c);
    setup(at, 0);
    CHECK(pa_throw(&G, 3) == PT_PASS && P->turn == 1 && P->at[0][0] == PA_END + 1);   /* held: no landing */
    setup(at, 0);
    CHECK(pa_throw(&G, 5) == PT_MOVE && pa_dest(P, 0) == PA_END + 6);   /* passing it is free */
    CHECK(pa_move(&G, 0) == 1 && P->at[1][0] != 0);
    /* the enemy gone, army 0 and its partner army 2 share the castle */
    at[1][0] = 0;
    at[2][1] = (c - 2 - 34 + PA_LAP) % PA_LAP + PA_END;     /* army 2's piece two short of it */
    setup(at, 0);
    CHECK(pa_throw(&G, 3) == PT_MOVE && pa_move(&G, 0) == 1 && pa_square(0, P->at[0][0]) == c);
    CHECK(pa_throw(&G, 2) == PT_PASS && P->turn == 0);  /* player 1: all in the charkoni, no grace */
    CHECK(pa_throw(&G, 2) == PT_MOVE && pa_move(&G, 5) == 1 && pa_square(2, P->at[2][1]) == c);
    /* now a piece of player 1 may not land there either; a capture there is impossible */
    at[0][0] = PA_END + 4;
    at[2][1] = (c - 34 + PA_LAP) % PA_LAP + PA_END;
    at[1][0] = (c - 3 - 17 + PA_LAP) % PA_LAP + PA_END;    /* army 1's piece three short */
    setup(at, 1);
    CHECK(pa_throw(&G, 3) == PT_PASS && P->at[0][0] == PA_END + 4 && P->turn == 0);
}

/* home: the exact throw only */
static void test_home(void)
{
    int at[4][4] = {{80, 84, 84, 84}, {0}, {84, 84, 84, 84}, {0}};
    setup(at, 0);
    CHECK(pa_throw(&G, 5) == PT_PASS && P->turn == 1);   /* 80 + 5 > 84 */
    CHECK(pa_throw(&G, 2) == PT_PASS);
    CHECK(pa_throw(&G, 0) == PT_AGAIN && P->turn == 0 && P->phase == PA_THROW);   /* a 25, no move: again */
    CHECK(pa_throw(&G, 4) == PT_MOVE && pa_dest(P, 0) == PA_HOME && pa_home(P, 0) == 7);
    CHECK(pa_move(&G, 0) == 1 && P->winner == 0 && P->phase == PA_OVER && !pa_throw(&G, 3));
    int b[4][4] = {{78, 84, 84, 84}, {0}, {84, 84, 84, 84}, {0}};   /* one short of home: never (no throw is a 1) */
    setup(b, 0);
    CHECK(pa_throw(&G, 5) == PT_PASS && P->at[0][0] == 78);
    CHECK(pa_throw(&G, 2) == PT_PASS && pa_throw(&G, 4) == PT_MOVE && pa_dest(P, 0) == 82);
}

/* the partnership: all eight home wins; one army home is not enough */
static void test_partners(void)
{
    int at[4][4] = {{84, 84, 84, 84}, {0}, {82, 30, 84, 84}, {0}};
    setup(at, 0);
    CHECK(pa_throw(&G, 2) == PT_MOVE && pa_movable(P) == (1 << 4 | 1 << 5));
    CHECK(pa_move(&G, 4) == 1 && P->winner == -1 && pa_home(P, 0) == 7 && P->turn == 1);
    CHECK(pa_throw(&G, 3) == PT_PASS);
    CHECK(pa_throw(&G, 0) == PT_MOVE && pa_move(&G, 5) == 1 && P->at[2][1] == 55 && P->turn == 0);
}

/* declining: the throw not used; after a grace the same player throws again */
static void test_decline(void)
{
    pa_new(&G, 2);
    CHECK(!pa_decline(&G));
    CHECK(pa_throw(&G, 4) == PT_MOVE && pa_decline(&G) && P->turn == 1 && P->phase == PA_THROW);
    CHECK(pa_throw(&G, 6) == PT_MOVE && pa_decline(&G) && P->turn == 1 && P->phase == PA_THROW);
    CHECK(pa_script(&G, "3 - 4 a") && P->at[0][0] == 4 && P->turn == 1);
}

/* undo: back to the last choice (a move or a decline), the throws after it too */
static void test_undo(void)
{
    pa_new(&G, 2);
    CHECK(!pa_undo(&G));
    CHECK(pa_script(&G, "3 a 10 e 2 - 4 a"));
    CHECK(P->at[0][0] == 7 && P->turn == 1 && P->throws == 4);
    CHECK(pa_undo(&G) && P->phase == PA_MOVE && P->roll == 4 && P->at[0][0] == 3);
    CHECK(pa_undo(&G) && P->phase == PA_MOVE && P->roll == 2 && P->turn == 1);
    CHECK(pa_undo(&G) && P->phase == PA_MOVE && P->roll == 10 && P->turn == 1 && P->at[3][0] == 0);
    CHECK(pa_undo(&G) && pa_undo(&G) && G.n == 0 && P->phase == PA_THROW && P->out == 0);
    CHECK(!pa_undo(&G));
}

/* four players: an army each (player p, army p), partners opposite (0 and 2 against 1 and 3), the turn round the table
 * (anticlockwise, as the pieces go); the partner's pieces are friends; a player whose army is home plays the partner's;
 * the side wins when both armies are home (the winner: the partner who brought the last piece in) */
static void setup4(const int at[4][4], int turn)
{
    setup(at, turn);
    G.pos.players = 4;
}

static void test_four(void)
{
    pa_new(&G, 4);
    CHECK(P->players == 4 && P->turn == 0 && P->phase == PA_THROW);
    CHECK(pa_throw(&G, 3) == PT_MOVE && pa_movable(P) == 0x0f);   /* its own army only */
    CHECK(pa_army(P, 0) == 0 && pa_army(P, 3) == 0 && pa_army(P, 4) == -1 && !pa_move(&G, 4) && !pa_script(&G, "e"));
    CHECK(pa_move(&G, 0) == 1 && P->at[0][0] == 3 && P->turn == 1);
    CHECK(pa_throw(&G, 4) == PT_MOVE && pa_army(P, 0) == 1 && pa_move(&G, 0) == 1 && P->at[1][0] == 4);
    CHECK(P->turn == 2 && pa_throw(&G, 2) == PT_MOVE && pa_move(&G, 1) == 1 && P->at[2][1] == 2);
    CHECK(P->turn == 3 && pa_throw(&G, 5) == PT_MOVE && pa_move(&G, 3) == 1 && P->at[3][3] == 5);
    CHECK(P->turn == 0);                        /* round the table */
    CHECK(pa_throw(&G, 1) == PT_MOVE && pa_move(&G, 1) == 1 && P->at[0][1] == 10 && P->turn == 0);   /* a grace */
    CHECK(pa_throw(&G, 2) == PT_MOVE && pa_decline(&G) && P->turn == 1);
    CHECK(pa_throw(&G, 3) == PT_MOVE && pa_decline(&G) && P->turn == 2);
    CHECK(pa_script(&G, "3 - 3 -") && P->turn == 0);
    /* the partner's piece is a friend; the others' pieces are taken (and a throw again) */
    int at[4][4] = {{20, 0, 0, 0}, {0}, {0}, {0}};
    int sq = pa_square(0, 20);
    at[2][0] = (sq - 34 + PA_LAP) % PA_LAP + PA_END - 4;     /* player 2's piece four short of player 0's */
    at[3][0] = (sq - 51 + PA_LAP) % PA_LAP + PA_END - 2;     /* player 3's two short */
    setup4(at, 2);
    CHECK(pa_throw(&G, 4) == PT_MOVE && pa_takes(P, 0) == 0 && pa_move(&G, 0) == 1);
    CHECK(pa_square(2, P->at[2][0]) == sq && P->at[0][0] == 20 && P->turn == 3);
    CHECK(pa_throw(&G, 2) == PT_MOVE && pa_takes(P, 0) == 2 && pa_move(&G, 0) == 3);
    CHECK(P->at[0][0] == 0 && P->at[2][0] == 0 && P->turn == 3 && P->phase == PA_THROW);
    /* a player whose army is home plays the partner's; one army home is not a win */
    int b[4][4] = {{84, 84, 84, 84}, {0}, {30, 82, 84, 84}, {0}};
    setup4(b, 0);
    CHECK(pa_army(P, 0) == 2 && pa_home(P, 0) == 6 && pa_army_home(P, 0) == 4 && P->winner == -1);
    CHECK(pa_throw(&G, 3) == PT_MOVE && pa_movable(P) == 1 && pa_move(&G, 0) == 1 && P->at[2][0] == 33);
    CHECK(P->turn == 1 && pa_throw(&G, 4) == PT_PASS && P->turn == 2);   /* player 1: all in, no grace */
    /* player 2 brings the side's last pieces home: player 2 wins, for both */
    int c[4][4] = {{84, 84, 84, 84}, {0}, {84, 82, 84, 84}, {0}};
    setup4(c, 2);
    CHECK(pa_throw(&G, 2) == PT_MOVE && pa_move(&G, 1) == 1 && P->winner == 2 && P->phase == PA_OVER);
    CHECK(pa_home(P, 0) == 8 && !pa_throw(&G, 3));
    /* undo goes back round the table */
    pa_new(&G, 4);
    CHECK(pa_script(&G, "3 a 4 a 2 a"));
    CHECK(P->turn == 3 && pa_undo(&G) && P->turn == 2 && P->phase == PA_MOVE && pa_undo(&G) && P->turn == 1);
}

/* whole games, two or four players, random throws: each player moves its furthest piece that can (a capture
 * first); someone wins */
static void test_games(void)
{
    for (int seed = 1; seed <= 40; seed++) {    /* 20 games for two, 20 for four */
        srand((unsigned)seed);
        pa_new(&G, seed > 20 ? 4 : 2);
        int ok = 1;
        while (P->phase != PA_OVER && G.n < PA_MAX_EVENTS - 1) {
            if (P->phase == PA_THROW) {
                int m = 0;
                for (int i = 0; i < 6; i++) m += rand() & 1;
                ok &= pa_throw(&G, m) != 0;
            } else {
                int moves = pa_movable(P), best = -1, score = -1;
                for (int i = 0; i < 8; i++) {
                    if (!(moves >> i & 1)) continue;
                    int a = pa_army(P, i), s = pa_takes(P, i) * 1000 + P->at[a][i % 4];
                    if (s > score) score = s, best = i;
                }
                ok &= pa_move(&G, best) != 0;
            }
            for (int sq = 0; sq < PA_LAP; sq++) {   /* never both players on one square */
                int who[2] = {0, 0};
                for (int a = 0; a < PA_ARMIES; a++)
                    for (int i = 0; i < PA_PIECES; i++) who[a & 1] |= pa_square(a, P->at[a][i]) == sq;
                ok &= !(who[0] && who[1]);
            }
        }
        CHECK(ok && P->phase == PA_OVER && pa_home(P, P->winner & 1) == 8);
        while (pa_undo(&G)) {}
        CHECK(G.n == 0 && P->winner == -1 && pa_home(P, 0) == 0);
    }
}

int main(void)
{
    test_table();
    test_board();
    test_enter();
    test_capture();
    test_castles();
    test_home();
    test_partners();
    test_decline();
    test_undo();
    test_four();
    test_games();
    printf("pachisi: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
