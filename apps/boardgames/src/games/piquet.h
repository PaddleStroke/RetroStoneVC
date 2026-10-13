/*
 * Piquet, the classic French game for two (Rubicon piquet), on one device with hidden hands: the rules. No SDL; the
 * deals come from a seed (cards.h's shuffle), so a game is its seed and its actions (undo replays them).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The piquet pack of 32 (ace high, then king, queen, jack, 10, 9, 8, 7). A partie is six deals, the deal changing
 * hands each time (player 2 deals first); the other player is the elder hand. Twelve cards each, eight in the talon.
 * The exchange: the elder throws 1 to 5 cards and takes as many from the top of the talon; then the younger may throw
 * up to what is left (0 to 3, more if the elder left some) and take as many. The cards left stay face down, unseen.
 * The declarations, in order (each counted for the better hand only, nothing when equal):
 * - carte blanche: twelve cards dealt without king, queen or jack, 10 (counted with the declarations);
 * - the point: the most cards in one suit (equal: the higher value, ace 11, courts 10, others their pips): 1 a card;
 * - sequences: three or more running in a suit; the longest wins (equal: the highest card), and its holder scores
 *   every sequence they hold: three 3, four 4, five 15, six 16, seven 17, eight 18;
 * - sets: three or four of a kind, tens and higher; four beats three, then the higher rank, and the winner scores
 *   every set they hold: four 14, three 3.
 * The elder declares, the younger answers: good (the elder's is better), not good (the younger's is), equal. Every
 * declaration is the best the hand holds (none is kept back) and both are made before the first lead; the better
 * combinations are shown to both players.
 * The play: the elder leads; follow suit if you can, else anything; no trumps; the higher card of the suit led wins and
 * leads next. Leading a 10 or higher scores 1; winning the other's lead with a 10 or higher, 1; the last trick 1 more.
 * Most tricks (seven or more): 10, all twelve (capot): 40 instead.
 * Repique: 30 from the declarations alone (counted in order: carte blanche, point, sequences, sets) before the other
 * scores anything: 60 more. Pique (the elder only, the classic rule): 30 from declarations and the tricks' points
 * (not the cards' 10 nor capot's 40, which come after the play) before the younger scores anything: 30 more.
 * The Rubicon: after six deals the higher total wins. The loser under 100 is rubiconed: the winner scores both totals
 * and 100; otherwise the difference and 100. Equal totals: a draw.
 * Notation (script): e<cards> throws them ("e7h8h", "e" alone: none), k declares (or answers), <card> plays one
 * ("As", tricks.h's notation), n the next deal, "." one plain action, "#" plain actions to the deal's end, "!" to the
 * game's end.
 */
#ifndef PIQUET_H
#define PIQUET_H

#include <stdint.h>

#include "cards.h"

#define PIQUET_P 2
#define PIQUET_HAND 12
#define PIQUET_TALON 8
#define PIQUET_DEALS 6
#define PIQUET_MAX_ACTS 1024

enum { PQ_EXCHANGE, PQ_DECLARE, PQ_PLAY, PQ_ROUND, PQ_OVER };
enum { PQ_POINT, PQ_SEQUENCE, PQ_SETS, PQ_KINDS };   /* the declarations compared */
enum { PQ_EQUAL, PQ_GOOD, PQ_NOT_GOOD, PQ_NONE };    /* the younger's answer to each: good (the elder's wins) */
/* an action: its kind (the top four bits) and a mask of the hand (bit i: the i-th card in sorted order) or a card */
enum { PQA_EXCHANGE = 1 << 12, PQA_DECLARE = 2 << 12, PQA_PLAY = 3 << 12, PQA_NEXT = 4 << 12 };
#define PQA_KIND(a) ((a) & 0xf000)
#define PQA_ARG(a) ((a) & 0x0fff)

typedef struct {
    uint8_t pn, pv, psuit;                      /* the point: cards, value, suit */
    uint8_t sn, stop, ssuit, seqs;              /* the best sequence: length (0: none), top rank, suit; all scored */
    uint8_t setn, setrank, sets;                /* the best set: 4 or 3 (0: none), its rank; all scored */
} piquet_decl;

typedef struct {
    uint8_t phase, turn, deal, dealer, elder, step;
    uint8_t hand[PIQUET_P][PIQUET_HAND], n[PIQUET_P];
    uint8_t talon[PIQUET_TALON], top;           /* the talon; the next card taken */
    uint8_t ex[PIQUET_P];                       /* the cards each exchanged */
    uint8_t blanche[PIQUET_P];
    piquet_decl decl[PIQUET_P];
    uint8_t verdict[PQ_KINDS];                  /* PQ_EQUAL / GOOD / NOT_GOOD / NONE (neither holds one) */
    uint8_t trick[PIQUET_P], ntrick, leader;
    uint8_t last[PIQUET_P], last_leader, last_winner, tricks, won[PIQUET_P];
    uint8_t pique, repique, capot;              /* who (255: nobody) */
    int16_t dpts[PIQUET_P], ppts[PIQUET_P], bonus[PIQUET_P];   /* the deal's: declarations, play, pique/repique */
    int16_t score[PIQUET_P], total[PIQUET_P];   /* the last deal's; the totals */
    int16_t final;                              /* PQ_OVER: the winner's score for the partie (Rubicon) */
    uint8_t rubicon;                            /* the loser stayed under 100 */
    uint32_t seed;
} piquet_pos;

typedef struct {
    piquet_pos pos;
    uint32_t seed;
    uint16_t act[PIQUET_MAX_ACTS];
    int n;
} piquet_game;

void piquet_new(piquet_game *g, uint32_t seed);
int  piquet_value(int card);                    /* the point's count: ace 11, courts 10, else the pips */
int  piquet_index(int card);                    /* 7 0 .. ace 7 */
void piquet_declare(const uint8_t *hand, int n, piquet_decl *d);
int  piquet_compare(const piquet_decl *e, const piquet_decl *y, int kind);   /* PQ_EQUAL / GOOD / NOT_GOOD / NONE */
int  piquet_kind_points(const piquet_decl *d, int kind);   /* what that declaration scores */
int  piquet_shown(const piquet_pos *p, int player, int kind, uint8_t *out);   /* the cards of a declaration */
int  piquet_blanche(const uint8_t *hand, int n);
int  piquet_kind_winner(const piquet_pos *p, int kind);   /* the player whose declaration counts (-1: none) */
int  piquet_legal(const piquet_pos *p, int act);
int  piquet_play(piquet_game *g, int act);      /* 0: not allowed */
int  piquet_undo(piquet_game *g);
int  piquet_can_play(const piquet_pos *p, int card);
int  piquet_throw_min(const piquet_pos *p);     /* PQ_EXCHANGE: how few and how many the player may throw */
int  piquet_throw_max(const piquet_pos *p);
int  piquet_winner(const piquet_pos *p);        /* PQ_OVER: the winner (-1: a draw or not over) */
int  piquet_auto(const piquet_pos *p);
int  piquet_script(piquet_game *g, const char *moves);

/* the bot (piquet_bot.c, bots.h), and what the screen (piquet.c) gives it; a headless test defines those two */
const piquet_game *piquet_live(void);           /* the game on the screen, read-only: the bot copies it at entry */
int piquet_show(const char *move);              /* main thread: plays one action of the notation as a tap does */
struct ai_stats;
struct tb_state;
int pqb_choose(const piquet_game *g, int seat, int level, uint64_t seed, struct ai_stats *stats);   /* 0: none */
void pqb_view(const piquet_game *g, int me, struct tb_state *v, piquet_game *scratch);   /* seat me's knowledge */

#endif
