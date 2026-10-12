/*
 * Mexico: the rules' test (the values and their ranking, a value at least as high, the call held or a lie, the
 * lives, a player out, the next round's first player, the end, the notation; whole random games from a seed). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/mexico.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static mxgame G;
static const mxpos *P = &G.pos;

static void test_values(void)
{
    CHECK(mx_rank(2, 1) == MX_MEXICO && mx_rank(1, 2) == MX_MEXICO && mx_number(MX_MEXICO) == 21);
    CHECK(mx_rank(3, 1) == 0 && mx_rank(1, 3) == 0 && mx_rank(6, 5) == 13 && mx_rank(1, 1) == 14);
    CHECK(mx_rank(6, 6) == 19 && mx_rank(5, 3) > mx_rank(5, 2) && mx_rank(6, 1) > mx_rank(5, 4));
    CHECK(mx_rank(1, 1) > mx_rank(6, 5) && mx_rank(2, 2) > mx_rank(1, 1) && mx_rank(2, 1) > mx_rank(6, 6));
    int all = 1, seen[MX_VALUES] = {0};
    for (int a = 1; a <= 6; a++)
        for (int b = 1; b <= 6; b++) {
            int r = mx_rank(a, b), hi, lo;
            if (r < 0) { all = 0; continue; }
            seen[r]++;
            mx_dice_of(r, &hi, &lo);
            all &= mx_rank(hi, lo) == r && hi * 10 + lo == mx_number(r);
            all &= (a == b) == (hi == lo);
        }
    for (int r = 0; r < MX_VALUES; r++) all &= seen[r] > 0 && mx_from_number(mx_number(r)) == r;
    CHECK(all && mx_from_number(12) < 0 && mx_from_number(77) < 0);
    int ways = 0;                               /* 21 comes two ways in 36, a double one way */
    for (int a = 1; a <= 6; a++)
        for (int b = 1; b <= 6; b++) ways += mx_rank(a, b) == MX_MEXICO;
    CHECK(ways == 2 && seen[MX_MEXICO] == 2 && seen[19] == 1 && seen[0] == 2);
}

static void test_round(void)
{
    mx_new(&G, 3, 3);
    CHECK(P->turn == 0 && P->phase == MX_THROW && P->claim < 0 && P->lives[2] == 3);
    CHECK(!mx_script(&G, "c"));                 /* nothing to call yet */
    mx_new(&G, 3, 3);
    CHECK(mx_script(&G, "t53 s53") && P->turn == 1 && P->claim == mx_from_number(53) && P->claimer == 0);
    CHECK(!mx_script(&G, "s54"));               /* a throw first */
    CHECK(mx_script(&G, "t21") && P->phase == MX_SAY && !mx_script(&G, "s52"));   /* lower: refused */
    CHECK(mx_script(&G, "s65") && P->turn == 2 && P->nclaims == 2);
    CHECK(mx_script(&G, "c") && P->phase == MX_REVEAL && P->held && P->loser == 2 && P->caller == 2);
    CHECK(P->lives[2] == 2 && P->lives[0] == 3 && P->lives[1] == 3);   /* 21 is at least 65: the caller loses */
    CHECK(!mx_script(&G, "t11") && mx_script(&G, "n") && P->turn == 2 && P->starter == 2 && P->claim < 0);
    CHECK(P->nclaims == 0 && P->round == 1);
    CHECK(mx_script(&G, "t31 s66 c") && !P->held && P->loser == 2 && P->lives[2] == 1);   /* a lie */
    CHECK(mx_script(&G, "n t42 s42 t42 s42") && P->turn == 1);   /* the same value again: allowed */
    CHECK(mx_script(&G, "t55 s21 t11") && mx_script(&G, "s21"));   /* Mexico, then Mexico again */
    CHECK(mx_script(&G, "c") && !P->held && P->loser == 2 && P->lives[2] == 0 && P->phase == MX_REVEAL);
    /* player 2 out: the next round starts with the next player in, and skips them */
    CHECK(mx_script(&G, "n") && P->turn == 0 && mx_script(&G, "t43 s43") && P->turn == 1);
    CHECK(mx_script(&G, "t43 s43") && P->turn == 0 && mx_alive(P) == 2);
}

static void test_end(void)
{
    mx_new(&G, 2, 1);
    CHECK(mx_script(&G, "t31 s66 c") && P->phase == MX_OVER && P->winner == 1 && !mx_script(&G, "n"));
    mx_new(&G, 2, 3);
    CHECK(mx_script(&G, "t66 s66 c n t66 s21 c n") && P->lives[1] == 1 && P->lives[0] == 3 && P->turn == 1);
    CHECK(mx_script(&G, "t65 s21 c") && !P->held && P->loser == 1 && P->phase == MX_OVER && P->winner == 0);
}

static unsigned seed;
static int rnd(int n)
{
    seed = seed * 1103515245u + 12345u;
    return (int)((seed >> 8) % (unsigned)n);
}

/* whole games from a seed: a call when the value is high or at random, else a throw and the truth when it is high
 * enough, a bluff above the last value when not; every call costs exactly one life, the round's first player has
 * lives, the game ends with one player in */
static void test_random(void)
{
    int bad = 0, games = 0;
    for (int k = 0; k < 400; k++) {
        int players = 2 + k % 5, lives = k % 2 ? 6 : 3;
        seed = 9000u + (unsigned)k;
        mx_new(&G, players, lives);
        int total = players * lives, calls = 0;
        for (int step = 0; step < 100000 && P->phase != MX_OVER; step++) {
            int act;
            if (P->phase == MX_REVEAL) act = MX_ACT(MXA_NEXT, 0);
            else if (P->phase == MX_THROW && P->claim >= 0 && (P->claim > 15 || rnd(4) == 0)) act = MX_ACT(MXA_CALL, 0);
            else if (P->phase == MX_THROW) act = MX_ACT(MXA_THROW, MX_DICE(1 + rnd(6), 1 + rnd(6)));
            else {
                int r = mx_rank(P->dice[0], P->dice[1]);
                if (r < P->claim) r = P->claim + rnd(3);
                act = MX_ACT(MXA_SAY, r > MX_MEXICO ? MX_MEXICO : r);
            }
            int before = 0, after = 0, turn = P->turn;
            for (int q = 0; q < players; q++) before += P->lives[q];
            if (!mx_play(&G, act)) { bad++; break; }
            for (int q = 0; q < players; q++) after += P->lives[q];
            if (MX_KIND(act) == MXA_CALL) {
                calls++;
                bad += after != before - 1;
                bad += P->held ? P->loser != turn : P->loser != P->claimer;
            } else bad += after != before;
            if (MX_KIND(act) == MXA_NEXT) bad += !P->lives[P->turn] || P->turn != P->starter;
            if (P->phase == MX_SAY || P->phase == MX_THROW) bad += !P->lives[P->turn];
        }
        bad += P->phase != MX_OVER || mx_alive(P) != 1 || P->winner < 0 || !P->lives[P->winner];
        bad += calls != total - P->lives[P->winner];
        games++;
    }
    CHECK(bad == 0 && games == 400);
}

int main(void)
{
    test_values();
    test_round();
    test_end();
    test_random();
    printf("mexico: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
