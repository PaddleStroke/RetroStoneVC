/*
 * The hanafuda deck of Koi-Koi, drawn by code in the house style (art.h's figures): 48 cards, 12 months of a plant
 * each, every card keeping its traditional subject in our own drawing (January's pine, crane and sun; February's plum
 * and bush warbler; March's cherry and curtain; April's wisteria and cuckoo; May's iris and bridge; June's peony and
 * butterflies; July's bush clover and boar; August's pampas, moon and geese; September's chrysanthemum and sake cup;
 * October's maple and deer; November's willow, rain man, swallow and lightning; December's paulownia and phoenix).
 * Each month draws its plant once for its four cards and adds each card's own subject; the ribbons are red (the
 * poetry ribbons carry strokes like brush writing, never real text) or blue. The cards are tall (1.6 times as high as
 * wide), paper in a dark rim; the back is black lacquer in a thin gold line. Shadows, the gold light of a card to tap
 * and the lift of a chosen card are the card kit's (cards.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#ifndef KOIKOI_DECK_H
#define KOIKOI_DECK_H

#include <stdint.h>

#include "cards.h"
#include "games/koikoi.h"

#define HANA_ASPECT 1.6f                        /* height / width */

/* a face (0..47) or the back (KK_NONE), w x h */
void hana_paint(int card, int w, int h, uint8_t *rgba);
/* the koi-koi decision's two icons, size x size: stop (a gold disc, an open hand raised), koi-koi (a red disc, a
 * double arrow onwards) */
enum { HI_STOP, HI_KOI };
void hana_icon(int which, int size, uint8_t *rgba);
/* n cards side by side, overlapping, each cw wide, in a w x h picture (a yaku's badge) */
void hana_fan(const uint8_t *cards, int n, int cw, int w, int h, uint8_t *rgba);
/* a card drawn like the kit's (cards_draw's `how`: CD_DOWN its back, CD_RAISED, CD_GLOW, CD_DIM, CD_FLAT) */
void hana_draw(int card, float cx, float cy, float w, float deg, int how);
void hana_draw_flip(int card, float cx, float cy, float w, float deg, float t, int how);   /* t 0 back .. 1 face */
int  hana_slide_draw(cards_slide *s);           /* a kit slide (its deck ignored) with these cards */

#endif
