/*
 * Alquerque: the rules' test (the board, the steps, the captures and their chains, compulsory capture, the ends,
 * draws, undo). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/alquerque.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static aqgame G;
static aqmove L[AQ_MAX_MOVES];

/* a position: player 0's pieces ("c3 d5"), player 1's, who moves */
static void setup(const char *light, const char *dark, int side)
{
    aq_new(&G);
    aqpos *p = &G.pos[0];
    memset(p->at, -1, sizeof p->at);
    p->left[0] = p->left[1] = 0;
    const char *list[2] = {light, dark};
    for (int c = 0; c < 2; c++)
        for (const char *t = list[c]; *t; t++) {
            int pt = aq_point(t);
            if (pt < 0) continue;
            p->at[pt] = (int8_t)c;
            p->left[c]++;
            t++;
        }
    p->side = (uint8_t)side;
}

static const aqpos *pos(void) { return aq_pos(&G); }
static int nlegal(void) { return aq_legal(pos(), L); }
static int at(const char *pt) { return pos()->at[aq_point(pt)]; }

/* the board: 25 points, 56 lines between neighbours (40 along rows and columns, 16 diagonals), the start */
static void test_board(void)
{
    int n = 0, diag = 0, ok = 1;
    for (int i = 0; i < AQ_POINTS; i++) {
        char name[3] = {(char)('a' + aq_x(i)), (char)('1' + aq_y(i)), 0};
        CHECK(aq_point(name) == i);
        for (int d = 0; d < 8; d++) {
            int j = aq_next(i, d);
            if (j < 0) continue;
            n++;
            diag += aq_dx[d] && aq_dy[d];
            ok &= aq_next(j, (d + 4) % 8) == i && aq_x(j) == aq_x(i) + aq_dx[d] && aq_y(j) == aq_y(i) + aq_dy[d];
            ok &= !(aq_dx[d] && aq_dy[d]) || (aq_x(i) + aq_y(i)) % 2 == 0;
        }
    }
    CHECK(ok && n == 2 * 56 && diag == 2 * 16);
    CHECK(aq_next(aq_point("b1"), 1) < 0 && aq_next(aq_point("c1"), 1) == aq_point("d2"));
    CHECK(aq_point("f1") < 0 && aq_point("a6") < 0 && aq_point("a0") < 0);
    aq_new(&G);
    CHECK(pos()->side == 0 && pos()->chain < 0 && pos()->left[0] == 12 && pos()->left[1] == 12);
    CHECK(at("c3") < 0 && at("d3") == 0 && at("e3") == 0 && at("a3") == 1 && at("b3") == 1);
    CHECK(at("a1") == 0 && at("e2") == 0 && at("a4") == 1 && at("e5") == 1);
    CHECK(nlegal() == 4);                       /* b2, c2, d2 and d3 into the middle */
    CHECK(aq_status(&G) == AS_PLAY);
}

/* steps: forwards (straight or slanting) or sideways, never backwards */
static void test_steps(void)
{
    setup("c3", "a5", 0);                       /* c3: N NE E W NW, not S SE SW */
    CHECK(nlegal() == 5);
    int ok = 1;
    for (int i = 0; i < 5; i++) ok &= aq_y(L[i].to) >= aq_y(L[i].from) && L[i].over < 0;
    CHECK(ok);
    CHECK(!aq_script(&G, "c3-c2") && !aq_script(&G, "c3-b2") && aq_script(&G, "c3-d3") && pos()->side == 1);
    setup("a1", "c3", 1);                       /* player 1 goes down the board */
    CHECK(nlegal() == 5);
    CHECK(!aq_script(&G, "c3-c4") && !aq_script(&G, "c3-d4") && aq_script(&G, "c3-b2"));
    setup("b3", "a5", 0);                       /* b3: no diagonals there */
    CHECK(nlegal() == 3);
    setup("c5", "a1", 0);                       /* the far row: only sideways */
    CHECK(nlegal() == 2);
    aq_new(&G);                                 /* a game's worth of steps: never backwards */
    ok = 1;
    for (int k = 0; k < 60 && aq_status(&G) == AS_PLAY; k++) {
        int n = nlegal(), side = pos()->side;
        for (int i = 0; i < n; i++)
            if (L[i].over < 0) ok &= side ? aq_y(L[i].to) <= aq_y(L[i].from) : aq_y(L[i].to) >= aq_y(L[i].from);
        aq_play(&G, L[(k * 5) % n]);
    }
    CHECK(ok);
}

/* captures: in all eight directions (backwards too), over a piece next to it, onto the empty point beyond */
static void test_captures(void)
{
    setup("c3", "b2 c2 d2 b3 d3 b4 c4 d4", 0);
    CHECK(nlegal() == 8);
    int ok = 1;
    for (int i = 0; i < 8; i++) ok &= L[i].over >= 0;
    CHECK(ok);
    CHECK(aq_script(&G, "c3xc1") && at("c2") < 0 && at("c1") == 0 && pos()->left[1] == 7);
    CHECK(pos()->side == 0 && pos()->chain == aq_point("c1"));   /* b2 next, a3 beyond: it jumps on */
    setup("b3", "b2 c3 c4", 0);                 /* b3 has no diagonals: c4 cannot be jumped from there */
    CHECK(nlegal() == 2);
    setup("c3", "c4", 0);                       /* no capture when the point beyond is taken or off the board */
    G.pos[0].at[aq_point("c5")] = 0;
    CHECK(nlegal() > 0 && L[0].over < 0);
    setup("a1", "b1 c1", 0);                    /* nor over two pieces, nor over one's own */
    CHECK(nlegal() == 2);                       /* a2, b2 */
    setup("c3 c4", "a1", 0);
    CHECK(!aq_script(&G, "c3xc5"));
}

