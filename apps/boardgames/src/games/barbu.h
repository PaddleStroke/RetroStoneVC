/*
 * Barbu, the French compendium game for four players on one device (hidden hands): the rules. No SDL; the deals come
 * from a seed (cards.h's shuffle), so a game is its seed and its actions (undo replays them).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The 52 cards, 13 each, ace high. Seven contracts, each chosen once by each declarer:
 * - no tricks: minus 2 a trick taken;
 * - no hearts: minus 2 a heart, the ace of hearts minus 6 (minus 30 in all); hearts may not be led while the leader
 *   holds anything else; the deal stops once every heart is taken;
 * - no queens: minus 6 a queen; the deal stops once the four are taken;
 * - no king of hearts (the barbu): minus 20 for whoever takes him; hearts may not be led while the leader holds
 *   anything else, and the king's holder, unable to follow suit, must throw him; the deal stops once he is taken;
 * - no last two tricks: the twelfth trick minus 10, the last minus 20;
 * - trumps (the declarer names the suit): plus 5 a trick; follow suit, beating the best trump of the trick when
 *   trumps are led; without the suit led, trump (over the best trump of the trick when possible);
 * - dominoes (fan-tan; the declarer names the starting rank): no tricks; in turn each player lays one card: a card of
 *   that rank starts its suit's row, a card next to a row's end (one rank up or down, the ace highest, no wrapping)
 *   lengthens it; a player who can lay must, else passes; the first out scores 45, the second 20, the third 5, the
 *   last minus 5.
 * Following suit is always required; in the other trick contracts a player without the suit led plays anything. The
 * declarer leads the first trick (or lays first), then the winner of each trick leads the next.
 * The game: each player in turn is the declarer for seven deals in a row (28 deals), choosing each contract once
 * (barbu's dealer). The short game: seven deals, the declarer turning one seat each deal, each contract played once,
 * chosen by whoever declares (players 1, 2 and 3 choose two, player 4 one).
 * Doubling (the variants with doubling): once the contract is named, each other player in turn may double any of the
 * others (in trumps and dominoes only the declarer; never one who has just doubled them); then everyone doubled, the
 * declarer last, may redouble those who doubled them. Each pair doubled settles the difference of their scores in the
 * deal: the better one takes it from the other (twice over when redoubled). The French obligation to double the
 * declarer twice in a game is left out. The scores of a whole game add up to nothing (plus 130, minus 130), doubles
 * included.
 * The highest total wins (a tie: shared).
 * Notation (script): c<n> chooses the contract n (0 no tricks .. 6 dominoes; trumps c5 and a suit "c5h", dominoes c6
 * and a rank "c6J"); d<players> doubles them ("d13", "d" alone: nobody); r<players> redoubles; <card> plays or lays
 * one ("Qs"); p passes (dominoes); n the next deal; "." one plain action, "#" plain actions to the deal's end, "!" to
 * the game's end.
 */
#ifndef BARBU_H
#define BARBU_H

#include <stdint.h>

#include "cards.h"

#define BARBU_P 4
#define BARBU_HAND 13
#define BARBU_CONTRACTS 7
#define BARBU_MAX_ACTS 8192

enum { BB_NOTRICKS, BB_NOHEARTS, BB_NOQUEENS, BB_KING, BB_LASTTWO, BB_TRUMPS, BB_DOMINO };
enum { BBP_CHOOSE, BBP_DOUBLE, BBP_REDOUBLE, BBP_PLAY, BBP_ROUND, BBP_OVER };
/* the variants: bit 0 the short game, bit 1 no doubling */
enum { BARBU_FULL, BARBU_SHORT, BARBU_FULL_PLAIN, BARBU_SHORT_PLAIN, BARBU_VARIANTS };
#define BARBU_IS_SHORT(v) ((v) & 1)
#define BARBU_DOUBLING(v) (!((v) & 2))

