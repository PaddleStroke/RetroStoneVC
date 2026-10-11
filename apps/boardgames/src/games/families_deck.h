/*
 * The family deck of Happy Families (jeu des 7 familles), drawn by code in the house style: 42 cards, 7 families of
 * animals (bears, rabbits, cats, mice, owls, lions, frogs), each with its own colour, 6 members each (grandfather,
 * grandmother, father, mother, son, daughter). No text: the family is the colour and the animal, the member is told
 * by what the animal wears (glasses and a white moustache, glasses and a grey bun, a bow tie, a pearl necklace, a
 * cap, a hair bow; the children smaller) and by a figure in the corner and a row of the six figures at the foot of
 * the card (this one's in the family's colour). The backs are the house's (the kit's French back).
 * A card is a byte: CARD(family, member), cards.h's layout (the family in the suit's bits). The pictures need SDL
 * (the shell); the numbers alone do not (the rules).
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

enum { FAM_BEARS, FAM_RABBITS, FAM_CATS, FAM_MICE, FAM_OWLS, FAM_LIONS, FAM_FROGS };
enum { FAM_GRANDFATHER, FAM_GRANDMOTHER, FAM_FATHER, FAM_MOTHER, FAM_SON, FAM_DAUGHTER };

/* the pictures (families_deck.c) */
void fam_paint(int card, int w, int h, uint8_t *rgba);     /* a face, w x h (1.4 times as tall as wide) */
void fam_emblem(int family, int size, uint8_t *rgba);      /* the family's badge: its animal in a disc of its colour */
/* a card drawn like the kit's (cards_draw's `how`: CD_DOWN its back, CD_RAISED, CD_GLOW, CD_DIM, CD_FLAT) */
void fam_draw(int card, float cx, float cy, float w, float deg, int how);
void fam_draw_flip(int card, float cx, float cy, float w, float deg, float t, int how);   /* t 0 back .. 1 face */
int  fam_slide_draw(cards_slide *s);           /* a kit slide (its deck ignored) with these faces: ct_draw's paint */

#endif
