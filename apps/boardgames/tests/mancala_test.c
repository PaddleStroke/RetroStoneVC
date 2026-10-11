/*
 * Mancala: the rules' test (src/games/mancala_rules.c). Oware: the sowing, the skip of the pit sown, chain captures,
 * the grand slam, the duty to feed, the ends (more than 24, no move, a repeated position). Kalah: the store and the
 * extra turn, the capture opposite, the other's store passed by, the sweep at the end, a draw. No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/mancala.h"

static int checks, failed;
static mc_game g;
static mc_move m;

#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

/* a position to start from: the 12 pits, the 2 stores, the player to move */
static void setup(int variant, const int *seeds, int turn)
{
    mc_new(&g, variant);
    for (int i = 0; i < MC_SLOTS; i++) g.pos[0].seeds[i] = (uint8_t)seeds[i];
    g.pos[0].turn = (uint8_t)turn;
}

/* plays "3 9 4" (pits 1-12) from where the game is; 0: a move was refused */
static int play(const char *s)
{
    for (char *end; *s; s = end) {
        long p = strtol(s, &end, 10);
        if (end == s) break;
        if (!mc_play(&g, (int)p - 1, &m)) return 0;
    }
    return 1;
}

static int at(int slot) { return mc_now(&g)->seeds[slot]; }

