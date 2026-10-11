/*
 * Bagh-Chal: the rules' test (the board, placing the goats, the steps, the tigers' jumps, the ends, draws, undo).
 * No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/baghchal.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static bcgame G;
static bcmove L[BC_MAX_MOVES];

/* a position: the tigers ("a1 e5"), the goats ("c3 d4"), goats set down so far, goats taken, who moves */
static void setup(const char *tigers, const char *goats, int placed, int taken, int side)
{
    bc_new(&G);
    bcpos *p = &G.pos[0];
    memset(p->at, -1, sizeof p->at);
    for (int who = 0; who < 2; who++)
        for (const char *t = who ? tigers : goats; *t; t++) {
            int pt = bc_point(t);
            if (pt < 0) continue;
            p->at[pt] = (int8_t)(who ? BC_TIGER : BC_GOAT);
            t++;
        }
    p->placed = (uint8_t)placed;
    p->taken = (uint8_t)taken;
    p->side = (uint8_t)side;
}

static const bcpos *pos(void) { return bc_pos(&G); }
static int nlegal(void) { return bc_legal(pos(), L); }
static int at(const char *pt) { return pos()->at[bc_point(pt)]; }

static int has(int from, int to, int over)
{
    int n = nlegal();
    for (int i = 0; i < n; i++)
        if (L[i].from == from && L[i].to == to && L[i].over == over) return 1;
    return 0;
}
#define P(s) bc_point(s)

/* the board: 25 points, 56 lines between neighbours (40 along rows and columns, 16 diagonals), the start */
static void test_board(void)
{
    int n = 0, diag = 0, ok = 1;
    for (int i = 0; i < BC_POINTS; i++) {
        char name[3] = {(char)('a' + bc_x(i)), (char)('1' + bc_y(i)), 0};
        CHECK(bc_point(name) == i);
        for (int d = 0; d < 8; d++) {
            int j = bc_next(i, d);
            if (j < 0) continue;
            n++;
            diag += bc_dx[d] && bc_dy[d];
            ok &= bc_next(j, (d + 4) % 8) == i;
        }
    }
    CHECK(ok && n == 2 * 56 && diag == 2 * 16);
    CHECK(bc_next(P("b3"), 1) < 0 && bc_next(P("c3"), 1) == P("d4") && bc_next(P("e5"), 0) < 0);
    CHECK(bc_point("f1") < 0 && bc_point("a6") < 0 && bc_point("") < 0);
    bc_new(&G);
    CHECK(at("a1") == BC_TIGER && at("e1") == BC_TIGER && at("a5") == BC_TIGER && at("e5") == BC_TIGER);
    CHECK(pos()->side == BC_GOAT && bc_to_place(pos()) == 20 && pos()->taken == 0);
    CHECK(nlegal() == 21);                      /* a goat on any of the 21 empty points */
    for (int i = 0; i < 21; i++) CHECK(L[i].from < 0 && L[i].over < 0 && pos()->at[L[i].to] < 0);
}

/* placing: one goat a turn, no goat moves before all 20 are in, then they move and no more come */
static void test_placing(void)
{
    bc_new(&G);
    CHECK(bc_script(&G, "c3") && at("c3") == BC_GOAT && pos()->side == BC_TIGER && bc_to_place(pos()) == 19);
    CHECK(!bc_script(&G, "c4"));               /* the tigers' turn: no goat set down */
    CHECK(bc_script(&G, "a1-a2 c4"));
    CHECK(!bc_script(&G, "a2-a1 c4-d4"));      /* a goat cannot move while goats are still to come... */
    bc_undo(&G);
    CHECK(G.ply == 3 && bc_script(&G, "a2-a1") && !bc_script(&G, "c3"));   /* ...nor go on a taken point */
    static const char *rest = "b1 a1-a2 d1 a2-a1 b3 a1-a2 d3 a2-a1 b5 a1-a2 d5 a2-a1 c1 a1-a2 c5 a2-a1 a3 "
                              "a1-a2 e3 a2-a1 b2 a1-a2 d2 a2-a1 d4 a1-a2 e2 a2-a1 e4 a1-a2 b4 a2-a1 c2 a1-a2";
    CHECK(bc_script(&G, rest));
    CHECK(pos()->placed == 19 && pos()->side == BC_GOAT && nlegal() == 2);   /* a1 and a4 empty */
    CHECK(bc_script(&G, "a4") && pos()->placed == 20 && bc_to_place(pos()) == 0);
    CHECK(pos()->side == BC_TIGER && nlegal() == 1 && L[0].from == P("a2") && L[0].to == P("a1"));
    CHECK(bc_script(&G, "a2-a1") && pos()->side == BC_GOAT);
    CHECK(!bc_script(&G, "a2"));               /* all in: no more goats */
    CHECK(nlegal() == 2 && bc_script(&G, "a3-a2"));   /* a3 and b2 may step to a2... */
    CHECK(at("a2") == BC_GOAT && at("a3") < 0 && pos()->placed == 20);
}

/* the tigers: steps along the lines, a jump over one goat to the empty point beyond; never over a tiger, never two
 * goats at once, never along a missing diagonal */
