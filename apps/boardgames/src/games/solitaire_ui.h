/*
 * The patience games (Klondike, FreeCell, Spider, Pyramid, TriPeaks, La Belle Lucie, Yukon, Forty Thieves, Golf,
 * Canfield, Montana, Clock) on the screen: what they share, on top of the card kit. A game sets where its piles go
 * (solui.at[]), and the rest is here: the piles drawn (columns fanned down, fans to the right, the waste fanned right,
 * the stock and the foundations as piles, the single cards of a layout), the cards sliding and turning over after
 * each move (a move and what the game then did by itself, chained; a shuffle: each card flying to its new place), the
 * finger (a tap sends a card or a run to its best place, a quick second tap only to a foundation; a drag carries a
 * run where the finger drops it; or, `pairs`, two taps pair two cards: the first one is lifted), the hint (the move
 * blinks), the table's size (solui_table: the cards as big as the room allows), the bar (the level or the deal's
 * number, the move counter), the hint button (the shell's tool), the stars (hints and undos counted), the empty
 * stock's arrow going round, and the win: the cards leaping off the foundations and bouncing across the table, each
 * with a trail of copies.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#ifndef SOLITAIRE_UI_H
#define SOLITAIRE_UI_H

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "cards_table.h"
#include "games/sollevels.h"
#include "shell.h"

#define SOLUI_ASPECT 1.4f
#define SOLUI_FLIP_MS 220
#define SOLUI_HINT_MS 2400
#define SOLUI_LIFE_MS 2600                      /* a leaping card's longest flight */

/* how a pile lies: stacked (its top card shows), a column fanned down, the waste (its last cards fanned right),
 * Spider's stock (a back for each row still to deal, from the right), a card of a layout (Pyramid, TriPeaks: one card
 * or nothing, not even its place; drawn in the piles' order, so the later ones lie over the earlier ones), a fan to the
 * right (La Belle Lucie: steps of fan_step card widths, closer when its last card would reach past room). A pile shows
 * its top card face down while all its cards are (a reserve turning its next card up). */
enum { SF_PILE, SF_COLUMN, SF_WASTE, SF_ROW, SF_SLOT, SF_FAN };

typedef struct {
    float x, y;                                 /* its first card's centre */
    float room;                                 /* a column: how far down its last card's centre may go (a fan:
                                                 * right) */
    int fan, first;                             /* first: the pile shows its bottom card (Spider's foundations) */
    int bare;                                   /* a column or a fan: no place drawn when it is empty (a fan of La
                                                 * Belle Lucie: never filled again) */
} solspot;

typedef struct solui {
    const solrules *R;
    solrec *rec;
    int deck, waste_fan;                        /* waste_fan: the waste's cards fanned (Klondike's draw) */
    float w, down_step, up_step;                /* the card's width; the columns' steps (parts of w) */
    box b;
    solspot at[SOL_PILES];
    ct_moves mv;                                /* the cards on the move: tag the pile, a the card (-1: it moves on) */
    int16_t land[SOL_PILES][SOL_CARDS], slot[SOL_PILES][SOL_CARDS];   /* the chaining of a move's slides */
    /* the finger: pressed on card idx of pile (-1: nothing); dragging; the press, the finger, its grip on the card */
    int pile, idx, drag;
    float x0, y0, fx, fy, gx, gy;
    Uint32 tap_t;
    float tap_x, tap_y;
    int hint;                                   /* the hint blinks */
    Uint32 hint_t0;
    solmove hm;
    int won;                                    /* the win: the cards leap off from win_t0 */
    Uint32 win_t0;
    int hints, undos;                           /* asked for since the deal (the stars: solui_stars) */
    float base_down, base_up;                   /* the steps as the game set them (solui_table grows them) */
    int made_bar;
    SDL_Texture *t_back;
    /* pairs (Pyramid): a tap chooses a card (lifted), a tap on another plays the move from the first one's pile to
     * the second one's (the game's rules pair them); a card that goes alone (a king) goes on one tap. sel: the chosen
     * card's pile + 1 (0: none). */
    int pairs, sel;
    SDL_Texture *t_again;                       /* the empty stock's arrow (solui_again) */
    int again_size;
    float fan_step;                             /* SF_FAN's step, card widths (0: 0.3) */
    /* a game's own slides for the moves from `first` on, played from `before` (a shuffle: solui_rearrange); 0: the
     * usual ones (NULL: always the usual ones) */
    int (*animate)(struct solui *u, const solpos *before, int first);
} solui;

/* ---- where the cards lie ----------------------------------------------------------------------------------------- */
/* a column's steps: as set, closer when it would reach past its room (the face-up ones first, then the others) */
static inline void solui_steps(const solui *u, const solpos *p, int pile, float *dd, float *du)
{
    ct_column_steps(p->n[pile], p->down[pile], u->at[pile].room, u->w, u->down_step, u->up_step, dd, du);
}

