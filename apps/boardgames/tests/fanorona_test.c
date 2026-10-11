/*
 * Fanorona: the rules' test (the board, the start, approach and withdrawal, the choice between them, compulsory
 * capture, chains and their limits, stopping, the ends, draws, undo, the notation). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/fanorona.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static fngame G;
static fnmove L[FN_MAX_MOVES];

/* a position: player 0's pieces ("c3 d5"), player 1's, who moves */
static void setup(const char *light, const char *dark, int side)
{
    fn_new(&G);
    fnpos *p = &G.pos[0];
    memset(p->at, -1, sizeof p->at);
    p->left[0] = p->left[1] = 0;
    const char *list[2] = {light, dark};
    for (int c = 0; c < 2; c++)
        for (const char *t = list[c]; *t; t++) {
            int pt = fn_point(t);
            if (pt < 0) continue;
            p->at[pt] = (int8_t)c;
            p->left[c]++;
            t++;
        }
    p->side = (uint8_t)side;
}

static const fnpos *pos(void) { return fn_pos(&G); }
static int nlegal(void) { return fn_legal(pos(), L); }
static int at(const char *pt) { return pos()->at[fn_point(pt)]; }
static int pt(const char *s) { return fn_point(s); }

/* the moves from a to b, of a kind (-1: any) */
static int count(const char *a, const char *b, int kind)
{
    int n = nlegal(), k = 0;
    for (int i = 0; i < n; i++) k += L[i].from == pt(a) && L[i].to == pt(b) && (kind < 0 || L[i].kind == kind);
    return k;
}

/* the board: 45 points, the lines between neighbours (76 along rows and columns, 32 diagonals), the start */
static void test_board(void)
{
    int n = 0, diag = 0, ok = 1;
    for (int i = 0; i < FN_POINTS; i++) {
        char name[3] = {(char)('a' + fn_x(i)), (char)('1' + fn_y(i)), 0};
        CHECK(fn_point(name) == i);
        for (int d = 0; d < 8; d++) {
            int j = fn_next(i, d);
            if (j < 0) continue;
            n++;
            diag += fn_dx[d] && fn_dy[d];
            ok &= fn_next(j, (d + 4) % 8) == i && fn_x(j) == fn_x(i) + fn_dx[d] && fn_y(j) == fn_y(i) + fn_dy[d];
            ok &= !(fn_dx[d] && fn_dy[d]) || (fn_x(i) + fn_y(i)) % 2 == 0;
        }
    }
    CHECK(ok && n == 2 * (76 + 32) && diag == 2 * 32);
    CHECK(fn_next(pt("a1"), 1) == pt("b2") && fn_next(pt("b1"), 1) < 0 && fn_next(pt("i5"), 5) == pt("h4"));
    CHECK(fn_point("j1") < 0 && fn_point("a6") < 0 && fn_point("a0") < 0);
    fn_new(&G);
    CHECK(pos()->side == 0 && pos()->chain < 0 && pos()->left[0] == 22 && pos()->left[1] == 22);
    CHECK(at("e3") < 0 && at("a1") == 0 && at("i2") == 0 && at("a4") == 1 && at("i5") == 1);
    CHECK(at("a3") == 1 && at("b3") == 0 && at("c3") == 1 && at("d3") == 0);
    CHECK(at("f3") == 1 && at("g3") == 0 && at("h3") == 1 && at("i3") == 0);
    /* the five opening captures: d3 approaching f3 or withdrawing from c3, e2, d2 and f2 into the centre */
    CHECK(nlegal() == 5 && count("d3", "e3", -1) == 2 && count("e2", "e3", FN_APPROACH) == 1);
    CHECK(count("d2", "e3", FN_APPROACH) == 1 && count("f2", "e3", FN_APPROACH) == 1);
    CHECK(fn_status(&G) == FS_PLAY);
}

