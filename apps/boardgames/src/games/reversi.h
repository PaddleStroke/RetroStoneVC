/*
 * Reversi's rules (no SDL): 8 x 8, the four discs of the start in the middle, ebony first. A move sets a disc down
 * where it outflanks the other player's discs (a straight line of them, ended by one of the mover's) and turns every
 * line it outflanks. A player who cannot move passes, by itself; the game ends when neither can move, and the most
 * discs win (equal: a draw).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Players: 0 ebony (moves first), 1 ivory. Squares: r * 8 + f, f 0..7 the columns a..h from player 0's left,
 * r 0..7 the rows 1..8 from the top (player 1's side), as in the usual notation: the start is d4 e5 ivory,
 * e4 d5 ebony, and ebony's four first moves are d3 c4 f5 e6.
 */
#ifndef REVERSI_H
#define REVERSI_H

#include <stdint.h>

#define RV_SQ(f, r) ((r) * 8 + (f))
#define RV_MAX 60                               /* moves in a game, at most (passes do not count) */

enum { RV_PLAY, RV_WIN0, RV_WIN1, RV_DRAW };

typedef struct {
    uint64_t disc[2];                           /* each player's discs, a bit a square */
    int side;                                   /* the player to move */
    int passed;                                 /* 1: the other player had no move and passed (side moved last too) */
    int over;                                   /* 1: neither player can move */
} rpos;

typedef struct {
    rpos pos[RV_MAX + 1];                       /* pos[ply]: the position now; pos[0] the start */
    int move[RV_MAX];                           /* the square of each move */
    int ply;
} rgame;

uint64_t rv_flips(const rpos *p, int side, int s);  /* the discs a disc of side set on s would turn (0: not a move) */
uint64_t rv_legal(const rpos *p, int side);         /* the squares side can play */
void rv_make(rpos *p, int s);                       /* plays a legal move of p->side; then the pass, or the end */
int rv_count(const rpos *p, int player);

void rv_new(rgame *g);
int rv_play(rgame *g, int s);                   /* 0: not a legal move */
int rv_undo(rgame *g);                          /* 0: nothing to undo */
int rv_status(const rgame *g);                  /* RV_* */
static inline const rpos *rv_pos(const rgame *g) { return &g->pos[g->ply]; }

/* the bot (reversi_bot.c, bots.h), and what the screen (reversi.c) gives it; a headless test defines those two */
const rgame *reversi_live(void);                /* the game on the screen, read-only: the bot copies it at entry */
int reversi_show(const char *move);             /* main thread: plays "d3" as a tap does (animated); 0: not legal */
struct ai_stats;
uint64_t rvb_moves(uint64_t P, uint64_t O);     /* rv_legal, rv_flips, rv_make on bitboards (P to move) */
uint64_t rvb_flips(uint64_t P, uint64_t O, int s);
void rvb_make(rpos *p, int s);
int rvb_choose(const rpos *p, int level, uint64_t seed, struct ai_stats *stats);   /* the square; -1: none */

#endif
