/*
 * The Game of the Goose: the rules' test (src/games/goose_rules.c). The board's squares, the geese (and their chains,
 * backwards after a bounce), the bridge, the inn, the well, the maze, the prison, death, the opening throws, the
 * exact finish and the bounce off 63, the swap of two pawns, two held pawns, the turn round the table, undo; whole
 * random games from seeds for two to six players. No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>

#include "games/goose.h"

static int checks, failed;

#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

/* a game of `players` with the pawns set (pos: one per player) and `turn` to throw */
static void setup(go_game *g, int players, const int *pos, int turn)
{
    go_new(g, players);
    for (int p = 0; p < players; p++) g->pos[p] = pos[p];
    g->turn = turn;
}

static int steps_are(const go_move *m, const int *sq, const int *how, int n)
{
    if (m->n != n) return 0;
    for (int i = 0; i < n; i++)
        if (m->step[i].sq != sq[i] || m->step[i].how != how[i]) return 0;
    return 1;
}

static uint32_t rng = 777;
static int die(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return 1 + (int)(rng % 6);
}

/* the rules' invariants: squares in range, one pawn a square (but the start), the held ones in the well or the
 * prison, the one to throw free */
static int sane(const go_game *g)
{
    for (int p = 0; p < g->players; p++) {
        if (g->pos[p] < 0 || g->pos[p] > GO_LAST) return 0;
        if (g->wait[p] == GO_HELD && go_kind(g->pos[p]) != GK_WELL && go_kind(g->pos[p]) != GK_PRISON) return 0;
        if (g->wait[p] > 0 && go_kind(g->pos[p]) != GK_INN) return 0;
        for (int q = p + 1; q < g->players; q++)
            if (g->pos[p] && g->pos[p] == g->pos[q]) return 0;
    }
    return g->winner >= 0 || g->wait[g->turn] == 0;
}

