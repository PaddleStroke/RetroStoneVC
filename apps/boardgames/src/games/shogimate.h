/*
 * Shogi Mate, for one player: mate problems on the shogi board (shogi.h's rules), as tsume shogi has them. The
 * attacker (sente, at the bottom) moves first and every one of its moves must give check; the defender (gote) holds
 * every piece that is neither on the board nor in the attacker's hand, and plays the longest defence. Mate in 1, 3,
 * 5 or 7 moves (both sides' counted).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * A problem: the board and the attacker's hand, and its main line (the attacker's moves, each the only one that mates
 * in time, but the last: any mate is right; the defender's replies, each one of its longest). Made by
 * tools/shogimate_gen.c with the mate solver here (shogimate_levels.h); the attacker never has a king, and its hand
 * is empty at the end of the line. On the device a check off the line is a mistake: the defender's reply that escapes
 * is shown, then both are taken back. A move that gives no check is not allowed.
 */
#ifndef SHOGIMATE_H
#define SHOGIMATE_H

#include <stdint.h>

#include "games/shogi.h"

#define SMT_LEVELS 150
#define SMT_CHAPTERS 6
#define SMT_MAX 9                               /* the longest line */

typedef struct {
    spos start;                                 /* sente to move; gote's hand: all the pieces left */
    smove line[SMT_MAX];
    int n;                                      /* the line's moves: 1, 3, 5, 7 */
} smt_problem;

/* ---- the solver -------------------------------------------------------------------------------------------------- */
int  smt_checks(const spos *p, smove *out);     /* the attacker's legal moves that give check */
int  smt_replies(const spos *p, smove *out);    /* the defender's legal moves (shogi.h's, faster: drops interpose) */
int  smt_mated(const spos *p);                  /* 1: the side to move is in check and has no legal move */
/* the fewest moves (1, 3, ..) the attacker (to move) needs to mate, every move a check, up to most; 0: none so far */
int  smt_mate_len(const spos *p, int most);
void smt_forget(void);                          /* the solver's table cleared */
long smt_searched(void);                        /* the positions the solver searched, since it started */
void smt_fill_hand(spos *p);                    /* gote's hand: every piece left (the kings aside) */

/* ---- the problems ------------------------------------------------------------------------------------------------ */
/* "<board> <sente's hand or -> <moves in USI>": "7kl/7p1/9/7N1/9/9/9/9/9 G G*2b"; 0: not a good one (each move
 * legal, the attacker's checks) */
int  smt_parse(smt_problem *pz, const char *s);
void smt_write(const smt_problem *pz, char *out, int size);
int  smt_level_load(smt_problem *pz, int level);

/* ---- a problem played -------------------------------------------------------------------------------------------- */
enum { SMT_ILLEGAL, SMT_WRONG, SMT_RIGHT, SMT_SOLVED };
typedef struct {
    smt_problem pz;
    spos pos[SMT_MAX + 3];                      /* pos[k]: after k moves */
    smove played[SMT_MAX + 2];
    int ply, level;
    int failed;                                 /* a mistake and its refutation on the board (smt_undo takes them) */
    int mistakes, hints, hint_level, hint_ply;
} smt_game;

int  smt_level(smt_game *g, int level);
void smt_begin(smt_game *g, const smt_problem *pz, int level);
int  smt_solved(const smt_game *g);
int  smt_to_find(const smt_game *g);            /* 1: the player's move, the problem not over */
smove smt_expected(const smt_game *g);          /* the line's move to find */
/* the player's move: SMT_ILLEGAL (not legal, or no check: nothing counted), SMT_WRONG (a mistake: the move and the
 * defender's escape both played), SMT_RIGHT (played, and the defender's reply), SMT_SOLVED (mate) */
int  smt_try(smt_game *g, smove m);
int  smt_hint(smt_game *g);                     /* its next level: 1 the piece, 2 its square too; 0: none */
int  smt_undo(smt_game *g);                     /* back to the player's previous move */
#define smt_pos(g) (&(g)->pos[(g)->ply])
int  smt_script(smt_game *g, const char *moves);   /* "G*2b 1c2b" (USI; "#n" first: level n + 1) */

#endif
