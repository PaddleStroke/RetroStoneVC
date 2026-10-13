/*
 * Manille, the French trick-taking game, for four players in two partnerships (partners sit across: players 0 and 2
 * against 1 and 3), the device passed round: the rules. No SDL; each deal is shuffled from the game's seed, so a game
 * is its seed and its actions (undo replays them).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The classic French rules, as played here:
 * - The 32-card pack. In each suit: the ten (the manille) highest, then the ace (the manillon), king, queen, jack, 9,
 *   8, 7. Points: ten 5, ace 4, king 3, queen 2, jack 1: 60 in the pack. Each trick counts 1 more: 68 a deal.
 * - The dealer deals 8 cards each, in two rounds of packets of 4, from the player after them; their own last card is
 *   turned face up: its suit is trumps (the card stays in their hand). Play goes round in player order (the next
 *   player is the one after); the player after the dealer leads first; the dealer moves on one seat each deal (player
 *   3 deals first).
 * - Doubling ("je double"), before the first card, in turn from the player after the dealer: each player passes or
 *   doubles (the deal's score x2); after a double, the doubler's opponents (each in turn, the one after first) may
 *   redouble (x4) or pass. A redouble, or everyone who could speak having spoken, ends it.
 * - The play: follow suit. Following, while an opponent holds the trick with a card of that suit, you must beat it if
 *   you can (monter). Void in the suit led: if an opponent holds the trick you must trump, and over-trump an opponent's
 *   trump if you can (else any card: no need to under-trump); if your partner holds the trick, any card. The highest
 *   trump wins, else the highest card of the suit led; the winner leads the next trick.
 * - The score: each team counts its card points and 1 a trick; the team with more than 34 (half of 68) scores the
 *   difference, times the doubling; 34 each: nobody scores. The first team to 34 points wins the game (a "partie
 *   seche" in 34; only one team scores a deal, so there is no tie).
 * Notation (script): p pass, d double, r redouble, <rank><suit> play a card (7 8 9 T J Q K A; s h d c), N the next
 * deal; "." one plain action (pass; the first card it may play, in the hand's order), "#" plain actions to the end of
 * the deal, "!" to the end of the game.
 */
#ifndef MANILLE_H
#define MANILLE_H

#include <stdint.h>

#include "cards.h"

#define MAN_P 4
#define MAN_HAND 8
#define MAN_TARGET 34
#define MAN_HALF 34
#define MAN_MAX_ACTS 4096
#define MAN_NONE 255

enum { MP_SPEAK, MP_PLAY, MP_DEAL, MP_OVER };
/* an action: its kind (high byte), a card for a play */
enum { MA_PASS = 1 << 8, MA_DOUBLE = 2 << 8, MA_REDOUBLE = 3 << 8, MA_PLAY = 4 << 8, MA_NEXT = 5 << 8 };
#define MA_KIND(a) ((a) & 0xff00)
#define MA_ARG(a) ((a) & 0xff)
/* what each player said */
enum { MS_NONE, MS_PASS, MS_DOUBLE, MS_REDOUBLE };

static inline int man_team(int player) { return player & 1; }

typedef struct {
    uint8_t phase, dealer, turn, deal;          /* deal: deals played */
    uint8_t hand[MAN_P][MAN_HAND], n[MAN_P];    /* each hand sorted (man_sort) */
    uint8_t turned, trump;                      /* the dealer's turned card; trumps */
    uint8_t mult, doubler, need;                /* 1, 2, 4; the doubling team (MAN_NONE); bit q: q may still speak */
    uint8_t said[MAN_P];                        /* MS_* */
    uint8_t leader, trick[MAN_P], ntrick, tricks;   /* trick[i]: played by leader + i */
    uint8_t last[MAN_P], last_leader, last_winner;  /* the trick before (MAN_NONE: none) */
    uint8_t won[2];                             /* tricks won by each team */
    int16_t pts[2];                             /* card points taken */
    int16_t made[2], score[2], total[2];        /* the deal: points and tricks, its score; the game's totals */
    uint32_t seed;
} mpos;

typedef struct {
    mpos pos;
    uint32_t seed;
    uint16_t act[MAN_MAX_ACTS];
    int n;
} mgame;

void man_new(mgame *g, uint32_t seed);
int  man_rank(int card);                        /* its strength in its suit: 7 1 .. ten 8 */
int  man_points(int card);                      /* ten 5, ace 4, king 3, queen 2, jack 1 */
void man_sort(uint8_t *cards, int n, int trump);   /* trumps first, then the colours alternating; strongest first */
int  man_best(const mpos *p);                   /* the index in the trick of the card that holds it (-1: empty) */
int  man_legal(const mpos *p, int act);
int  man_can_play(const mpos *p, int card);
int  man_play(mgame *g, int act);               /* 0: not allowed */
int  man_undo(mgame *g);                        /* back one action (a replay); 0: none */
int  man_winner_team(const mpos *p);            /* MP_OVER: 0 or 1; else -1 */
int  man_auto(const mpos *p);                   /* a plain action (the "." of the notation) */
int  man_script(mgame *g, const char *moves);   /* 0: an action is bad */

/* the bot (manille_bot.c, bots.h), and what the screen (manille.c) gives it; a headless test defines those two */
const mgame *manille_live(void);   /* the game on the screen, read-only: the bot copies it at entry */
int manille_show(const char *move);   /* main thread: plays one action of the notation as a tap does */
struct ai_stats;
struct tb_state;
int manb_choose(const mgame *g, int seat, int level, uint64_t seed, struct ai_stats *stats);   /* the action; 0: none */
void manb_view(const mgame *g, int me, struct tb_state *v, mgame *scratch);   /* seat me's knowledge (trickbot.h) */

#endif
