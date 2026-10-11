/*
 * Shut the Box: the rules' test (the combinations, the tiles that can join a choice, the one-die rule, the end of a
 * turn, the scores, shutting the box, one to four players and the turn round the table, ties, every variant, undo,
 * the notation). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/shutthebox.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static sgame G;
static const spos *P = &G.pos;

static unsigned T(int a, int b, int c) { return (a ? 1u << a : 0) | (b ? 1u << b : 0) | (c ? 1u << c : 0); }

/* the sets of tiles making a sum */
static void test_combos(void)
{
    unsigned all = stb_full(9);
    CHECK(all == 0x3fe && stb_sum(all) == 45 && stb_sum(stb_full(10)) == 55 && stb_sum(stb_full(12)) == 78);
    CHECK(stb_combos(all, 1) == 1 && stb_combos(all, 2) == 1);
    CHECK(stb_combos(all, 3) == 2);             /* 3, 1+2 */
    CHECK(stb_combos(all, 7) == 5);             /* 7, 1+6, 2+5, 3+4, 1+2+4 */
    CHECK(stb_combos(all, 12) == 12);           /* the 15 ways in distinct parts but 12, 11+1, 10+2 */
    CHECK(stb_combos(stb_full(12), 12) == 15);
    CHECK(stb_combos(T(1, 2, 0), 4) == 0 && stb_combos(T(1, 3, 0), 4) == 1 && stb_combos(0, 5) == 0);
    CHECK(stb_combos(all, 0) == 0 && stb_combos(all, 46) == 0 && stb_combos(all, 45) == 1);
}

/* the tiles that can still join a choice */
static void test_usable(void)
{
    stb_new(&G, STB_NINE, 1);
    CHECK(stb_usable(P, 0) == 0);               /* no throw yet */
    CHECK(stb_throw(&G, 2, 3, 1) == ST_SHUT && stb_total(P) == 4);
    CHECK(stb_usable(P, 0) == T(1, 3, 4));      /* 4, 1+3 */
    CHECK(stb_usable(P, T(1, 0, 0)) == T(3, 0, 0));
    CHECK(stb_usable(P, T(4, 0, 0)) == 0);      /* complete: nothing more */
    CHECK(!stb_shut(&G, T(1, 2, 0)) && !stb_shut(&G, T(5, 0, 0)) && !stb_shut(&G, 0));   /* 3 or 5: not 4 */
    CHECK(stb_shut(&G, T(1, 3, 0)) == 1 && P->open[0] == (stb_full(9) & ~T(1, 3, 0)) && P->phase == STB_THROW);
    CHECK(!stb_shut(&G, T(4, 0, 0)));           /* no throw to shut now */
    CHECK(stb_throw(&G, 2, 2, 2) == ST_SHUT && stb_usable(P, 0) == T(4, 0, 0));   /* 1 and 3 shut: 4 only */
    CHECK(!stb_shut(&G, T(1, 3, 0)));           /* shut already */
}