/* approach: the line in front of the step's end; withdrawal: the line behind its start */
static void test_approach_withdrawal(void)
{
    setup("a3", "c3 d3 e3 g3", 0);              /* a3-b3 approaches c3: c3 d3 e3 go, not g3 (f3 empty) */
    CHECK(count("a3", "b3", FN_APPROACH) == 1 && count("a3", "b3", FN_WITHDRAW) == 0);
    CHECK(fn_script(&G, "a3-b3"));
    CHECK(at("c3") < 0 && at("d3") < 0 && at("e3") < 0 && at("g3") == 1 && pos()->left[1] == 1);
    setup("a3 f3", "c3 d3 e3 g3", 0);           /* the enemy line stops at a piece of one's own too */
    CHECK(fn_script(&G, "a3-b3") && at("e3") < 0 && at("g3") == 1 && pos()->left[1] == 1);
    setup("e3", "f3 g3 h3 b3", 0);              /* e3-d3 withdraws from f3: f3 g3 h3 go; b3 is not in reach */
    CHECK(count("e3", "d3", FN_WITHDRAW) == 1 && count("e3", "d3", FN_APPROACH) == 0);
    CHECK(fn_script(&G, "e3-d3") && at("f3") < 0 && at("g3") < 0 && at("h3") < 0 && at("b3") == 1);
    setup("c3", "e5 b2 a1", 0);                 /* diagonals: c3 strong, c3-d4 approaches e5, withdraws from b2 */
    CHECK(count("c3", "d4", FN_APPROACH) == 1 && count("c3", "d4", FN_WITHDRAW) == 1);
    CHECK(fn_script(&G, "c3-d4w") && at("b2") < 0 && at("a1") < 0 && at("e5") == 1);
    setup("b3", "d5 a2", 0);                    /* b3 is a weak point: no diagonal steps from it */
    CHECK(count("b3", "c4", -1) == 0 && count("b3", "a2", -1) == 0);
    setup("a1", "a4", 0);                       /* nothing to take beyond an empty point: only paika moves */
    CHECK(nlegal() == 3 && L[0].kind == FN_PAIKA && L[1].kind == FN_PAIKA && L[2].kind == FN_PAIKA);
    setup("a1", "a3", 0);                       /* a1-a2 approaches a3: the only move */
    CHECK(nlegal() == 1 && count("a1", "a2", FN_APPROACH) == 1);
    fnmove m = {(int8_t)pt("a1"), (int8_t)pt("a2"), FN_APPROACH};
    int8_t t[FN_MAX_TAKE];
    CHECK(fn_taken(pos(), m, t) == 1 && t[0] == pt("a3"));
}

/* the choice: a step that both approaches and withdraws takes one line, the player's pick */
static void test_choice(void)
{
    setup("e3", "g3 h3 d3 c3 b3", 0);           /* e3-f3: approach takes g3 h3, withdrawal d3 c3 b3 */
    CHECK(count("e3", "f3", FN_APPROACH) == 1 && count("e3", "f3", FN_WITHDRAW) == 1);
    CHECK(!fn_script(&G, "e3-f3"));             /* the notation must say which */
    CHECK(fn_script(&G, "e3-f3a") && at("g3") < 0 && at("h3") < 0 && at("d3") == 1 && pos()->left[1] == 3);
    setup("e3", "g3 h3 d3 c3 b3", 0);
    CHECK(fn_script(&G, "e3-f3w") && at("d3") < 0 && at("b3") < 0 && at("g3") == 1 && pos()->left[1] == 2);
    fn_new(&G);                                 /* the opening's choice: d3-e3 takes f3 or c3 */
    CHECK(fn_script(&G, "d3-e3a") && at("f3") < 0 && at("c3") == 1 && pos()->side == 1);
    fn_new(&G);
    CHECK(fn_script(&G, "d3-e3w") && at("c3") < 0 && at("f3") == 1);
}