int main(void)
{
    /* ---- Oware ---- */
    mc_new(&g, MC_OWARE);
    CHECK(mc_turn(&g) == 0 && mc_status(&g) == MC_PLAY && mc_legal(&g, 0) && !mc_legal(&g, 6));
    CHECK(play("1") && at(0) == 0 && at(1) == 5 && at(4) == 5 && at(5) == 4 && mc_turn(&g) == 1);
    CHECK(m.n == 4 && m.to[0] == 1 && m.to[3] == 4 && m.ntake == 0);
    CHECK(!mc_play(&g, 0, NULL) && play("12") && at(0) == 1 && at(3) == 6 && mc_turn(&g) == 0);
    CHECK(mc_undo(&g) && mc_undo(&g) && !mc_undo(&g) && at(0) == 4 && mc_turn(&g) == 0);

    /* 12 seeds or more pass by their pit: the 12th lands in the next one */
    setup(MC_OWARE, (const int[]){12, 1, 1, 1, 1, 1, 4, 4, 4, 4, 4, 4, 3, 4}, 0);
    CHECK(play("1") && at(0) == 0 && m.to[10] == 11 && m.to[11] == 1 && at(1) == 3);

    /* the last seed makes 3, the pits before it 2 and 3: all three taken; a 4 stops the chain */
    setup(MC_OWARE, (const int[]){0, 0, 0, 0, 0, 3, 1, 2, 1, 0, 4, 0, 0, 0}, 0);
    CHECK(play("6") && m.ntake == 3 && m.take[0].pit == 8 && m.take[2].pit == 6 && at(MC_STORE) == 7);
    CHECK(at(6) == 0 && at(7) == 0 && at(8) == 0 && at(10) == 4 && mc_turn(&g) == 1);
    setup(MC_OWARE, (const int[]){0, 0, 0, 0, 0, 3, 1, 3, 1, 0, 4, 0, 0, 0}, 0);
    CHECK(play("6") && m.ntake == 1 && m.take[0].pit == 8 && at(MC_STORE) == 2 && at(7) == 4 && at(6) == 2);

    /* the grand slam: it would take every seed the other player has, so it takes none */
    setup(MC_OWARE, (const int[]){4, 0, 0, 0, 0, 2, 1, 1, 0, 0, 0, 0, 0, 0}, 0);
    CHECK(play("6") && m.grand_slam && m.ntake == 0 && at(6) == 2 && at(7) == 2 && at(MC_STORE) == 0);

    /* the other player has no seed: only a move that gives them some */
    setup(MC_OWARE, (const int[]){2, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 20, 23}, 0);
    CHECK(!mc_legal(&g, 0) && mc_legal(&g, 4) && !play("1") && play("5") && at(6) == 1 && at(7) == 1);

    /* more than 24 wins at once (the seeds left stay on the board) */
    setup(MC_OWARE, (const int[]){0, 0, 0, 0, 0, 1, 1, 0, 0, 4, 4, 4, 23, 7}, 0);
    CHECK(play("6") && mc_status(&g) == MC_WIN0 && at(MC_STORE) == 25 && at(9) == 4 && !mc_any(&g));

    /* no move: the grand slam leaves ivory 2 seeds that cannot reach ebony, each side's seeds go to its player:
     * 24 each, a draw */
    setup(MC_OWARE, (const int[]){1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 22, 24}, 1);
    CHECK(play("12") && m.grand_slam && mc_status(&g) == MC_DRAW && m.ntake == 1 && m.take[0].sweep);
    CHECK(at(0) == 0 && at(MC_STORE) == 24 && at(MC_STORE + 1) == 24);

    /* the same position again: two seeds go once round the board, ebony must feed ivory on the way; then each
     * player takes their side's seed */
    setup(MC_OWARE, (const int[]){0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 22, 24}, 0);
    CHECK(play("6") && !mc_legal(&g, 6) && mc_legal(&g, 11));
    CHECK(play("12 1 7 2 8 3 9 4 10") && mc_status(&g) == MC_PLAY);
    CHECK(play("5 11") && mc_status(&g) == MC_WIN1 && at(MC_STORE) == 23 && at(MC_STORE + 1) == 25 && g.n == 12);
    CHECK(mc_undo(&g) && mc_status(&g) == MC_PLAY && at(10) == 1);

    /* ---- Kalah ---- */
    mc_new(&g, MC_KALAH);
    CHECK(play("3") && m.again && at(MC_STORE) == 1 && at(5) == 5 && mc_turn(&g) == 0);
    CHECK(play("1") && !m.again && at(4) == 6 && mc_turn(&g) == 1);
    CHECK(play("9") && m.to[3] == MC_STORE + 1 && m.again && mc_turn(&g) == 1);

    /* past the other player's store */
    setup(MC_KALAH, (const int[]){1, 1, 0, 0, 0, 9, 4, 4, 4, 4, 4, 4, 5, 9}, 0);
    CHECK(play("6") && m.to[0] == MC_STORE && m.to[6] == 11 && m.to[7] == 0 && m.to[8] == 1);
    CHECK(at(MC_STORE + 1) == 9 && at(0) == 2 && mc_turn(&g) == 1);

    /* the last seed in an empty pit of the sower's: it and the pit opposite to the store; opposite empty: nothing */
    setup(MC_KALAH, (const int[]){1, 0, 2, 0, 0, 0, 3, 3, 3, 3, 5, 3, 10, 12}, 0);
    CHECK(play("1") && m.ntake == 2 && m.take[0].pit == 10 && m.take[1].pit == 1 && at(MC_STORE) == 16);
    CHECK(at(1) == 0 && at(10) == 0 && mc_turn(&g) == 1);
    setup(MC_KALAH, (const int[]){1, 0, 2, 0, 0, 0, 3, 3, 3, 3, 0, 3, 10, 20}, 0);
    CHECK(play("1") && m.ntake == 0 && at(1) == 1 && at(MC_STORE) == 10);

    /* a row empty: the other player takes the rest; here 24 each, a draw */
    setup(MC_KALAH, (const int[]){0, 0, 0, 0, 0, 1, 2, 0, 5, 0, 1, 0, 23, 16}, 0);
    CHECK(play("6") && m.again && m.ntake == 3 && m.take[0].sweep && m.take[0].store == MC_STORE + 1);
    CHECK(mc_status(&g) == MC_DRAW && at(MC_STORE) == 24 && at(MC_STORE + 1) == 24 && !mc_any(&g));
    CHECK(mc_undo(&g) && mc_status(&g) == MC_PLAY && at(5) == 1);

    /* a whole game of first moves, both variants: the seeds are all still there */
    for (int v = 0; v < 2; v++) {
        mc_new(&g, v);
        while (mc_status(&g) == MC_PLAY) {
            int p = 0;
            while (!mc_legal(&g, p)) p++;
            mc_play(&g, p, NULL);
        }
        int sum = 0;
        for (int i = 0; i < MC_SLOTS; i++) sum += at(i);
        CHECK(sum == MC_SEEDS && g.n < MC_MAX);
    }

    printf("mancala: %d checks, %d failed\n", checks, failed);
    return failed ? 1 : 0;
}
