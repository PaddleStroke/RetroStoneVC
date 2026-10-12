/*
 * RetroStone Board's card table (the card kit's second part, on top of cards.h and the shell): what the card games
 * draw and handle round their cards, written once.
 * - Moving cards: a list of slides, each tagged by where it lands (the game leaves that card out there until it has
 *   landed: ct_hides); a slide can wait unseen (ct_show) or wait for another one (ct_after); the list is busy while
 *   one moves and a moment after each lands (its rest), so the hand-off and the end card wait for it.
 * - The trick table: the viewer's hand fanned at the bottom, a seat for each player round the middle (their chip, the
 *   card they play placed towards them), the trick (its cards by player: a lone hand leaves a seat out), gathered by
 *   its winner, the trick pile (tap it: the last trick), the deal and the play flying in, the finger (tap a card: it
 *   rises; tap it again, its place in the middle, or drag it there: it goes).
 * - The others along the top (small fans of backs, a chip and a count); the viewer's hand in one row or two, fitted in
 *   a box or along the bottom of the table.
 * - Pictures kept at the size they are drawn: the players' chips, suit signs, discs, panels, the shell's icons tinted,
 *   round buttons (a tick, an arrow), badges (a number, a pass), the round's summary panel, points rising.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The seats: the viewer at the bottom; the others in turn order from the left round the top (the order of their bars
 * at the top of the screen), so play goes round clockwise.
 */
#ifndef CARDS_TABLE_H
#define CARDS_TABLE_H

#include <stdint.h>

#include "cards.h"
#include "shell.h"

/* ---- moving cards ------------------------------------------------------------------------------------------------ */
#define CT_MOVES 200
typedef struct {
    cards_slide s;
    int tag, a, b;                              /* where it lands: the game's tag and numbers (ct_hides) */
    int after;                                  /* not drawn while this move still moves (-1: none) */
    Uint32 show, end;                           /* drawn from show on (0: at once); busy until end */
} ct_move;
/* rest: how long the list stays busy after a card lands (each new slide's; ct_rest for one) */
typedef struct { ct_move m[CT_MOVES]; int n, rest; Uint32 quiet; } ct_moves;

void ct_calm(ct_moves *mv);                     /* no more moves (they land at once) */
/* a card from (x0, y0) to (x1, y1), w0 to w1 wide, turned a0 to a1 degrees, face up or down at each end, after
 * `delay` ms, for ms; it lands at (tag, a, b); sound: played as it lands (-1: none). Its index (-1: no room). */
int  ct_slide(ct_moves *mv, int deck, int card, float x0, float y0, float w0, float a0, int up0, float x1, float y1,
              float w1, float a1, int up1, int ms, int delay, int tag, int a, int b, int sound);
void ct_after(ct_moves *mv, int i, int first);  /* move i not drawn while move `first` moves (it goes on from there) */
void ct_show(ct_moves *mv, int i, int ms);      /* move i drawn only ms after it starts (held unseen at A till then) */
void ct_rest(ct_moves *mv, int i, int ms);      /* busy ms after move i lands (instead of the list's rest) */
void ct_hop(ct_moves *mv, int i, float hop, float reach);   /* move i's lift (cards_slide's) */
void ct_until(ct_moves *mv, Uint32 t);          /* busy until then at least */
int  ct_moving(const ct_moves *mv);             /* a card moves (or waits to) */
int  ct_busy(const ct_moves *mv);               /* a card moves, or one landed a moment ago */
Uint32 ct_quiet(const ct_moves *mv);           /* when it will be quiet: the last landing and its rest */
int  ct_hides(const ct_moves *mv, int tag, int a, int b);   /* the cards still on their way to (tag, a, b); -1: any */
void ct_draw(ct_moves *mv);                     /* the moving cards where they are now */

/* ---- small pictures, kept ----------------------------------------------------------------------------------------
 * Each made once at a size and kept in a cache the kit shares: take it again each frame, do not keep it. It comes
 * back untinted and opaque (ct_icon: tinted r, g, b); tint or fade it after. */
