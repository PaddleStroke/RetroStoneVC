/*
 * Surakarta (Java, Indonesia) for two players on one device: the rules (steps, captures around the loops, the ends of
 * the game). No engine, no clock.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The board: 6 x 6 points, a1 .. f6 (sk_x, sk_y: 0..5, a1 at 0, 0; a point is x + 6 y). Two circuits run through it,
 * each a closed track: the inner one along the lines 1 and 4 (b and e, 2 and 5), joined by the four small loops (a
 * quarter of the board's corner left out of a circle of radius 1 round each corner point), the outer one along the
 * lines 2 and 3 (c and d, 3 and 4), joined by the four big loops (radius 2). sk_track[c]: circuit c's 24 points in
 * order (a point where its two lines cross comes twice); a loop joins the entries 5 and 6, 11 and 12, 17 and 18, 23
 * and 0.
 * A point holds -1 (empty), or the player whose piece is there.
 * The rules: 12 pieces each, on the two rows nearest their player. Player 0 (ivory) moves first. A piece steps to any
 * of its 8 neighbouring points (straight or slanting) if it is empty, or captures: it runs along a circuit, through
 * one loop at least, over empty points only (its own starting point counts as empty when the run crosses it again),
 * and lands on the first piece it meets, an enemy one, which is taken. Capturing is never compulsory.
 * The end: a player with no pieces left, or no move, loses. The game also ends when the same position comes back for
 * the third time, or after 40 moves each without a capture (SK_QUIET plies): then the player with more pieces left
 * wins; with as many, it is a draw.
 * Notation (script, the screenshots and the tests): "b2-c3" (or "b2c3") a step, "b2xe5" a capture; separated by
 * spaces: "b2-b3 e5-e4 b3xe4".
 */
#ifndef SURAKARTA_H
#define SURAKARTA_H

#include <stdint.h>

#define SK_SIDE 6
#define SK_POINTS 36
#define SK_PIECES 12
#define SK_TRACK 24                             /* a circuit's entries */
#define SK_MAX_MOVES 200                        /* 12 pieces: 8 steps and 8 runs each at most */
#define SK_QUIET 80                             /* moves without a capture: the end of the game */
#define SK_MAX_PLY 2100                         /* more than a game can last (23 captures, 80 quiet moves between) */

extern const int8_t sk_dx[8], sk_dy[8];         /* the 8 neighbours: N, NE, E, SE, S, SW, W, NW (y up) */
extern const int8_t sk_track[2][SK_TRACK];      /* the circuits: 0 the inner (red), 1 the outer (green) */

static inline int sk_x(int pt) { return pt % SK_SIDE; }
static inline int sk_y(int pt) { return pt / SK_SIDE; }
static inline int sk_loop_after(int i) { return i % 6 == 5; }   /* a loop joins entry i and the next one */
int  sk_point(const char *s);                   /* "c3": its point; -1 */
/* the corner point a loop goes round (the loop after entry i of circuit c; its radius: c + 1) */
int  sk_corner(int c, int i);

/* A move: a step (circuit -1), or a capture's run: along circuit `circuit`, from its entry `start`, `steps` entries
 * on in direction `dir` (+1 or -1, round the track) to the piece taken on `to`. */
typedef struct { int8_t from, to, circuit, start, dir, steps; } skmove;

typedef struct {
    int8_t at[SK_POINTS];
    uint8_t side;                               /* who moves */
    uint8_t left[2];                            /* each player's pieces on the board */
    uint16_t quiet;                             /* moves since the last capture */
} skpos;

void sk_start(skpos *p);
/* the legal moves: the steps, then the captures (one run for each piece that can be taken from a point: the
 * shortest); their count */
int  sk_legal(const skpos *p, skmove *out);
void sk_make(skpos *p, skmove m);               /* plays a (legal) move */
int  sk_loops(skmove m);                        /* the loops a capture's run goes through */
float sk_length(skmove m);                      /* a run's length, the points' spacing 1 (a step: its own) */

/* A game: every position since the start, so undo is free and repetitions can be counted. */
typedef enum { SS_PLAY, SS_STUCK, SS_REPETITION, SS_QUIET } skstatus;
typedef struct {
    skpos pos[SK_MAX_PLY + 1];
    skmove move[SK_MAX_PLY];
    int ply;
} skgame;
void sk_new(skgame *g);
int  sk_play(skgame *g, skmove m);              /* 0: the game is too long */
int  sk_undo(skgame *g);                        /* 0: nothing to undo */
/* SS_STUCK: the player to move has no pieces or no move, and lost; SS_REPETITION, SS_QUIET: the game ended there */
skstatus sk_status(const skgame *g);
/* the winner (0, 1), -1 while the game goes on, 2 a draw */
int  sk_winner(const skgame *g);
int  sk_script(skgame *g, const char *moves);   /* plays moves in the notation above; 0: one is bad */
#define sk_pos(g) (&(g)->pos[(g)->ply])

#endif
