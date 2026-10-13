/*
 * Tiến lên ("go forward"), the Vietnamese climbing game (the southern rules), for 2 to 4 players on one device passed
 * round: the rules. No SDL; the deal comes from a seed, so a game is its seed and its actions (the play itself:
 * games/shed.h, shared with Big Two).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Thirteen cards each (with two or three players the rest stay out of play). Ranks from 3 (low) .. 10, jack, queen,
 * king, ace, up to the 2 (high); suits from low to high: spades, clubs, diamonds, hearts. Cards compare by rank, then
 * by suit. The lowest card dealt (the 3 of spades with four players) leads the first trick, in a play holding it.
 * Plays: a single card, a pair, three or four of a kind, a sequence (three or more ranks in a row, any suits: the 2
 * is never in one), a double sequence (three or more pairs in a row, no 2s). Each in turn plays the same kind, as many
 * cards, higher (by the highest card), or passes; a player who passes is out of the trick until the next lead. When
 * everyone else has passed, the last player who played leads any play.
 * The bombs ("chopping" the 2s): three pairs in a row beat a single 2; four of a kind beats a single 2 or three pairs
 * in a row; four pairs in a row beat a single 2, a pair of 2s, four of a kind or three pairs in a row; five pairs in a
 * row beat three 2s too. A bomb also beats a lower bomb of its own kind.
 * The first player out of cards wins; the others play on for the places, until one is left with cards: the last.
 */
#ifndef TIENLEN_H
#define TIENLEN_H

#include "games/shed.h"

enum { TLN_SINGLE = 1, TLN_PAIR, TLN_TRIPLE, TLN_QUAD, TLN_SEQ, TLN_DSEQ };

extern const shed_rules tln_rules;

int  tln_power(int card);                       /* 3 of spades 0 .. 2 of hearts 51 */
int  tln_classify(const uint8_t *cards, int n, shed_combo *c);
int  tln_beats(const shed_combo *a, const shed_combo *top);
int  tln_bomb(const shed_combo *c);             /* a bomb's class (three pairs 1, four of a kind 2, four pairs 3,
                                                 * five pairs or more 4); 0: not a bomb */

/* the bot (tienlen_bot.c, bots.h) */
const shed_game *tienlen_live(void);            /* the game on the screen, read-only: the bot copies it at entry */
int tienlen_show(const char *move);             /* main thread: plays one action of the notation as a tap does */
struct ai_stats;
uint64_t tienlen_bot_choose(const shed_game *g, int seat, int level, uint64_t seed, struct ai_stats *stats);

#endif
