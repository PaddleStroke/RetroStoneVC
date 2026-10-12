/*
 * Pig: the rules' test (rolls adding up, a 1 losing the turn's total, holding, two-dice pig's single and double 1s,
 * the race to 100, undo of a hold only, the notation; whole random games of both variants from a seed). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/pig.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static piggame G;
static const pigpos *P = &G.pos;

static void test_one(void)
{
    pig_new(&G, PIG_ONE, 3);
    CHECK(pig_ndice(P) == 1 && P->turn == 0 && !pig_can_hold(P) && !pig_script(&G, "h"));   /* roll first */
    CHECK(pig_script(&G, "r4 r3") && P->total == 7 && P->turn == 0 && pig_can_hold(P));
    CHECK(pig_script(&G, "h") && P->score[0] == 7 && P->turn == 1 && P->total == 0 && P->ended == PIG_HELD);
    CHECK(P->ended_pts == 7 && P->ended_by == 0);
    CHECK(pig_script(&G, "r6 r6 r1") && P->score[1] == 0 && P->turn == 2 && P->ended == PIG_LOST && P->ended_pts == 12);
    CHECK(pig_script(&G, "r1") && P->turn == 0 && P->ended_pts == 0);   /* a 1 first: nothing lost but the turn */
    CHECK(!pig_script(&G, "r7") && !pig_script(&G, "r45"));               /* not a face; one die only */
}

static void test_two(void)
{
    pig_new(&G, PIG_TWO, 2);
    CHECK(pig_ndice(P) == 2 && pig_script(&G, "r34 r66 h") && P->score[0] == 19 && P->turn == 1);
    CHECK(pig_script(&G, "r25 r15") && P->score[1] == 0 && P->turn == 0 && P->ended == PIG_LOST && P->ended_pts == 7);
    CHECK(pig_script(&G, "r23 r11") && P->score[0] == 0 && P->ended == PIG_WIPED && P->ended_score == 19);
    CHECK(P->ended_pts == 5 && P->turn == 1);
    CHECK(!pig_script(&G, "r3") && !pig_script(&G, "r30"));                /* two dice */
}

static void test_end_undo(void)
{
    pig_new(&G, PIG_ONE, 2);
    CHECK(pig_script(&G, "r6 r6 r6 r6 r6 r6 r6 r6 r6 r6 r6 r6 r6 r6 r6 r6 r5") && P->total == 101);
    CHECK(pig_undo(&G) == 0);                   /* a roll is never taken back */
    CHECK(pig_script(&G, "h") && P->phase == PIG_OVER && P->winner == 0 && P->score[0] == 101);
    CHECK(!pig_script(&G, "r3") && pig_undo(&G) && P->phase == PIG_PLAY && P->total == 101 && P->turn == 0);
    CHECK(!pig_undo(&G) && pig_script(&G, "r2 h") && P->score[0] == 103 && P->winner == 0);
    pig_new(&G, PIG_ONE, 2);
    CHECK(pig_script(&G, "r5 h") && pig_undo(&G) && P->turn == 0 && P->total == 5 && P->score[0] == 0);
    CHECK(pig_script(&G, "r4 h") && P->score[0] == 9 && pig_leader(P) == 0);
}

static unsigned seed;
static int rnd(int n)
{
    seed = seed * 1103515245u + 12345u;
    return (int)((seed >> 8) % (unsigned)n);
}

/* whole games: hold at 20 or more (or when it wins); the scores never go down but by two 1s, a turn ends only on a
 * 1 or a hold, the winner has 100 or more */
static void test_random(void)
{
    int bad = 0;
    for (int k = 0; k < 400; k++) {
        int variant = k % 2, players = 2 + k % 5;
        seed = 31u + (unsigned)k;
        pig_new(&G, variant, players);
        for (int step = 0; step < 200000 && P->phase == PIG_PLAY; step++) {
            pigpos was = *P;
            if (pig_can_hold(P) && (P->total >= 20 || P->score[P->turn] + P->total >= PIG_TARGET)) {
                if (!pig_hold(&G)) { bad++; break; }
                bad += P->score[was.turn] != was.score[was.turn] + was.total;
                continue;
            }
            int d1 = 1 + rnd(6), d2 = 1 + rnd(6);
            if (!pig_roll(&G, d1, d2)) { bad++; break; }
            int ones = (d1 == 1) + (variant == PIG_TWO && d2 == 1);
            if (ones == 2) bad += P->score[was.turn] != 0 || P->turn == was.turn;
            else if (ones) bad += P->score[was.turn] != was.score[was.turn] || P->turn == was.turn;
            else bad += P->turn != was.turn || P->total != was.total + d1 + (variant == PIG_TWO ? d2 : 0);
        }
        bad += P->phase != PIG_OVER || P->winner < 0 || P->score[P->winner] < PIG_TARGET;
        for (int q = 0; q < players; q++) bad += q != P->winner && P->score[q] >= PIG_TARGET;
    }
    CHECK(bad == 0);
}

int main(void)
{
    test_one();
    test_two();
    test_end_undo();
    test_random();
    printf("pig: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
