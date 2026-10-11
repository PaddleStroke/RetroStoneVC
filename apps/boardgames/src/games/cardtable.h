/*
 * The table of the card games with hidden hands (crazy eights, old maid, happy families) and war: helpers they share
 * on top of the card kit (cards.h) and the shell. Static inline, so each game keeps its own state.
 * - Moving cards: a list of slides, each tagged by where it lands (the game leaves that card out until it has
 *   landed); busy while one moves and a moment after (the hand-off and the end card wait for it).
 * - The others' seats along the top of the table, in turn order from the left (as the shell's bars).
 * - The viewer's hand: one row of cards, or two when one would hide too much, fitting a box.
 * - A small fan of backs (another player's hand) and a count under it.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#ifndef CARDTABLE_H
#define CARDTABLE_H

#include <math.h>

#include "cards.h"
#include "shell.h"

/* ---- moving cards ------------------------------------------------------------------------------------------------ */
#define CT_MOVES 200
#define CT_REST 300                             /* after the last card lands: a moment before the device passes */

typedef struct { cards_slide s; int tag, a, b, after; } ct_move;   /* after: a move to wait for (-1) */
typedef struct { ct_move m[CT_MOVES]; int n; Uint32 quiet; } ct_moves;

static inline void ct_calm(ct_moves *mv)
{
    mv->n = 0;
    mv->quiet = 0;
}

/* a card from (x0, y0) to (x1, y1), w0 to w1 wide, turned a0 to a1 degrees, face up or down at each end, after
 * `delay` ms; it lands at (tag, a, b); sound: played as it lands (-1: none). Its index (-1: no room). */
static inline int ct_slide(ct_moves *mv, int deck, int card, float x0, float y0, float w0, float a0, int up0,
                           float x1, float y1, float w1, float a1, int up1, int ms, int delay, int tag, int a, int b,
                           int sound)
{
    if (mv->n >= CT_MOVES) return -1;
    ct_move *m = &mv->m[mv->n++];
    cards_slide_start(&m->s, deck, card, x0, y0, w0, a0, up0, x1, y1, w1, a1, up1, ms, delay);
    m->tag = tag;
    m->a = a;
    m->b = b;
    m->after = -1;
    Uint32 end = ui_ms() + (Uint32)(delay + ms + CT_REST);
    if (end > mv->quiet) mv->quiet = end;
    if (sound >= 0) ui_sound_later(sound, delay + ms > 40 ? delay + ms - 40 : 0);
    return mv->n - 1;
}

/* move i is not drawn while move `first` still moves (a card going on from where another lands: no double while it
 * waits) */
static inline void ct_after(ct_moves *mv, int i, int first)
{
    if (i >= 0 && first >= 0) mv->m[i].after = first;
}

static inline int ct_moving(const ct_moves *mv)
{
    for (int i = 0; i < mv->n; i++)
        if (cards_slide_busy(&mv->m[i].s)) return 1;
    return 0;
}

static inline int ct_busy(const ct_moves *mv) { return ct_moving(mv) || (mv->quiet && ui_ms() < mv->quiet); }

/* how many cards still on their way to (tag, a, b) (a or b below zero: any) */
static inline int ct_hides(const ct_moves *mv, int tag, int a, int b)
{
    int n = 0;
    for (int i = 0; i < mv->n; i++) {
        const ct_move *m = &mv->m[i];
        if (m->tag == tag && (a < 0 || m->a == a) && (b < 0 || m->b == b) && cards_slide_busy(&m->s)) n++;
    }
    return n;
}

/* the moving cards where they are now; paint: a game's own deck (NULL: the kit's) */
static inline void ct_draw(ct_moves *mv, int (*paint)(cards_slide *s))
{
    for (int i = 0; i < mv->n; i++) {
        ct_move *m = &mv->m[i];
        if (m->after >= 0 && cards_slide_busy(&mv->m[m->after].s)) { ui_animate(); continue; }
        (paint ? paint : cards_slide_draw)(&m->s);
    }
}

/* ---- seats ------------------------------------------------------------------------------------------------------- */
/* the others' places from the left: the player after the viewer first */
static inline int ct_seat(int player, int viewer, int players) { return (player - viewer - 1 + 2 * players) % players; }

static inline float ct_seat_x(float x, float w, int seats, int i) { return x + w * (i + 0.5f) / seats; }

/* a small fan of n backs (at most 14 drawn: a thick hand looks thick), centred on (cx, cy), no wider than width */
static inline void ct_backs(int deck, float cx, float cy, float width, float w, int n, int how)
{
    cards_spot s[14];
    int k = n < 14 ? n : 14;
    if (k <= 0) return;
    cards_fan(cx, cy, width, w, cards_aspect(deck), k, k > 1 ? 18 : 0, s);
    for (int i = 0; i < k; i++) cards_draw(deck, CARD_NONE, s[i].x, s[i].y, w, s[i].deg, CD_DOWN | how);
}

/* where the next card of a fan of n backs lands (the last one's spot) */
static inline void ct_backs_end(int deck, float cx, float cy, float width, float w, int n, float *x, float *y,
                                float *deg)
{
    cards_spot s[14];
    int k = n < 1 ? 1 : n < 14 ? n : 14;
    cards_fan(cx, cy, width, w, cards_aspect(deck), k, k > 1 ? 18 : 0, s);
    *x = s[k - 1].x;
    *y = s[k - 1].y;
    *deg = s[k - 1].deg;
}

/* under a seat's fan: the player's token and how many cards they hold, centred on (cx, cy), h tall */
static inline void ct_label(SDL_Texture *token, float cx, float cy, float h, int n)
{
    float digits = n >= 10 ? 2 : 1, w = 1.15f * h + 0.62f * h * digits;
    ui_put_c(token, cx - w / 2 + h / 2, cy, h, h);
    ui_number(cx + w / 2 - 0.31f * h * digits, cy, 0.85f * h, n, 1);
}

/* ---- the viewer's hand ------------------------------------------------------------------------------------------- */
/* n cards in box b (one row, or two when one row would show too little of each): the cards' width (wmax at most)
 * and their spots, the first row behind the second */
static inline float ct_hand(box b, int n, float aspect, float wmax, cards_spot *spots)
{
    if (n <= 0) return wmax;
    float w = fminf(wmax, b.h / (aspect * 1.1f));
    float step = n > 1 ? (b.w - w) / (n - 1) : w;
    if (step >= 0.4f * w || n < 8) {
        cards_fan(b.x + b.w / 2, b.y + b.h / 2 - 0.03f * w * aspect, b.w, w, aspect, n, n > 1 ? 8 : 0, spots);
        return w;
    }
    int top = n / 2, bot = n - top;             /* two rows: the second overlaps the lower part of the first */
    w = fminf(wmax, b.h / (aspect * 1.62f));
    float h = w * aspect, y0 = b.y + (b.h - 1.55f * h) / 2 + h / 2;
    cards_fan(b.x + b.w / 2, y0, b.w, w, aspect, top, 0, spots);
    cards_fan(b.x + b.w / 2, y0 + 0.55f * h, b.w, w, aspect, bot, 0, spots + top);
    return w;
}

/* ---- textures ---------------------------------------------------------------------------------------------------- */
/* the players' tokens at one size (remade when it changes) */
static inline void ct_tokens(SDL_Texture **t, int *made, int size)
{
    if (size < 4 || *made == size) return;
    *made = size;
    for (int q = 0; q < MAX_PLAYERS; q++) {
        ui_free(&t[q]);
        cards_token(q, size, ui_buf(size, size));
        t[q] = ui_tex(size, size, ui_buf(size, size));
    }
}

#endif
