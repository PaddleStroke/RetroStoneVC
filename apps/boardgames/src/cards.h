/*
 * RetroStone Board's card and tile kit: the decks (their cards as one byte each, the packs, a seeded shuffle), their
 * faces and backs drawn by code in the house style (no image files, no letters: digits, pips, the house's chess
 * figures for the courts), and the helpers every card game shares (a texture cache, a card drawn anywhere, turned,
 * raised, flipping; a hand fanned in a box and the card under the finger; piles; cards sliding from A to B).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The first part (the cards, the packs, the shuffle) has no SDL: the rules (<id>_rules.c) and their tests use it.
 * The drawing part is cards.c (SDL, the shell).
 */
#ifndef CARDS_H
#define CARDS_H

#include <stdint.h>

/* ---- the cards: suit (or colour) and rank in one byte ------------------------------------------------------------ */
enum { DECK_FRENCH, DECK_TAROT, DECK_NUMBER, DECK_TILES, DECK_ACTION, DECKS };
enum { SUIT_SPADES, SUIT_HEARTS, SUIT_DIAMONDS, SUIT_CLUBS, SUIT_TRUMP, SUIT_JOKER };
#define CARD(suit, rank) ((uint8_t)((suit) << 5 | (rank)))
#define CARD_SUIT(c) ((c) >> 5 & 7)
#define CARD_RANK(c) ((c) & 31)
#define CARD_NONE 255                           /* no card (an empty place) */

/* DECK_FRENCH: four suits, ace 1, 2..10, jack 11, queen 12, king 13; jokers CARD(SUIT_JOKER, 0 red / 1 black).
 * The piquet pack (Belote): 7..10, jack, queen, king, ace. */
enum { RANK_ACE = 1, RANK_JACK = 11, RANK_QUEEN = 12, RANK_KING = 13 };
/* DECK_TAROT (French tarot, 78): four suits of 1..10, jack 11, knight (cavalier) 12, queen 13, king 14; the 21
 * trumps CARD(SUIT_TRUMP, 1..21); the excuse CARD(SUIT_TRUMP, 0). */
enum { TAROT_JACK = 11, TAROT_KNIGHT, TAROT_QUEEN, TAROT_KING };
#define TAROT_EXCUSE CARD(SUIT_TRUMP, 0)
/* DECK_NUMBER (the number golf deck): one value -2..12 a card, coloured by band */
#define NUMBER_CARD(v) CARD(0, (v) + 2)
#define NUMBER_VALUE(c) (CARD_RANK(c) - 2)
/* DECK_TILES (tile rummy): colours 0 red, 1 lapis blue, 2 amber, 3 ebony black, numbers 1..13; jokers
 * CARD(SUIT_JOKER, 0 red / 1 black) */
/* DECK_ACTION (the family game of colours and action cards, "Last Card"): colours 0 red lacquer, 1 jade, 2 amber,
 * 3 lapis; 0..9, skip 10, reverse 11, draw two 12; wild CARD(SUIT_JOKER, 0), wild draw four CARD(SUIT_JOKER, 1) */
enum { ACTION_SKIP = 10, ACTION_REVERSE, ACTION_DRAW2 };
#define ACTION_WILD CARD(SUIT_JOKER, 0)
#define ACTION_WILD4 CARD(SUIT_JOKER, 1)

/* ---- the packs --------------------------------------------------------------------------------------------------- */
enum { PACK_52, PACK_54, PACK_32, PACK_TAROT, PACK_NUMBER, PACK_TILES, PACK_ACTION };
#define CARDS_MAX 160                           /* the biggest pack: number golf's 150 */

/* a whole pack in order, into out (CARDS_MAX); its size. PACK_54: the 52 and two jokers; PACK_32: piquet; PACK_NUMBER:
 * 150 cards, five -2, ten -1, fifteen 0, ten of each 1..12; PACK_TILES: 106, each colour's 1..13 twice and two
 * jokers; PACK_ACTION: 108, a 0 of each colour, two of each 1..9, skip, reverse and draw two of each colour, four
 * wilds and four wild draw fours. */
static inline int cards_pack(int pack, uint8_t *out)
{
    int n = 0;
    switch (pack) {
    case PACK_52:
    case PACK_54:
    case PACK_32:
        for (int s = 0; s < 4; s++)
            for (int r = 1; r <= 13; r++)
                if (pack != PACK_32 || r == 1 || r >= 7) out[n++] = CARD(s, r);
        if (pack == PACK_54) out[n++] = CARD(SUIT_JOKER, 0), out[n++] = CARD(SUIT_JOKER, 1);
        break;
    case PACK_TAROT:
        for (int s = 0; s < 4; s++)
            for (int r = 1; r <= 14; r++) out[n++] = CARD(s, r);
        for (int r = 0; r <= 21; r++) out[n++] = CARD(SUIT_TRUMP, r);
        break;
    case PACK_NUMBER:
        for (int v = -2; v <= 12; v++)
            for (int k = 0; k < (v == -2 ? 5 : v == 0 ? 15 : 10); k++) out[n++] = NUMBER_CARD(v);
        break;
    case PACK_TILES:
        for (int k = 0; k < 2; k++)
            for (int s = 0; s < 4; s++)
                for (int r = 1; r <= 13; r++) out[n++] = CARD(s, r);
        out[n++] = CARD(SUIT_JOKER, 0);
        out[n++] = CARD(SUIT_JOKER, 1);
        break;
    case PACK_ACTION:
        for (int s = 0; s < 4; s++)
            for (int r = 0; r <= ACTION_DRAW2; r++)
                for (int k = 0; k < (r ? 2 : 1); k++) out[n++] = CARD(s, r);
        for (int k = 0; k < 4; k++) out[n++] = ACTION_WILD, out[n++] = ACTION_WILD4;
        break;
    }
    return n;
}

