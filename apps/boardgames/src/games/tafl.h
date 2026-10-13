/*
 * Hnefatafl (Viking chess) for two players on one device: the Copenhagen rules on 11 x 11, Tablut on 9 x 9, and
 * Brandubh, the Irish game, on 7 x 7.
 * No engine, no clock.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Squares: y * n + x on an n x n board, (0, 0) the bottom left corner (a1), x the file (a, b, c...), y the rank (1, 2,
 * 3...). Player 0: the attackers (ebony), who move first; player 1: the defenders and their king (ivory).
 *
 * The rules (Copenhagen; Tablut and Brandubh: the differences after their names):
 * - every piece moves like a chess rook; only the king may stop on the throne (the centre) and the four corners; any
 *   piece may pass over the empty throne (Tablut: the corners are ordinary squares);
 * - a piece moved so that an enemy piece is between it and another of its pieces, or a hostile square (a corner, the
 *   empty throne), along a row or a column, captures it (several at once if so); moving between two enemies is safe;
 *   the king captures too;
 * - the king is captured by attackers on its four sides, or on three sides and the empty throne on the fourth; never
 *   on the edge (Tablut: away from the throne and its four neighbours, the king is captured like any other piece;
 *   Brandubh the same, by two attackers, or an attacker and a corner);
 * - the defenders win when the king reaches a corner (Tablut: any edge square); the attackers when they capture it;
 *   a player who cannot move loses; the same position for the third time (the same player to move) is a draw, and
 *   so is a game of TF_MAX_PLY moves. Left out: the shieldwall, edge forts and encirclement.
 * - Brandubh: 8 attackers against 4 defenders and the king, in a cross; the corners and the throne as in Copenhagen
 *   (the king escapes to a corner).
 */
#ifndef TAFL_H
#define TAFL_H

#include <stdint.h>

enum { TF_HNEFATAFL, TF_TABLUT, TF_BRANDUBH };
enum { TF_EMPTY, TF_ATT, TF_DEF, TF_KING };     /* a piece: an attacker (player 0), a defender or the king (player 1) */
#define TF_OWNER(pc)    ((pc) != TF_ATT)
#define TF_MAX_N        11
#define TF_MAX_SQ       (TF_MAX_N * TF_MAX_N)
#define TF_MAX_MOVES    1024

typedef struct {
    uint8_t sq[TF_MAX_SQ];
    uint8_t side;                       /* who moves */
} tpos;

typedef struct {
    uint8_t from, to;
    uint8_t ncap;                       /* filled by tf_make: the pieces it took (the king among them, maybe) */
    uint8_t cap[4];
} tmove;

int  tf_size(int variant);                                 /* squares a side: 11, 9 or 7 */
int  tf_count(int variant, int piece);                     /* how many of a piece the game starts with */
void tf_start(tpos *p, int variant);
int  tf_throne(int variant);
int  tf_restricted(int variant, int s);                    /* 1: the throne or (not Tablut) a corner */
int  tf_escape(int variant, int s);                        /* 1: the king wins there (a corner; Tablut: the edge) */
int  tf_legal(const tpos *p, int variant, tmove *out);     /* the legal moves of the player to move; their count */
void tf_make(tpos *p, int variant, tmove *m);              /* plays a (legal) move, fills in what it takes */
int  tf_king(const tpos *p, int variant);                  /* the king's square; -1: taken */
/* a square from "a4" (files a.., ranks 1..): -1 if none; *used: the characters read */
int  tf_square(int variant, const char *s, int *used);

/* A game: every position since the start, so undo is free and repetitions can be counted. */
typedef enum { TF_PLAY, TF_WON0, TF_WON1, TF_DRAW } tstatus;
#define TF_MAX_PLY 2000
typedef struct {
    int variant;
    tpos pos[TF_MAX_PLY + 1];
    tmove move[TF_MAX_PLY];
    int ply;
} tgame;
void tf_new(tgame *g, int variant);
int  tf_play(tgame *g, tmove *m);                          /* 0: not legal, or the game is over */
int  tf_undo(tgame *g);                                    /* 0: nothing to undo */
tstatus tf_status(const tgame *g);
/* plays "a4-d4 f2-f3 ...": 0 at the first move that is not legal */
int  tf_script(tgame *g, const char *moves);
#define tf_pos(g) (&(g)->pos[(g)->ply])

/* the bot (tafl_bot.c, bots.h: either side, every board), and what the screen (tafl.c) gives it; a headless test
 * defines those two */
const tgame *tafl_live(void);                   /* the game on the screen, read-only: the bot copies it at entry */
int tafl_show(const char *move);                /* main thread: plays "a4-d4" as a tap does (animated); 0: not legal */
struct ai_stats;
/* the move (from | to << 7) in the game g, its side to move, at a level (g read at once: the position and the ones
 * before it that may come again); -1: none */
int tfb_choose(const tgame *g, int level, uint64_t seed, struct ai_stats *stats);

#endif