int main(void)
{
    go_game g;

    /* the board: thirteen geese in two series of nine, the six hazards, the dice squares, the garden */
    int geese = 0, ok = 1;
    for (int s = 1; s < GO_LAST; s++) geese += go_kind(s) == GK_GOOSE;
    const int gs[] = {5, 9, 14, 18, 23, 27, 32, 36, 41, 45, 50, 54, 59};
    for (int i = 0; i < 13; i++) ok &= go_kind(gs[i]) == GK_GOOSE;
    CHECK(geese == 13 && ok);
    CHECK(go_kind(6) == GK_BRIDGE && go_kind(19) == GK_INN && go_kind(31) == GK_WELL && go_kind(42) == GK_MAZE);
    CHECK(go_kind(52) == GK_PRISON && go_kind(58) == GK_DEATH && go_kind(63) == GK_GARDEN && go_kind(0) == GK_PLAIN);
    CHECK(go_kind(26) == GK_DICE && go_kind(53) == GK_DICE && go_kind(12) == GK_PLAIN && go_kind(30) == GK_PLAIN);

    /* the start: everyone off the board, player 0 throws; a plain move; bad dice refused */
    go_new(&g, 2);
    CHECK(go_turn(&g) == 0 && go_status(&g) == GO_PLAY && g.pos[0] == 0 && g.pos[1] == 0);
    CHECK(!go_play(&g, 0, 3) && !go_play(&g, 2, 7) && g.n == 0);
    CHECK(go_script(&g, "12") && g.pos[0] == 3 && go_turn(&g) == 1 && g.last.to == 3 && g.last.n == 3);
    CHECK(!go_script(&g, "17") && !go_script(&g, "1"));

    /* a goose: again by the same throw; goose after goose */
    go_new(&g, 2);
    CHECK(go_script(&g, "23") && g.pos[0] == 10 && g.last.n == 10);   /* 5, then 10 */
    setup(&g, 2, (int[]){1, 0}, 0);
    CHECK(go_play(&g, 1, 3) && g.pos[0] == 13 && g.last.n == 12);      /* 5, 9, 13 */

    /* the bridge: 6 to 12, along the spiral */
    go_new(&g, 2);
    CHECK(go_script(&g, "33") && g.pos[0] == 12);
    CHECK(steps_are(&g.last, (int[]){1, 2, 3, 4, 5, 6, 12},
                    (int[]){GO_HOP, GO_HOP, GO_HOP, GO_HOP, GO_HOP, GO_HOP, GO_GLIDE}, 7));

    /* the opening throws: a 9 from the start, 6 and 3 to 26, 5 and 4 to 53, through the air */
    go_new(&g, 2);
    CHECK(go_script(&g, "63 45") && g.pos[0] == 26 && g.pos[1] == 53);
    CHECK(g.last.n == 1 && g.last.step[0].sq == 53 && g.last.step[0].how == GO_FLY);
    go_new(&g, 2);
    CHECK(go_script(&g, "36 54") && g.pos[0] == 26 && g.pos[1] == 53);
    setup(&g, 2, (int[]){8, 0}, 0);              /* a 9 elsewhere: a plain 9 (8 + 9 = 17) */
    CHECK(go_play(&g, 6, 3) && g.pos[0] == 17);

    /* the maze: 42 back to 30; death: 58 back to the start, and from there the opening rule again */
    setup(&g, 2, (int[]){37, 0}, 0);
    CHECK(go_play(&g, 2, 3) && g.pos[0] == 30 && g.last.step[g.last.n - 1].how == GO_GLIDE);
    setup(&g, 2, (int[]){53, 0}, 0);
    CHECK(go_play(&g, 1, 4) && g.pos[0] == 0 && g.last.step[g.last.n - 1].how == GO_FLY && g.last.to == 0);
    g.turn = 0;
    CHECK(go_play(&g, 5, 4) && g.pos[0] == 53);

    /* 63: the exact throw wins; more bounces back by the extra points */
    setup(&g, 2, (int[]){60, 10}, 0);
    CHECK(go_play(&g, 3, 3) && g.pos[0] == 60 && go_status(&g) == GO_PLAY);   /* 61 62 63 62 61 60 */
    CHECK(steps_are(&g.last, (int[]){61, 62, 63, 62, 61, 60}, (int[]){0, 0, 0, 0, 0, 0}, 6));
    setup(&g, 2, (int[]){57, 10}, 0);
    CHECK(go_play(&g, 4, 4) && g.pos[0] == 61);
    setup(&g, 2, (int[]){61, 10}, 0);
    CHECK(go_play(&g, 1, 1) && g.pos[0] == 63 && go_status(&g) == GO_WIN0 && g.winner == 0);
    CHECK(!go_play(&g, 1, 1) && g.n == 1);
    /* a bounce onto a goose goes on backwards; onto death, back to the start */
    setup(&g, 2, (int[]){61, 10}, 0);
    CHECK(go_play(&g, 3, 3) && g.pos[0] == 53);  /* 67: 59, a goose, 6 more back: 53 */
    setup(&g, 2, (int[]){61, 10}, 0);
    CHECK(go_play(&g, 3, 4) && g.pos[0] == 0);   /* 68: 58 */
    setup(&g, 3, (int[]){62, 10, 20}, 2);        /* player 2 wins */
    g.pos[2] = 59 - 4;                            /* 55 + 8 = 63 */
    CHECK(go_play(&g, 4, 4) && go_status(&g) == GO_WIN0 + 2);

    /* the inn: two turns missed */
    setup(&g, 2, (int[]){13, 2}, 0);
    CHECK(go_play(&g, 3, 3) && g.pos[0] == 19 && g.wait[0] == 2 && go_turn(&g) == 1);
    CHECK(go_play(&g, 1, 1) && go_turn(&g) == 1 && g.wait[0] == 1);   /* 0 misses one */
    CHECK(go_play(&g, 1, 1) && go_turn(&g) == 1 && g.wait[0] == 0);   /* and two */
    CHECK(go_play(&g, 1, 1) && go_turn(&g) == 0);
    CHECK(g.pos[1] == 16);                        /* 4, 6 (the bridge: 12), 14 (a goose: 16) */
    /* with three: the others go on round the table */
    setup(&g, 3, (int[]){13, 2, 3}, 0);
    CHECK(go_play(&g, 3, 3) && go_turn(&g) == 1 && go_play(&g, 1, 1) && go_turn(&g) == 2);
    CHECK(go_play(&g, 1, 1) && go_turn(&g) == 1 && g.wait[0] == 1);

    /* the well: held until another pawn falls in; that one sends it to where it came from, and stays */
    setup(&g, 3, (int[]){25, 2, 29}, 0);
    CHECK(go_play(&g, 3, 3) && g.pos[0] == 31 && g.wait[0] == GO_HELD);
    CHECK(go_play(&g, 1, 2) && go_turn(&g) == 2);                    /* 1: 2 to 5, a goose, 8 */
    CHECK(g.pos[1] == 8 && go_play(&g, 1, 1) && go_turn(&g) == 0);   /* 2: 31; 0 freed, to 29 */
    CHECK(g.pos[2] == 31 && g.wait[2] == GO_HELD && g.pos[0] == 29 && g.wait[0] == 0 && g.last.swap == 0);
    CHECK(go_play(&g, 1, 2) && go_turn(&g) == 1 && go_play(&g, 1, 1) && go_turn(&g) == 0);   /* 2 passed over */
    /* the prison the same */
    setup(&g, 3, (int[]){47, 46, 1}, 0);
    CHECK(go_play(&g, 2, 3) && g.pos[0] == 52 && g.wait[0] == GO_HELD);
    CHECK(go_play(&g, 3, 3) && g.pos[1] == 52 && g.pos[0] == 46 && g.wait[0] == 0 && g.wait[1] == GO_HELD);

    /* two players, one in the well, the other going to prison: nobody can free them, the next is freed */
    setup(&g, 2, (int[]){25, 47}, 0);
    CHECK(go_play(&g, 3, 3) && g.wait[0] == GO_HELD && go_turn(&g) == 1);
    CHECK(go_play(&g, 2, 3) && g.wait[1] == GO_HELD && g.last.freed == 0 && go_turn(&g) == 0 && g.wait[0] == 0);
    CHECK(go_play(&g, 1, 1) && g.pos[0] == 33 && go_turn(&g) == 0);  /* 1 still held: 0 again */

    /* a swap: the pawn on the square goes where the mover came from; passing over another does nothing */
    setup(&g, 2, (int[]){10, 4}, 1);
    CHECK(go_play(&g, 2, 4) && g.pos[1] == 10 && g.pos[0] == 4 && g.last.swap == 0);
    setup(&g, 2, (int[]){7, 4}, 1);
    CHECK(go_play(&g, 2, 4) && g.pos[1] == 10 && g.pos[0] == 7 && g.last.swap == -1);
    /* swapped out of the inn: free again; the mover waits instead */
    setup(&g, 2, (int[]){13, 15}, 0);
    CHECK(go_play(&g, 3, 3) && g.wait[0] == 2 && go_play(&g, 2, 2) && g.pos[1] == 19 && g.pos[0] == 15);
    CHECK(g.wait[0] == 0 && g.wait[1] == 2 && go_turn(&g) == 0);
    /* the start holds them all */
    go_new(&g, 3);
    CHECK(go_script(&g, "11 11") && g.pos[1] == 2 && g.pos[0] == 0 && g.last.swap == 0);   /* 0 sent to the start */
    setup(&g, 3, (int[]){53, 0, 0}, 0);
    CHECK(go_play(&g, 1, 4) && g.pos[0] == 0 && g.last.swap == -1);  /* death: the start, with the others */

    /* undo: the throws come back the same, in order */
    go_new(&g, 3);
    CHECK(go_script(&g, "12 63 54 22"));
    int p0 = g.pos[0], p1 = g.pos[1], p2 = g.pos[2], d1, d2;
    CHECK(go_undo(&g) && g.n == 3 && go_next(&g, &d1, &d2) && d1 == 2 && d2 == 2 && go_turn(&g) == 0);
    CHECK(go_play(&g, 2, 2) && g.pos[0] == p0 && g.pos[1] == p1 && g.pos[2] == p2 && g.known == 4);
    CHECK(go_undo(&g) && go_undo(&g) && go_play(&g, 1, 1) && g.known == 3 && !go_next(&g, &d1, &d2));
    while (go_undo(&g)) {}
    CHECK(g.n == 0 && g.pos[0] == 0 && g.pos[1] == 0 && go_turn(&g) == 0 && g.players == 3);
    CHECK(go_script(&g, "12 63") && go_last_throw(&g, 1, &d1, &d2) && d1 == 6 && d2 == 3);
    CHECK(!go_last_throw(&g, 2, &d1, &d2));

    /* six players: round the table */
    go_new(&g, 6);
    ok = 1;
    for (int i = 0; i < 12; i++) ok &= go_turn(&g) == i % 6 && go_play(&g, 1, 1 + i % 2);   /* 2s and 3s */
    CHECK(ok && go_turn(&g) == 0);
    go_new(&g, 9);
    CHECK(g.players == GO_PLAYERS);

    /* whole random games, two to six players: they end, the rules hold all along, undo goes back to the start */
    ok = 1;
    int longest = 0;
    for (int seed = 1; seed <= 600; seed++) {
        int np = 2 + seed % 5;
        rng = (uint32_t)seed * 2654435761u + 1;
        go_new(&g, np);
        while (go_status(&g) == GO_PLAY && g.n < 4000) {
            ok &= go_play(&g, die(), die()) && sane(&g);
            ok &= g.last.to == g.pos[g.last.player];
        }
        ok &= go_status(&g) != GO_PLAY && g.pos[g.winner] == GO_LAST;
        if (g.n > longest) longest = g.n;
        int n = g.n, win = g.winner;
        if (seed % 50 == 0) {                   /* back and forth: the same end */
            while (go_undo(&g)) {}
            ok &= g.n == 0 && g.pos[0] == 0 && g.turn == 0;
            for (int i = 0; i < n; i++) {
                ok &= go_next(&g, &d1, &d2) && go_play(&g, d1, d2) && sane(&g);
            }
            ok &= g.winner == win;
        }
    }
    CHECK(ok && longest < 4000);

    printf("goose: %d checks, %d failed (longest game %d throws)\n", checks, failed, longest);
    return failed != 0;
}