/* capturing is compulsory at the start of a turn: no paika while a capture is possible */
static void test_compulsory(void)
{
    setup("a1 e3", "g3", 0);                    /* e3 can take g3 (e3-f3); a1 may not move */
    int n = nlegal(), caps = 1;
    for (int i = 0; i < n; i++) caps &= L[i].kind != FN_PAIKA;
    CHECK(n == 1 && caps && L[0].from == pt("e3"));
    CHECK(!fn_script(&G, "a1-a2") && !fn_script(&G, "e3-e4") && fn_script(&G, "e3-f3"));
    setup("a1", "i5", 0);                       /* no capture anywhere: paika moves */
    n = nlegal();
    caps = 0;
    for (int i = 0; i < n; i++) caps |= L[i].kind != FN_PAIKA;
    CHECK(n == 3 && !caps);
    fn_new(&G);                                 /* a real game: every turn begins with a capture while one exists */
    int ok = 1;
    for (int k = 0; k < 200 && fn_status(&G) == FS_PLAY; k++) {
        int m = nlegal(), anycap = 0, anypaika = 0;
        for (int i = 0; i < m; i++) anycap |= L[i].kind != FN_PAIKA, anypaika |= L[i].kind == FN_PAIKA;
        ok &= !(anycap && anypaika);
        fn_play(&G, L[(k * 7) % m]);
    }
    CHECK(ok);
}

/* chains: the same piece captures on, not onto a point it stood on, not twice in a row in one direction */
static void test_chains(void)
{
    /* a chain of three: c3-d3 approaches e3; d3-d4 approaches d5; d4-c4 withdraws from e4 */
    setup("c3 i1", "e3 d5 e4 a1", 0);
    CHECK(count("c3", "d3", FN_APPROACH) == 1);
    CHECK(fn_script(&G, "c3-d3+") && pos()->chain == pt("d3") && at("e3") < 0);
    CHECK(nlegal() == 1 && count("d3", "d4", FN_APPROACH) == 1);   /* only d3 moves now, and only to capture */
    CHECK(fn_script(&G, "d3-d4+") && at("d5") < 0 && pos()->chain == pt("d4"));
    CHECK(count("d4", "c4", FN_WITHDRAW) == 1 && fn_script(&G, "d4-c4") && at("e4") < 0);
    CHECK(pos()->side == 1 && pos()->chain < 0 && pos()->left[1] == 1);   /* nothing more: the turn ended */
    /* never twice running in the same direction, even to capture: c2-d2 withdraws from b2 (east), then d2-e2 east
     * again would approach f2 */
    setup("c2 i5", "b2 f2 d4", 0);
    CHECK(fn_script(&G, "c2-d2+") && at("b2") < 0 && pos()->chain == pt("d2") && pos()->side == 0);
    CHECK(count("d2", "e2", -1) == 0 && count("d2", "d3", FN_APPROACH) == 1 && nlegal() == 1);
    setup("c2 i5", "b2 f2", 0);                 /* without d4 nothing else is left: the turn ends at once */
    CHECK(fn_script(&G, "c2-d2+") && pos()->side == 1 && at("f2") == 1);
    /* never back onto a point it stood on, the start too: after c3-d3 (approaching e3), d3-c3 would approach b3 */
    setup("c3 i1", "e3 b3 d5", 0);
    CHECK(count("c3", "d3", FN_APPROACH) == 1 && count("c3", "d3", FN_WITHDRAW) == 1);
    CHECK(fn_script(&G, "c3-d3a+") && pos()->chain == pt("d3") && at("b3") == 1);
    CHECK(count("d3", "c3", -1) == 0 && count("d3", "d4", FN_APPROACH) == 1);
    /* nor onto a point visited mid-chain: a2-b2 approaches c2, b2-b3 approaches b4; b3-b2 would approach b1 */
    setup("a2 i5", "c2 b4 b1 d3", 0);
    CHECK(fn_script(&G, "a2-b2+") && fn_script(&G, "b2-b3a+") && pos()->chain == pt("b3") && at("b1") == 1);
    CHECK(count("b3", "b2", -1) == 0 && count("b3", "c3", FN_APPROACH) == 1);
}

