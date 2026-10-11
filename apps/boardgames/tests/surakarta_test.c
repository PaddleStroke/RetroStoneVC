/*
 * Surakarta: the rules' test (the circuits and their loops, the steps, the captures' runs: through a loop, blocked,
 * crossing their own starting point, the long way round; the ends, the count of pieces, undo). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/surakarta.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static skgame G;
static skmove L[SK_MAX_MOVES];

/* a position: player 0's pieces ("c3 d5"), player 1's, who moves */
static void setup(const char *light, const char *dark, int side)
{
    sk_new(&G);
    skpos *p = &G.pos[0];
    memset(p->at, -1, sizeof p->at);
    p->left[0] = p->left[1] = 0;
    const char *list[2] = {light, dark};
    for (int c = 0; c < 2; c++)
        for (const char *t = list[c]; *t; t++) {
            int pt = sk_point(t);
            if (pt < 0) continue;
            p->at[pt] = (int8_t)c;
            p->left[c]++;
            t++;
        }
    p->side = (uint8_t)side;
}

static const skpos *pos(void) { return sk_pos(&G); }
static int nlegal(void) { return sk_legal(pos(), L); }
static int at(const char *pt) { return pos()->at[sk_point(pt)]; }

/* the legal move from a to b, or NULL */
static const skmove *find(const char *a, const char *b)
{
    int n = nlegal();
    for (int i = 0; i < n; i++)
        if (L[i].from == sk_point(a) && L[i].to == sk_point(b)) return &L[i];
    return NULL;
}

static int captures(void)
{
    int n = nlegal(), k = 0;
    for (int i = 0; i < n; i++) k += L[i].circuit >= 0;
    return k;
}

/* the circuits: straight steps between neighbours on one line, loops round a corner at their radius; the lines 1..4
 * each run whole in one circuit; the start */
static void test_board(void)
{
    int ok = 1, seen[2][SK_POINTS] = {{0}};
    for (int c = 0; c < 2; c++)
        for (int i = 0; i < SK_TRACK; i++) {
            int a = sk_track[c][i], b = sk_track[c][(i + 1) % SK_TRACK], r = c + 1;
            seen[c][a]++;
            int dx = abs(sk_x(a) - sk_x(b)), dy = abs(sk_y(a) - sk_y(b));
            if (sk_loop_after(i)) {
                int k = sk_corner(c, i), ka = abs(sk_x(a) - sk_x(k)) + abs(sk_y(a) - sk_y(k));
                int kb = abs(sk_x(b) - sk_x(k)) + abs(sk_y(b) - sk_y(k));
                ok &= ka == r && kb == r && (sk_x(k) % 5 == 0) && (sk_y(k) % 5 == 0);
                ok &= (sk_x(a) == sk_x(k)) != (sk_x(b) == sk_x(k));   /* in on a column, out on a row, or back */
            } else {
                ok &= dx + dy == 1;
            }
        }
    CHECK(ok);
    for (int pt = 0; pt < SK_POINTS; pt++) {
        int x = sk_x(pt), y = sk_y(pt), on0 = (x == 1 || x == 4) + (y == 1 || y == 4);
        int on1 = (x == 2 || x == 3) + (y == 2 || y == 3);
        ok &= seen[0][pt] == on0 && seen[1][pt] == on1;
    }
    CHECK(ok);
    CHECK(sk_corner(0, 5) == sk_point("a6") && sk_corner(1, 11) == sk_point("f6") && sk_corner(0, 23) == 0);
    CHECK(sk_point("g1") < 0 && sk_point("a7") < 0 && sk_point("f6") == 35);
    sk_new(&G);
    CHECK(pos()->side == 0 && pos()->left[0] == 12 && pos()->left[1] == 12);
    CHECK(at("a1") == 0 && at("f2") == 0 && at("a5") == 1 && at("f6") == 1 && at("c3") < 0 && at("d4") < 0);
    CHECK(nlegal() == 16 && captures() == 0);   /* the second row steps forwards; every run is blocked */
    CHECK(sk_status(&G) == SS_PLAY && sk_winner(&G) == -1);
}

