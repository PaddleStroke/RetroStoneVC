/*
 * Bao for two players on one device: the rules of Bao la kujifunza, the learner's Bao of Tanzania and Zanzibar (no
 * SDL). The board: 4 rows of 8 pits; each player owns the two rows nearer them, an inner row (by the middle) and an
 * outer one. All 64 seeds start on the board, 2 in every pit.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Pits: each player's 16 make a ring, sown round and round: 0..7 the inner row from the player's left, 8..15 the outer
 * row from their right (so 7 -> 8 and 15 -> 0 are neighbours). A step of +1 goes clockwise (seen from above, for both
 * players), -1 anticlockwise. Player p's inner pit i faces the other player's inner pit 7 - i. The kichwa are the
 * inner row's end pits (0 the left one, 7 the right one); the kimbi the two pits at each end (0, 1 and 6, 7).
 * The rules:
 * - A move: take all the seeds of one of your pits holding 2 or more and sow them one by one into the next pits of
 *   your ring, clockwise or anticlockwise as you choose.
 * - The last seed falls in a pit that held seeds: if it is in your inner row and the pit facing it holds seeds, you
 *   capture them (in a capturing move only); else you take up all that pit's seeds and sow on (relay sowing).
 *   It falls in an empty pit: the move ends.
 * - Captured seeds are sown into your inner row from a kichwa: from the left one (going right) when the capture was
 *   in the left kimbi, from the right one (going left) in the right kimbi; in the middle pits, from the end that keeps
 *   the sowing turning the same way. Their last seed may capture again, or sow on, as above.
 * - A capture must be taken: when some move's first sowing ends in a capture, only such moves may be played (and
 *   that whole move may capture). Otherwise the move captures nothing (takasa), and must start from the inner row if
 *   a pit there holds 2 seeds or more.
 * - A player whose inner row is empty, or who cannot move (no pit holds 2 seeds), loses.
 * - A move that would sow on for ever stops after BAO_MAX_SOWN seeds; a game of BAO_MAX_MOVES moves ends: the most
 *   seeds wins (equal: a draw).
 * Notation (script, the screenshots, the tests): a pit as a column a..h from ivory's left and a row 1..4 from ivory's
 * side (1 ivory's outer row, 2 its inner row, 3 ebony's inner row, 4 its outer row), then + clockwise or -
 * anticlockwise: "d2+ e3- c2-".
 */
#ifndef BAO_H
#define BAO_H

#include <stdint.h>

#define BAO_RING 16
#define BAO_SEEDS 64
#define BAO_MAX_MOVES 600
#define BAO_MAX_SOWN 2000                       /* seeds sown in one move, at most */
#define BAO_TRACE 2600                          /* the steps of a move kept for the animation */

typedef struct {
    uint8_t pit[2][BAO_RING];                   /* each player's ring */
    uint8_t turn;
    int8_t winner;                              /* -1 playing, 0, 1, 2 a draw */
    uint16_t moves;
} bao_pos;

/* a move: pit * 2 + (1 clockwise, 0 anticlockwise) */
static inline int bao_mv(int pit, int cw) { return pit * 2 + (cw ? 1 : 0); }
static inline int bao_mv_pit(int m) { return m >> 1; }
static inline int bao_mv_cw(int m) { return m & 1; }

/* what a move did, step by step (the board's animation): seeds taken into the hand from a pit (PICK, a player's own;
 * TAKE, captured from the other's), one dropped into a pit (DROP) */
enum { BAO_PICK, BAO_TAKE, BAO_DROP };
typedef struct { uint8_t kind, player, pit, n; } bao_step;
typedef struct { int n; bao_step s[BAO_TRACE]; int captures; } bao_trace;

typedef struct {
    bao_pos pos[BAO_MAX_MOVES + 1];             /* pos[n]: now */
    uint8_t move[BAO_MAX_MOVES];
    int n;
} bao_game;

void bao_new(bao_game *g);
void bao_start(bao_pos *p);
int  bao_moves(const bao_pos *p, int *out);     /* the legal moves (at most 32); 0: none */
int  bao_capturing(const bao_pos *p, int move); /* 1: its first sowing ends in a capture */
int  bao_legal(const bao_pos *p, int move);
void bao_apply(bao_pos *p, int move, bao_trace *t);   /* plays a legal move (t may be NULL) */
int  bao_seeds(const bao_pos *p, int player);   /* in the player's 16 pits */
int  bao_inner(const bao_pos *p, int player);   /* in their inner row */
/* the pit on the board: col 0..7 from ivory's left, row 0..3 from ivory's side (bao.h's 1..4) */
void bao_cell(int player, int pit, int *col, int *row);
int  bao_at(int col, int row, int *player);     /* the pit of that cell (its player in *player) */
/* the game, with its history (undo) */
const bao_pos *bao_now(const bao_game *g);
int  bao_play(bao_game *g, int move, bao_trace *t);   /* 0: not legal */
int  bao_undo(bao_game *g);
int  bao_parse(const bao_pos *p, const char *s, int *move);   /* one move's notation; returns its length, 0: bad */
void bao_name(const bao_pos *p, int move, char *out);         /* "d2+" (4 bytes) */
int  bao_script(bao_game *g, const char *moves);

/* the bot (bao_bot.c, bots.h) */
const bao_game *bao_live(void);                 /* the game on the screen, read-only: the bot copies it at entry */
int  bao_show(const char *move);                /* main thread: sows "d2+" as taps do (animated); 0: not legal */

#endif