static inline void solui_xy(const solui *u, const solpos *p, int pile, int i, float *x, float *y)
{
    const solspot *s = &u->at[pile];
    *x = s->x;
    *y = s->y;
    int n = p->n[pile];
    switch (s->fan) {
    case SF_COLUMN: {
        float dd, du;
        solui_steps(u, p, pile, &dd, &du);
        int down = p->down[pile];
        *y += (i < down ? i : down) * dd + (i > down ? i - down : 0) * du;
        break;
    }
    case SF_WASTE: {
        int k = n < u->waste_fan ? n : u->waste_fan, j = i - (n - k);
        if (j > 0) *x += j * 0.3f * u->w;
        break;
    }
    case SF_ROW: {
        int rows = (n + 9) / 10;
        *x -= (rows - 1 - i / 10) * 0.2f * u->w;
        break;
    }
    case SF_FAN: {
        float dd, du;
        ct_column_steps(n, 0, s->room, u->w, 0, u->fan_step > 0 ? u->fan_step : 0.3f, &dd, &du);
        *x += i * du;
        break;
    }
    }
}

static inline void solui_tool(const solui *u);
static inline int solui_tool_press(solui *u, float x, float y);

/* ---- the table: a row of piles over the columns ----------------------------------------------------------------- */
/* where solui_table put things: the card's width, the pitch across, the first place's centre x, the top row's centre
 * y, the columns' first card's centre y, how far down a column's last card may go from it */
typedef struct { float w, pitch, x0, ty, cy, room; } soltable;

/* the card as big as the table allows: `across` places side by side (the columns, or the top row when it is wider; a
 * gap between two piles counted as a part of a place), and under the top row at least `need` card widths of room
 * for a column's cards below its first one (a longer column closes up: ct_column_steps). So a phone held upright gets
 * cards as wide as the columns allow, and a wide screen cards as tall as its height allows, the table centred across.
 * The steps grow with the room left (a column of `longest` cards reaching down to the bottom), the face-up one up to
 * three quarters of a card wide: an upright phone's height used, each card shown more. */
static inline soltable solui_table(solui *u, box b, float across, float need, int longest)
{
    soltable t;
    if (u->base_up <= 0) u->base_up = u->up_step, u->base_down = u->down_step;
    float w = floorf(fminf(b.w / ((across - 1) * 1.06f + 1.1f), b.h / (3.06f + need)));
    t.w = u->w = w;
    t.pitch = across > 1 ? fminf(1.16f * w, (b.w - 1.1f * w) / (across - 1)) : 0;
    t.x0 = b.x + b.w / 2 - (across - 1) / 2 * t.pitch;
    t.ty = b.y + 0.06f * w + 0.7f * w;
    t.cy = t.ty + 1.58f * w;
    t.room = b.y + b.h - 0.72f * w - t.cy;
    float up = clampf(t.room / w / (longest > 1 ? longest - 1 : 1), u->base_up, 0.75f);
    u->up_step = up;
    u->down_step = fminf(0.25f, u->base_down * up / u->base_up);
    return t;
}

/* ---- the slides -------------------------------------------------------------------------------------------------- */
static inline void solui_calm(solui *u)
{
    ct_calm(&u->mv);
    u->hint = 0;
    u->drag = 0;
    u->pile = -1;
    u->sel = 0;
}

/* a card from A to B, to card idx of pile in the end (-1: it moves on), drawn only from vis (ms; 0: at once) */
static inline int solui_slide(solui *u, int card, int up0, float x0, float y0, int up1, float x1, float y1, int ms,
                              int delay, int pile, int idx, Uint32 vis)
{
    u->mv.rest = 90;
    int k = ct_slide(&u->mv, u->deck, card, x0, y0, u->w, 0, up0, x1, y1, u->w, 0, up1, ms, delay, pile, idx, -1, -1);
    ct_show(&u->mv, k, (int)vis);
    return k;
}

static inline int solui_hidden(const solui *u, int pile, int i)
{
    if (u->drag && pile == u->pile && i >= u->idx) return 1;
    return ct_hides(&u->mv, pile, i, -1) > 0;
}

static inline int solui_ms_for(const solui *u, float x0, float y0, float x1, float y1)
{
    float d = hypotf(x1 - x0, y1 - y0) / u->w;
    return (int)fminf(380, 170 + 32 * d);
}

/* a card leaves card i of pile a (in the position cur) for card j of pile b (in nxt): a slide chained after the one
 * that brought it there, if any */
static inline void solui_carry(solui *u, const solpos *cur, const solpos *nxt, int a, int i, int b, int j, int up0,
                               int up1, int delay, const float *from, int sound)
{
    float x0, y0, x1, y1;
    if (from) x0 = from[0], y0 = from[1];
    else solui_xy(u, cur, a, i, &x0, &y0);
    solui_xy(u, nxt, b, j, &x1, &y1);
    int ms = from ? 130 : solui_ms_for(u, x0, y0, x1, y1), was = u->slot[a][i];
    if (was >= 0) u->mv.m[was].tag = -1;        /* it moves on: nothing to hide where it had gone */
    int k = solui_slide(u, cur->c[a][i], up0, x0, y0, up1, x1, y1, ms, delay, b, j, (Uint32)u->land[a][i]);
    u->slot[a][i] = -1;
    u->land[a][i] = 0;
    u->slot[b][j] = (int16_t)k;
    u->land[b][j] = (int16_t)(delay + ms);
    if (sound >= 0) ui_sound_later(sound, delay + ms - 40 > 0 ? delay + ms - 40 : 0);
}

/* the moves from move `first` on, played from the position `before`: their slides and flips, each move's after the
 * cards it takes have landed. drag: the dragged cards' centres (x, y each; NULL: from their places). */