/* steps: to the 8 neighbours, empty ones only, never off the board */
static void test_steps(void)
{
    setup("c3", "a6", 0);
    CHECK(nlegal() == 8);
    CHECK(sk_script(&G, "c3-d4") && at("d4") == 0 && pos()->side == 1);
    setup("a1 b1", "f6", 0);                    /* a corner: three neighbours, one taken */
    CHECK(find("a1", "a2") && find("a1", "b2") && !find("a1", "b1"));
    CHECK(!sk_script(&G, "a1-c3") && !sk_script(&G, "a1-b1"));
    setup("c3", "d4", 0);                       /* no step onto a piece: d4 is taken by a run round a4 */
    CHECK(nlegal() == 8 && find("c3", "d4") && find("c3", "d4")->circuit == 1);
}

/* captures: along a circuit, through a loop at least, onto the first piece met */
static void test_runs(void)
{
    setup("a2", "b3", 0);                       /* a2 runs left, round the small loop of a1, up the b line */
    const skmove *m = find("a2", "b3");
    CHECK(m && m->circuit == 0 && sk_loops(*m) == 1 && m->steps == 3);
    CHECK(sk_script(&G, "a2xb3") && at("b3") == 0 && at("a2") < 0 && pos()->left[1] == 0 && pos()->quiet == 0);
    setup("c1", "a3", 0);                       /* the big loop: c1 down, round, out along the 3rd row */
    m = find("c1", "a3");
    CHECK(m && m->circuit == 1 && m->steps == 1 && sk_loops(*m) == 1);
    CHECK(fabsf(sk_length(*m) - 1.5f * 3.14159265f * 2) < 1e-3f);
    setup("b1", "b4", 0);                       /* straight up the b line: no loop, no capture; the long way round
                                                 * (all four loops) does */
    m = find("b1", "b4");
    CHECK(m && sk_loops(*m) == 4 && m->steps == 21);
    setup("b1 d5", "b4", 0);                    /* d5 blocks b1's long way; d5 itself takes b4 round a6's loop */
    CHECK(!find("b1", "b4"));
    m = find("d5", "b4");
    CHECK(m && sk_loops(*m) == 1);
    setup("a2", "c2 b3", 0);                    /* an enemy piece before the loop blocks the run: no capture of
                                                 * c2 (no loop), b3 reached round a1 */
    CHECK(!find("a2", "c2") && find("a2", "b3"));
    setup("a2", "b1 f6", 0);                    /* b1 is met right after the loop: taken */
    CHECK(find("a2", "b1") && sk_loops(*find("a2", "b1")) == 1);
    setup("a1 f1 a6 f6", "c3", 0);              /* the corners are on no circuit: they never capture */
    CHECK(captures() == 0);
    setup("b2", "f6", 0);                       /* alone on its circuits: round and back to itself, nothing taken */
    CHECK(captures() == 0);
}

/* the runs cross their own starting point as an empty point */
static void test_start_point(void)
{
    setup("b2", "b3", 0);                       /* b2 left to a2, round a1, up through b1 and b2 (itself) to b3 */
    const skmove *m = find("b2", "b3");
    CHECK(m && m->steps == 4 && sk_loops(*m) == 1 && m->dir == 1);
    setup("b2 b1", "b3", 0);                    /* b1 blocks that way: the only run left is the long way round */
    m = find("b2", "b3");
    CHECK(m && sk_loops(*m) == 3);
    setup("b2 b1 d5", "b3", 0);                 /* and d5 blocks it too */
    CHECK(!find("b2", "b3"));
    setup("e5", "e3", 1);                       /* player 1 too: e5 right to f5, round f6, down the e line through
                                                 * e5 to e4... e3 is ivory's: taken */
    G.pos[0].at[sk_point("e3")] = 0, G.pos[0].at[sk_point("e5")] = 1;
    m = find("e5", "e3");
    CHECK(m && sk_loops(*m) == 1 && m->steps == 5);
}

