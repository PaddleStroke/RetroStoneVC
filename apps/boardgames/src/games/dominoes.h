/*
 * Dominoes, the double-six set, for 2 to 4 players on one device: the rules. No SDL; the deal comes from a seed
 * (cards.h's shuffle), so a game is its seed and its actions (undo replays them).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The 28 bones 0-0 .. 6-6. Every player for themselves (no partnerships, even with four). A round: 7 bones each with
 * two players, 5 each with three or four; the rest stay face down, the boneyard. In the first round the holder of the
 * highest double leads it (nobody has a double: the heaviest bone); in the next rounds the winner of the last one
 * leads any bone (a blocked round without a winner: the player after the last leader). A turn: lay a bone whose end
 * matches an open end of the line (a bone that fits both ends: the player chooses). Stuck:
 * DOM_BLOCK, the block game: pass.
 * DOM_DRAW, the draw game: draw from the boneyard until a bone fits; pass only when the boneyard is empty.
 * DOM_FIVES, All Fives (Muggins): drawn like the draw game; the first double played is the spinner, open on all four
 * sides once both its long sides are played on; after each play, open ends that add up to a multiple of 5 score that
 * sum at once (a double at an end counts both halves; the spinner's short sides count once played on).
 * The round ends when a player lays their last bone (they win it) or when nobody can play (blocked: the lowest pip
 * count wins it; a tie for it: nobody). The winner scores the pips left in the other hands (All Fives: rounded to the
 * nearest 5). The game ends after the round in which a player reaches 100 (All Fives: as soon as they reach it); the
 * highest total wins.
 * Notation (script; the screenshots and the tests): p<a><b><end> lay the bone a-b on an end (w e n s; none for the
 * first bone), d draw, x pass, n the next round, a one plain action (the first bone that fits, else draw, else pass),
 * "#" plain actions to the end of the round, "*" to the end of the game.
 */
#ifndef DOMINOES_H
#define DOMINOES_H

#include <stdint.h>

#define DOM_MAXP 4
#define DOM_BONES 28
#define DOM_NONE 255
#define DOM_TARGET 100
#define DOM_MAX_ACTS 4096

enum { DOM_BLOCK, DOM_DRAW, DOM_FIVES, DOM_VARIANTS };
enum { DP_PLAY, DP_ROUND, DP_OVER };
enum { END_W, END_E, END_N, END_S, ENDS, END_C = 7 };      /* the line's ends; END_C: the first bone */
/* an action: its kind, the bone, the end */
enum { DA_PLAY = 1 << 8, DA_DRAW = 2 << 8, DA_PASS = 3 << 8, DA_NEXT = 4 << 8 };
#define DA_KIND(a) ((a) & 0xff00)
#define DA_BONE(a) ((a) >> 3 & 31)
#define DA_END(a) ((a) & 7)
#define DA(kind, bone, end) ((kind) | (bone) << 3 | (end))

/* a bone's two halves: bone 0 is 0-0, then 0-1 .. 0-6, 1-1 .. 1-6, .. 6-6 */
int dom_lo(int bone);
int dom_hi(int bone);
int dom_bone(int a, int b);
static inline int dom_double(int bone) { return dom_lo(bone) == dom_hi(bone); }
static inline int dom_pips(int bone) { return dom_lo(bone) + dom_hi(bone); }

typedef struct {
    uint8_t variant, players, phase, turn, leader, lead, round;   /* lead: the bone the leader must lay (DOM_NONE) */
    uint8_t hand[DOM_MAXP][DOM_BONES], nhand[DOM_MAXP];
    uint8_t yard[DOM_BONES], nyard;             /* the boneyard, its top last */
    /* the line: the bone in the middle (DOM_NONE: nothing laid), its pips on the west and east sides, whether it is
     * the spinner (All Fives); four arms going out from it (N and S: the spinner's), each bone and its outer pip */
    uint8_t center, cw, ce, spinner;
    uint8_t arm[ENDS][DOM_BONES], out[ENDS][DOM_BONES], narm[ENDS];
    uint8_t passes;                             /* passes in a row */
    uint8_t winner;                             /* the round's winner (DOM_NONE: blocked, a tie) */
    int16_t total[DOM_MAXP], last[DOM_MAXP];    /* the totals; the round's points */
    int16_t made[DOM_MAXP];                     /* All Fives: the points scored by laying bones this round */
    int16_t scored;                             /* All Fives: what the last bone laid scored (0: nothing) */
    uint32_t seed;
} dpos;

typedef struct {
    dpos pos;
    uint32_t seed;
    uint8_t variant, players;
    uint16_t act[DOM_MAX_ACTS];
    int n;
} dgame;

void dom_new(dgame *g, int variant, int players, uint32_t seed);
int  dom_open(const dpos *p, int end);           /* 1: the end can take a bone */
int  dom_end(const dpos *p, int end);            /* the pips showing at an end */
int  dom_fits(const dpos *p, int bone, int end); /* the bone can be laid there (the player's, their turn aside) */
int  dom_can_play(const dpos *p);                /* the player to move has a bone that fits */
int  dom_legal(const dpos *p, int act);
int  dom_play(dgame *g, int act);                /* 0: not allowed */
int  dom_undo(dgame *g);                         /* back one action (a replay from the deal); 0: none */
int  dom_count(const dpos *p);                   /* the open ends added up (All Fives) */
int  dom_hand_pips(const dpos *p, int player);
int  dom_auto(const dpos *p);                    /* a plain action */
int  dom_script(dgame *g, const char *moves);    /* 0: an action is bad */
unsigned dom_winners(const dpos *p);             /* the game over: the highest total (all tied for it) */

/* the screen's game (dominoes.c), for its bot (dominoes_bot.c) */
const dgame *dominoes_live(void);               /* read-only: the bot copies it at entry */
int dominoes_show(const char *move);            /* main thread: one action of the notation, as a tap plays it */

#endif
