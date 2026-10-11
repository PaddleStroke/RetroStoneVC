/*
 * The Game of the Amazons: the rules' test (the start, queen lines and arrows, the end, undo, the notation). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/amazons.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static amgame G;

static const ampos *pos(void) { return am_pos(&G); }
static int at(const char *s) { return pos()->sq[am_square(s)]; }

/* a position: player 0's amazons, player 1's, the burnt squares ("a1 b2"), who moves */
static void setup(const char *mine, const char *theirs, const char *burnt, int side)
{
    am_new(&G);
    ampos *p = &G.pos[0];
    memset(p->sq, AM_EMPTY, sizeof p->sq);
    const char *list[3] = {mine, theirs, burnt};
    const int what[3] = {AM_AMAZON, AM_AMAZON + 1, AM_ARROW};
    for (int k = 0; k < 3; k++)
        for (const char *t = list[k]; *t;) {
            int s = am_square(t);
            if (s >= 0) p->sq[s] = (uint8_t)what[k];
            while (*t && *t != ' ') t++;
            while (*t == ' ') t++;
        }
    p->side = (uint8_t)side;
}

static int line_count(const char *from)
{
    uint8_t out[AM_MAX_LINE];
    return am_line(pos(), am_square(from), out);
}

/* the squares' names, the start, its 2176 whole turns */
static void test_start(void)
{
    CHECK(am_square("a1") == 0 && am_square("j1") == 9 && am_square("a10") == 90 && am_square("j10") == 99);
    CHECK(am_square("k1") < 0 && am_square("a0") < 0 && am_square("a11") < 0 && am_square("") < 0);
    am_new(&G);
    CHECK(at("a4") == AM_AMAZON && at("d1") == AM_AMAZON && at("g1") == AM_AMAZON && at("j4") == AM_AMAZON);
    CHECK(at("a7") == AM_AMAZON + 1 && at("d10") == AM_AMAZON + 1 && at("g10") == AM_AMAZON + 1);
    CHECK(at("j7") == AM_AMAZON + 1 && at("e5") == AM_EMPTY && pos()->side == 0 && pos()->stage == AM_MOVE);
    CHECK(am_full_moves(pos()) == 2176);
    CHECK(am_winner(&G) == -1);
    CHECK(am_script(&G, "d1-d7/g7"));           /* and the other player's, symmetrical by the board's turn */
    CHECK(pos()->side == 1 && at("g7") == AM_ARROW && at("d7") == AM_AMAZON);
    am_new(&G);
    G.pos[0].side = 1;
    CHECK(am_full_moves(pos()) == 2176);
}

/* queen lines: stopped by amazons of either side and by burnt squares, no capturing */
static void test_lines(void)
{
    setup("e5", "", "", 0);
    CHECK(line_count("e5") == 35);              /* the file 9, the rank 9, the diagonals 9 and 8 */
    setup("a1", "", "", 0);
    CHECK(line_count("a1") == 27);
    setup("e5", "e7", "c5 g7 c4", 0);          /* up: e6 only (-4); left: d5 only (-3); up-right: f6 only (-4) */
    CHECK(line_count("e5") == 35 - 4 - 3 - 4);
    CHECK(!am_move(&G, am_square("e5"), am_square("e7")));    /* no capture */
    CHECK(!am_move(&G, am_square("e5"), am_square("e8")));    /* no jumping over an amazon */
    CHECK(!am_move(&G, am_square("e5"), am_square("c5")));    /* not onto a burnt square */
    CHECK(!am_move(&G, am_square("e5"), am_square("b5")));    /* nor across one */
    CHECK(!am_move(&G, am_square("e5"), am_square("f7")));    /* not a queen line */
    CHECK(!am_move(&G, am_square("e7"), am_square("e8")));    /* not the other player's amazon */
    CHECK(!am_shoot(&G, am_square("e6")));                     /* the move comes first */
    CHECK(am_move(&G, am_square("e5"), am_square("e6")) && pos()->stage == AM_SHOOT && pos()->side == 0);
    CHECK(!am_move(&G, am_square("e6"), am_square("e5")));    /* one move a turn */
    CHECK(!am_shoot(&G, am_square("e7")) && !am_shoot(&G, am_square("e8")));   /* arrows stop at amazons */
    CHECK(!am_shoot(&G, am_square("c4")) && !am_shoot(&G, am_square("b3")));   /* and at burnt squares */
    CHECK(!am_shoot(&G, am_square("e6")) && !am_shoot(&G, am_square("g7")));
    CHECK(am_shoot(&G, am_square("f7")) && pos()->side == 1 && pos()->stage == AM_MOVE && at("f7") == AM_ARROW);
    CHECK(!am_script(&G, "e7-e5"));             /* across the amazon on e6 */
    CHECK(!am_script(&G, "e7-e8/e5") && pos()->stage == AM_SHOOT && at("e8") == AM_AMAZON + 1);
}