/* captures are never compulsory */
static void test_optional(void)
{
    setup("a2 f6", "b3", 0);
    int n = nlegal(), steps = 0;
    for (int i = 0; i < n; i++) steps += L[i].circuit < 0;
    CHECK(captures() == 1 && steps > 0);
    CHECK(sk_script(&G, "f6-e6") && at("b3") == 1);
}

/* the ends: no pieces, no move, the third repetition, 40 moves each without a capture; the count decides */
static void test_ends(void)
{
    setup("a2", "b3", 0);                       /* the last piece taken: player 1 has none, and lost */
    CHECK(sk_script(&G, "a2xb3") && sk_status(&G) == SS_STUCK && sk_winner(&G) == 0);
    setup("a1", "a2 b1 b2", 0);                 /* a1 hemmed in, on no circuit: no move, it lost */
    CHECK(nlegal() == 0 && sk_status(&G) == SS_STUCK && sk_winner(&G) == 1);
    setup("a1 a3", "f6", 0);                    /* the same position a third time: two pieces against one */
    CHECK(sk_script(&G, "a1-b1 f6-e6 b1-a1 e6-f6 a1-b1 f6-e6 b1-a1"));
    CHECK(sk_status(&G) == SS_PLAY);
    CHECK(sk_script(&G, "e6-f6") && sk_status(&G) == SS_REPETITION && sk_winner(&G) == 0);
    setup("a1", "f6", 0);                       /* 40 moves each without a capture, as many pieces: a draw */
    G.pos[0].quiet = SK_QUIET - 1;
    CHECK(sk_status(&G) == SS_PLAY && sk_script(&G, "a1-b1") && sk_status(&G) == SS_QUIET);
    CHECK(sk_winner(&G) == 2);
    setup("a1", "f6 f5", 1);                    /* ...more pieces: a win */
    G.pos[0].quiet = SK_QUIET - 1;
    CHECK(sk_script(&G, "f6-e6") && sk_status(&G) == SS_QUIET && sk_winner(&G) == 1);
    setup("a2 f6", "b3 e1", 0);                 /* a capture starts the count again */
    G.pos[0].quiet = SK_QUIET - 1;
    CHECK(sk_script(&G, "a2xb3") && pos()->quiet == 0 && sk_status(&G) == SS_PLAY);
}

/* a whole game of moves, legal ones only, and undo all the way back */
static void test_game(void)
{
    sk_new(&G);
    skpos start = *pos();
    CHECK(!sk_undo(&G));
    unsigned seed = 7;
    int plies = 0, caps = 0, ok = 1;
    while (sk_status(&G) == SS_PLAY && plies < 600) {
        int n = nlegal(), k = 0, before = pos()->left[0] + pos()->left[1];
        for (int i = 0; i < n; i++)
            if (L[i].circuit >= 0) k = i + 1;   /* take when it can, else a step at random */
        seed = seed * 1103515245u + 12345u;
        skmove m = k ? L[k - 1] : L[(seed >> 16) % (unsigned)n];
        ok &= m.circuit < 0 ? pos()->at[m.to] < 0 : pos()->at[m.to] == !pos()->side && sk_loops(m) > 0;
        sk_play(&G, m);
        caps += before - (pos()->left[0] + pos()->left[1]);
        plies++;
    }
    CHECK(ok && caps > 0 && sk_status(&G) != SS_PLAY && sk_winner(&G) >= 0);
    int n = 0;
    while (sk_undo(&G)) n++;
    CHECK(n == plies && !memcmp(pos(), &start, sizeof start));
    CHECK(!sk_script(&G, "a1") && !sk_script(&G, "zz-a1") && !sk_script(&G, "b2-b3x"));
}

int main(void)
{
    test_board();
    test_steps();
    test_runs();
    test_start_point();
    test_optional();
    test_ends();
    test_game();
    printf("surakarta: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
