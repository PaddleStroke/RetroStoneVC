/*
 * Mancala's rules (no SDL): two rows of six pits, four seeds in each, sown counter-clockwise. Two games:
 * Oware (abapa): captures of 2 or 3 in the other player's row, the grand slam that takes nothing, the duty to feed a
 * player who has no seed; more than 24 seeds wins. Kalah: a store at each end, sown into, an extra turn for the last
 * seed there, the capture of the pit opposite; the seeds left go to their side's player when a row is empty.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Players: 0 ivory (moves first, the bottom row), 1 ebony. Slots: the pits 0..11 counter-clockwise from player 0's
 * left (0..5 player 0's from their left, 6..11 player 1's from their left: pit 11 - i faces pit i), then the stores
 * MC_STORE + player (Oware: the seeds each player has captured; player 0's at their right end, player 1's at the
 * left). The moves' notation (script, the screenshots, the tests): the pits sown, numbered 1 to 12, "3 9 4 12".
 */
#ifndef MANCALA_H
#define MANCALA_H

#include <stdint.h>

#define MC_PITS 12
#define MC_STORE 12
#define MC_SLOTS 14
#define MC_SEEDS 48
#define MC_MAX 1000                             /* moves in a game, at most: then it ends as by repetition */
#define MC_OWNER(pit) ((pit) / 6)

enum { MC_OWARE, MC_KALAH };
enum { MC_PLAY, MC_WIN0, MC_WIN1, MC_DRAW };

typedef struct {
    uint8_t seeds[MC_SLOTS];                    /* in each pit, then in each store */
    uint8_t turn;                               /* the player to move */
    uint8_t over;                               /* 1: the game has ended */
} mc_pos;

/* seeds taken off the board after the sowing: a capture, or the end's sweep (each side's seeds to its player) */
typedef struct { uint8_t pit, n, store, sweep; } mc_take;

/* what a move did, seed by seed (the board's animation) */
typedef struct {
    int pit, n;                                 /* the pit sown, its seeds */
    uint8_t to[MC_SEEDS];                       /* where each seed went, in order: a pit, or a store (Kalah) */
    int ntake;
    mc_take take[MC_PITS + 6];                  /* the captures in the order taken, then the sweep */
    int grand_slam;                             /* Oware: it would have taken all the other player's seeds */
    int again;                                  /* Kalah: the last seed in the store, the same player again */
} mc_move;

typedef struct {
    int variant;                                /* MC_OWARE, MC_KALAH */
    mc_pos pos[MC_MAX + 1];                     /* pos[n]: the position now; pos[0] the start */
    uint8_t move[MC_MAX];                       /* the pits sown */
    int n;
} mc_game;

void mc_new(mc_game *g, int variant);
const mc_pos *mc_now(const mc_game *g);
int  mc_legal(const mc_game *g, int pit);       /* 1: the player to move may sow this pit */
int  mc_any(const mc_game *g);                  /* 1: the player to move has a move */
int  mc_play(mc_game *g, int pit, mc_move *m);  /* sows it (m: what happened, may be NULL); 0: not allowed */
int  mc_undo(mc_game *g);                       /* 0: nothing to undo */
int  mc_turn(const mc_game *g);
int  mc_status(const mc_game *g);               /* MC_* */
int  mc_score(const mc_game *g, int player);    /* the seeds in the player's store */

#endif
