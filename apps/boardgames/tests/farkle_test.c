/*
 * Farkle: the rules' test (every scoring combination and the best reading of a set, the dice that can score, the
 * throws and the dice set aside, banking and the entry, farkles, hot dice, the final round and the winner, ties,
 * alone within twenty turns, undo, the notation). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/farkle.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static fgame G;
static const fpos *P = &G.pos;

static int V(const char *s)                     /* the value of these faces taken together */
{
    uint8_t d[8];
    int n = 0;
    while (*s && n < 8) d[n++] = (uint8_t)(*s++ - '0');
    return fk_value(d, n);
}

static void test_values(void)
{
    CHECK(V("1") == 100 && V("5") == 50 && V("15") == 150 && V("11") == 200 && V("55") == 100);
    CHECK(V("2") == 0 && V("12") == 0 && V("") == 0 && V("66") == 0);
    CHECK(V("111") == 1000 && V("222") == 200 && V("333") == 300 && V("444") == 400 && V("555") == 500);
    CHECK(V("666") == 600 && V("2221") == 300 && V("6665") == 650 && V("2225") == 250);
    CHECK(V("2222") == 1000 && V("6666") == 1000 && V("1111") == 1100 && V("5555") == 1000);   /* four 1s: 1000 + 100 */
    CHECK(V("33333") == 2000 && V("11111") == 2000 && V("444444") == 3000 && V("111111") == 3000);
    CHECK(V("123456") == 1500 && V("654321") == 1500);
    CHECK(V("223344") == 1500 && V("115566") == 1500 && V("222266") == 1500);   /* three pairs, four and a pair */
    CHECK(V("111155") == 1500 && V("222333") == 2500 && V("111555") == 2500 && V("666111") == 2500);
    CHECK(V("12345") == 0 && V("23456") == 0 && V("22334") == 0);   /* no straight of five; one die does not score */
    CHECK(V("22233") == 0 && V("2223") == 0 && V("11555") == 700 && V("15555") == 1100);
    CHECK(V("111115") == 2050 && V("555551") == 2100);
}

/* the dice of a throw that can score, the value of a choice */
static void test_choice(void)
{
    fk_new(&G, 2);
    CHECK(fk_scoring(P) == 0 && fk_sel_value(P, 1) == 0);   /* before a throw */
    CHECK(fk_script(&G, "d215363"));            /* 2 1 5 3 6 3 */
    CHECK(fk_scoring(P) == (1u << 1 | 1u << 2));
    CHECK(fk_sel_value(P, 1u << 1) == 100 && fk_sel_value(P, 1u << 2) == 50 && fk_sel_value(P, 6) == 150);
    CHECK(fk_sel_value(P, 1) == 0 && fk_sel_value(P, 7) == 0);   /* a 2 does not score */
    fk_new(&G, 2);
    CHECK(fk_script(&G, "d443424") && fk_scoring(P) == 0x2b);   /* four 4s; the 3 and the 2 do not */
    CHECK(fk_sel_value(P, 0x0b) == 400 && fk_sel_value(P, 0x2b) == 1000 && fk_sel_value(P, 0x2f) == 0);
    fk_new(&G, 2);
    CHECK(fk_script(&G, "d364215") && fk_scoring(P) == 0x3f && fk_sel_value(P, 0x3f) == 1500);   /* a straight */
    CHECK(fk_sel_value(P, 0x30) == 150);
    fk_new(&G, 2);
    CHECK(fk_script(&G, "d223366") && fk_scoring(P) == 0x3f && fk_sel_value(P, 0x0f) == 0);   /* three pairs only
                                                                                                * all together */
}

