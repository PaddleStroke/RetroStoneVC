/*
 * Écarté, the French game for two on one device (hidden hands): the rules. No SDL; the deals come from a seed
 * (cards.h's shuffle), so a game is its seed and its actions (undo replays them).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The piquet pack of 32; in each suit the king is highest, then queen, jack, ace, 10, 9, 8, 7. The dealer changes
 * each deal (player 2 deals first); the other is the elder hand. Five cards each, the next card turned: its suit is
 * trumps; a king turned scores 1 for the dealer. The rest is the stock.
 * The exchange: the elder may propose (ask for cards) or play at once. Proposed, the dealer accepts or refuses;
 * refused, the play begins. Accepted, the elder throws 1 to 5 cards and draws as many from the stock, then the dealer
 * may do the same (0 to 5); then the elder may propose again, as long as the stock lasts (the last draws take what is left).
 * Before the play, whoever holds the king of trumps scores 1 (counted for them; the French announce it).
 * The play: five tricks, the elder leading the first, the winner of a trick the next. Follow suit and win the trick if
 * you can (a higher card of the suit led); without the suit led, trump if you can; else anything. The higher card of
 * the suit led wins, or the trump.
 * The score: three or four tricks, 1 point; all five (the vole), 2. The elder who played without proposing, or the
 * dealer who refused the first proposal, gives 2 to the other if they then take fewer than three tricks. The first to 5
 * points wins (even by the king, before the play).
 * Notation (script): q proposes, y accepts, x refuses, e<cards> throws them ("eQs7h", "e" alone: none), <card> plays
 * one ("Kh", tricks.h's notation), n the next deal, "." one plain action, "#" plain actions to the deal's end, "!" to
 * the game's end.
 */
#ifndef ECARTE_H
#define ECARTE_H

#include <stdint.h>

#include "cards.h"

#define ECARTE_P 2
#define ECARTE_HAND 5
#define ECARTE_STOCK 21
#define ECARTE_GOAL 5
#define ECARTE_MAX_ACTS 2048

enum { EC_PROPOSE, EC_ANSWER, EC_DISCARD, EC_PLAY, EC_ROUND, EC_OVER };
/* an action: its kind (high byte) and a card or a mask of the hand's cards (bit i: the i-th in sorted order) */
enum { ECA_ASK = 1 << 8, ECA_YES = 2 << 8, ECA_NO = 3 << 8, ECA_DISCARD = 4 << 8, ECA_PLAY = 5 << 8,
       ECA_NEXT = 6 << 8 };
#define ECA_KIND(a) ((a) & 0xff00)
#define ECA_ARG(a) ((a) & 0xff)

typedef struct {
    uint8_t phase, turn, deal, dealer, elder;
    uint8_t hand[ECARTE_P][ECARTE_HAND], n[ECARTE_P];
    uint8_t stock[ECARTE_STOCK], nstock;        /* the stock: drawn from its end */
    uint8_t turned, trump, thrown;              /* the turned card, its suit; the cards thrown this deal */
    uint8_t asked, refused, forced;             /* proposals made; the first refused; played without proposing */
    uint8_t drew[ECARTE_P];                     /* how many each drew at their last exchange */
    uint8_t got[ECARTE_P][ECARTE_HAND];         /* the cards they drew then (CARD_NONE: none) */
    uint8_t trick[ECARTE_P], ntrick, leader;
    uint8_t last[ECARTE_P], last_leader, last_winner, tricks, won[ECARTE_P];
    uint8_t king[ECARTE_P];                     /* the king's point this deal (turned: the dealer's; held) */
    int8_t pts[ECARTE_P], score[ECARTE_P];      /* the deal's points for the tricks; with the king's */
    int8_t total[ECARTE_P];
    uint32_t seed;
} ecarte_pos;

typedef struct {
    ecarte_pos pos;
    uint32_t seed;
    uint16_t act[ECARTE_MAX_ACTS];
    int n;
} ecarte_game;

void ecarte_new(ecarte_game *g, uint32_t seed);
int  ecarte_rank(int card);                     /* the strength in its suit: 7 1 .. king 8 */
void ecarte_sort(uint8_t *hand, int n);
int  ecarte_legal(const ecarte_pos *p, int act);
int  ecarte_play(ecarte_game *g, int act);      /* 0: not allowed */
int  ecarte_undo(ecarte_game *g);
int  ecarte_can_play(const ecarte_pos *p, int card);
int  ecarte_throw_min(const ecarte_pos *p);     /* EC_DISCARD: how few and how many the player to move may throw */
int  ecarte_throw_max(const ecarte_pos *p);
int  ecarte_trick_winner(const uint8_t *trick, int trump);   /* 0 or 1: which card wins */
int  ecarte_winner(const ecarte_pos *p);        /* EC_OVER: who reached 5 (-1: not over) */
int  ecarte_auto(const ecarte_pos *p);
int  ecarte_script(ecarte_game *g, const char *moves);

/* the bot (ecarte_bot.c, bots.h), and what the screen (ecarte.c) gives it; a headless test defines those two */
const ecarte_game *ecarte_live(void);           /* the game on the screen, read-only: the bot copies it at entry */
int ecarte_show(const char *move);              /* main thread: plays one action of the notation as a tap does */
struct ai_stats;
struct tb_state;
int ecb_choose(const ecarte_game *g, int seat, int level, uint64_t seed, struct ai_stats *stats);   /* 0: none */
void ecb_view(const ecarte_game *g, int me, struct tb_state *v, ecarte_game *scratch);   /* seat me's knowledge */

#endif
