/*
 * Fox and geese: the rules' test (the board, the geese's steps, the fox's jumps and chains, the ends, draws, undo).
 * No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/foxgeese.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static fggame G;
static fgmove L[FG_MAX_MOVES];

/* a position: the geese ("c3 d5"), the fox's point, who moves */
static void setup(const char *geese, const char *fox, int side)
{
    fg_new(&G);
    fgpos *p = &G.pos[0];
    memset(p->at, -1, sizeof p->at);
    p->geese = 0;
    for (const char *t = geese; *t; t++) {
        int pt = fg_point(t);
        if (pt < 0) continue;
        p->at[pt] = FG_GOOSE;
        p->geese++;
        t++;
    }
    p->fox = (int8_t)fg_point(fox);
    p->at[p->fox] = FG_FOX;
    p->side = (uint8_t)side;
}

static const fgpos *pos(void) { return fg_pos(&G); }
static int nlegal(void) { return fg_legal(pos(), L); }
static int at(const char *pt) { return pos()->at[fg_point(pt)]; }

/* the board: 33 points, 72 lines between neighbours (52 along rows and columns, 20 diagonals), the start */
static void test_board(void)
{
    fg_init();
    int n = 0, ok = 1, diag = 0;
    for (int i = 0; i < FG_POINTS; i++) {
        char name[3] = {(char)('a' + fg_x[i]), (char)('1' + fg_y[i]), 0};
        CHECK(fg_point(name) == i);
        for (int d = 0; d < 8; d++) {
            int j = fg_next[i][d];
            if (j < 0) continue;
            n++;
            diag += fg_dx[d] && fg_dy[d];
            ok &= fg_next[j][(d + 4) % 8] == i && fg_x[j] == fg_x[i] + fg_dx[d] && fg_y[j] == fg_y[i] + fg_dy[d];
        }
    }
    CHECK(ok && n == 2 * 72 && diag == 2 * 20);
    CHECK(fg_point("a1") < 0 && fg_point("b2") < 0 && fg_point("f6") < 0 && fg_point("h4") < 0);
    fg_new(&G);
    CHECK(pos()->geese == 13 && pos()->side == 0 && at("d4") == FG_FOX && at("g3") == FG_GOOSE && at("d1") == 0);
    CHECK(nlegal() == 10);                      /* the third row: a3 2, b3 1, c3 2, d3 0, e3 2, f3 1, g3 2 */
}

/* the geese: forwards and sideways, never backwards, never a jump */
static void test_geese(void)
{
    setup("d5", "d1", 0);                       /* d5: no diagonals there */
    CHECK(nlegal() == 3);
    setup("c5", "d1", 0);                       /* c5: up, up and right, both sides */
    CHECK(nlegal() == 4);
    CHECK(!fg_script(&G, "c5-c4") && !fg_script(&G, "c5-b4") && fg_script(&G, "c5-b5") && pos()->side == 1);
    setup("d3 d4", "d5", 0);                    /* no jumping over the fox, nor over a goose */
    CHECK(!fg_script(&G, "d3-d5") && !fg_script(&G, "d4-d6"));
    fg_new(&G);                                 /* a whole game's worth of geese moves: never down */
    int ok = 1;
    for (int k = 0; k < 40 && fg_status(&G) == FS_PLAY; k++) {
        int n = nlegal();
        for (int i = 0; i < n && pos()->side == 0; i++) ok &= fg_y[L[i].to] >= fg_y[L[i].from] && L[i].over < 0;
        fg_play(&G, L[(k * 7) % n]);
    }
    CHECK(ok);
}