/* a turn: throws, dice set aside, the bank, the entry of 500 */
static void test_turn(void)
{
    fk_new(&G, 2);
    CHECK(P->phase == FK_THROW && fk_to_throw(P, 0) == 6);
    int v6[6] = {1, 1, 5, 2, 3, 4};
    CHECK(fk_throw(&G, 1, v6) == 0);            /* nothing to set aside yet */
    CHECK(fk_throw(&G, 0, v6) == FK_THROWN && P->phase == FK_CHOOSE && P->nd == 6 && P->throws == 1);
    CHECK(!fk_bank(&G, 0) && !fk_bank(&G, 1u << 3));       /* nothing, or a 2 */
    CHECK(!fk_bank(&G, 3));                     /* 200: under the entry */
    CHECK(fk_to_throw(P, 3) == 4);
    CHECK(fk_script(&G, "a11 d5523"));          /* 200 aside; then 5 5 2 3 */
    CHECK(P->turn_pts == 200 && P->naside == 2 && P->nd == 4 && P->group[0] == 1);
    CHECK(!fk_can_bank(P, 1) && fk_can_bank(P, 3) == 0);    /* 250 and 300: under 500 */
    CHECK(fk_script(&G, "a55 d55"));            /* 300; then 5 5 */
    CHECK(P->turn_pts == 300 && P->naside == 4 && !fk_can_bank(P, 3) && !fk_bank(&G, 3));   /* 400: under 500 */
    CHECK(fk_to_throw(P, 3) == 6 && fk_script(&G, "a55 d111222 a111 b") && P->score[0] == 1400);
    fk_new(&G, 2);
    CHECK(fk_script(&G, "d111234 a111 d552 a55 b"));      /* 1000 + 100 */
    CHECK(P->score[0] == 1100 && P->turn == 1 && P->phase == FK_THROW && P->turns == 1 && P->naside == 0);
    CHECK(fk_script(&G, "d512346 a5 d11111"));  /* player 1: 50, then five 1s */
    CHECK(fk_script(&G, "a11111 b") && P->score[1] == 2050);
    CHECK(fk_script(&G, "d500000") == 0);       /* faces 1..6 only */
    CHECK(fk_script(&G, "d51") == 0);           /* six dice at a turn's start */
    CHECK(fk_script(&G, "d512346 a5 b"));       /* on the board: any bank */
    CHECK(P->score[0] == 1150);
}

/* a farkle: the turn's points lost; hot dice */
static void test_farkle_hot(void)
{
    fk_new(&G, 2);
    CHECK(fk_script(&G, "d111234 a111"));
    int v[3] = {2, 3, 4};
    CHECK(fk_throw(&G, P->dice[0] == 1 ? 0x07 : 0, v) == FK_FARKLE);
    CHECK(P->turn == 1 && P->score[0] == 0 && P->phase == FK_THROW && P->farkles == 1 && P->turns == 1);
    CHECK(fk_script(&G, "d234662") == 1 && P->turn == 0);   /* a farkle at once (no 1, 5, triple, three pairs) */
    /* hot dice: the six scored, thrown again */
    fk_new(&G, 1);
    CHECK(fk_script(&G, "d112555 a11555"));
    CHECK(fk_to_throw(P, 0x3b) == 1);
    CHECK(fk_script(&G, "d5") && P->nd == 1 && P->naside == 5);
    CHECK(fk_to_throw(P, 1) == 6);              /* the last die scored: all six again */
    CHECK(fk_script(&G, "a5 d123456") && P->nd == 6 && P->naside == 0 && P->turn_pts == 750);
    CHECK(fk_script(&G, "a123456 b") && P->score[0] == 2250);
    fk_new(&G, 1);
    CHECK(fk_script(&G, "d223366 a223366 d222333 a222333 b") && P->score[0] == 4000);   /* hot dice twice over */
}