/* the arrow may fly back to the square the amazon left, and over it */
static void test_back(void)
{
    setup("a1", "j10", "", 0);
    CHECK(am_script(&G, "a1-a5/a1") && at("a1") == AM_ARROW && at("a5") == AM_AMAZON);
    setup("c1", "j10", "", 0);
    CHECK(am_script(&G, "c1-c3/c1") && at("c1") == AM_ARROW);
    setup("c1", "j10", "", 0);
    CHECK(am_script(&G, "c1-c3") && am_script(&G, "/c1"));    /* the steps one at a time */
    setup("b1", "j10", "", 0);
    CHECK(am_script(&G, "b1-b3/b1") && at("b1") == AM_ARROW);
    setup("c2", "j10", "", 0);                  /* from c4 the arrow crosses c2, left empty, to c1 */
    CHECK(am_script(&G, "c2-c4/c1") && at("c1") == AM_ARROW && at("c2") == AM_EMPTY);
}

/* the end: a player whose amazons cannot move loses (no draws) */
static void test_end(void)
{
    setup("a1", "j10", "a2 b1 b2", 0);          /* player 0 walled in */
    CHECK(!am_can_move(pos()) && am_full_moves(pos()) == 0 && am_winner(&G) == 1);
    setup("a1", "j10", "a2 b1", 1);             /* player 1 to move: the game goes on */
    CHECK(am_winner(&G) == -1);
    setup("a1", "j10", "a2 b1 c3", 0);          /* b2 the only move; its arrows: a1 c1 a3, the rank 8, the file 8 */
    CHECK(line_count("a1") == 1 && am_full_moves(pos()) == 19 && am_winner(&G) == -1);
    setup("a1", "j10", "a2 b1 i10 j9 i9", 0);   /* player 0 moves and shoots: player 1 is stuck, player 0 wins */
    CHECK(am_script(&G, "a1-b2/a1") && am_winner(&G) == 0);
    setup("a1", "j10 a3", "b1 b2", 0);          /* the amazon's only move, a2, closed by an amazon */
    CHECK(line_count("a1") == 1 && am_winner(&G) == -1);
    CHECK(am_script(&G, "a1-a2/a1") && am_winner(&G) == -1);
}

/* undo: one step at a time, the arrow then the move */
static void test_undo(void)
{
    am_new(&G);
    ampos start = *pos();
    CHECK(!am_undo(&G));
    CHECK(am_script(&G, "d1-d7/g7 g10-e8/e1 g1-c5"));
    CHECK(G.ply == 5 && pos()->stage == AM_SHOOT && pos()->at == am_square("c5"));
    CHECK(am_undo(&G) && pos()->stage == AM_MOVE && pos()->side == 0 && at("g1") == AM_AMAZON && at("c5") == 0);
    CHECK(am_undo(&G) && pos()->stage == AM_SHOOT && pos()->side == 1 && at("e1") == AM_EMPTY && at("e8"));
    CHECK(am_undo(&G) && at("g10") == AM_AMAZON + 1 && at("e8") == AM_EMPTY && pos()->side == 1);
    CHECK(am_undo(&G) && am_undo(&G) && !memcmp(pos(), &start, sizeof start) && !am_undo(&G));
    am_new(&G);
    CHECK(!am_script(&G, "d1d7g7"));
    am_new(&G);
    CHECK(!am_script(&G, "d1-d7/d1x") && !am_script(&G, "x1-d7") && !am_script(&G, "/d2"));
}

/* a whole game of first choices ends, with a winner, within the plies' bound */
static void test_game(void)
{
    am_new(&G);
    int guard = 0;
    while (am_winner(&G) < 0 && guard++ < AM_MAX_PLY) {
        uint8_t to[AM_MAX_LINE];
        const ampos *p = pos();
        if (p->stage == AM_SHOOT) {
            CHECK(am_line(p, p->at, to) > 0);
            am_shoot(&G, to[(G.ply * 7) % am_line(p, p->at, to)]);
            continue;
        }
        for (int s = 0; s < AM_SQUARES; s++) {
            int n;
            if (p->sq[s] == AM_AMAZON + p->side && (n = am_line(p, s, to)) > 0) {
                am_move(&G, s, to[(G.ply * 5) % n]);
                break;
            }
        }
    }
    CHECK(am_winner(&G) >= 0 && G.ply <= AM_MAX_PLY && G.ply % 2 == 0);
}

int main(void)
{
    test_start();
    test_lines();
    test_back();
    test_end();
    test_undo();
    test_game();
    printf("amazons: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
