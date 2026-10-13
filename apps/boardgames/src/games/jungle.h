/*
 * Jungle (Dou Shou Qi) for two players on one device: the rules (ranks, the rivers, the jumps, the traps, the dens,
 * the ends of the game). No engine, no clock.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The board: 7 files (a..g) x 9 ranks (1..9, from player 0's side); a square is file + 7 * (rank - 1), a1 = 0.
 * The rivers: b4..c6 and e4..f6 (two areas of 2 x 3 water). The dens: d1 (player 0's), d9 (player 1's); each den's
 * three traps: c1 e1 d2, and c9 e9 d8.
 * The animals, by rank: 8 elephant, 7 lion, 6 tiger, 5 leopard, 4 wolf, 3 dog, 2 cat, 1 rat. The start, player 0
 * (ivory, moves first): lion g1, tiger a1, dog f2, cat b2, rat g3, leopard e3, wolf c3, elephant a3; player 1 (ebony)
 * the same turned round: lion a9, tiger g9, dog b8, cat f8, rat a7, leopard c7, wolf e7, elephant g7.
 * A move: one animal one square along a rank or a file. Only the rat swims (enters the water). The lion and the
 * tiger jump across a river in a straight line, to the first square beyond the water, unless a rat (of either side)
 * is in the water on the way. No animal enters its own den.
 * Captures: onto an enemy of equal or lower rank; the rat takes the elephant, the elephant never takes the rat. An
 * animal in one of its enemy's traps (the traps round the enemy's den) has rank 0: any enemy animal takes it. A
 * capture is never made between the water and the land: a rat in the water takes only the other rat in the water,
 * and a rat on land cannot be taken from the water (nor one in the water from the land).
 * The end: an animal entering the enemy den wins; so does taking every enemy animal, or leaving the enemy without a
 * move. A draw: the same position (and player to move) the third time, or 50 moves each without a capture.
 * Notation (script, the screenshots and the tests): a move is its two squares, "a3a4" (or "a3-a4", "a3xa4"); a
 * jump the same, "a1a5" ... separated by spaces: "a3a4 a7a6 g3g4".
 */
#ifndef JUNGLE_H
#define JUNGLE_H

#include <stdint.h>

#define JG_W 7
#define JG_H 9
#define JG_SQUARES 63
#define JG_MAX_MOVES 32                         /* 8 animals, 4 ways each */
#define JG_QUIET 100                            /* moves (both players) without a capture: a draw */
#define JG_MAX_PLY 1700                         /* more than a game can last: 15 captures, 100 quiet moves after each */

enum { JG_RAT = 1, JG_CAT, JG_DOG, JG_WOLF, JG_LEOPARD, JG_TIGER, JG_LION, JG_ELEPHANT };
#define JG_PC(player, rank) ((player) << 4 | (rank))    /* a square's animal; 0: empty */
#define JG_RANK(pc) ((pc) & 15)
#define JG_OWNER(pc) ((pc) >> 4)
#define JG_SQ(file, rank) ((file) + JG_W * (rank))     /* file 0..6, rank 0..8 */
#define JG_FILE(s) ((s) % JG_W)
#define JG_ROW(s) ((s) / JG_W)

int jg_water(int s);                            /* 1: a river square */
int jg_trap(int s);                             /* the player whose den the trap guards; -1: not a trap */
int jg_den(int s);                              /* the den's player; -1: not a den */
int jg_square(const char *s);                   /* "d4": its square; -1 */

typedef struct { int8_t from, to; uint8_t taken; } jgmove;   /* taken: the animal captured, 0: none */

typedef struct {
    uint8_t sq[JG_SQUARES];
    uint8_t side;                               /* who moves */
    uint8_t left[2];                            /* each player's animals left */
    uint16_t quiet;                             /* moves since the last capture */
} jgpos;

void jg_start(jgpos *p);
int  jg_legal(const jgpos *p, jgmove *out);     /* the legal moves; their count */
void jg_make(jgpos *p, jgmove m);               /* plays a (legal) move */

/* A game: every position since the start, so undo is free and repetitions can be counted. */
typedef enum { JS_PLAY, JS_DEN, JS_ALL, JS_STUCK, JS_REPETITION, JS_QUIET } jgstatus;
typedef struct {
    jgpos pos[JG_MAX_PLY + 1];
    jgmove move[JG_MAX_PLY];
    int ply;
} jggame;
void jg_new(jggame *g);
int  jg_play(jggame *g, jgmove m);              /* 0: the game is too long */
int  jg_undo(jggame *g);                        /* 0: nothing to undo */
/* JS_DEN (an animal in the enemy den), JS_ALL (no animal left), JS_STUCK (no move): the player to move lost;
 * JS_REPETITION, JS_QUIET: a draw */
jgstatus jg_status(const jggame *g);
int  jg_script(jggame *g, const char *moves);   /* plays moves in the notation above; 0: one is bad */
#define jg_pos(g) (&(g)->pos[(g)->ply])

/* the bot (jungle_bot.c, bots.h): the game on the screen, read-only, and one move ("a3a4") played as if tapped
 * (jungle.c; a headless test defines both) */
const jggame *jungle_live(void);
int jungle_show(const char *move);

#endif