/* chains: the same piece jumps on in the same turn, a jump at a time; it alone moves */
static void test_chains(void)
{
    setup("a1 e1", "b2 d4 a5", 0);
    CHECK(aq_script(&G, "a1xc3"));
    CHECK(pos()->side == 0 && pos()->chain == aq_point("c3") && aq_status(&G) == AS_PLAY);
    CHECK(nlegal() == 1 && L[0].from == aq_point("c3"));   /* e1 cannot move now */
    CHECK(!aq_script(&G, "e1-e2"));
    CHECK(aq_script(&G, "c3xe5"));
    CHECK(pos()->side == 1 && pos()->chain < 0 && pos()->left[1] == 1 && at("e5") == 0);
    setup("a1", "b2 c4 e2", 0);                 /* written at once: a1xc3xc5, then nothing more to take */
    CHECK(aq_script(&G, "a1xc3xc5") && pos()->side == 1 && G.ply == 2);
    setup("a1", "b2 d4", 0);                    /* a chain may not stop half-way */
    CHECK(aq_script(&G, "a1xc3") && pos()->side == 0 && !aq_script(&G, "c3-c4"));
}

/* captures are compulsory: a step is not allowed while a capture is */
static void test_compulsory(void)
{
    setup("a1 e1", "e2", 1);                    /* e2 can take... nothing (e1 has no beyond); e2-d2 etc. are steps */
    int n = nlegal(), steps = 1;
    for (int i = 0; i < n; i++) steps &= L[i].over < 0;
    CHECK(n > 0 && steps);
    setup("a1 c2", "c3", 0);                    /* c2 can take c3 (onto c4): a1's steps are not allowed */
    CHECK(nlegal() == 1 && L[0].from == aq_point("c2") && L[0].over == aq_point("c3"));
    CHECK(!aq_script(&G, "a1-a2") && !aq_script(&G, "c2-b2") && aq_script(&G, "c2xc4"));
    aq_new(&G);                                 /* a real opening: after d3-c3, b3 must take */
    CHECK(aq_script(&G, "d3-c3"));
    n = nlegal();
    int caps = 1;
    for (int i = 0; i < n; i++) caps &= L[i].over >= 0;
    CHECK(n > 0 && caps);
}

/* the ends: no pieces, no move; the draws */
static void test_ends(void)
{
    setup("a1", "b2", 0);                       /* the last piece taken: player 1 has none, and lost */
    CHECK(aq_script(&G, "a1xc3") && pos()->left[1] == 0 && aq_status(&G) == AS_STUCK && pos()->side == 1);
    setup("b1 c1", "a1", 1);                    /* a1 cannot step sideways (b1), nor take (c1 behind), nor go up */
    CHECK(nlegal() == 0 && aq_status(&G) == AS_STUCK);
    setup("a5", "e1", 0);                       /* sideways and back: the same position a third time */
    CHECK(aq_script(&G, "a5-b5 e1-d1 b5-a5 d1-e1 a5-b5 e1-d1 b5-a5"));
    CHECK(aq_status(&G) == AS_PLAY);
    CHECK(aq_script(&G, "d1-e1") && aq_status(&G) == AS_REPETITION);
    setup("a5", "e1", 0);                       /* fifty moves each without a capture */
    G.pos[0].quiet = AQ_QUIET - 1;
    CHECK(aq_status(&G) == AS_PLAY && aq_script(&G, "a5-b5") && aq_status(&G) == AS_QUIET);
    setup("a1", "b2 e5", 0);                    /* a capture starts the count again */
    G.pos[0].quiet = AQ_QUIET - 1;
    CHECK(aq_script(&G, "a1xc3") && pos()->quiet == 0 && aq_status(&G) == AS_PLAY);
}

/* undo: back one move (one jump) at a time, all the way to the start */
static void test_undo(void)
{
    aq_new(&G);
    aqpos start = *pos();
    CHECK(!aq_undo(&G));
    CHECK(aq_script(&G, "d3-c3 b3xd3 e3xc3"));
    CHECK(G.ply == 3 && pos()->left[0] == 11 && pos()->left[1] == 11);
    int n = 0;
    while (aq_undo(&G)) n++;
    CHECK(n == 3 && !memcmp(pos(), &start, sizeof start));
    setup("a1", "b2 d4 e5", 0);
    CHECK(aq_script(&G, "a1xc3") && aq_undo(&G) && pos()->chain < 0 && at("a1") == 0 && at("b2") == 1);
    CHECK(!aq_script(&G, "a1-c3x") && !aq_script(&G, "zz"));
}

int main(void)
{
    test_board();
    test_steps();
    test_captures();
    test_chains();
    test_compulsory();
    test_ends();
    test_undo();
    printf("alquerque: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
