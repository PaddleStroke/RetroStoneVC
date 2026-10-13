/*
 * Big Two, the Cantonese climbing game, for 4 players (or 3) on one device passed round: the rules. No SDL; the deal
 * comes from a seed, so a game is its seed and its actions (the play itself: games/shed.h, shared with Tiến lên).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * All 52 cards dealt, 13 each (three players: 17 each, the last card to the holder of the 3 of diamonds). Ranks from
 * 3 (low) .. 10, jack, queen, king, ace, up to the 2 (high); suits from low to high: diamonds, clubs, hearts, spades.
 * Cards compare by rank, then by suit. The holder of the 3 of diamonds leads the first trick, with a play holding it.
 * Plays: a single card, a pair, three of a kind, or a five-card hand. Five-card hands from low to high: a straight
 * (five ranks in a row, any suits: 3-4-5-6-7 up to 10-J-Q-K-A; the 2 is never in one), a flush (five of a suit), a
 * full house (three of a kind and a pair), four of a kind with any fifth card, a straight flush. Within a kind:
 * straights and straight flushes by their highest card; flushes by their highest card, then the next ones, then the
 * suit; a full house and four of a kind by the rank of their three or four. Singles by the card, pairs by their higher
 * card, threes by their rank.
 * Each in turn plays the same kind (as many cards) and higher, or passes; a pass does not stop a player from playing
 * later in the trick. Once the others have all passed in a row, the last player who played leads any play.
 * The first player out of cards wins the deal. Each other player scores a penalty point per card left, doubled with
 * ten or more (13 left: 26).
 */
#ifndef BIGTWO_H
#define BIGTWO_H

#include "games/shed.h"

enum { BIGT_SINGLE = 1, BIGT_PAIR, BIGT_TRIPLE, BIGT_FIVE };
enum { BIGT_STRAIGHT = 1, BIGT_FLUSH, BIGT_HOUSE, BIGT_FOUR, BIGT_SFLUSH };   /* a five-card hand's class (key >> 24) */

extern const shed_rules bigt_rules;

int  bigt_power(int card);                      /* 3 of diamonds 0 .. 2 of spades 51 */
int  bigt_classify(const uint8_t *cards, int n, shed_combo *c);
int  bigt_beats(const shed_combo *a, const shed_combo *top);
int  bigt_penalty(int left);                    /* the points for cards left: one each, doubled at ten or more */

/* the bot (bigtwo_bot.c, bots.h) */
const shed_game *bigtwo_live(void);             /* the game on the screen, read-only: the bot copies it at entry */
int bigtwo_show(const char *move);              /* main thread: plays one action of the notation as a tap does */
struct ai_stats;
uint64_t bigtwo_bot_choose(const shed_game *g, int seat, int level, uint64_t seed, struct ai_stats *stats);

#endif
