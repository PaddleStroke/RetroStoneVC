/*
 * Snakes and ladders: the rules' test (src/games/snakes_rules.c). The zigzag numbering, the board's ladders and snakes,
 * climbing and sliding, the extra throw on a 6, the exact finish and the bounce off 100, the win, undo;
 * three and four players: the start, the turn round the table, pawns sharing a square, any one winning. No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>

#include "games/snakes.h"

static int checks, failed;

#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

/* throws "3 6 1" from the start; 0: a throw was refused */
static int play(sn_game *g, const char *s)
{
    sn_new(g);
    for (char *end; *s; s = end) {
        long r = strtol(s, &end, 10);
        if (end == s) break;
        if (!sn_play(g, (int)r)) return 0;
    }
    return 1;
}

/* the same with four players; 1: all played */
static int play4(sn_game *g, const char *s)
{
    sn_new_players(g, 4);
    for (char *end; *s; s = end) {
        long r = strtol(s, &end, 10);
        if (end == s) break;
        if (!sn_play(g, (int)r)) return 0;
    }
    return 1;
}

static int cell_is(int square, int col, int row)
{
    int c, r;
    sn_cell(square, &c, &r);
    return c == col && r == row;
}

int main(void)
{
    sn_game g;

    /* the zigzag: 1 bottom left, 10 bottom right, 11 above it, 100 top left */
    CHECK(cell_is(1, 0, 0) && cell_is(10, 9, 0) && cell_is(11, 9, 1) && cell_is(20, 0, 1) && cell_is(21, 0, 2));
    CHECK(cell_is(55, 5, 5) && cell_is(91, 9, 9) && cell_is(100, 0, 9));
    int seen[SN_LAST + 1] = {0}, all = 1;
    for (int s = 1; s <= SN_LAST; s++) {
        int c, r;
        sn_cell(s, &c, &r);
        all &= c >= 0 && c < SN_SIDE && r >= 0 && r < SN_SIDE && !seen[r * SN_SIDE + c]++;
    }
    CHECK(all);                                 /* every square has its own place */
    for (int s = 1; s < SN_LAST; s++) {         /* the next square is always beside it, or right above it */
        int c0, r0, c1, r1;
        sn_cell(s, &c0, &r0);
        sn_cell(s + 1, &c1, &r1);
        all &= abs(c1 - c0) + abs(r1 - r0) == 1;
    }
    CHECK(all);

    /* the board: 8 ladders up, 8 snakes down, no square shared, none from the start or the goal */
    int used[SN_LAST + 1] = {0}, ok = 1;
    for (int i = 0; i < SN_LINKS; i++) {
        const sn_link *l = &sn_links[i];
        ok &= (i < 8) == (l->to > l->from) && l->from > 1 && l->from < SN_LAST && l->to > 1 && l->to < SN_LAST;
        ok &= !used[l->from]++ && !used[l->to]++;
        ok &= sn_link_at(l->from) == i && sn_link_at(l->to) < 0;   /* no chains */
    }
    CHECK(ok);

    /* the start: both off the board; ivory throws first; a ladder climbs */
    sn_new(&g);
    CHECK(sn_status(&g) == SN_PLAY && sn_turn(&g) == 0 && g.pos[0] == 0 && g.pos[1] == 0 && sn_next(&g) == 0);
    CHECK(!sn_play(&g, 0) && !sn_play(&g, 7) && g.n == 0);
    CHECK(play(&g, "3") && g.last.land == 3 && g.last.to == 22 && g.pos[0] == 22 && sn_turn(&g) == 1);
    CHECK(play(&g, "2") && g.pos[0] == 2 && sn_turn(&g) == 1);

    /* a 6 throws again; any other number passes the die */
    CHECK(play(&g, "3 6") && g.pos[1] == 6 && g.last.again && sn_turn(&g) == 1);
    CHECK(play(&g, "3 6 6 1") && g.pos[1] == 13 && sn_turn(&g) == 0);
    CHECK(play(&g, "3 6 6 1 6 6 1") && g.pos[0] == 83 && g.pos[1] == 13 && sn_turn(&g) == 1);   /* 28: 76, 82, 83 */

    /* a snake slides: its head at 17, its tail at 7; two pawns may share a square */
    sn_new(&g);
    g.pos[0] = 14;
    g.pos[1] = 7;
    CHECK(sn_play(&g, 3) && g.last.land == 17 && g.last.to == 7 && g.pos[0] == 7 && g.pos[1] == 7);

    /* 100 by the exact number; a bigger throw bounces back from it by the excess */
    sn_new(&g);
    g.pos[0] = 96;
    CHECK(sn_play(&g, 6) && g.last.land == 98 && g.pos[0] == 98 && sn_status(&g) == SN_PLAY);
    CHECK(sn_hop(96, 4) == 100 && sn_hop(98, 5) == 97 && sn_hop(99, 6) == 95 && sn_hop(94, 6) == 100);
    sn_new(&g);
    g.pos[0] = 98;
    CHECK(sn_play(&g, 5) && g.last.land == 97 && g.pos[0] == 78);     /* bounced onto the snake at the top */

    /* the win: the first on 100; nothing after it, not even after a 6 */
    sn_new(&g);
    g.pos[1] = 96;
    g.turn = 1;
    CHECK(sn_play(&g, 4) && g.pos[1] == 100 && sn_status(&g) == SN_WIN1 && !sn_play(&g, 1));
    sn_new(&g);
    g.pos[0] = 94;
    CHECK(sn_play(&g, 6) && sn_status(&g) == SN_WIN0 && !sn_play(&g, 6) && g.n == 1);

    /* a whole game, to the win: 22 22, 28: 76 and 80: 99 for ivory, 24, then 100 */
    CHECK(play(&g, "3 3 6 4 2 1") && g.pos[0] == 100 && g.pos[1] == 24 && sn_status(&g) == SN_WIN0);
    CHECK(!sn_play(&g, 3) && g.n == 6);

    /* undo: the throws come back in order; one undone is thrown again, never a new one */
    CHECK(play(&g, "3 6 1") && sn_undo(&g) && g.n == 2 && g.pos[0] == 22 && g.pos[1] == 6 && sn_turn(&g) == 1);
    CHECK(sn_next(&g) == 1 && g.last.roll == 6 && g.last.player == 1);
    CHECK(sn_undo(&g) && sn_undo(&g) && g.n == 0 && g.pos[0] == 0 && sn_turn(&g) == 0 && !sn_undo(&g));
    CHECK(sn_next(&g) == 3 && sn_play(&g, 3) && sn_next(&g) == 6 && g.known == 3);
    CHECK(sn_play(&g, 2) && g.known == 2 && sn_next(&g) == 0);          /* another throw: the rest is gone */
    CHECK(play(&g, "3 6 1") && sn_undo(&g) && sn_play(&g, 1) && g.pos[1] == 7 && sn_turn(&g) == 0);
    CHECK(play(&g, "3 3 6 4 2 1") && sn_undo(&g) && sn_status(&g) == SN_PLAY && g.pos[0] == 99 && sn_turn(&g) == 0);

    /* three and four players: all off the board, the die round the table 0, 1, 2 (, 3), a 6 throws again */
    for (int np = 3; np <= 4; np++) {
        sn_new_players(&g, np);
        ok = g.players == np && sn_turn(&g) == 0 && sn_status(&g) == SN_PLAY;
        for (int p = 0; p < SN_PLAYERS; p++) ok &= g.pos[p] == 0;
        CHECK(ok);
        ok = 1;
        for (int i = 0; i < 2 * np; i++) {      /* a 4 each, then a 5 each: no ladder, no snake */
            int p = sn_turn(&g);
            ok &= p == i % np && sn_play(&g, 4 + i / np) && g.last.player == p;
        }
        CHECK(ok && sn_turn(&g) == 0);
        for (int p = 0; p < np; p++) CHECK(g.pos[p] == 9);
        CHECK(sn_play(&g, 6) && sn_turn(&g) == 0 && sn_play(&g, 1) && sn_turn(&g) == 1);   /* 6: again; then on */
    }
    sn_new_players(&g, 1);                      /* out of range: two at least, four at most */
    CHECK(g.players == 2);
    sn_new_players(&g, 9);
    CHECK(g.players == SN_PLAYERS);

    /* several pawns on one square: three on 5 (no link there) */
    CHECK(play4(&g, "5 5 5 2"));
    CHECK(g.pos[0] == 5 && g.pos[1] == 5 && g.pos[2] == 5 && g.pos[3] == 2 && sn_turn(&g) == 0);

    /* the first on 100 wins, whoever it is: players 2 and 3 */
    sn_new_players(&g, 3);
    g.pos[2] = 96;
    g.turn = 2;
    CHECK(sn_play(&g, 4) && sn_status(&g) == SN_WIN2 && !sn_play(&g, 1));
    sn_new_players(&g, 4);
    g.pos[3] = 94;
    g.pos[0] = 99;
    g.turn = 3;
    CHECK(sn_play(&g, 6) && sn_status(&g) == SN_WIN3 && g.winner == 3 && !sn_play(&g, 1));

    /* four players, a whole game: all on 22, player 0 to 24; player 1: 6 (28: 76) and again 4 (80: 99); a 1 each
     * for 2, 3 and 0; player 1's 1 reaches 100 */
    CHECK(play4(&g, "3 3 3 3 2 6 4 1 1 1 1"));
    CHECK(sn_status(&g) == SN_WIN1 && g.pos[1] == 100 && g.pos[0] == 25 && g.pos[2] == 23 && g.pos[3] == 23);

    /* undo with four: back through the turns, the throws known again */
    CHECK(sn_undo(&g) && sn_status(&g) == SN_PLAY && sn_turn(&g) == 1 && g.pos[1] == 99 && sn_next(&g) == 1);
    for (int i = 0; i < 4; i++) CHECK(sn_undo(&g));
    CHECK(g.n == 6 && sn_turn(&g) == 1 && g.pos[1] == 76 && g.pos[3] == 22 && g.players == 4);
    while (sn_undo(&g)) {}
    CHECK(g.n == 0 && sn_turn(&g) == 0 && g.pos[0] == 0 && g.pos[1] == 0 && g.pos[3] == 0 && g.players == 4);

    printf("snakes: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
