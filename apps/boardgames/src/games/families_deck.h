/*
 * The family deck of Happy Families (jeu des 7 familles), drawn by code in the house style: 42 cards, 7 families of
 * animals (bears, rabbits, cats, mice, owls, lions, frogs), each with its own colour, 6 members each (grandfather,
 * grandmother, father, mother, son, daughter). No text: the family is the colour and the animal, the member is told
 * by what the animal wears (glasses and a white moustache, glasses and a grey bun, a bow tie, a pearl necklace, a
 * cap, a hair bow; the children smaller) and by a figure in the corner and a row of the six figures at the foot of
 * the card (this one's in the family's colour). The backs are the house's.
 * A card is a byte: CARD(family, member), cards.h's layout (the family in the suit's bits). The numbers serve the
 * rules (no SDL); the faces are painted with the art kit (art.h) in the card kit's src/cards_families.c, built once for
 * every game that uses them (Happy Families; Pairs, its emblems), and drawn by the card kit as a deck of its own:
 * fam_deck() is its number for cards_draw, cards_pile, slides and ct_suit (its suits' signs: the emblems).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#ifndef FAMILIES_DECK_H
#define FAMILIES_DECK_H

#include <stdint.h>

#include "cards.h"

#define FAM_FAMILIES 7
#define FAM_MEMBERS 6
#define FAM_CARD(f, m) CARD(f, m)
#define FAM_FAMILY(c) CARD_SUIT(c)
#define FAM_MEMBER(c) CARD_RANK(c)
#define FAM_PI 3.14159265f

enum { FAM_BEARS, FAM_RABBITS, FAM_CATS, FAM_MICE, FAM_OWLS, FAM_LIONS, FAM_FROGS };
enum { FAM_GRANDFATHER, FAM_GRANDMOTHER, FAM_FATHER, FAM_MOTHER, FAM_SON, FAM_DAUGHTER };

/* the pictures (cards_families.c, SDL side: not for the rules) */
void fam_paint(int card, int w, int h, uint8_t *rgba);   /* a face, w x h */
void fam_emblem(int family, int size, uint8_t *rgba);    /* a family's emblem: its animal's head on a disc */
int  fam_deck(void);                                     /* the deck's number for the card kit */

#endif
