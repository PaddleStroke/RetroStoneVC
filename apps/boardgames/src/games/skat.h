/*
 * Skat, for three players on one device (hidden hands), by the International Skat Order (ISkO): the rules. No SDL;
 * the deal comes from a seed (cards.h's shuffle), so a game is its seed and its actions (undo replays them).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The 32-card pack (7 to ace), French suits. Each deal: 10 cards each (packets of 3, the skat's 2, 4, 3), 2 face down
 * (the skat). The dealer moves on one seat each deal (player 2 deals first); the player after the dealer is forehand
 * (Vorhand: speaks last, leads first), then middlehand, then rearhand (the dealer).
 * - Bidding (Reizen) by the game values 18, 20, 22, 23, 24, 27...: middlehand says to forehand, who holds (yes) or
 *   passes; the one left listens to rearhand saying; a pass ends each duel. The survivor is the declarer at the last
 *   value said. Nobody said anything: forehand may still bid (18 or more) or pass; all three passed: the deal is
 *   thrown in (it still counts as one of the list's deals, no score).
 * - The declarer picks up the skat and lays two cards away (they count for him), or plays from the hand (Hand, the
 *   skat untouched, still his at the end). Then he names the game: a suit (diamonds 9, hearts 10, spades 11, clubs 12),
 *   grand (24: the jacks the only trumps) or null (no trumps, he must take no trick: 23; hand 35; ouvert 46; hand
 *   ouvert 59; a null worth less than the bid may not be named). Hand games may announce schneider, schwarz (with
 *   schneider) and ouvert (with schwarz: the hand shown to all before the first card).
 * - Trumps in a suit game: the jacks (clubs, spades, hearts, diamonds), then A 10 K Q 9 8 7 of the suit; grand: the
 *   four jacks alone. Other suits A 10 K Q 9 8 7; null: A K Q J 10 9 8 7 in every suit. A jack belongs to the trumps
 *   (not its suit) in suit games and grand. Follow suit; void: anything. Forehand leads; a trick's winner leads next.
 * - Card points: A 11, 10 10, K 4, Q 3, J 2 (120 in all). The declarer (with the skat's cards) wins with 61 or more;
 *   schneider: one side 30 or less; schwarz: one side no trick. Announced schneider needs 90, schwarz every trick.
 *   Null: lost as soon as the declarer takes a trick (the deal ends there).
 * - The game value: suit and grand, the base times the levels: matadors (with or without: the run of top trumps held,
 *   or missing, from the club jack down, counting the skat) + 1 (game) + hand + schneider + announced + schwarz +
 *   announced + ouvert (an announcement counts with the level it announces). Overbid (the value under the bid): lost,
 *   at the lowest multiple of the base reaching the bid. Won: + the value; lost: - twice the value.
 * - The list: 12 or 36 deals (SKAT_SHORT, SKAT_LONG). The totals by the Seeger-Fabian system: the game scores, + 50 for
 *   each game won, - 50 for each game lost, and + 40 to each of the two others for each game a declarer lost. The
 *   highest total wins; equal: the tied players share the win.
 * Notation (script): b<value> says a bid, h holds, p passes, s picks up the skat, d<card> lays a card away (into the
 * skat), u<card> takes it back, g<s|h|d|c|g|n>[S][Z][O] names the game (suit, grand, null; schneider, schwarz, ouvert
 * announced), <card> plays one ("Jc", "Ts"; tricks.h), N the next deal; "." one plain action, "#" plain actions to the
 * end of the deal, "!" to the end of the game.
 */
#ifndef SKAT_H
#define SKAT_H

#include <stdint.h>

#include "cards.h"

#define SKAT_P 3
#define SKAT_HAND 12                            /* 10, and the skat picked up */
#define SKAT_MAX_ACTS 8192
#define SKAT_DEALS_MAX 36
#define SKAT_NONE 255

enum { SKAT_SHORT, SKAT_LONG, SKAT_VARIANTS };
enum { SK_BID, SK_SKAT, SK_DISCARD, SK_PLAY, SK_DEAL, SK_OVER };
/* the games: the suits as cards.h numbers them, then grand and null */
enum { SG_GRAND = 4, SG_NULL = 5, SG_TYPES };
/* the announcements (a game's flags) */
enum { SF_SCHNEIDER = 1, SF_SCHWARZ = 2, SF_OUVERT = 4 };
/* an action: its kind and its argument (a value, a card, a game: type | flags << 4) */
enum { SA_SAY = 1 << 16, SA_HOLD = 2 << 16, SA_PASS = 3 << 16, SA_PICKUP = 4 << 16, SA_PUT = 5 << 16,
       SA_TAKE = 6 << 16, SA_GAME = 7 << 16, SA_PLAY = 8 << 16, SA_NEXT = 9 << 16 };