/* the fox: steps every way, a jump not compulsory, a chain of jumps that must go on */
static void test_fox(void)
{
    setup("d5 d3 a3 b3 f3 g3 c1 d1 e1", "d6", 1);
    CHECK(nlegal() == 7 + 1);                   /* steps all round but d5, the jump over d5 */
    CHECK(fg_script(&G, "d6-c6") && pos()->side == 0 && pos()->geese == 9);   /* the fox may step instead */
    fg_undo(&G);
    CHECK(fg_script(&G, "d6xd4") && pos()->side == 1 && pos()->chain && at("d5") < 0 && pos()->geese == 8);
    CHECK(nlegal() == 1 && L[0].over == fg_point("d3") && L[0].to == fg_point("d2"));
    CHECK(fg_status(&G) == FS_FEW);            /* 8 geese left: already won */
    setup("d5 d3 a3 b3 f3 g3 c1 d1 e1 a5", "d6", 1);
    CHECK(fg_script(&G, "d6xd4xd2") && pos()->side == 0 && !pos()->chain && pos()->geese == 8 && at("d2") == 1);
    setup("e4 c1 d1 e1 c2 d2 e2 a3 b3", "d5", 1);   /* no diagonal through d5: no jump over e4, four steps */
    CHECK(!fg_script(&G, "d5xf3") && nlegal() == 4);
    setup("e5 c3 a3 b3 f3 g3 c1 d1 e1 a5 g5", "e6", 1);   /* one jump, then none: the geese's turn */
    CHECK(fg_script(&G, "e6xe4") && pos()->side == 0 && !pos()->chain && pos()->geese == 10);
}

/* the ends: the fox trapped, fewer than 9 geese, geese that cannot move */
static void test_ends(void)
{
    setup("c7 e7 d5 d4 c6 a3 b3 c3 e3 f3 g3", "d7", 0);
    CHECK(fg_status(&G) == FS_PLAY);
    CHECK(fg_script(&G, "c6-d6") && pos()->side == 1 && fg_status(&G) == FS_TRAPPED);
    setup("d5 a3 b3 c3 e3 f3 g3 c1 d1 e1", "d6", 1);
    CHECK(fg_script(&G, "d6xd4") && pos()->geese == 9 && fg_status(&G) == FS_PLAY);
    setup("d5 a3 b3 c3 e3 f3 g3 c1 d1", "d6", 1);
    CHECK(fg_script(&G, "d6xd4") && pos()->geese == 8 && fg_status(&G) == FS_FEW);
    setup("a5 b5 c5 d5 e5 f5 g5 c6 d6 e6 c7 d7 e7", "d3", 0);
    CHECK(fg_status(&G) == FS_STUCK);
    G.pos[0].side = 1;
    CHECK(fg_status(&G) == FS_PLAY);
}

/* draws: the third time the same position; 50 moves each without a capture or a goose going forwards */
static void test_draws(void)
{
    setup("c1 d1 e1 c2 d2 e2 a3 b3 d3", "d7", 0);
    CHECK(fg_script(&G, "d3-c3 d7-d6 c3-d3 d6-d7") && fg_status(&G) == FS_PLAY);
    CHECK(fg_script(&G, "d3-c3 d7-d6 c3-d3") && fg_status(&G) == FS_PLAY);
    CHECK(fg_script(&G, "d6-d7") && fg_status(&G) == FS_REPETITION);
    setup("c1 d1 e1 c2 d2 e2 a3 b3 d3", "d7", 0);
    G.pos[0].quiet = FG_QUIET - 2;
    CHECK(fg_script(&G, "d3-c3") && fg_status(&G) == FS_PLAY);
    CHECK(fg_script(&G, "d7-d6") && fg_status(&G) == FS_QUIET);
    fg_undo(&G);
    CHECK(fg_undo(&G) && fg_script(&G, "d3-d4") && pos()->quiet == 0);   /* forwards: the count starts again */
}

/* undo: a move at a time, through a chain of jumps, back to the start */
static void test_undo(void)
{
    setup("d5 d3 a3 b3 f3 g3 c1 d1 e1 a5", "d6", 1);
    fgpos start = *pos();
    CHECK(fg_script(&G, "d6xd4xd2"));
    CHECK(fg_undo(&G) && pos()->chain && pos()->side == 1 && at("d3") == 0 && at("d4") == 1 && pos()->geese == 9);
    while (fg_undo(&G)) {}
    CHECK(G.ply == 0 && !memcmp(pos(), &start, sizeof start) && !fg_undo(&G));
    fg_new(&G);
    CHECK(!fg_script(&G, "d4-d5") && !fg_script(&G, "c3") && !fg_script(&G, "b2-b3") && !fg_script(&G, "zz"));
}

int main(void)
{
    test_board();
    test_geese();
    test_fox();
    test_ends();
    test_draws();
    test_undo();
    printf("foxgeese: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