/* an action: its kind (the top four bits) and its argument */
enum { BBA_CHOOSE = 1 << 12, BBA_DOUBLE = 2 << 12, BBA_REDOUBLE = 3 << 12, BBA_PLAY = 4 << 12, BBA_PASS = 5 << 12,
       BBA_NEXT = 6 << 12 };
#define BBA_KIND(a) ((a) & 0xf000)
#define BBA_ARG(a) ((a) & 0x0fff)
#define BBA_CONTRACT(c, x) (BBA_CHOOSE | (c) | (x) << 4)   /* x: the trump suit, the dominoes' rank (2..14) */
#define BARBU_KING CARD(SUIT_HEARTS, RANK_KING)

typedef struct {
    uint8_t variant, phase, deal, deals, declarer, turn, step;   /* step: who has had their say (doubling) */
    uint8_t contract, trump, rank;              /* trump: its suit (255: none); rank: dominoes' start (2..14) */
    uint8_t chosen[BARBU_P], used;              /* the contracts each declarer has chosen; all chosen (short) */
    uint8_t hand[BARBU_P][BARBU_HAND], n[BARBU_P];
    uint8_t trick[BARBU_P], ntrick, leader;
    uint8_t last[BARBU_P], last_leader, last_winner, tricks;
    uint8_t won[BARBU_P];                       /* tricks taken this deal */
    uint8_t taken;                              /* the penalty cards taken (hearts, queens, the king) */
    uint8_t dbl[BARBU_P][BARBU_P];              /* dbl[a][b]: 1 a doubled b, 2 b redoubled */
    uint8_t lo[4], hi[4];                       /* dominoes: each suit's row, its ends (0: empty) */
    uint8_t out[BARBU_P], nout;                 /* dominoes: the players out, in order */
    int16_t pts[BARBU_P];                       /* the deal's points so far (the contract's) */
    int16_t settle[BARBU_P];                    /* the last deal: what the doubles moved */
    int16_t score[BARBU_P], total[BARBU_P];     /* the last deal's score (doubles in); the totals */
    int16_t table[BARBU_P][BARBU_CONTRACTS];    /* each player's points in each contract */
    uint32_t seed;
} barbu_pos;

typedef struct {
    barbu_pos pos;
    uint32_t seed;
    uint8_t variant;
    uint16_t act[BARBU_MAX_ACTS];
    int n;
} barbu_game;

void barbu_new(barbu_game *g, int variant, uint32_t seed);
int  barbu_legal(const barbu_pos *p, int act);
int  barbu_play(barbu_game *g, int act);        /* 0: not allowed */
int  barbu_undo(barbu_game *g);                 /* back one action (a replay); 0: none */
int  barbu_can_choose(const barbu_pos *p, int contract);
int  barbu_can_play(const barbu_pos *p, int card);   /* the player to move may play (lay) it */
int  barbu_must_pass(const barbu_pos *p);       /* dominoes: nothing to lay */
unsigned barbu_double_mask(const barbu_pos *p); /* BBP_DOUBLE / REDOUBLE: whom the player to move may (re)double */
int  barbu_card_points(int contract, int card); /* a penalty card's points (no hearts, no queens, the king) */
int  barbu_declarer_of(int variant, int deal);
unsigned barbu_winners(const barbu_pos *p);     /* BBP_OVER: the highest total (bit n: player n; ties) */
int  barbu_auto(const barbu_pos *p);            /* a plain action (the "." of the notation) */
int  barbu_script(barbu_game *g, const char *moves);   /* 0: an action is bad */

/* the bot (barbu_bot.c, bots.h), and what the screen (barbu.c) gives it; a headless test defines those two */
const barbu_game *barbu_live(void);   /* the game on the screen, read-only: the bot copies it at entry */
int barbu_show(const char *move);   /* main thread: plays one action of the notation as a tap does */
struct ai_stats;
struct tb_state;
/* the action (0: none); seat me's knowledge (trickbot.h) */
int bab_choose(const barbu_game *g, int seat, int level, uint64_t seed, struct ai_stats *stats);
void bab_view(const barbu_game *g, int me, struct tb_state *v, barbu_game *scratch);

#endif