/* the shuffle: a seeded generator (xorshift), so the rules stay pure and a game repeats from its seed (the game takes
 * the seed from ui_random(): seeded in the picture mode, so tests and screenshots repeat) */
static inline uint32_t cards_rand(uint32_t *s)
{
    uint32_t x = *s ? *s : 0x9e3779b9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return *s = x;
}

static inline void cards_shuffle(uint8_t *c, int n, uint32_t seed)
{
    uint32_t s = seed * 2654435761u + 0x7f4a7c15u;
    for (int i = n - 1; i > 0; i--) {
        int j = (int)(cards_rand(&s) % (uint32_t)(i + 1));
        uint8_t t = c[i];
        c[i] = c[j];
        c[j] = t;
    }
}

/* ---- the pictures (cards.c) -------------------------------------------------------------------------------------
 * A card is drawn w pixels wide, cards_aspect(deck) times as tall. The faces: no letters. French and tarot: the
 * index in the corners (digits; the court's figure; the ace's big pip) over the suit; pips in the classic layouts;
 * the courts as the house's chess figures in the suit's colour, double-headed: jack a bishop, (tarot) knight a knight,
 * queen a queen, king a king; jokers a jester's cap; tarot's trumps their number on a gold sun, the excuse a star.
 * Small cards (under about 52 px) get a plainer face: the index big, the suit under it. Number cards: lacquer by band
 * (-2..-1 lapis, 0 sky, 1..4 jade, 5..8 amber, 9..12 red). Tiles: ivory on wood, the number in its colour. The action
 * deck: lacquer cards, drawn icons for its actions. Backs: the green table in a walnut frame (tiles: wood; the action
 * deck: its own, ebony). */
float cards_aspect(int deck);                   /* height / width */
void cards_paint(int deck, int card, int w, int h, uint8_t *rgba);   /* a face (CARD_NONE: the back), w x h */

/* how a card is drawn (cards_draw): face down; raised (chosen: lifted, a gold edge); dimmed (cannot be played);
 * a gold light round it (a card that can be played, a place to tap); no shadow */
enum { CD_DOWN = 1, CD_RAISED = 2, CD_DIM = 4, CD_GLOW = 8, CD_FLAT = 16 };
/* a card centred on (cx, cy), w wide, turned deg degrees (clockwise) */
void cards_draw(int deck, int card, float cx, float cy, float w, float deg, int how);
/* a card turning over in place: t 0 its back, 1 its face (it narrows to its edge and widens again: scale-x) */
void cards_draw_flip(int deck, int card, float cx, float cy, float w, float deg, float t, int how);
/* a pile of n cards (its thickness shows a little), the top one face up (card) or its back (CARD_NONE), drawn `how`
 * (CD_GLOW: tap it); n 0: the empty place, a faint outline */
void cards_pile(int deck, float cx, float cy, float w, int n, int top, int how);
/* a place for a card: the faint outline (an empty pile, a slot) */
void cards_place(int deck, float cx, float cy, float w);

/* a hand: n cards w wide in a row centred on (cx, cy), no wider than `width` (they overlap when they must), fanned
 * by `arc` degrees in all (0: straight). Their centres and angles into spots; cards_hit: the top card under (x, y)
 * (-1: none). */
typedef struct { float x, y, deg; } cards_spot;
void cards_fan(float cx, float cy, float width, float w, float aspect, int n, float arc, cards_spot *spots);
int  cards_hit(const cards_spot *spots, int n, float w, float aspect, float x, float y);
int  cards_in(float cx, float cy, float w, float aspect, float x, float y);    /* (x, y) on a card lying straight */

/* a card sliding from A to B (eased, with a little lift), turning over on the way if up0 != up1 (a flip in place:
 * the same A and B), after `delay` ms. cards_slide_draw draws it where it is (and asks for the next frame): 1 while
 * it moves or waits (drawn at A), 0 once it has landed (nothing drawn: the game draws it in its place). */
typedef struct {
    int on, deck, card, up0, up1, ms, delay;
    float x0, y0, w0, a0, x1, y1, w1, a1;
    uint32_t t0;
} cards_slide;
void cards_slide_start(cards_slide *s, int deck, int card, float x0, float y0, float w0, float a0, int up0, float x1,
                       float y1, float w1, float a1, int up1, int ms, int delay);
int  cards_slide_draw(cards_slide *s);
int  cards_slide_busy(const cards_slide *s);

/* a card game's token (the bars, the cover, the end card): a player's chip, a flat disc in their colour */
void cards_token(int player, int size, uint8_t *rgba);

#endif
