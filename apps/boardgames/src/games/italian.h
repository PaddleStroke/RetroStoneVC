/*
 * The Italian-suited pack of 40 (Scopa, Briscola): the cards as bytes and their notation. No SDL: the rules and their
 * tests use it; the pictures are games/italian_deck.h.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Four suits: coins (denari), cups (coppe), swords (spade), clubs (bastoni); in each, 1 (the ace) to 7, then the three
 * courts: fante 8, cavallo 9, re 10. A card is cards.h's byte: CARD(suit, rank).
 * Notation: a rank (1..7, F fante, C cavallo, R re; A for the ace too) and a suit (d denari, c coppe, s spade,
 * b bastoni): "7d" the settebello, "Rb" the re of clubs.
 */
#ifndef ITALIAN_H
#define ITALIAN_H

#include <stdint.h>

#include "cards.h"

#define IT_CARDS 40
enum { IT_COINS, IT_CUPS, IT_SWORDS, IT_CLUBS };
enum { IT_ACE = 1, IT_FANTE = 8, IT_CAVALLO = 9, IT_RE = 10 };

/* the 40 cards in order (suit by suit, 1..re) into out; their number */
static inline int it_pack(uint8_t *out)
{
    int n = 0;
    for (int s = 0; s < 4; s++)
        for (int r = 1; r <= IT_RE; r++) out[n++] = CARD(s, r);
    return n;
}

/* a card read at *s (moved past it); CARD_NONE: not a card */
static inline int it_parse(const char **s)
{
    static const char ranks[] = "1234567FCR", suits[] = "dcsb";
    const char *p = *s;
    int r = -1, su = -1;
    if (p[0] == 'A' || p[0] == 'a') r = 1;
    for (int i = 0; ranks[i] && r < 0; i++)
        if (p[0] == ranks[i] || p[0] == ranks[i] + ('a' - 'A') * (ranks[i] >= 'A')) r = i + 1;
    if (r < 0 || !p[1]) return CARD_NONE;
    for (int i = 0; suits[i]; i++)
        if (p[1] == suits[i]) su = i;
    if (su < 0) return CARD_NONE;
    *s += 2;
    return CARD(su, r);
}

/* the card's two characters (and a 0) into out[3] */
static inline void it_name(int card, char *out)
{
    static const char ranks[] = "?1234567FCR", suits[] = "dcsb";
    int r = CARD_RANK(card);
    out[0] = ranks[r >= 1 && r <= IT_RE ? r : 0];
    out[1] = suits[CARD_SUIT(card) & 3];
    out[2] = 0;
}

/* a seed mixed with a number (each round's shuffle) */
static inline uint32_t it_mix(uint32_t a, uint32_t b) { return (a ^ (b * 0x9e3779b9u)) * 0x85ebca6bu + b; }

#endif
