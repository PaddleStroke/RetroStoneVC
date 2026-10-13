/*
 * The Marienbad game (Nim) for two players on one device: the rules. No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Rows of matches: the Marienbad layout, 1, 3, 5 and 7 from the top; or a random layout, 3 to 5 rows of 1 to 8
 * matches each, from the shortest row at the top to the longest. A turn: take one match or more, as many as you like,
 * from one row only. Misere (the classic): the player who takes the last match loses. Normal play: the player who
 * takes the last match wins. Player 0 (ivory) moves first.
 * Each match keeps its place in its row (bit i of a row's mask: the i-th match from the left is still there), so the
 * board shows which ones were taken.
 * Notation (script; the screenshots and the tests): a row's letter (a: the top row) and how many to take, the
 * rightmost ones first ("d3"); or the row's letter and the matches' places ("b:013": the 1st, 2nd and 4th matches of
 * row b, from the left). Moves separated by spaces.
 */
#ifndef NIM_H
#define NIM_H

#include <stdint.h>

#define NIM_ROWS 5
#define NIM_LONG 8                              /* the longest row */
#define NIM_MAX_MOVES 64                        /* a game: at most one move a match */

enum { NIM_MARIENBAD, NIM_MARIENBAD_NORMAL, NIM_RANDOM, NIM_RANDOM_NORMAL, NIM_VARIANTS };
enum { NIM_PLAY, NIM_WIN0, NIM_WIN1 };

typedef struct { uint8_t row, mask; } nim_move;

typedef struct {
    int variant, rows, misere;
    uint8_t start[NIM_ROWS];                    /* each row's matches at the start (its length) */
    uint8_t mask[NIM_ROWS];                     /* the matches still there */
    int taken[2];                               /* how many each player has taken */
    nim_move move[NIM_MAX_MOVES];
    int n, turn;
} nim_game;

/* a new game; the random layouts come from the seed */
void nim_new(nim_game *g, int variant, uint32_t seed);
int  nim_left(const nim_game *g);               /* matches still on the board */
int  nim_count(uint8_t mask);
int  nim_legal(const nim_game *g, int row, int mask);
int  nim_play(nim_game *g, int row, int mask);  /* 0: not allowed */
int  nim_undo(nim_game *g);                     /* 0: nothing to undo */
int  nim_status(const nim_game *g);             /* NIM_* */
/* the move that leaves a position its mover's opponent cannot win (misere or normal); 0: none, every move loses */
int  nim_best(const nim_game *g, int *row, int *mask);
int  nim_script(nim_game *g, const char *moves);   /* 0: a move is bad */

/* the bot (nim_bot.c, bots.h), and what the screen (nim.c) gives it; a headless test defines those two */
const nim_game *nim_live(void);                 /* the game on the screen, read-only: the bot copies it at entry */
int  nim_show(const char *move);                /* main thread: plays "d3" as the take button does; 0: not legal */
/* the bot's move for the player to move at a level (AI_EASY..AI_HARD): a row and how many to take; 0: none */
int  nmb_choose(const nim_game *g, int level, uint64_t seed, int *row, int *take);

#endif
