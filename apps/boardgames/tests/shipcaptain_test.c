/*
 * Ship, Captain and Crew: the rules' test (the ship, captain and crew set aside in order, from one roll or several,
 * the cargo, stopping, rolling the cargo again, three rolls without them, the round's chips and its ties, the next
 * round's first player, the end and a shared win, undo of a stop only, the notation; whole random games). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/shipcaptain.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static sccgame G;
static const sccpos *P = &G.pos;

static void test_turn(void)
{
    scc_new(&G, 2);
    CHECK(P->turn == 0 && !scc_can_stop(P) && !scc_script(&G, "s") && scc_free(P) == 31);
    CHECK(scc_script(&G, "r65432") && scc_complete(P) && scc_cargo(P) == 5 && P->aside[0] == 0 && P->aside[1] == 1);
    CHECK(P->aside[2] == 2 && scc_free(P) == 24 && scc_can_stop(P));
    CHECK(scc_script(&G, "r66") && scc_cargo(P) == 12 && P->rolls == 2 && P->turn == 0);   /* the cargo again */
    CHECK(scc_script(&G, "s") && P->turn == 1 && P->res[0] == 12 && P->rolls == 0 && P->aside[0] < 0);
    /* in order: a 5 or a 4 before the ship does not count */
    CHECK(scc_script(&G, "r54321") && P->aside[0] < 0 && P->aside[1] < 0 && scc_free(P) == 31);
    CHECK(scc_script(&G, "r64111") && P->aside[0] == 0 && P->aside[1] < 0 && P->aside[2] < 0 && !scc_can_stop(P));
    CHECK(scc_script(&G, "r5413"));   /* captain and crew in one roll, the third: cargo 1 + 3 */
    /* the third roll ends the turn, the round settles: player 0's 12 beats 4 */
    CHECK(P->res[0] == -1 && P->last_res[0] == 12 && P->last_res[1] == 4 && P->last_won == 1);
    CHECK(P->chips[0] == 1 && P->chips[1] == 0 && P->rounds == 1);
    CHECK(P->starter == 1 && P->turn == 1);     /* the next round's first player */
}

static void test_none_tie(void)
{
    scc_new(&G, 3);
    CHECK(scc_script(&G, "r55555 r44444 r33333") && P->turn == 1 && P->res[0] == 0);   /* no ship: nothing */
    CHECK(scc_script(&G, "r65411 s") && P->res[1] == 2);
    CHECK(scc_script(&G, "r12654 r11 s") && P->last_res[2] == 2 && P->turn == 1);   /* a tie: both win a chip */
    CHECK(P->last_won == 6 && P->chips[1] == 1 && P->chips[2] == 1 && P->chips[0] == 0);
    scc_new(&G, 2);
    CHECK(scc_script(&G, "r11111 r22222 r33333 r11111 r22222 r33333") && P->last_won == 0 && P->chips[0] == 0);
}

static void test_end_undo(void)
{
    scc_new(&G, 2);
    for (int t = 0; t < 8; t++) CHECK(scc_script(&G, P->turn == 0 ? "r65466 s" : "r65411 s"));   /* 12 for player 0 */
    CHECK(P->chips[0] == 4 && P->chips[1] == 0 && P->phase == SCC_PLAY);
    CHECK(scc_script(&G, "r65466 s") && P->turn == 1 && scc_undo(&G) && P->turn == 0 && scc_cargo(P) == 12);
    CHECK(!scc_undo(&G) && scc_script(&G, "s r65411 s") && P->phase == SCC_OVER && P->winners == 1);
    CHECK(P->chips[0] == 5 && scc_undo(&G) && P->phase == SCC_PLAY);
    scc_new(&G, 2);
    for (int r = 0; r < 4; r++) CHECK(scc_script(&G, "r65466 s r65466 s"));
    CHECK(P->chips[0] == 4 && P->chips[1] == 4 && scc_script(&G, "r65433 s r65433 s"));
    CHECK(P->phase == SCC_OVER && P->winners == 3);   /* reached together: shared */
    CHECK(!scc_script(&G, "r65432"));
}

static unsigned seed;
static int rnd(int n)
{
    seed = seed * 1103515245u + 12345u;
    return (int)((seed >> 8) % (unsigned)n);
}

/* whole games: stop at a cargo of 8 or more; ship, captain and crew always 6, 5, 4 on different dice, never lost in a
 * turn, a turn at most three rolls, each round one chip or more to the best (or none without cargo) */
static void test_random(void)
{
    int bad = 0;
    for (int k = 0; k < 400; k++) {
        int players = 2 + k % 5;
        seed = 555u + (unsigned)k;
        scc_new(&G, players);
        for (int step = 0; step < 100000 && P->phase == SCC_PLAY; step++) {
            sccpos was = *P;
            if (scc_can_stop(P) && scc_cargo(P) >= 8) {
                if (!scc_stop(&G)) { bad++; break; }
                continue;
            }
            int f[SCC_DICE];
            for (int i = 0; i < SCC_DICE; i++) f[i] = 1 + rnd(6);
            if (!scc_roll(&G, f)) { bad++; break; }
            if (P->turn == was.turn && P->rounds == was.rounds) {
                for (int j = 0; j < 3; j++) {
                    bad += was.aside[j] >= 0 && P->aside[j] != was.aside[j];
                    bad += P->aside[j] >= 0 && P->dice[P->aside[j]] != 6 - j;
                    bad += j && P->aside[j] >= 0 && P->aside[j - 1] < 0;
                }
                bad += P->aside[0] >= 0 && (P->aside[0] == P->aside[1] || P->aside[0] == P->aside[2]);
                bad += P->aside[1] >= 0 && P->aside[1] == P->aside[2];
                bad += P->rolls > SCC_ROLLS;
            }
            if (P->rounds != was.rounds) {
                int gained = 0;
                for (int q = 0; q < players; q++) gained += P->chips[q] - was.chips[q];
                int best = 0;
                for (int q = 0; q < players; q++) best = P->last_res[q] > best ? P->last_res[q] : best;
                bad += best ? gained < 1 : gained != 0;
            }
        }
        int top = 0;
        for (int q = 0; q < players; q++) top = P->chips[q] > top ? P->chips[q] : top;
        bad += P->phase != SCC_OVER || top < SCC_TARGET || !P->winners;
    }
    CHECK(bad == 0);
}

int main(void)
{
    test_turn();
    test_none_tie();
    test_end_undo();
    test_random();
    printf("shipcaptain: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
