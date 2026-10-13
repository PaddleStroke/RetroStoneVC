/*
 * Hearts, for 3 to 6 players on one device (hidden hands): the rules. No SDL; the deal comes from a seed (cards.h's
 * shuffle), so a game is its seed and its actions (undo replays them).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The 52 cards, all dealt: four players get 13 each. Three players: the 2 of diamonds is taken out (17 each); five:
 * the 2 of diamonds and the 2 of spades (10 each); six: the 2 and 3 of diamonds and of spades (8 each). Every heart
 * and the queen of spades stay in, so a round always has 26 points, and the 2 of clubs always leads.
 * Before each round's play, each player passes three cards: to the left (the next player), to the right, across (four
 * or six players), then a round without passing; with three or five players: left, right, no pass. The players
 * choose in turn from the round's first player (one seat on each round); the cards change hands once all have chosen.
 * The 2 of clubs leads the first trick. Follow suit if you can; the highest card of the suit led wins the trick (no
 * trumps; ace high) and its winner leads the next. On the first trick nobody may play a heart or the queen of spades,
 * unless their hand holds nothing else. Hearts may not be led until one has been played (broken), unless the leader
 * holds nothing but hearts. Each heart taken is 1 point, the queen of spades 13. A player who takes all 26 shoots
 * the moon: they score 0 and every other player 26. After the round in which someone reaches 100, the game ends; the
 * lowest total wins (a tie: the tied players share it).
 * Notation (script): p<card><card><card> passes three cards, <card> plays one ("2c", "Qs", "Th"), n the next round,
 * "." one plain action (the first legal card; the first three cards to pass), "#" plain actions to the round's end,
 * "!" to the game's end.
 */
#ifndef HEARTS_H
#define HEARTS_H

#include <stdint.h>

#include "cards.h"

#define HEARTS_MAXP 6
#define HEARTS_HAND 17
#define HEARTS_MAX_ACTS 8192
#define HEARTS_LIMIT 100
#define HEARTS_QUEEN CARD(SUIT_SPADES, RANK_QUEEN)
#define HEARTS_LEAD CARD(SUIT_CLUBS, 2)

enum { HP_PASS, HP_PLAY, HP_ROUND, HP_OVER };
enum { PASS_LEFT, PASS_RIGHT, PASS_ACROSS, PASS_HOLD };
/* an action: its kind (top byte) and up to three cards */
enum { HA_PASS = 1 << 24, HA_PLAY = 2 << 24, HA_NEXT = 3 << 24 };
#define HA_KIND(a) ((a) & 0xff000000u)
#define HA_CARD(a, i) ((int)((a) >> (8 * (2 - (i))) & 0xff))   /* the pass's cards 0..2; a play's card: 2 */
#define HA_PASS3(a, b, c) (HA_PASS | (uint32_t)(a) << 16 | (uint32_t)(b) << 8 | (uint32_t)(c))

typedef struct {
    uint8_t players, phase, turn, round, first, dir, size;   /* size: cards in a hand at the deal */
    uint8_t hand[HEARTS_MAXP][HEARTS_HAND], n[HEARTS_MAXP];
    uint8_t pass[HEARTS_MAXP][3], passed;       /* the cards each one chose to pass; how many have chosen */
    uint8_t got[HEARTS_MAXP][3];                /* the cards each one received (CARD_NONE: none) */
    uint8_t trick[HEARTS_MAXP], ntrick, leader; /* the trick on the table, in the order played */
    uint8_t last[HEARTS_MAXP], last_leader, last_winner, tricks;   /* the last trick; tricks played this round */
    uint8_t broken, moon;                       /* hearts broken; who shot the moon last round (255: nobody) */
    uint8_t taken[HEARTS_MAXP];                 /* points taken this round */
    int16_t total[HEARTS_MAXP], score[HEARTS_MAXP];   /* the totals; the last round's points */
    uint32_t seed;
} hpos;

typedef struct {
    hpos pos;
    uint32_t seed;
    uint8_t players;
    uint32_t act[HEARTS_MAX_ACTS];
    int n;
} hgame;

void hearts_new(hgame *g, int players, uint32_t seed);
int  hearts_legal(const hpos *p, uint32_t act);
int  hearts_play(hgame *g, uint32_t act);       /* 0: not allowed */
int  hearts_undo(hgame *g);                     /* back one action (a replay from the deal); 0: none */
int  hearts_points(int card);                   /* a heart 1, the queen of spades 13 */
int  hearts_target(const hpos *p, int player);  /* whom the player passes to this round (themselves: no pass) */
int  hearts_can_play(const hpos *p, int card);  /* the player to move may play it */
int  hearts_winner(const hpos *p);              /* HP_OVER: the lowest total (-1: a tie for it, or not over) */
uint32_t hearts_auto(const hpos *p);            /* a plain action (the "." of the notation) */
int  hearts_script(hgame *g, const char *moves);   /* 0: an action is bad */

/* the bot (hearts_bot.c, bots.h), and what the screen (hearts.c) gives it; a headless test defines those two */
const hgame *hearts_live(void);                 /* the game on the screen, read-only: the bot copies it at entry */
int hearts_show(const char *move);              /* main thread: plays one action of the notation as a tap does */
struct ai_stats;
struct tb_state;
uint32_t hb_choose(const hgame *g, int seat, int level, uint64_t seed, struct ai_stats *stats);   /* 0: none */
void hb_view(const hgame *g, int me, struct tb_state *v, hgame *scratch);   /* seat me's knowledge (trickbot.h) */

#endif