/* one die, once 7, 8 and 9 (and up) are shut; the player's choice */
static void test_one_die(void)
{
    stb_new(&G, STB_NINE, 1);
    CHECK(!stb_one_die(P) && !stb_throw(&G, 1, 4, 0));
    CHECK(stb_script(&G, "d63 9 d44 8"));
    CHECK(!stb_one_die(P) && !stb_throw(&G, 1, 3, 0));   /* 7 still open */
    CHECK(stb_script(&G, "d52 7"));
    CHECK(stb_one_die(P));
    CHECK(stb_throw(&G, 1, 5, 0) == ST_SHUT && P->ndice == 1 && stb_total(P) == 5);
    CHECK(!stb_one_die(P));                     /* not while shutting */
    CHECK(stb_shut(&G, T(5, 0, 0)) == 1 && stb_one_die(P));
    CHECK(stb_throw(&G, 2, 1, 3) == ST_SHUT);   /* two dice still allowed */
    CHECK(stb_shut(&G, T(4, 0, 0)) == 1);
    CHECK(stb_script(&G, "d6 6"));              /* the notation's one die */
    CHECK(P->open[0] == T(1, 2, 3));
    /* ten and twelve tiles: 7 and up all shut */
    stb_new(&G, STB_TEN, 1);
    CHECK(stb_script(&G, "d63 9 d44 8 d52 7"));
    CHECK(!stb_one_die(P) && stb_script(&G, "d55 10") && stb_one_die(P));
    stb_new(&G, STB_TWELVE, 1);
    CHECK(stb_script(&G, "d66 12 d65 11 d55 10 d63 9 d44 8 d52 7") && stb_one_die(P));
    stb_new(&G, STB_TWELVE, 1);
    CHECK(stb_script(&G, "d63 9 d44 8 d52 7 d55 10") && !stb_one_die(P));   /* 11, 12 open */
    CHECK(!stb_script(&G, "d4"));
}

/* the end of a turn: a throw no open tiles can make; its score */
static void test_end_of_turn(void)
{
    stb_new(&G, STB_NINE, 1);
    CHECK(stb_script(&G, "d66 9+3 d65 8+2+1 d52 7"));   /* open: 4, 5, 6 */
    CHECK(stb_score(P, 0) == 15 && P->phase == STB_THROW);
    CHECK(stb_throw(&G, 2, 1, 2) == ST_DEAD);   /* 3: no 1, 2, 3 open */
    CHECK(P->phase == STB_OVER && P->winner == -1 && !P->boxed && stb_score(P, 0) == 15);
    CHECK(!stb_throw(&G, 2, 2, 2));             /* over */
    /* a throw bigger than all the open tiles */
    stb_new(&G, STB_NINE, 1);
    CHECK(stb_script(&G, "d63 9 d44 8 d52 7 d6 6 d5 5 d4 4 d3 3"));   /* open: 1, 2 */
    CHECK(stb_one_die(P) && stb_throw(&G, 2, 2, 3) == ST_DEAD && stb_score(P, 0) == 3);
}

/* shutting the box: a win at once */
static void test_shut_box(void)
{
    stb_new(&G, STB_NINE, 1);
    CHECK(stb_script(&G, "d63 9 d44 8 d52 7 d6 6 d5 5 d4 4 d3 3 d21 2+1"));
    CHECK(P->phase == STB_OVER && P->boxed && P->winner == 0 && stb_score(P, 0) == 0);
    stb_new(&G, STB_NINE, 3);                   /* with others still to play: the game ends there */
    CHECK(stb_script(&G, "d66 9+3 d65 8+2+1 d52 7 d6 6"));   /* open: 4, 5 */
    CHECK(stb_throw(&G, 2, 6, 3) == ST_SHUT && stb_shut(&G, T(4, 5, 0)) == 2);
    CHECK(P->phase == STB_OVER && P->boxed && P->winner == 0 && !(P->played & 6));
    CHECK(!stb_throw(&G, 2, 1, 1));
    stb_new(&G, STB_TWELVE, 2);                 /* 78 = 12 + 11 + 10 + 9 + 8 + 7 + 6 + 5 + 4 + 3 + 2 + 1 */
    CHECK(stb_script(&G, "d66 12 d65 11 d55 10 d63 9 d44 8 d52 7 d6 6 d5 5 d4 4 d3 3 d2 2 d1 1"));
    CHECK(P->boxed && P->winner == 0);
}