#define SKA_KIND(a) ((a) & 0xff0000u)
#define SKA_ARG(a) ((int)((a) & 0xffff))
#define SKA_GAME(type, flags) (SA_GAME | (uint32_t)(type) | (uint32_t)(flags) << 4)

/* each player's last word in the bidding */
enum { SW_NONE, SW_SAID, SW_HELD, SW_PASSED };

/* a deal of the list, once scored */
typedef struct {
    uint8_t declarer;                           /* SKAT_NONE: thrown in (everyone passed) */
    uint8_t type, flags, hand, won, overbid;
    uint8_t schneider, schwarz;                 /* reached (by either side), or announced */
    int8_t matadors;                            /* with (> 0) or without (< 0) */
    uint8_t levels, base;
    int16_t bid, value, score;                  /* the value (overbid: raised to reach the bid); the declarer's score */
    uint8_t points;                             /* the declarer's card points (and the skat's) */
    uint8_t tricks;                             /* the declarer's tricks */
} skdeal;

typedef struct {
    uint8_t variant, phase, deal, dealer, turn;
    uint8_t hand[SKAT_P][SKAT_HAND], n[SKAT_P];
    uint8_t skat[2], nskat;                     /* the skat: as dealt, then the cards laid away */
    uint8_t dealt[2];                           /* the skat as dealt */
    /* bidding */
    uint8_t stage, sayer, hearer;
    int16_t bid;                                /* the highest value said and held (0: none yet) */
    uint8_t word[SKAT_P];                       /* SW_* */
    int16_t said[SKAT_P];                       /* the value each one said or held last */
    /* the game */
    uint8_t declarer, type, flags, hand_game;   /* SKAT_NONE / 0 until named */
    uint8_t cards12[SKAT_HAND];                 /* the declarer's 10 and the skat, when the game was named */
    /* play */
    uint8_t leader, trick[SKAT_P], ntrick, tricks;
    uint8_t last[SKAT_P], last_leader, last_winner;
    uint8_t won[SKAT_P], points[SKAT_P];        /* tricks and card points taken this deal */
    /* the list */
    skdeal list[SKAT_DEALS_MAX];
    uint8_t ndeals;
    int16_t score[SKAT_P];                      /* the game scores added */
    uint8_t wins[SKAT_P], losses[SKAT_P];
    uint32_t seed;
} skpos;

typedef struct {
    skpos pos;
    uint32_t seed;
    uint8_t variant;
    uint32_t act[SKAT_MAX_ACTS];
    int n;
} skgame;

/* the bid values in order (all the game values a suit, grand or null game can reach), how many */
int  skat_bids(int16_t *out);
int  skat_next_bid(int value);                  /* the lowest bid value above value (0: none) */
int  skat_deals(int variant);                   /* 12 or 36 */
void skat_new(skgame *g, int variant, uint32_t seed);
int  skat_legal(const skpos *p, uint32_t act);
int  skat_play(skgame *g, uint32_t act);        /* 0: not allowed */
int  skat_undo(skgame *g);                      /* back one action (a replay from the start); 0: none */
int  skat_points(int card);                     /* A 11, 10 10, K 4, Q 3, J 2 */
int  skat_base(int type, int flags, int hand);  /* the base value; null: the whole value */
int  skat_trump(int card, int type);            /* a trump in this game */
int  skat_suit(int card, int type);             /* the suit it follows (SUIT_TRUMP for a trump) */
int  skat_power(int card, int type);            /* its strength in that suit (higher wins) */
int  skat_trick_best(const uint8_t *cards, int n, int type);   /* the index of the card that wins */
int  skat_can_play(const skpos *p, int card);   /* the player to move may play it */
int  skat_matadors(const uint8_t *cards, int n, int type);     /* with (> 0) or without (< 0) */
int  skat_game_ok(const skpos *p, int type, int flags);   /* the declarer may name this game now */
void skat_sort(uint8_t *cards, int n, int type);   /* as shown, left to right (type: -1 bidding: jacks, then suits) */
int  skat_total(const skpos *p, int player);    /* the Seeger-Fabian total */
int  skat_saying(const skpos *p);              /* bidding: 1 the player to move says a value, 0 holds or passes */
int  skat_winners(const skpos *p);              /* SK_OVER: the players with the highest total (bit n) */
uint32_t skat_auto(const skpos *p);             /* a plain action (the "." of the notation) */
int  skat_script(skgame *g, const char *moves); /* 0: an action is bad */

/* the bot (skat_bot.c) and the game on the screen it reads (skat.c) */
struct ai_stats;
struct tb_state;
const skgame *skat_live(void);                  /* the game on the screen, read-only: the bot copies it at entry */
int skat_show(const char *move);                /* main thread: plays one action of the notation as a tap does */
uint32_t sktb_choose(const skgame *g, int seat, int level, uint64_t seed, struct ai_stats *stats);   /* 0: none */
void sktb_view(const skgame *g, int me, struct tb_state *v);   /* seat me's knowledge (trickbot.h), in play */

#endif