static inline void solui_animate(solui *u, const solpos *before, int first, const float *drag)
{
    solpos cur = *before, nxt;
    for (int a = 0; a < SOL_PILES; a++)
        for (int i = 0; i < SOL_CARDS; i++) u->land[a][i] = 0, u->slot[a][i] = -1;
    int t = 0, sounds = 0;
    for (int m = first; m < u->rec->n; m++) {
        solmove mv = u->rec->mv[m];
        int a = mv.from, b = mv.to, n = mv.n;
        nxt = cur;
        sol_apply(&nxt, mv);
        switch (SM_KIND(mv.kind)) {
        case SM_MOVE: {
            int start = t, run = n == 13 && cur.kind[b] == SP_FOUND;   /* Spider's run peels off from the ace */
            for (int k = 0; k < n; k++)
                if (u->land[a][cur.n[a] - n + k] > start) start = u->land[a][cur.n[a] - n + k];
            int snd = sounds++ % 2 == 0 || m == first ? (cur.kind[b] == SP_FOUND ? SND_PLACE : SND_MOVE) : -1;
            for (int k = 0; k < n; k++)
                solui_carry(u, &cur, &nxt, a, cur.n[a] - n + k, b, nxt.n[b] - n + k, cur.n[a] - n + k >= cur.down[a], 1,
                            start + (run ? (n - 1 - k) * 32 : 0), drag && m == first ? drag + 2 * k : NULL,
                            k == 0 ? snd : -1);
            if (cur.kind[a] == SP_TAB && nxt.down[a] < cur.down[a]) {   /* the card left on top turns up */
                int j = nxt.n[a] - 1;
                float x, y;
                solui_xy(u, &nxt, a, j, &x, &y);
                int delay = start + (run ? 13 * 32 : 0) + 140;
                int k = solui_slide(u, nxt.c[a][j], 0, x, y, 1, x, y, SOLUI_FLIP_MS, delay, a, j, 0);
                u->slot[a][j] = (int16_t)k;
                u->land[a][j] = (int16_t)(delay + SOLUI_FLIP_MS);
            }
            t = start + 85;
            break;
        }
        case SM_DRAW:
            for (int k = 0; k < n; k++)
                solui_carry(u, &cur, &nxt, a, cur.n[a] - 1 - k, b, cur.n[b] + k, 0, 1, t + 90 * k, NULL, SND_MOVE);
            t += 90 * n;
            break;
        case SM_REDEAL:
            for (int k = 0; k < n; k++)
                solui_carry(u, &cur, &nxt, a, cur.n[a] - 1 - k, b, k, 1, 0, t + 9 * k, NULL, k ? -1 : SND_MOVE);
            t += 9 * n;
            break;
        case SM_DEAL:
            for (int k = 0; k < n; k++)
                solui_carry(u, &cur, &nxt, a, cur.n[a] - 1 - k, b + k, cur.n[b + k], 0, 1, t + 65 * k, NULL,
                            k % 2 ? -1 : SND_MOVE);
            t += 65 * n;
            break;
        case SM_TURN: {                         /* a card uncovered turns up, once the one over it has gone */
            int j = nxt.n[a] - 1, delay = t + 60;
            float x, y;
            if (j < 0) break;
            solui_xy(u, &nxt, a, j, &x, &y);
            int k = solui_slide(u, nxt.c[a][j], 0, x, y, 1, x, y, SOLUI_FLIP_MS, delay, a, j, 0);
            u->slot[a][j] = (int16_t)k;
            u->land[a][j] = (int16_t)(delay + SOLUI_FLIP_MS);
            t += 30;
            break;
        }
        }
        cur = nxt;
    }
}

/* ---- the win: the cards leap off the foundations ----------------------------------------------------------------- */
static inline int solui_found_n(const solui *u, int *list)   /* the foundations holding cards, in order */
{
    const solpos *p = &u->rec->pos;
    int k = 0;
    for (int i = 0; i < p->npiles; i++)
        if (p->kind[i] == SP_FOUND && p->n[i]) list[k++] = i;
    return k;
}

static inline int solui_leap_gap(const solui *u)  /* ms between two cards leaving */
{
    int n = sol_on_found(&u->rec->pos);
    return n ? (int)clampf(3200.0f / n, 28, 80) : 80;
}

static inline Uint32 solui_leap_end(const solui *u)
{
    int n = sol_on_found(&u->rec->pos);
    return u->win_t0 + (Uint32)((n > 0 ? n - 1 : 0) * solui_leap_gap(u) + SOLUI_LIFE_MS);
}

static inline int solui_leaping(const solui *u) { return u->won && ui_ms() < solui_leap_end(u); }

