/*
 * The Spanish-suited pack's notation (Mus, Truco): no SDL, the rules and their tests use it. The pack itself is the
 * kit's (cards.h: DECK_SPANISH, its suits ES_OROS..ES_BASTOS and courts ES_SOTA..ES_REY, PACK_ES40 and PACK_ES48;
 * the pictures in cards_spanish.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * A card is cards.h's byte: CARD(suit, number). Notation: the number (1..9, S sota, C caballo, R rey; A for the as
 * too) and the suit (o oros, c copas, e espadas, b bastos): "1e" the as of espadas, "Ro" the rey of oros.
 */
#ifndef SPANISH_H
#define SPANISH_H

#include <stdint.h>

#include "cards.h"

#define ES_CARDS 40

/* a card read at *s (moved past it); CARD_NONE: not a card */
static inline int es_parse(const char **s)
{
    static const char suits[] = "oceb";
    const char *p = *s;
    int r = -1, su = -1;
    if (p[0] >= '1' && p[0] <= '9') r = p[0] - '0';
    else if (p[0] == 'A' || p[0] == 'a') r = ES_AS;
    else if (p[0] == 'S') r = ES_SOTA;
    else if (p[0] == 'C') r = ES_CABALLO;
    else if (p[0] == 'R') r = ES_REY;
    if (r < 0 || !p[1]) return CARD_NONE;
    for (int i = 0; suits[i]; i++)
        if (p[1] == suits[i]) su = i;
    if (su < 0) return CARD_NONE;
    *s += 2;
    return CARD(su, r);
}

/* the card's two characters (and a 0) into out[3] */
static inline void es_name(int card, char *out)
{
    static const char ranks[] = "?123456789SCR", suits[] = "oceb";
    int r = CARD_RANK(card);
    out[0] = ranks[r >= 1 && r <= ES_REY ? r : 0];
    out[1] = suits[CARD_SUIT(card) & 3];
    out[2] = 0;
}

/* a seed mixed with a number (each deal's shuffle) */
static inline uint32_t es_mix(uint32_t a, uint32_t b) { return (a ^ (b * 0x9e3779b9u)) * 0x85ebca6bu + b; }

#endif