/* stopping: going on is optional; the turn also ends by itself when nothing more can be taken */
static void test_stop(void)
{
    setup("c3 i5", "e3 d5", 0);                 /* after c3-d3 (takes e3), d3-d4 could take d5 */
    CHECK(fn_script(&G, "c3-d3+") && pos()->chain == pt("d3") && fn_status(&G) == FS_PLAY);
    CHECK(!fn_script(&G, "i5-i4"));             /* only the chain's piece moves */
    CHECK(fn_script(&G, "stop") && pos()->side == 1 && pos()->chain < 0 && at("d5") == 1);
    setup("c3 i5", "e3 d5", 0);                 /* the same written as one word: the space stops it */
    CHECK(fn_script(&G, "c3-d3") && pos()->side == 1 && G.ply == 2);
    setup("c3 i5", "e3 d5", 0);                 /* or it goes on */
    CHECK(fn_script(&G, "c3-d3-d4") && pos()->side == 1 && pos()->left[1] == 0);
    setup("a1 i5", "c1 e5", 0);                 /* nothing more to take: the turn ends at once */
    CHECK(fn_script(&G, "a1-b1+") && pos()->side == 1 && pos()->chain < 0);
    CHECK(!fn_script(&G, "stop"));              /* no chain: nothing to stop */
}

/* the ends: no pieces, no move; the draws */
static void test_ends(void)
{
    setup("a1", "c1", 0);                       /* the last piece taken: player 1 has none, and lost */
    CHECK(fn_script(&G, "a1-b1") && pos()->left[1] == 0 && fn_status(&G) == FS_LOST && pos()->side == 1);
    setup("a1", "b1 a2 b2", 0);                 /* walled in: ivory's only piece has no empty neighbour, and lost */
    CHECK(nlegal() == 0 && fn_status(&G) == FS_LOST);
    setup("a1", "i5", 0);                       /* back and forth: the same position a third time */
    CHECK(fn_script(&G, "a1-a2 i5-i4 a2-a1 i4-i5 a1-a2 i5-i4 a2-a1"));
    CHECK(fn_status(&G) == FS_PLAY);
    CHECK(fn_script(&G, "i4-i5") && fn_status(&G) == FS_REPETITION);
    setup("a1", "i5", 0);                       /* fifty moves each without a capture */
    G.pos[0].quiet = FN_QUIET - 1;
    CHECK(fn_status(&G) == FS_PLAY && fn_script(&G, "a1-a2") && fn_status(&G) == FS_QUIET);
    setup("a1 e5", "a3 i1", 0);                 /* a capture starts the count again */
    G.pos[0].quiet = FN_QUIET - 1;
    CHECK(fn_script(&G, "a1-a2") && pos()->quiet == 0 && fn_status(&G) == FS_PLAY);
}

/* undo: back one step at a time, all the way to the start */
static void test_undo(void)
{
    fn_new(&G);
    fnpos start = *pos();
    CHECK(!fn_undo(&G));
    CHECK(fn_script(&G, "d3-e3w"));
    CHECK(G.ply == 1 && pos()->left[1] == 21 && pos()->side == 1);
    int n = 0;
    while (fn_undo(&G)) n++;
    CHECK(n == 1 && !memcmp(pos(), &start, sizeof start));
    setup("c3 i5", "e3 d5", 0);
    CHECK(fn_script(&G, "c3-d3+") && fn_undo(&G) && pos()->chain < 0 && at("c3") == 0 && at("e3") == 1);
    CHECK(!fn_script(&G, "c3-z3") && !fn_script(&G, "zz"));
}

int main(void)
{
    test_board();
    test_approach_withdrawal();
    test_choice();
    test_compulsory();
    test_chains();
    test_stop();
    test_ends();
    test_undo();
    printf("fanorona: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