/* two to four players: the turn round the table, one turn each, the lowest score wins, a tie is a draw */
static void test_players(void)
{
    /* the turns below: "d21 2+1 d11" leaves 42 (a 2 with 1 and 2 shut ends it), "d11 2 d11" 43, "d21 2+1 d21 3 d11"
     * 39 */
    stb_new(&G, STB_NINE, 2);
    CHECK(P->players == 2 && P->turn == 0 && P->open[1] == stb_full(9) && P->open[2] == 0);
    CHECK(stb_script(&G, "d21 2+1"));
    CHECK(stb_throw(&G, 2, 1, 1) == ST_DEAD);   /* player 0: 42 */
    CHECK(P->turn == 1 && P->phase == STB_THROW && stb_score(P, 0) == 42 && P->played == 1);
    CHECK(stb_script(&G, "d11 2 d11"));         /* player 1: 43 */
    CHECK(P->phase == STB_OVER && P->winner == 0 && !P->boxed && stb_score(P, 1) == 43);
    stb_new(&G, STB_NINE, 2);                   /* a tie: a draw */
    CHECK(stb_script(&G, "d21 2+1 d11 d21 2+1 d11") && P->phase == STB_OVER && P->winner == -1);
    stb_new(&G, STB_NINE, 1);                   /* one player: no winner but the shut box */
    CHECK(stb_script(&G, "d21 2+1 d11") && P->phase == STB_OVER && P->winner == -1 && stb_score(P, 0) == 42);
}

static void test_players_3_4(void)
{
    stb_new(&G, STB_NINE, 3);                   /* three: 0, 2, 1 (their sides clockwise from the bottom) */
    CHECK(stb_seat(0) == 0 && stb_seat(2) == 1 && stb_seat(1) == 2 && stb_seat(3) == 3);
    CHECK(P->open[2] == stb_full(9) && P->open[3] == 0);
    CHECK(stb_script(&G, "d21 2+1 d11") && P->turn == 2);     /* player 0: 42 */
    CHECK(stb_script(&G, "d11 2 d11") && P->turn == 1);       /* player 2: 43 */
    CHECK(stb_script(&G, "d21 2+1 d21 3 d11"));               /* player 1: 39 */
    CHECK(P->phase == STB_OVER && P->winner == 1 && P->played == 7);
    stb_new(&G, STB_TEN, 4);                    /* four: 0, 2, 1, 3; a tie for the lowest (not the others) */
    CHECK(P->open[3] == stb_full(10) && stb_score(P, 3) == 55);
    CHECK(stb_script(&G, "d21 2+1 d11") && P->turn == 2);     /* player 0: 52 */
    CHECK(stb_script(&G, "d21 2+1 d11") && P->turn == 1);     /* player 2: 52 */
    CHECK(stb_script(&G, "d11 2 d11") && P->turn == 3);       /* player 1: 53 */
    CHECK(P->phase == STB_THROW && stb_script(&G, "d21 2+1 d11"));    /* player 3: 52 */
    CHECK(P->phase == STB_OVER && P->winner == -1);
    stb_new(&G, STB_TEN, 4);
    CHECK(stb_script(&G, "d21 2+1 d11 d21 2+1 d11 d11 2 d11 d21 2+1 d21 3 d11"));   /* player 3: 49 */
    CHECK(P->phase == STB_OVER && P->winner == 3);
    stb_new(&G, STB_NINE, 4);                   /* the box shut by the last player: a win at once all the same */
    CHECK(stb_script(&G, "d21 2+1 d11 d21 2+1 d11 d11 2 d11"));
    CHECK(P->turn == 3 && stb_script(&G, "d63 9 d44 8 d52 7 d6 6 d5 5 d4 4 d3 3 d21 2+1"));
    CHECK(P->phase == STB_OVER && P->boxed && P->winner == 3);
}

