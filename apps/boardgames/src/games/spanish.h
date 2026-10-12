/*
 * The Spanish-suited pack (baraja española: Mus, Truco): its cards, its pack of 40, its notation. No SDL: the rules
 * and their tests use it. The pictures are the kit's (cards_spanish.c): es_deck() gives the deck's number for the card
 * kit (cards_draw, cards_pile, slides: cards_deck_add, once).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Four suits: oros (coins), copas (cups), espadas (swords), bastos (clubs); in each 1 (the as) to 7, then the three
 * courts with the numbers they carry: sota 10 (the page), caballo 11 (the knight), rey 12 (the king). The 8s and 9s
 * belong to the pack of 48 only (es_pack(1)). A card is cards.h's byte: CARD(suit, number).
 * Notation: the number (1..9, S sota, C caballo, R rey; A for the as too) and the suit (o oros, c copas, e espadas,
 * b bastos): "1e" the as of espadas, "Ro" the rey of oros.
 */
#ifndef SPANISH_H
#define SPANISH_H

#include <stdint.h>

#include "cards.h"

enum { ES_OROS, ES_COPAS, ES_ESPADAS, ES_BASTOS };
enum { ES_AS = 1, ES_SOTA = 10, ES_CABALLO = 11, ES_REY = 12 };
#define ES_CARDS 40

/* the pack in order, suit by suit (1..7, sota, caballo, rey; with89: the 8s and 9s too, 48 cards); its size */
static inline int es_pack(int with89, uint8_t *out)
{
    int n = 0;
    for (int s = 0; s < 4; s++)
        for (int r = 1; r <= ES_REY; r++)
            if (with89 || r <= 7 || r >= ES_SOTA) out[n++] = CARD(s, r);
    return n;
}

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

/* ---- the pictures (cards_spanish.c, SDL side: not for the rules) ------------------------------------------------- */
int  es_deck(void);                             /* the deck's number for the card kit */
void es_face(int card, int w, int h, uint8_t *rgba);   /* a face, w x h */
void es_symbol(int suit, int size, uint8_t *rgba);     /* a suit's symbol alone, size x size */

#endif