static void test_tigers(void)
{
    setup("c3", "c4 d4 b3 d2", 4, 0, BC_TIGER);
    CHECK(has(P("c3"), P("c5"), P("c4")) && has(P("c3"), P("e5"), P("d4")) && has(P("c3"), P("a3"), P("b3")));
    CHECK(has(P("c3"), P("e1"), P("d2")) && has(P("c3"), P("c2"), -1) && has(P("c3"), P("b4"), -1));
    CHECK(nlegal() == 4 + 4);                   /* four jumps, steps to b2 c2 d3 b4 */
    CHECK(bc_script(&G, "c3xc5") && at("c4") < 0 && at("c5") == BC_TIGER && pos()->taken == 1);
    CHECK(pos()->side == BC_GOAT);              /* one capture a move: the goats' turn */
    setup("c3 c5", "c4", 4, 0, BC_TIGER);       /* the landing point taken: no jump */
    CHECK(!has(P("c3"), P("c5"), P("c4")) && !has(P("c5"), P("c3"), P("c4")));
    setup("c3 c4", "", 4, 0, BC_TIGER);         /* never over a tiger */
    CHECK(!has(P("c3"), P("c5"), P("c4")) && !bc_script(&G, "c3-c5"));
    setup("b3", "c4 c3", 4, 0, BC_TIGER);       /* b3: no diagonals, so no jump over c4 to d5 */
    CHECK(!has(P("b3"), P("d5"), P("c4")) && has(P("b3"), P("d3"), P("c3")));
    setup("a1", "b1 c1", 4, 0, BC_TIGER);       /* two goats in a row: no jump */
    CHECK(!has(P("a1"), P("c1"), P("b1")) && nlegal() == 2);
    setup("a1", "b2", 4, 0, BC_TIGER);
    CHECK(bc_script(&G, "a1xc3") && pos()->taken == 1 && at("b2") < 0);
    setup("a1 e5", "b2 d4", 20, 0, BC_GOAT);    /* goats never jump, step any way along a line */
    CHECK(!bc_script(&G, "b2-a1") && !has(P("d4"), P("e5"), -1));
    CHECK(has(P("b2"), P("a3"), -1) && has(P("b2"), P("b1"), -1) && nlegal() == 7 + 7);
}

/* the ends: 5 goats taken, the tigers trapped, the goats unable to move */
static void test_ends(void)
{
    setup("a1 e1 a5 c3", "c4", 20, 4, BC_TIGER);
    CHECK(bc_status(&G) == BS_PLAY);
    CHECK(bc_script(&G, "c3xc5") && pos()->taken == 5 && bc_status(&G) == BS_FIVE);
    /* the four corner tigers walled in: their neighbours and the points beyond them all taken */
    setup("a1 e1 a5 e5", "b1 a2 b2 c1 a3 c3 d1 e2 d2 e3 a4 b5 b4 c5 d5 e4 c4", 20, 3, BC_GOAT);
    CHECK(bc_status(&G) == BS_PLAY);
    CHECK(bc_script(&G, "c4-d4") && pos()->side == BC_TIGER && nlegal() == 0 && bc_status(&G) == BS_TRAPPED);
    bc_undo(&G);
    CHECK(bc_script(&G, "c3-d3") && bc_status(&G) == BS_PLAY);   /* c3 left: a1 and e5 may jump b2 and d4 */
    /* twenty goats, one empty point that only tigers touch: the goats cannot move */
    setup("b1 a2 b2 e5", "c1 d1 e1 a3 b3 c3 d3 e3 a4 b4 c4 d4 e4 a5 b5 c5 d5 c2 d2 e2", 20, 0, BC_GOAT);
    CHECK(nlegal() == 0 && bc_status(&G) == BS_STUCK);
    G.pos[0].side = BC_TIGER;
    CHECK(bc_status(&G) == BS_PLAY);
}

/* draws: the same position the third time; 50 moves each without a capture or a goat set down */
static void test_draws(void)
{
    setup("a1 e1 a5 e5", "c3", 20, 0, BC_GOAT);
    CHECK(bc_script(&G, "c3-c4 a1-a2 c4-c3 a2-a1") && bc_status(&G) == BS_PLAY);
    CHECK(bc_script(&G, "c3-c4 a1-a2 c4-c3") && bc_status(&G) == BS_PLAY);
    CHECK(bc_script(&G, "a2-a1") && bc_status(&G) == BS_REPETITION);
    setup("a1 e1 a5 e5", "c3", 20, 0, BC_GOAT);
    G.pos[0].quiet = BC_QUIET - 2;
    CHECK(bc_script(&G, "c3-c4") && bc_status(&G) == BS_PLAY);
    CHECK(bc_script(&G, "a1-a2") && bc_status(&G) == BS_QUIET);
    setup("a1 e1 a5 e5", "", 3, 0, BC_GOAT);   /* a goat set down starts the count again */
    G.pos[0].quiet = BC_QUIET - 1;
    CHECK(bc_script(&G, "c3") && pos()->quiet == 0 && bc_status(&G) == BS_PLAY);
    setup("a1", "b2", 20, 0, BC_TIGER);         /* so does a capture */
    G.pos[0].quiet = 50;
    CHECK(bc_script(&G, "a1-c3") && pos()->quiet == 0);
}

/* undo: a move at a time, back to the start; bad moves refused */
static void test_undo(void)
{
    bc_new(&G);
    bcpos start = *pos();
    CHECK(bc_script(&G, "b2 a1-a2 c3 a2-a1 d4"));
    CHECK(bc_undo(&G) && at("d4") < 0 && pos()->placed == 2 && pos()->side == BC_GOAT);
    while (bc_undo(&G)) {}
    CHECK(G.ply == 0 && !memcmp(pos(), &start, sizeof start) && !bc_undo(&G));
    CHECK(!bc_script(&G, "a1") && !bc_script(&G, "f3") && !bc_script(&G, "c3-") && !bc_script(&G, "c3c4c5"));
    CHECK(G.ply == 0);
}

int main(void)
{
    test_board();
    test_placing();
    test_tigers();
    test_ends();
    test_draws();
    test_undo();
    printf("baghchal: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