static inline void solui_draw_leaps(const solui *u)
{
    const solpos *p = &u->rec->pos;
    int fl[SOL_PILES], nf = solui_found_n(u, fl), total = sol_on_found(p), gap = solui_leap_gap(u);
    Uint32 now = ui_ms();
    for (int i = 0; i < p->npiles; i++)        /* the empty ones (Montana's gaps) */
        if (p->kind[i] == SP_FOUND && !p->n[i]) cards_place(u->deck, u->at[i].x, u->at[i].y, u->w);
    int gone = now < u->win_t0 ? 0 : (int)((now - u->win_t0) / (Uint32)gap) + 1;
    if (gone > total) gone = total;
    for (int f = 0; f < nf; f++) {              /* what is left on each foundation */
        int pile = fl[f], left = p->n[pile] - (gone / nf + (f < gone % nf));
        const solspot *s = &u->at[pile];
        if (left <= 0) cards_place(u->deck, s->x, s->y, u->w);
        else cards_pile(u->deck, s->x, s->y, u->w, left, p->c[pile][s->first ? p->n[pile] - left : left - 1], 0);
    }
    SDL_Rect clip = {(int)u->b.x, (int)u->b.y, (int)u->b.w, (int)u->b.h};
    SDL_RenderSetClipRect(ui.r, &clip);
    for (int j = 0; j < gone; j++) {
        int pile = fl[j % nf], d = j / nf, i = u->at[pile].first ? d : p->n[pile] - 1 - d;
        float t = (float)(now - u->win_t0 - (Uint32)(j * gap)), xs[CT_TRAIL + 1], ys[CT_TRAIL + 1];
        if (i < 0 || i >= p->n[pile] || t > SOLUI_LIFE_MS) continue;
        const solspot *s = &u->at[pile];
        ct_leap(u->b, u->w, j, s->x, s->y, t, xs, ys);
        for (int k = CT_TRAIL; k >= 0; k--) {   /* the trail: its ghosts where it was (faint outlines); the card */
            if (isnan(xs[k]) || xs[k] < u->b.x - u->w || xs[k] > u->b.x + u->b.w + u->w) continue;
            if (k) cards_place(u->deck, xs[k], ys[k], u->w);
            else cards_draw(u->deck, p->c[pile][i], xs[k], ys[k], u->w, 0, 0);
        }
    }
    SDL_RenderSetClipRect(ui.r, NULL);
    ui_animate();
}

/* the game won by the last move: the cards leap once the slides have landed */
static inline void solui_check_win(solui *u, Uint32 at)
{
    u->won = u->R->won(&u->rec->pos);
    u->win_t0 = at;
}

/* a new deal: the cards of the piles of these kinds (bit SP_*) fly from (x, y) to their places, row by row (the
 * first card of each pile, then the second...), the face-up ones turning up */
static inline void solui_deal_kinds(solui *u, float x, float y, int gap, unsigned kinds)
{
    const solpos *p = &u->rec->pos;
    int k = 0, more = 1;
    for (int r = 0; more; r++) {
        more = 0;
        for (int c = 0; c < p->npiles; c++) {
            if (!(kinds >> p->kind[c] & 1) || r >= p->n[c]) continue;
            float cx, cy;
            solui_xy(u, p, c, r, &cx, &cy);
            int up = r >= p->down[c];
            solui_slide(u, p->c[c][r], 0, x, y, up, cx, cy, 300, k * gap, c, r, 0);
            if (k % 3 == 0) ui_sound_later(SND_MOVE, k * gap + 260);
            k++;
            more = 1;
        }
    }
    for (int i = 0; i < u->mv.n; i++) ct_show(&u->mv, i, u->mv.m[i].s.delay);   /* unseen till it goes */
}

/* a new deal: the tableau's cards fly from (x, y) to their places, row by row, the top ones turning up */
static inline void solui_deal(solui *u, float x, float y, int gap)
{
    solui_deal_kinds(u, x, y, gap, 1u << SP_TAB);
}

/* a shuffle (a redeal, a card drawn from under others): each card that is no longer where it was in `before` flies
 * from there to its new place, turning over if it must; it waits at its old place till then. They go a pile after
 * another, each pile's top card first, gap ms apart. A pack of 52 (each card once). For the game's `animate`. */
static inline int solui_rearrange(solui *u, const solpos *before, int gap)
{
    const solpos *p = &u->rec->pos;
    int16_t to[256];
    for (int c = 0; c < 256; c++) to[c] = -1;
    for (int b = 0; b < p->npiles; b++)
        for (int j = 0; j < p->n[b]; j++) to[p->c[b][j]] = (int16_t)(b * SOL_CARDS + j);
    int k = 0;
    for (int a = 0; a < before->npiles; a++) {  /* the slides bottom card first (drawn in that order), the top one
                                                 * leaving first */
        int moving = 0;
        for (int i = 0; i < before->n[a]; i++) moving += to[before->c[a][i]] != a * SOL_CARDS + i;
        for (int i = 0, m = 0; i < before->n[a]; i++) {
            int card = before->c[a][i], t = to[card], b = t / SOL_CARDS, j = t % SOL_CARDS;
            if (t < 0 || t == a * SOL_CARDS + i) continue;
            float x0, y0, x1, y1;
            solui_xy(u, before, a, i, &x0, &y0);
            solui_xy(u, p, b, j, &x1, &y1);
            int ms = solui_ms_for(u, x0, y0, x1, y1), delay = (k + moving - 1 - m) * gap;
            solui_slide(u, card, i >= before->down[a], x0, y0, j >= p->down[b], x1, y1, ms, delay, b, j, 0);
            if ((k + m) % 4 == 0) ui_sound_later(SND_MOVE, delay + ms - 40);
            m++;
        }
        k += moving;
    }
    return 1;
}

static inline int solui_busy(const solui *u)
{
    return ct_busy(&u->mv) || solui_leaping(u);
}