SDL_Texture *ct_token(int player, int size);    /* a player's chip (cards_token), size x size */
SDL_Texture *ct_suit(int deck, int suit, int size);   /* a suit's sign (cards_suit_icon) */
/* a disc of one colour fading from alpha a0 at its middle (art_disc; a0 in hundredths: 85 = 0.85) */
SDL_Texture *ct_disc(const uint8_t rgb[3], int a0, int size);
SDL_Texture *ct_round(int w, int h, float radius, const uint8_t rgba[4]);   /* a rounded rectangle of one colour */
SDL_Texture *ct_icon(int which, int size, Uint8 r, Uint8 g, Uint8 b);       /* the shell's icon (art.h ICON_*) */
/* a game's own picture `kind` painted by `paint` at size x size (a few dozen kept) */
SDL_Texture *ct_picture(void (*paint)(int kind, int size, uint8_t *rgba), int kind, int size);

/* the kit's pictures, white (tinted when drawn): an arrow pointing right, a tick, a slash (a 0 struck through, a
 * player out), a cream crescent moon; discs in the shell's button colours (off, on) and a dark one (a badge) */
enum { CT_ARROW, CT_TICK, CT_SLASH, CT_MOON, CT_DISC_OFF, CT_DISC_ON, CT_DISC_DARK };
/* a picture centred, d wide, turned deg, tinted (r, g, b, a) */
void ct_pic(int kind, float cx, float cy, float d, double deg, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
/* a round button (the shell's colours): `on` gold, its picture (CT_ARROW, CT_TICK; -1: none) turned deg, faint off */
void ct_button(float cx, float cy, float d, int on, int pic, double deg);
int  ct_on(float cx, float cy, float d, float x, float y);   /* (x, y) on a round button d wide */
void ct_number(float cx, float cy, float h, int v, int nil);   /* a number in cream, h high; nil: the 0 struck out */
void ct_badge(float cx, float cy, float d, int v);   /* a number in a dark disc (a bid) */
void ct_pass(float cx, float cy, float d, int glow);   /* the pass button: the shell's pass icon on a round button */
void ct_pass_badge(float cx, float cy, float d);     /* the pass icon in a dark disc (a player who passed) */
/* a choice (a suit, a game): its picture on the button, raised in gold light when chosen, faint when it cannot be */
void ct_choice(SDL_Texture *pic, float cx, float cy, float d, int chosen, int enabled);
float ct_number_w(int v, float h, int plus);    /* the width of ui_number_signed(v, plus) at height h */
void ct_rule(float x, float y, float w);        /* a faint cream line across a panel */
void ct_panel(float x, float y, float w, float h, int alpha);   /* a dark panel (a round's summary) */
void ct_plus(float cx, float cy, float h, int value);   /* points scored: a plus and the digits, cream */

/* ---- points rising: a dark pill, a badge's number or an icon, a plus and the digits; they rise and fade ------- */
#define CT_FLOATS 48
#define CT_FLOAT_MS 1000
typedef struct { struct { float x, y; int value, badge; Uint32 t0; } f[CT_FLOATS]; int n; } ct_floats;
/* points rising from (x, y) at t0 (badge: a number before them, -1 an icon); the moves stay busy until they are gone */
void ct_float(ct_floats *fl, ct_moves *mv, float x, float y, int value, int badge, Uint32 t0);
void ct_floats_draw(const ct_floats *fl, SDL_Texture *pill, SDL_Texture *icon);

/* ---- the trick table --------------------------------------------------------------------------------------------- */
#define CT_MAXP 6
#define CT_HAND 52                              /* the most cards a hand of the kit holds (a whole pack) */
#define CT_SLIDE_MS 300
#define CT_LOOK_MS 900                          /* a full trick stays a moment before its winner gathers it */
#define CT_DEAL_MS 260
typedef struct {
    box b;                                      /* the table (between the bars) */
    int players, me, deck;                      /* me: the viewer, at the bottom; deck: the cards' (0: French) */
    float asp;                                  /* a card's height / width */
    float wh, hx, hy, hw, arc;                  /* the hand: card width, the fan's centre and width, its arc */
    float hand_top;                             /* above it: the trick's area */
    float wt, cx, cy, rx, ry;                   /* the trick: card width, centre, the cards' ellipse */
    float tok, sx[CT_MAXP], sy[CT_MAXP], ang[CT_MAXP];   /* the seats: chip size, centres, angles (degrees) */
    float tx[CT_MAXP], ty[CT_MAXP], tdeg[CT_MAXP];       /* each player's card in the trick */
    float px, py, pw;                           /* the trick pile */
    float u;
} ct_table;
enum { CT_NONE, CT_TRICK, CT_HAND_TAG, CT_SEAT };   /* the trick table's tags (a game's own beside them) */

float ct_angle(int players, int me, int pl);    /* where a player sits (degrees, y down; the viewer 90) */
/* the table in b for `players`, the viewer `me`, a hand of max_hand at most (T->deck kept) */
void ct_layout(ct_table *T, box b, int players, int me, int max_hand);
void ct_fan(const ct_table *T, int n, cards_spot *s);   /* the viewer's n cards fanned */
void ct_seat_spot(const ct_table *T, int pl, int k, float *x, float *y);   /* beside a seat's chip: its numbers (k) */
void ct_draw_seats(const ct_table *T, int turn);   /* the seats' chips; the player to move in gold light */
/* the cards of a trick on the table, played from `leader` on (who: each card's player; NULL: in turn from leader),
 * those still on their way left out; the last trick over the table, its winner's seat in gold */
void ct_draw_trick(const ct_table *T, const ct_moves *mv, const uint8_t *cards, const uint8_t *who, int n, int leader);
void ct_draw_last(const ct_table *T, const uint8_t *cards, const uint8_t *who, int n, int leader, int winner);
/* the trick gathered: its cards slide to the winner's seat after look_ms; the last one is still on its way (it shows
 * from last_lands ms on) */
void ct_gather(ct_moves *mv, const ct_table *T, const uint8_t *cards, const uint8_t *who, int n, int leader, int winner,
               int look_ms, int last_lands);
void ct_draw_pile(const ct_table *T, int tricks);   /* the tricks played, face down (tap it: the last trick) */
int  ct_on_pile(const ct_table *T, int tricks, float x, float y);
/* the round's summary: the panel for `rows` rows over the table (dimmed); its left, its width, the first row's centre
 * and the row height */
void ct_summary(const ct_table *T, int rows, float *x0, float *pw, float *y_row, float *rh);
void ct_bar(box b, int cards, int score);       /* a bar: a small back and the cards in hand, then the score */
void ct_thumb(const int *cards, int n, int size, uint8_t *rgba);   /* the menu's card: n cards, the last on top */

/* the viewer's hand: each card drawn how[i] (CD_RAISED: chosen), lifted lift[i] card widths in gold light (cards just
 * received; NULL: none); the card at index `skip` (dragged; -1: none) and the cards still on their way (CT_HAND_TAG,
 * their index) left out. Face down: under the hand-off cover. Cards are told apart by their index in the hand as
 * shown, never by their byte: a double pack holds two equal cards. ct_hand_hit: the index under (x, y) (the raised or
 * lifted ones first; -1: none); ct_hand_at: its card (CARD_NONE: none). */
void ct_hand_draw(const ct_table *T, const ct_moves *mv, const uint8_t *h, int n, const int *how, const float *lift,
                  int skip);
void ct_hand_backs(const ct_table *T, int n);
int  ct_hand_hit(const ct_table *T, int n, const int *how, const float *lift, float x, float y);
int  ct_hand_at(const ct_table *T, const uint8_t *h, int n, const int *how, const float *lift, float x, float y);
void ct_deal(ct_moves *mv, const ct_table *T, const uint8_t *h, int n);   /* the viewer's cards fly in, turn up */
int  ct_index(const uint8_t *h, int n, int card);   /* the first index of the card in the hand (-1: none) */

/* a finger on a card of the hand: a tap (it rises; tapped again, it goes) or a drag (released above the hand: it goes
 * to the middle). The cards by their index in the hand as shown (at, raised_at) and their bytes (card, raised). */
typedef struct {
    int card, on, raised;                       /* the card touched, dragged (moved far enough), the one risen */
    int at, raised_at;                          /* their indexes in the hand (-1: none) */
    float x0, y0, x, y;
} ct_touch;
enum { CT_T_NONE, CT_T_TAP, CT_T_SEND };
void ct_touch_reset(ct_touch *t);
void ct_touch_down(ct_touch *t, int at, int card, float x, float y);   /* the card at index `at` touched */
/* a press on the viewer's hand (h, n as shown, drawn how, lift: ct_hand_draw's): the card under it touched (1); none:
 * the risen card goes back down, or SND_DENY (0) */
int  ct_touch_hand(ct_touch *t, const ct_table *T, const uint8_t *h, int n, const int *how, const float *lift, float x,
                   float y);
void ct_touch_move(ct_touch *t, float x, float y);
int  ct_touch_up(ct_touch *t, const ct_table *T, float x, float y, int *card);   /* CT_T_*; *card: the card */
void ct_touch_draw(const ct_touch *t, const ct_table *T);   /* the dragged card under the finger */
int  ct_tap_play(ct_touch *t, int card, int legal);   /* a tap: it rises (0), or, risen, it goes (1); SND_DENY */
void ct_lower(ct_touch *t);                     /* the risen card back down */
int  ct_dragged(const ct_touch *t);             /* the index of the card dragged (ct_hand_draw's skip; -1: none) */
static inline int ct_is_raised(const ct_touch *t, int i) { return t->raised != CARD_NONE && i == t->raised_at; }
void ct_draw_target(const ct_table *T);         /* the risen card's place in the trick, in gold light */
int  ct_on_target(const ct_table *T, float x, float y);
/* the viewer plays `card`: from its place in their hand as shown (h, n; the finger's or the risen card's index), or
 * from the finger, to the trick */
void ct_play(ct_moves *mv, const ct_table *T, const ct_touch *f, const uint8_t *h, int n, int card);

/* ---- the others along the top, the viewer's hand in a box ---------------------------------------------------------
 * The others' places from the left: the player after the viewer first; a place's centre in a strip x..x+w */
static inline int ct_seat(int player, int viewer, int players) { return (player - viewer - 1 + 2 * players) % players; }
static inline float ct_seat_x(float x, float w, int seats, int i) { return x + w * (i + 0.5f) / seats; }
/* a small fan of n backs (at most 14 drawn), centred on (cx, cy), no wider than width; where its next card lands */
void ct_backs(int deck, float cx, float cy, float width, float w, int n, int how);
void ct_backs_end(int deck, float cx, float cy, float width, float w, int n, float *x, float *y, float *deg);
/* under a seat's fan: the player's chip (token: its texture) and how many cards they hold, centred, h tall */
void ct_label(SDL_Texture *token, float cx, float cy, float h, int n);
/* n cards in box b (one row, or two when one row would show too little of each): the cards' width (wmax at most)
 * and their spots, the first row behind the second */
float ct_hand(box b, int n, float aspect, float wmax, cards_spot *spots);
/* n cards along the bottom of the table b in one row, or two when more than per_row (the first half behind, the
 * second lower, in front), as big as w_max and the height h_max allow: their spots, the cards' width, the hand's top
 * (above a raised card); the card under (x, y) (the front one first; -1: none) */
typedef struct { float w, top; int n, rows, per; cards_spot spot[CT_HAND]; } ct_rows;
void ct_rows_layout(ct_rows *h, box b, int n, int per_row, float w_max, float h_max);
int  ct_rows_hit(const ct_rows *h, float x, float y);

/* ---- patience: a column fanned down, a run under the finger, the cards leaping off at a win --------------------
 * A column of n cards (`down` of them face down at its bottom), w wide, steps of down_step and up_step card widths:
 * the steps, closer when they would reach past `room` (the face-up ones first, then the others). */
void ct_column_steps(int n, int down, float room, float w, float down_step, float up_step, float *dd, float *du);
/* n cards fanned down from (x, y) by step, w wide, the first raised (a run dragged) */
void ct_run(int deck, const uint8_t *cards, int n, float x, float y, float w, float step);
/* card j's flight, t ms after it left (x0, y0): thrown sideways, falling, bouncing on the bottom edge of b (cards w
 * wide). Where it is (xs[0], ys[0]) and where it was k x CT_TRAIL_MS before (xs[k]; NAN: not yet thrown then). */
#define CT_TRAIL 4
#define CT_TRAIL_MS 60
void ct_leap(box b, float w, int j, float x0, float y0, float t, float *xs, float *ys);

#endif