/* every variant: its tiles, a whole turn to the end */
static void test_variants(void)
{
    static const int n[STB_VARIANTS] = {9, 10, 12};
    for (int v = 0; v < STB_VARIANTS; v++) {
        stb_new(&G, v, 1);
        CHECK(P->tiles == n[v] && stb_tiles(v) == n[v] && P->open[0] == stb_full(n[v]));
        CHECK(stb_score(P, 0) == n[v] * (n[v] + 1) / 2);
        CHECK(stb_throw(&G, 2, 6, 6) == ST_SHUT);
        CHECK(stb_usable(P, 0) == stb_full(n[v]));   /* 12: every tile can be in a set */
        CHECK(stb_shut(&G, n[v] == 12 ? 1u << 12 : T(9, 3, 0)) == 1);
        CHECK(stb_score(P, 0) == n[v] * (n[v] + 1) / 2 - 12);
    }
    stb_new(&G, STB_TWELVE, 2);                 /* 11 and 12 */
    CHECK(stb_script(&G, "d65 11 d66 12") && P->open[0] == (stb_full(12) & ~T(11, 12, 0)));
    stb_new(&G, STB_NINE, 1);
    CHECK(!stb_script(&G, "d55 10"));           /* no 10 with nine tiles */
}

/* undo: back to before the last tiles shut, the same throw to shut again */
static void test_undo(void)
{
    stb_new(&G, STB_NINE, 2);
    CHECK(!stb_undo(&G));
    CHECK(stb_script(&G, "d53 8") && P->open[0] == (stb_full(9) & ~T(8, 0, 0)));
    CHECK(stb_undo(&G) && P->phase == STB_SHUT && stb_total(P) == 8 && P->open[0] == stb_full(9));
    CHECK(stb_shut(&G, T(1, 7, 0)) == 1);
    CHECK(stb_script(&G, "d21 2+1") == 0);      /* 1 shut already */
    stb_new(&G, STB_NINE, 2);
    CHECK(stb_script(&G, "d21 2+1 d62 8 d11")); /* the turn over */
    CHECK(P->turn == 1 && P->phase == STB_THROW);
    CHECK(stb_undo(&G) && P->turn == 0 && stb_total(P) == 8 && P->die[0] == 6 && P->die[1] == 2 && !P->played);
    CHECK(stb_undo(&G) && stb_total(P) == 3 && P->open[0] == stb_full(9));
    CHECK(!stb_undo(&G));                       /* the first throw: never thrown again */
    CHECK(G.n == 1 && P->phase == STB_SHUT);
    stb_new(&G, STB_NINE, 1);                   /* undo out of the end */
    CHECK(stb_script(&G, "d63 9 d44 8 d52 7 d6 6 d5 5 d4 4 d3 3 d21 2+1") && P->boxed);
    CHECK(stb_undo(&G) && !P->boxed && P->winner == -1 && P->phase == STB_SHUT && stb_total(P) == 3);
    CHECK(stb_shut(&G, T(3, 0, 0)) == 0);       /* 3 is shut already */
    CHECK(stb_shut(&G, T(1, 2, 0)) == 2);
}

static void test_script(void)
{
    stb_new(&G, STB_NINE, 1);
    CHECK(!stb_script(&G, "x"));
    stb_new(&G, STB_NINE, 1);
    CHECK(!stb_script(&G, "d"));
    stb_new(&G, STB_NINE, 1);
    CHECK(!stb_script(&G, "d123"));
    stb_new(&G, STB_NINE, 1);
    CHECK(!stb_script(&G, "d70"));
    stb_new(&G, STB_NINE, 1);
    CHECK(!stb_script(&G, "d53 4+4"));
    stb_new(&G, STB_NINE, 1);
    CHECK(!stb_script(&G, "d53 3+"));
    stb_new(&G, STB_NINE, 1);
    CHECK(stb_script(&G, "  d53   3+5  ") && P->open[0] == (stb_full(9) & ~T(3, 5, 0)));
    CHECK(G.n == 2 && (G.ev[0] & STB_THROW_EV) && G.ev[1] == T(3, 5, 0));
}

int main(void)
{
    test_combos();
    test_usable();
    test_one_die();
    test_end_of_turn();
    test_shut_box();
    test_players();
    test_players_3_4();
    test_variants();
    test_undo();
    test_script();
    printf("shutthebox: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