/* the final round: everyone else once more, then the highest score; a tie */
static void test_end(void)
{
    fk_new(&G, 3);
    /* each turn 3000 (six 4s) */
    for (int r = 0; r < 3; r++) CHECK(fk_script(&G, "d444444 a444444 b d444444 a444444 b d444444 a444444 b"));
    CHECK(P->score[0] == 9000 && P->final_by < 0);
    CHECK(fk_script(&G, "d444444 a444444 b") && P->final_by == 0 && P->final_left == 6 && P->turn == 1);
    CHECK(fk_script(&G, "d234662") && P->turn == 2 && P->final_left == 4 && P->phase == FK_THROW);   /* a farkle */
    CHECK(fk_script(&G, "d444444 a444444 d111111 a111111 b") && P->phase == FK_OVER);
    CHECK(P->score[2] == 15000 && P->winner == 2);
    CHECK(!fk_script(&G, "d123456"));
    fk_new(&G, 2);                              /* a tie */
    for (int r = 0; r < 4; r++) CHECK(fk_script(&G, "d444444 a444444 b d444444 a444444 b"));
    CHECK(P->phase == FK_OVER && P->winner == -1 && P->score[0] == 12000 && P->score[1] == 12000);
    /* six players: the turn round, the final round */
    fk_new(&G, 6);
    for (int q = 0; q < 6; q++) CHECK(P->turn == q && fk_script(&G, "d234662"));
    CHECK(P->turn == 0 && P->turns == 6);
}

/* alone: 10000 within 20 turns */
static void test_solo(void)
{
    fk_new(&G, 1);
    for (int r = 0; r < 3; r++) CHECK(fk_script(&G, "d444444 a444444 b") && P->phase == FK_THROW && P->turn == 0);
    CHECK(fk_script(&G, "d111111 a111111 b") && P->phase == FK_OVER && P->winner == 0 && P->turns == 4);
    fk_new(&G, 1);
    for (int r = 0; r < 19; r++) CHECK(fk_script(&G, "d234662") && P->phase == FK_THROW);
    CHECK(fk_script(&G, "d234662") && P->phase == FK_OVER && P->winner == -1 && P->turns == 20);
}

/* undo: back to before a bank, never through dice set aside to throw again */
static void test_undo(void)
{
    fk_new(&G, 2);
    CHECK(!fk_undo(&G));
    CHECK(fk_script(&G, "d111234") && !fk_undo(&G));        /* a throw is never undone */
    CHECK(fk_script(&G, "a111 b") && P->turn == 1 && P->score[0] == 1000);
    CHECK(fk_undo(&G) && P->turn == 0 && P->score[0] == 0 && P->phase == FK_CHOOSE && P->dice[0] == 1);
    CHECK(fk_script(&G, "a111 d455") == 1);     /* set aside, threw on: 4 5 5 */
    CHECK(!fk_undo(&G));                        /* never back through a throw on */
    CHECK(fk_script(&G, "a5 b") && P->score[0] == 1050 && fk_undo(&G) && P->turn == 0 && P->nd == 3);
    CHECK(fk_script(&G, "a5 b d234662"));       /* the next player's farkle: still back to the bank */
    CHECK(fk_undo(&G) && P->turn == 0 && P->phase == FK_CHOOSE && G.n == 2);
    CHECK(!fk_undo(&G));
    /* out of the end */
    fk_new(&G, 1);
    CHECK(fk_script(&G, "d444444 a444444 b d444444 a444444 b d444444 a444444 b d444444 a444444 b"));
    CHECK(P->phase == FK_OVER && fk_undo(&G) && P->phase == FK_CHOOSE && P->score[0] == 9000);
}

static void test_script(void)
{
    fk_new(&G, 2);
    CHECK(!fk_script(&G, "x"));
    fk_new(&G, 2);
    CHECK(!fk_script(&G, "a1"));                /* no throw yet */
    fk_new(&G, 2);
    CHECK(!fk_script(&G, "d123456 a7"));
    fk_new(&G, 2);
    CHECK(!fk_script(&G, "d123456 a11"));       /* one 1 only */
    fk_new(&G, 2);
    CHECK(!fk_script(&G, "d123456 a1 d123456"));   /* five dice left, not six */
    fk_new(&G, 2);
    CHECK(fk_script(&G, " d123456  a15  d1234 ") && P->turn_pts == 150 && P->nd == 4 && G.n == 2);
    CHECK(G.ev[1].kind == FE_THROW && G.ev[1].nd == 4 && G.ev[1].v[3] == 4 && G.ev[1].sel == 0x11);
}

int main(void)
{
    test_values();
    test_choice();
    test_turn();
    test_farkle_hot();
    test_end();
    test_solo();
    test_undo();
    test_script();
    printf("farkle: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
