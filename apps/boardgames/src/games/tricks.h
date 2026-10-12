/*
 * Trick-taking games, the rules they share (Hearts, Spades): no SDL. A hand kept sorted, following suit, the winner
 * of a trick (the led suit, or a trump), the cards in the scripts' notation. Static inline: each game takes what it
 * uses. The drawing side (the table, the trick, the hand, the round's summary) is the kit's cards_table.h.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Cards are cards.h's bytes (DECK_FRENCH: ace 1 .. king 13). In these games the ace ranks high: trick_rank() gives
 * 2..14. A hand is sorted by suit (clubs, diamonds, spades, hearts: the colours alternate) and rank, low to high.
 * Notation: a rank (2..9, T, J, Q, K, A) and a suit (c, d, s, h): "2c", "Qs", "Th".
 */
#ifndef TRICKS_H
#define TRICKS_H

#include <stdint.h>

#include "cards.h"

#define TRICK_NONE 255

/* the strength of a card in its suit: 2..10, jack 11, queen 12, king 13, ace 14 */
static inline int trick_rank(int card)
{
    int r = CARD_RANK(card);
    return r == RANK_ACE ? 14 : r;
}

/* where a suit goes in a sorted hand */
static inline int trick_suit_order(int suit)
{
    static const int8_t order[4] = {2, 3, 1, 0};   /* spades, hearts, diamonds, clubs */
    return suit < 4 ? order[suit] : 4;
}

static inline int trick_sort_key(int card) { return trick_suit_order(CARD_SUIT(card)) * 16 + trick_rank(card); }

static inline void trick_sort(uint8_t *hand, int n)
{
    for (int i = 1; i < n; i++) {
        uint8_t c = hand[i];
        int j = i;
        while (j > 0 && trick_sort_key(hand[j - 1]) > trick_sort_key(c)) { hand[j] = hand[j - 1]; j--; }
        hand[j] = c;
    }
}

static inline int trick_find(const uint8_t *hand, int n, int card)
{
    for (int i = 0; i < n; i++)
        if (hand[i] == card) return i;
    return -1;
}

/* takes a card out of a hand (the order kept); 0: not in it */
static inline int trick_remove(uint8_t *hand, uint8_t *n, int card)
{
    int i = trick_find(hand, *n, card);
    if (i < 0) return 0;
    for (; i < *n - 1; i++) hand[i] = hand[i + 1];
    (*n)--;
    return 1;
}

static inline int trick_has_suit(const uint8_t *hand, int n, int suit)
{
    for (int i = 0; i < n; i++)
        if (CARD_SUIT(hand[i]) == suit) return 1;
    return 0;
}

static inline int trick_only_suit(const uint8_t *hand, int n, int suit)
{
    for (int i = 0; i < n; i++)
        if (CARD_SUIT(hand[i]) != suit) return 0;
    return n > 0;
}

/* following suit: with a card of the led suit in hand, one must be played (led: -1, leading: anything) */
static inline int trick_follows(const uint8_t *hand, int n, int led, int card)
{
    return led < 0 || CARD_SUIT(card) == led || !trick_has_suit(hand, n, led);
}

/* the card that wins a trick (its index in the order played): the highest trump if any (trump -1: none), else the
 * highest card of the suit led (the first card's) */
static inline int trick_winner(const uint8_t *cards, int n, int trump)
{
    int best = 0;
    for (int i = 1; i < n; i++) {
        int s = CARD_SUIT(cards[i]), bs = CARD_SUIT(cards[best]);
        if (s == bs ? trick_rank(cards[i]) > trick_rank(cards[best]) : s == trump) best = i;
    }
    return best;
}

/* ---- the notation ------------------------------------------------------------------------------------------------ */
/* a card read at *s (moved past it); CARD_NONE: not a card */
static inline int trick_parse(const char **s)
{
    static const char ranks[] = "A23456789TJQK", suits[] = "shdc";
    int r = -1, su = -1;
    for (int i = 0; ranks[i]; i++)
        if ((*s)[0] == ranks[i] || (ranks[i] == 'T' && (*s)[0] == 't')) r = i + 1;
    if (r < 0 || !(*s)[1]) return CARD_NONE;
    for (int i = 0; suits[i]; i++)
        if ((*s)[1] == suits[i]) su = i;
    if (su < 0) return CARD_NONE;
    *s += 2;
    return CARD(su, r);
}

/* the card's two characters (and a 0) into out[3] */
static inline void trick_name(int card, char *out)
{
    static const char ranks[] = "?A23456789TJQK", suits[] = "shdc";
    out[0] = ranks[CARD_RANK(card) <= 13 ? CARD_RANK(card) : 0];
    out[1] = suits[CARD_SUIT(card) & 3];
    out[2] = 0;
}

/* the deal's shuffle seed for a round (the game's seed mixed with the round) */
static inline uint32_t trick_mix(uint32_t a, uint32_t b) { return (a ^ (b * 0x9e3779b9u)) * 0x85ebca6bu + b; }

#endif
