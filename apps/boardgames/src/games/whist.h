/*
 * Whist, for four players in two partnerships, or German whist for two, on one device (hidden hands): the rules. No
 * SDL; the deal comes from a seed (cards.h's shuffle), so a game is its seed and its actions (undo replays them).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Four players: partners sit across, players 0 and 2 against 1 and 3. The 52 cards, 13 each, dealt from the player
 * after the dealer; the dealer's last card is turned face up: its suit is trumps for the deal, and it stays in sight
 * until the dealer plays it. The player after the dealer leads. Follow suit if you can, else play anything; the
 * highest trump wins, or else the highest card of the suit led (ace high); the winner leads the next. A side scores
 * a point for each trick over six (the book). Honours (variants): the ace, king, queen and jack of trumps held by one
 * side when dealt: all four 4 points, three of them 2; tricks count first, and a side one point short of the game
 * scores no honours. The game: 5 points (short whist), or 10 (long whist). The dealer moves on one seat a deal.
 * Two players (German whist): 13 cards each, the rest a stock with its top card face up: its suit is trumps for the
 * deal. While the stock lasts, the winner of each trick takes the face-up card and the loser the next one, unseen;
 * then the next card turns up. Once it is gone, the last 13 tricks are played; only they count: a point for each
 * trick over six. Following suit is required throughout. The non-dealer leads; the deal alternates.
 * Notation (script): a card ("2c", "Qs", "Th"; tricks.h) plays it, n the next deal, "." one plain action (the first
 * legal card), "#" plain actions to the deal's end, "!" to the game's end.
 */
#ifndef WHIST_H
#define WHIST_H

#include <stdint.h>

#include "cards.h"

#define WHIST_MAXP 4
#define WHIST_HAND 13
#define WHIST_STOCK 26
#define WHIST_MAX_ACTS 8192
#define WHIST_NONE 255

enum { WP_PLAY, WP_ROUND, WP_OVER };
enum { WV_SHORT, WV_HONOURS, WV_LONG, WHIST_VARIANTS };   /* 5 points; 5 and honours; 10 and honours */
/* an action: its kind (high byte) and a card */
enum { WA_PLAY = 1 << 8, WA_NEXT = 2 << 8 };
#define WA_KIND(a) ((a) & 0xff00)
#define WA_ARG(a) ((a) & 0xff)

typedef struct {
    uint8_t players, variant, target, phase, turn, dealer, round;
    uint8_t hand[WHIST_MAXP][WHIST_HAND], n[WHIST_MAXP];
    uint8_t stock[WHIST_STOCK], nstock;         /* two players: drawn from its end, the top (the last) face up */
    uint8_t up, trump;                          /* the turned card, its suit (trumps) */
    uint8_t trick[WHIST_MAXP], ntrick, leader;
    uint8_t last[WHIST_MAXP], last_leader, last_winner, tricks;
    uint8_t drew[2];                            /* the cards drawn after the last trick: its winner's, the loser's */
    uint8_t won[WHIST_MAXP], late[WHIST_MAXP];  /* tricks taken this deal; two players: those once the stock is gone */
    uint8_t honours[2];                         /* each side's honours of trumps when dealt (four players) */
    int16_t score[2], pts[2], hon[2];           /* a side (two players: a player); the last deal's tricks, honours */
    uint32_t seed;
} whist_pos;

typedef struct {
    whist_pos pos;
    uint32_t seed;
    uint8_t players, variant;
    uint16_t act[WHIST_MAX_ACTS];
    int n;
} whist_game;

void whist_new(whist_game *g, int players, int variant, uint32_t seed);
int  whist_legal(const whist_pos *p, int act);
int  whist_can_play(const whist_pos *p, int card);
int  whist_play(whist_game *g, int act);        /* 0: not allowed */
int  whist_undo(whist_game *g);                 /* back one action (a replay from the deal); 0: none */
int  whist_side(const whist_pos *p, int player);
int  whist_side_tricks(const whist_pos *p, int side);   /* the tricks that count this deal */
int  whist_honours_count(const whist_pos *p);   /* 1: honours are scored in this game */
int  whist_winner(const whist_pos *p);          /* WP_OVER: the winning side (-1: not over) */
int  whist_auto(const whist_pos *p);            /* a plain action (the "." of the notation) */
int  whist_script(whist_game *g, const char *moves);   /* 0: an action is bad */

/* the bot (whist_bot.c, bots.h), and what the screen (whist.c) gives it; a headless test defines those two */
const whist_game *whist_live(void);   /* the game on the screen, read-only: the bot copies it at entry */
int whist_show(const char *move);     /* main thread: plays one action of the notation as a tap does */
struct ai_stats;
struct tb_state;
int whb_choose(const whist_game *g, int seat, int level, uint64_t seed, struct ai_stats *stats);   /* 0: none */
void whb_view(const whist_game *g, int me, struct tb_state *v, whist_game *scratch);   /* seat me's knowledge */

#endif