/* the game's undo(): back one move of the player's, the cards at rest (counted for the stars) */
static inline int solui_undo(solui *u)
{
    if (!sol_undo(u->rec)) return 0;
    solui_calm(u);
    u->won = 0;
    u->undos++;
    ui_handoff(0);
    return 1;
}

/* a new deal on the table: nothing moving, no win, no hint nor undo asked for yet */
static inline void solui_new(solui *u)
{
    solui_calm(u);
    u->won = 0;
    u->hints = u->undos = 0;
}

/* a designed level (game.levels; clamped): its pack in the order dealt into deck (n cards); its variant */
static inline int solui_level(const sollevel *levels, int count, int level, uint8_t *deck, int n)
{
    const sollevel *l = &levels[level < 0 ? 0 : level >= count ? count - 1 : level];
    if (sol_cards_read(l->cards, deck, n) != n) memset(deck, 0, (size_t)n);
    return l->variant;
}

/* the stars for a deal won: the logic puzzles' rule (ui_logic_stars), an undo counting as a mistake */
static inline int solui_stars(const solui *u) { return ui_logic_stars(u->hints, u->undos); }

/* the game's script() after its rules played the moves (ok: they all could): the cards at rest, the win at once */
static inline int solui_scripted(solui *u, int ok)
{
    solui_calm(u);
    solui_check_win(u, ui_ms());
    return ok;
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static inline int solui_glows(const solui *u, int pile, int i)  /* the hint's cards, blinking */
{
    if (!u->hint || (ui_ms() - u->hint_t0) / 300 % 2) return 0;
    const solpos *p = &u->rec->pos;
    solmove m = u->hm;
    if (SM_KIND(m.kind) != SM_MOVE)             /* the stock (turning the waste over: the stock's place) */
        return pile == (SM_KIND(m.kind) == SM_REDEAL ? m.to : m.from) && i == p->n[pile] - 1;
    return (pile == m.from && i >= p->n[pile] - m.n) || (pile == m.to && i == p->n[pile] - 1);
}

static inline int solui_chosen(const solui *u, int pile) { return u->pairs && u->sel == pile + 1; }

static inline void solui_draw_pile(const solui *u, int pile)
{
    const solpos *p = &u->rec->pos;
    const solspot *s = &u->at[pile];
    int n = p->n[pile], deck = u->deck;
    float x, y, w = u->w;
    switch (s->fan) {
    case SF_COLUMN:
    case SF_FAN:
        if ((!n || solui_hidden(u, pile, 0)) && !s->bare) cards_place(deck, s->x, s->y, w);
        if (!n && solui_glows(u, pile, -1)) ui_mark_gold(s->x, s->y, 1.3f * w);
        for (int i = 0; i < n; i++) {
            if (solui_hidden(u, pile, i)) continue;
            solui_xy(u, p, pile, i, &x, &y);
            cards_draw(deck, p->c[pile][i], x, y, w, 0, (i < p->down[pile] ? CD_DOWN : 0) |
                                                       (solui_glows(u, pile, i) ? CD_GLOW : 0));
        }
        break;
    case SF_PILE: {
        int k = n;
        while (k > 0 && solui_hidden(u, pile, k - 1)) k--;
        if (s->first && k < n) k = 0;           /* a run arriving: it shows once the king has landed */
        int top = !k || p->kind[pile] == SP_STOCK || k <= p->down[pile] ? CARD_NONE : p->c[pile][s->first ? 0 : k - 1];
        int glow = solui_glows(u, pile, n - 1) || (!n && solui_glows(u, pile, -1));
        cards_pile(deck, s->x, s->y, w, k, top, (glow ? CD_GLOW : 0) | (k && solui_chosen(u, pile) ? CD_RAISED : 0));
        break;
    }
    case SF_SLOT:
        if (!n || solui_hidden(u, pile, n - 1)) break;
        cards_draw(deck, p->c[pile][n - 1], s->x, s->y, w, 0, (p->down[pile] >= n ? CD_DOWN : 0) |
                   (solui_glows(u, pile, n - 1) ? CD_GLOW : 0) | (solui_chosen(u, pile) ? CD_RAISED : 0));
        break;
    case SF_WASTE: {
        int k = n;
        while (k > 0 && solui_hidden(u, pile, k - 1)) k--;
        int fan = k < u->waste_fan ? k : u->waste_fan, base = k - fan;
        if (!k) { cards_place(deck, s->x, s->y, w); break; }
        if (base > 0) cards_pile(deck, s->x, s->y, w, base, p->c[pile][base - 1], 0);
        for (int j = 0; j < fan; j++) {
            int i = base + j;
            cards_draw(deck, p->c[pile][i], s->x + j * 0.3f * w, s->y, w, 0,
                       i == k - 1 && solui_glows(u, pile, i) ? CD_GLOW : 0);
        }
        break;
    }
    case SF_ROW: {
        int k = n;
        while (k > 0 && solui_hidden(u, pile, k - 1)) k--;
        if (!k) { cards_place(deck, s->x, s->y, w); break; }
        int glow = solui_glows(u, pile, n - 1);
        for (int r = 0; r < (k + 9) / 10; r++) {
            solui_xy(u, p, pile, r * 10, &x, &y);
            cards_pile(deck, x, y, w, k - r * 10 < 10 ? k - r * 10 : 10, CARD_NONE,
                       glow && r == (k - 1) / 10 ? CD_GLOW : 0);
        }
        break;
    }
    }
}

/* the table: the piles, the cards on their way, the run under the finger; or the win's leaps */
static inline void solui_draw(solui *u)
{
    const solpos *p = &u->rec->pos;
    Uint32 now = ui_ms();
    if (u->hint && now - u->hint_t0 > SOLUI_HINT_MS) u->hint = 0;
    if (u->hint) ui_animate();
    int leaping = u->won && now >= u->win_t0 && solui_leaping(u);
    for (int i = 0; i < p->npiles; i++)
        if (!(leaping && p->kind[i] == SP_FOUND)) solui_draw_pile(u, i);
    ct_draw(&u->mv);
    if (u->won && !leaping && now < u->win_t0) ui_animate();   /* the leaps are coming */
    if (leaping) solui_draw_leaps(u);
    if (u->drag) {
        float du = p->kind[u->pile] == SP_TAB ? 0 : 0.3f * u->w, dd;
        if (p->kind[u->pile] == SP_TAB) solui_steps(u, p, u->pile, &dd, &du);
        ct_run(u->deck, &p->c[u->pile][u->idx], p->n[u->pile] - u->idx, u->fx - u->gx, u->fy - u->gy, u->w, du);
    }
    solui_tool(u);
}

/* ---- the finger -------------------------------------------------------------------------------------------------- */
/* the pile under (x, y), and its card (-1: an empty place); -1: none */
static inline int solui_hit(const solui *u, float x, float y, int *idx)
{
    const solpos *p = &u->rec->pos;
    float w = u->w, h = SOLUI_ASPECT * w, cx, cy;
    for (int pile = p->npiles - 1; pile >= 0; pile--)   /* the cards of a layout: the one on top */
        if (u->at[pile].fan == SF_SLOT && p->n[pile] &&
            cards_in(u->at[pile].x, u->at[pile].y, w, SOLUI_ASPECT, x, y)) {
            *idx = p->n[pile] - 1;
            return pile;
        }
    for (int pile = p->npiles - 1; pile >= 0; pile--) {   /* the later ones first: they lie over the earlier
                                                         * ones (Montana's places overlapping) */
        const solspot *s = &u->at[pile];
        int n = p->n[pile];
        if (s->fan == SF_SLOT) continue;
        if (s->fan == SF_COLUMN || s->fan == SF_FAN) {
            for (int i = n - 1; i >= 0; i--) {
                solui_xy(u, p, pile, i, &cx, &cy);
                float extra = i == n - 1 && s->fan == SF_COLUMN ? 0.35f * h : 0;   /* a little below the last one */
                if (fabsf(x - cx) <= w / 2 && y >= cy - h / 2 && y <= cy + h / 2 + extra) { *idx = i; return pile; }
            }
            if (!n && cards_in(s->x, s->y, w, SOLUI_ASPECT, x, y)) { *idx = -1; return pile; }
            continue;
        }
        if (n) solui_xy(u, p, pile, n - 1, &cx, &cy);
        else cx = s->x, cy = s->y;
        float left = s->fan == SF_ROW ? cx - ((n + 9) / 10 - 1) * 0.2f * w : cx;
        if (x >= left - w / 2 && x <= cx + w / 2 && fabsf(y - cy) <= h / 2) { *idx = n - 1; return pile; }
    }
    return -1;
}

static inline void solui_deny(void) { ui_sound(SND_DENY); }

/* a move by the player: played, its slides started (drag: from the cards' centres under the finger) */
static inline int solui_play(solui *u, solmove m, const float *drag)
{
    solpos before = u->rec->pos;
    int first = u->rec->n;
    if (!u->R->play(u->rec, m)) return 0;
    solui_calm(u);
    if (!u->animate || !u->animate(u, &before, first)) solui_animate(u, &before, first, drag);
    solui_check_win(u, ct_quiet(&u->mv));
    ui_handoff(0);
    return 1;
}

static inline void solui_press(solui *u, float x, float y)
{
    u->drag = 0;
    u->pile = -1;
    if (solui_tool_press(u, x, y)) return;
    u->hint = 0;
    if (u->won) return;
    int idx, pile = solui_hit(u, x, y, &idx);
    if (pile < 0) return;
    u->pile = pile;
    u->idx = idx;
    u->x0 = u->fx = x;
    u->y0 = u->fy = y;
    if (idx >= 0) {
        float cx, cy;
        solui_xy(u, &u->rec->pos, pile, idx, &cx, &cy);
        u->gx = x - cx;
        u->gy = y - cy;
    }
}

static inline void solui_motion(solui *u, float x, float y)
{
    if (u->pile < 0) return;
    u->fx = x;
    u->fy = y;
    const solpos *p = &u->rec->pos;
    if (!u->drag && u->idx >= 0 && hypotf(x - u->x0, y - u->y0) > 0.14f * u->w &&
        p->kind[u->pile] != SP_STOCK && u->R->pickable(p, u->pile, u->idx)) {
        ct_calm(&u->mv);                        /* cards still moving: they land at once */
        u->drag = 1;
        u->sel = 0;
    }
    if (u->drag) ui_animate();
}

/* the drop: the legal place the run overlaps most */
static inline int solui_drop(const solui *u, int n)
{
    const solpos *p = &u->rec->pos;
    float w = u->w, h = SOLUI_ASPECT * w, x = u->fx - u->gx, y = u->fy - u->gy, best = 0;
    int to = -1;
    for (int b = 0; b < p->npiles; b++) {
        if (b == u->pile || (p->kind[b] != SP_TAB && p->kind[b] != SP_FOUND && p->kind[b] != SP_CELL &&
                             !(u->pairs && p->kind[b] == SP_WASTE)))
            continue;
        if (!u->R->legal(p, sol_mv(SM_MOVE, u->pile, b, n))) continue;
        float cx = u->at[b].x, cy0 = u->at[b].y, cy1 = cy0;
        if (p->n[b]) solui_xy(u, p, b, p->n[b] - 1, &cx, &cy1);
        if (u->at[b].fan != SF_COLUMN) cy0 = cy1;
        float ox = fminf(x + w / 2, cx + w / 2) - fmaxf(x - w / 2, cx - w / 2);
        float oy = fminf(y + h / 2, cy1 + h / 2 + 0.3f * h) - fmaxf(y - h / 2, cy0 - h / 2);
        if (ox > 0 && oy > 0 && ox * oy > best) best = ox * oy, to = b;
    }
    return to;
}

/* pairs: a tap on card idx of pile (the top one): chosen, or played with the card chosen before, or alone (a king) */
static inline void solui_pair_tap(solui *u, int pile, int idx)
{
    const solpos *p = &u->rec->pos;
    int was = u->sel - 1;
    u->sel = 0;
    if (was == pile) { ui_sound(SND_TAP); return; }         /* the same card again: no longer chosen */
    if (was >= 0) {
        solmove m = sol_mv(SM_MOVE, was, pile, 1);
        if (!u->R->legal(p, m) || !solui_play(u, m, NULL)) solui_deny();
        return;
    }
    solmove m = u->R->tap(p, pile, idx, 0);
    if (m.kind != SM_NONE) { if (!solui_play(u, m, NULL)) solui_deny(); return; }
    if (!u->R->pickable(p, pile, idx)) { solui_deny(); return; }
    u->sel = pile + 1;
    ui_sound(SND_TAP);
}

static inline void solui_release(solui *u, float x, float y)
{
    int pile = u->pile, idx = u->idx;
    u->pile = -1;
    if (pile < 0 && !u->drag) u->sel = 0;       /* a tap on the table: nothing chosen */
    if (pile < 0 || u->won) { u->drag = 0; return; }
    const solpos *p = &u->rec->pos;
    if (u->drag) {                              /* the drop */
        u->fx = x;
        u->fy = y;
        u->pile = pile;
        int n = p->n[pile] - idx, to = solui_drop(u, n);
        float at[2 * SOL_CARDS], du = 0.3f * u->w, dd;
        if (p->kind[pile] == SP_TAB) solui_steps(u, p, pile, &dd, &du);
        for (int k = 0; k < n; k++) at[2 * k] = x - u->gx, at[2 * k + 1] = y - u->gy + k * du;
        u->drag = 0;
        u->pile = -1;
        if (to >= 0 && solui_play(u, sol_mv(SM_MOVE, pile, to, n), at)) return;
        float bx, by, gone = 0;
        for (int k = 0; k < n; k++) {           /* nowhere to go: back to its place */
            solui_xy(u, p, pile, idx + k, &bx, &by);
            if (!k) gone = hypotf(at[0] - bx, at[1] - by);
            solui_slide(u, p->c[pile][idx + k], 1, at[2 * k], at[2 * k + 1], 1, bx, by, 160, 0, pile, idx + k, 0);
        }
        if (gone > 0.7f * u->w) solui_deny();   /* (put back where it was: no fuss) */
        return;
    }
    Uint32 now = ui_ms();                       /* a tap; a quick second one: a foundation or nothing */
    int twice = now - u->tap_t < 380 && hypotf(x - u->tap_x, y - u->tap_y) < 0.6f * u->w;
    u->tap_t = now;
    u->tap_x = x;
    u->tap_y = y;
    if (p->kind[pile] == SP_STOCK) {
        solmove m = u->R->stock(p);
        u->sel = 0;
        if (twice && m.kind == SM_NONE) return;
        if (m.kind == SM_NONE || !solui_play(u, m, NULL)) solui_deny();
        return;
    }
    if (idx < 0) { u->sel = 0; return; }        /* an empty place */
    if (u->pairs) { solui_pair_tap(u, pile, idx); return; }
    solmove m = u->R->tap(p, pile, idx, twice);
    if (m.kind == SM_NONE) { if (!twice) solui_deny(); return; }
    if (!solui_play(u, m, NULL)) solui_deny();
}

/* ---- the empty stock: an arrow going round (a tap there turns the waste back over) ------------------------------- */
static inline void solui_again_pic(int size, uint8_t *o)
{
    art_style st = {{255, 255, 255}, {0, 0, 0}, {0, 0, 0}, {255, 255, 255}, 0, 0};
    art_fig *f = art_fig_new();
    art_arc(f, ART_PART, 0.5f, 0.5f, 0.19f, 0.27f, 70, 360);
    const float head[] = {0.6f, 0.53f, 0.86f, 0.53f, 0.73f, 0.33f};
    art_poly(f, ART_PART, head, 3);
    art_render(f, &st, size, size, (float)size, 0, 0, o);
    art_fig_free(f);
}

/* the arrow in a picture (a thumbnail: round the corner), faint cream, d wide, centred on (cx, cy) */
static inline void solui_paint_again(uint8_t *rgba, int size, float cx, float cy, int d)
{
    uint8_t *a = malloc((size_t)d * d * 4);
    if (!a) return;
    solui_again_pic(d, a);
    for (int i = 0; i < d * d; i++)
        a[4 * i] = 246, a[4 * i + 1] = 238, a[4 * i + 2] = 220, a[4 * i + 3] = (uint8_t)(a[4 * i + 3] * 0.85f);
    art_blend(rgba, size, size, a, d, d, (int)(cx - d / 2.0f), (int)(cy - d / 2.0f));
    free(a);
}

/* the arrow, faint cream, on the empty stock's place (x, y) */
static inline void solui_again(solui *u, float x, float y)
{
    int d = (int)(0.8f * u->w);
    if (u->again_size != d) {
        u->again_size = d;
        ui_free(&u->t_again);
        solui_again_pic(d, ui_buf(d, d));
        u->t_again = ui_tex(d, d, ui_buf(d, d));
        SDL_SetTextureColorMod(u->t_again, 246, 238, 220);
        SDL_SetTextureAlphaMod(u->t_again, 120);
    }
    ui_put_c(u->t_again, x, y, (float)d, (float)d);
}

/* ---- the bar: the level or the deal's number, the moves; the hint, the shell's tool 0 ---------------------------- */
static inline void solui_bar_textures(solui *u)
{
    int key = (int)ui.unit * 8 + u->deck;
    if (u->made_bar == key) return;
    u->made_bar = key;
    int cw = (int)(0.36f * ui.unit), ch = (int)(cw * SOLUI_ASPECT);
    ui_free(&u->t_back);
    cards_paint(u->deck, CARD_NONE, cw, ch, ui_buf(cw, ch));
    u->t_back = ui_tex(cw, ch, ui_buf(cw, ch));
}

static inline float solui_digits_w(int v, float h)
{
    int d = 1;
    for (; v >= 10; v /= 10) d++;
    return (0.74f * (d - 1) + 0.62f) * h;
}

/* the level in play (ui.level: a star and its number from 1), else deal > 0 (FreeCell's number) after a small card
 * back; then the move counter */
static inline void solui_bar(solui *u, box b, int deal)
{
    solui_bar_textures(u);
    float cy = b.y + b.h / 2, nh = 0.46f * ui.unit, x = b.x;
    int n = ui.level >= 0 ? ui.level + 1 : deal;
    if (n > 0) {
        float cw = 0.36f * ui.unit;
        if (ui.level >= 0) ui_star(x + cw / 2 + 0.04f * ui.unit, cy, 0.5f * ui.unit, 1);
        else ui_put_c(u->t_back, x + cw / 2, cy, cw, cw * SOLUI_ASPECT);
        x += cw + 0.16f * ui.unit;
        float dw = solui_digits_w(n, 0.4f * ui.unit);
        ui_number(x + dw / 2, cy, 0.4f * ui.unit, n, 1);
        x += dw + 0.45f * ui.unit;
    }
    float mw = solui_digits_w(u->rec->moves, nh);
    ui_number(x + mw / 2, cy, nh, u->rec->moves, 1);
}

/* the hint button: the shell's tool 0 (game.tools 1), in gold while the hint blinks, dim once won */
static inline void solui_tool(const solui *u)
{
    int lit = u->hint && (ui_ms() - u->hint_t0) / 300 % 2 == 0;
    ui_tool(0, ICON_HINT, u->won ? UI_DIM : lit ? UI_ON : 0);
}

/* a press on the hint: its move blinks (none: a refusal); 1: it was there */
static inline int solui_tool_press(solui *u, float x, float y)
{
    if (ui_tool_hit(x, y) != 0) return 0;
    if (u->won) return 1;
    solmove m = u->R->hint(&u->rec->pos);
    if (m.kind == SM_NONE) { ui_sound(SND_DENY); return 1; }
    u->hm = m;
    u->hint = 1;
    u->hint_t0 = ui_ms();
    u->hints++;
    ui_sound(SND_TAP);
    ui_animate();
    return 1;
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
/* a column in a picture: `down` backs, then the cards (fractions of size: its first card's centre, the card's width,
 * the steps) */
static inline void solui_paint_column(uint8_t *rgba, int size, int deck, float x, float y, float w, int down,
                                      const int *cards, int n, float dd, float du)
{
    for (int i = 0; i < down + n; i++)
        cards_paint_at(rgba, size, deck, i < down ? CARD_NONE : cards[i - down], x * size,
                    (y + (i < down ? i : down) * dd + (i > down ? i - down : 0) * du) * size, (int)(w * size));
}

/* the token: two cards, one over the other */
static inline void solui_token(int size, uint8_t *rgba, int under, int over)
{
    memset(rgba, 0, (size_t)size * size * 4);
    int w = (int)(size * 0.5f);
    cards_paint_at(rgba, size, DECK_FRENCH, under, size * 0.4f, size * 0.42f, w);
    cards_paint_at(rgba, size, DECK_FRENCH, over, size * 0.6f, size * 0.58f, w);
}

#endif
