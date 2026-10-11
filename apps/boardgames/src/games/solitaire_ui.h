/*
 * The patience games (Klondike, FreeCell, Spider) on the screen: what they share, on top of the card kit. A game sets
 * where its piles go (solui.at[]), and the rest is here: the piles drawn (columns fanned down, the waste fanned
 * right, the stock and the foundations as piles), the cards sliding and turning over after each move (a move and
 * what the game then did by itself, chained), the finger (a tap sends a card or a run to its best place, a quick
 * second tap only to a foundation; a drag carries a run where the finger drops it), the hint (the move blinks), the
 * bar (the deal's number, the move counter, the hint button) and the win: the cards leaping off the foundations and
 * bouncing across the table, each with a trail of copies.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#ifndef SOLITAIRE_UI_H
#define SOLITAIRE_UI_H

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "games/solitaire.h"
#include "shell.h"

#define SOLUI_SLIDES 192
#define SOLUI_ASPECT 1.4f
#define SOLUI_FLIP_MS 220
#define SOLUI_HINT_MS 2400
#define SOLUI_TRAIL 4                           /* the ghosts a leaping card leaves behind it */
#define SOLUI_TRAIL_MS 60
#define SOLUI_LIFE_MS 2600                      /* a leaping card's longest flight */

/* how a pile lies: stacked (its top card shows), a column fanned down, the waste (its last cards fanned right),
 * Spider's stock (a back for each row still to deal, from the right) */
enum { SF_PILE, SF_COLUMN, SF_WASTE, SF_ROW };

typedef struct {
    float x, y;                                 /* its first card's centre */
    float room;                                 /* a column: how far down its last card's centre may go */
    int fan, first;                             /* first: the pile shows its bottom card (Spider's foundations) */
} solspot;

typedef struct {
    const solrules *R;
    solrec *rec;
    int deck, waste_fan;                        /* waste_fan: the waste's cards fanned (Klondike's draw) */
    float w, down_step, up_step;                /* the card's width; the columns' steps (parts of w) */
    box b;
    solspot at[SOL_PILES];
    struct { cards_slide s; int pile, idx; Uint32 vis; } mv[SOLUI_SLIDES];
    int nmv;
    Uint32 quiet;                               /* busy until then */
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
    box bar;
    int made_bar;
    SDL_Texture *t_disc, *t_bulb, *t_back;
} solui;

/* ---- where the cards lie ----------------------------------------------------------------------------------------- */
/* a column's steps: as set, closer when it would reach past its room (the face-up ones first, then the others) */
static inline void solui_steps(const solui *u, const solpos *p, int pile, float *dd, float *du)
{
    float w = u->w, d = u->down_step * w, s = u->up_step * w, room = u->at[pile].room;
    int n = p->n[pile], nd = p->down[pile] < n - 1 ? p->down[pile] : n - 1, nu = n - 1 - nd;
    if (nd < 0) nd = 0;
    if (nu < 0) nu = 0;
    if (nd * d + nu * s > room && nu) s = fmaxf(0.21f * w, (room - nd * d) / nu);
    if (nd * d + nu * s > room && nd) d = fmaxf(0.045f * w, (room - nu * s) / nd);
    if (nd * d + nu * s > room && nu) s = fmaxf(0.02f * w, (room - nd * d) / nu);
    *dd = d;
    *du = s;
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
    }
}

/* ---- the slides -------------------------------------------------------------------------------------------------- */
static inline void solui_calm(solui *u)
{
    u->nmv = 0;
    u->quiet = 0;
    u->hint = 0;
    u->drag = 0;
    u->pile = -1;
}

/* a card from A to B, to card idx of pile in the end (-1: it moves on), drawn only from vis (ms; 0: at once) */
static inline int solui_slide(solui *u, int card, int up0, float x0, float y0, int up1, float x1, float y1, int ms,
                              int delay, int pile, int idx, Uint32 vis)
{
    if (u->nmv >= SOLUI_SLIDES) return -1;
    int k = u->nmv++;
    cards_slide_start(&u->mv[k].s, u->deck, card, x0, y0, u->w, 0, up0, x1, y1, u->w, 0, up1, ms, delay);
    u->mv[k].pile = pile;
    u->mv[k].idx = idx;
    u->mv[k].vis = vis ? u->mv[k].s.t0 + vis : 0;
    Uint32 end = u->mv[k].s.t0 + (Uint32)(delay + ms + 90);
    if (end > u->quiet) u->quiet = end;
    return k;
}

static inline int solui_hidden(const solui *u, int pile, int i)
{
    if (u->drag && pile == u->pile && i >= u->idx) return 1;
    for (int k = 0; k < u->nmv; k++)
        if (u->mv[k].pile == pile && u->mv[k].idx == i && cards_slide_busy(&u->mv[k].s)) return 1;
    return 0;
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
    if (was >= 0) u->mv[was].pile = -1;         /* it moves on: nothing to hide where it had gone */
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
                solui_carry(u, &cur, &nxt, a, cur.n[a] - n + k, b, nxt.n[b] - n + k, 1, 1,
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
        }
        cur = nxt;
    }
}

/* ---- the win: the cards leap off the foundations ----------------------------------------------------------------- */
static inline int solui_found_n(const solui *u, int *list)   /* the foundations, in order */
{
    const solpos *p = &u->rec->pos;
    int k = 0;
    for (int i = 0; i < p->npiles; i++)
        if (p->kind[i] == SP_FOUND) list[k++] = i;
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

/* card j's flight, t ms after it left (x0, y0): thrown sideways, falling, bouncing on the table's bottom edge.
 * Where it is (xs[0], ys[0]) and where it was k x SOLUI_TRAIL_MS before (xs[k]; NAN: not yet thrown then). */
static inline void solui_flight(const solui *u, int j, float x0, float y0, float t, float *xs, float *ys)
{
    box b = u->b;
    uint32_t h = (uint32_t)j * 2654435761u + 0x9e37u;
    h ^= h >> 15;
    h *= 2246822519u;
    h ^= h >> 13;
    float r1 = (h & 1023) / 1023.0f, r2 = ((h >> 10) & 1023) / 1023.0f;
    float dir = x0 > b.x + b.w / 2 ? -1 : 1;
    if ((h >> 20) % 3 == 0) dir = -dir;         /* a third of them the other way */
    float vx = dir * (0.55f + 0.35f * r1) * b.w, vy = -(0.15f + 0.65f * r2) * b.h, g = 2.4f * b.h;
    float floor_y = b.y + b.h - 0.7f * u->w, px = x0, py = y0, dt = 1 / 200.0f;
    int at[SOLUI_TRAIL + 1], k = SOLUI_TRAIL;
    for (int i = 0; i <= SOLUI_TRAIL; i++) {
        float ti = t - i * SOLUI_TRAIL_MS;
        at[i] = ti < 0 ? -1 : (int)(ti / 1000 / dt);
        xs[i] = ys[i] = NAN;
    }
    while (k >= 0 && at[k] < 0) k--;
    for (int s = 0; k >= 0; s++) {
        while (k >= 0 && at[k] == s) xs[k] = px, ys[k] = py, k--;
        vy += g * dt;
        px += vx * dt;
        py += vy * dt;
        if (py > floor_y && vy > 0) { py = floor_y; vy = -vy * 0.74f; }
    }
}

static inline void solui_draw_leaps(const solui *u)
{
    const solpos *p = &u->rec->pos;
    int fl[SOL_PILES], nf = solui_found_n(u, fl), total = sol_on_found(p), gap = solui_leap_gap(u);
    Uint32 now = ui_ms();
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
        float t = (float)(now - u->win_t0 - (Uint32)(j * gap)), xs[SOLUI_TRAIL + 1], ys[SOLUI_TRAIL + 1];
        if (i < 0 || i >= p->n[pile] || t > SOLUI_LIFE_MS) continue;
        const solspot *s = &u->at[pile];
        solui_flight(u, j, s->x, s->y, t, xs, ys);
        for (int k = SOLUI_TRAIL; k >= 0; k--) {   /* the trail: its ghosts where it was (faint outlines); the card */
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

/* a new deal: the tableau's cards fly from (x, y) to their places, row by row, the top ones turning up */
static inline void solui_deal(solui *u, float x, float y, int gap)
{
    const solpos *p = &u->rec->pos;
    int k = 0, more = 1;
    for (int r = 0; more; r++) {
        more = 0;
        for (int c = 0; c < p->npiles; c++) {
            if (p->kind[c] != SP_TAB || r >= p->n[c]) continue;
            float cx, cy;
            solui_xy(u, p, c, r, &cx, &cy);
            int up = r >= p->down[c];
            solui_slide(u, p->c[c][r], 0, x, y, up, cx, cy, 300, k * gap, c, r, 0);
            if (k % 3 == 0) ui_sound_later(SND_MOVE, k * gap + 260);
            k++;
            more = 1;
        }
    }
    for (int i = 0; i < u->nmv; i++) u->mv[i].vis = u->mv[i].s.t0 + (Uint32)u->mv[i].s.delay;
}

static inline int solui_busy(const solui *u)
{
    for (int k = 0; k < u->nmv; k++)
        if (cards_slide_busy(&u->mv[k].s)) return 1;
    return (u->quiet && ui_ms() < u->quiet) || solui_leaping(u);
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

static inline void solui_draw_pile(const solui *u, int pile)
{
    const solpos *p = &u->rec->pos;
    const solspot *s = &u->at[pile];
    int n = p->n[pile], deck = u->deck;
    float x, y, w = u->w;
    switch (s->fan) {
    case SF_COLUMN:
        if (!n || solui_hidden(u, pile, 0)) cards_place(deck, s->x, s->y, w);
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
        int top = !k || p->kind[pile] == SP_STOCK ? CARD_NONE : p->c[pile][s->first ? 0 : k - 1];
        int glow = solui_glows(u, pile, n - 1) || (!n && solui_glows(u, pile, -1));
        if (!k && glow) ui_mark_gold(s->x, s->y, 1.3f * w);
        cards_pile(deck, s->x, s->y, w, k, top, glow ? CD_GLOW : 0);
        break;
    }
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
    for (int k = 0; k < u->nmv; k++) {
        if (!u->mv[k].s.on) continue;
        if (u->mv[k].vis && now < u->mv[k].vis) { ui_animate(); continue; }
        cards_slide_draw(&u->mv[k].s);
    }
    if (u->won && !leaping && now < u->win_t0) ui_animate();   /* the leaps are coming */
    if (leaping) solui_draw_leaps(u);
    if (u->drag) {
        float du = p->kind[u->pile] == SP_TAB ? 0 : 0.3f * u->w, dd;
        if (p->kind[u->pile] == SP_TAB) solui_steps(u, p, u->pile, &dd, &du);
        for (int i = u->idx; i < p->n[u->pile]; i++)
            cards_draw(u->deck, p->c[u->pile][i], u->fx - u->gx, u->fy - u->gy + (i - u->idx) * du, u->w, 0,
                       i == u->idx ? CD_RAISED : 0);
    }
}

/* ---- the finger -------------------------------------------------------------------------------------------------- */
/* the pile under (x, y), and its card (-1: an empty place); -1: none */
static inline int solui_hit(const solui *u, float x, float y, int *idx)
{
    const solpos *p = &u->rec->pos;
    float w = u->w, h = SOLUI_ASPECT * w, cx, cy;
    for (int pile = 0; pile < p->npiles; pile++) {
        const solspot *s = &u->at[pile];
        int n = p->n[pile];
        if (s->fan == SF_COLUMN) {
            for (int i = n - 1; i >= 0; i--) {
                solui_xy(u, p, pile, i, &cx, &cy);
                float extra = i == n - 1 ? 0.35f * h : 0;      /* a little below the last card too */
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
    solui_animate(u, &before, first, drag);
    solui_check_win(u, u->quiet);
    ui_handoff(0);
    return 1;
}

static inline void solui_press(solui *u, float x, float y)
{
    u->hint = 0;
    u->drag = 0;
    u->pile = -1;
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
        if (u->nmv) {                           /* cards still moving: they land at once */
            u->nmv = 0;
            u->quiet = 0;
        }
        u->drag = 1;
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
        if (b == u->pile || (p->kind[b] != SP_TAB && p->kind[b] != SP_FOUND && p->kind[b] != SP_CELL)) continue;
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

static inline void solui_release(solui *u, float x, float y)
{
    int pile = u->pile, idx = u->idx;
    u->pile = -1;
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
        if (twice && m.kind == SM_NONE) return;
        if (m.kind == SM_NONE || !solui_play(u, m, NULL)) solui_deny();
        return;
    }
    if (idx < 0) return;                        /* an empty place */
    solmove m = u->R->tap(p, pile, idx, twice);
    if (m.kind == SM_NONE) { if (!twice) solui_deny(); return; }
    if (!solui_play(u, m, NULL)) solui_deny();
}

/* ---- the bar: the deal's number (FreeCell), the moves, the hint button ------------------------------------------ */
static inline void solui_bulb(int size, uint8_t *o)   /* the hint's icon: a lamp, lit */
{
    art_style st = {{255, 255, 255}, {0, 0, 0}, {0, 0, 0}, {255, 255, 255}, 0, 0};
    art_fig *f = art_fig_new();
    art_ellipse(f, ART_PART, 0.5f, 0.44f, 0.2f, 0.2f);
    const float neck[] = {0.36f, 0.56f, 0.64f, 0.56f, 0.6f, 0.69f, 0.4f, 0.69f};
    art_poly(f, ART_PART, neck, 4);
    art_rect(f, ART_PART, 0.4f, 0.71f, 0.6f, 0.75f);
    art_rect(f, ART_PART, 0.42f, 0.77f, 0.58f, 0.81f);
    art_rect(f, ART_PART, 0.46f, 0.83f, 0.54f, 0.86f);
    for (int i = -2; i <= 2; i++) {             /* the light: short strokes round its top */
        float a = (90 + 38 * i) * 3.14159265f / 180, c = cosf(a), s = -sinf(a), r0 = 0.27f, r1 = 0.36f, t = 0.024f;
        float px = -s * t, py = c * t, x0 = 0.5f + c * r0, y0 = 0.44f + s * r0, x1 = 0.5f + c * r1, y1 = 0.44f + s * r1;
        const float ray[] = {x0 + px, y0 + py, x1 + px, y1 + py, x1 - px, y1 - py, x0 - px, y0 - py};
        art_poly(f, ART_PART, ray, 4);
    }
    art_render(f, &st, size, size, (float)size, 0, 0, o);
    art_fig_free(f);
}

static inline void solui_bar_textures(solui *u)
{
    int key = (int)ui.unit * 8 + u->deck;
    if (u->made_bar == key) return;
    u->made_bar = key;
    static const uint8_t house[3] = {54, 72, 64};
    int d = (int)(0.84f * ui.unit), ic = (int)(0.56f * ui.unit), cw = (int)(0.36f * ui.unit);
    int ch = (int)(cw * SOLUI_ASPECT);
    ui_free(&u->t_disc);
    art_disc(d, house, 1, 0, ui_buf(d, d));
    u->t_disc = ui_tex(d, d, ui_buf(d, d));
    ui_free(&u->t_bulb);
    solui_bulb(ic, ui_buf(ic, ic));
    u->t_bulb = ui_tex(ic, ic, ui_buf(ic, ic));
    SDL_SetTextureColorMod(u->t_bulb, 252, 236, 170);
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

static inline void solui_hint_at(const solui *u, float *x, float *y, float *r)
{
    *r = 0.42f * ui.unit;
    *x = u->bar.x + u->bar.w - *r - 0.05f * ui.unit;
    *y = u->bar.y + u->bar.h / 2;
}

/* deal > 0: its number after a small card back; then the move counter; the hint button at the right end */
static inline void solui_bar(solui *u, box b, int deal)
{
    solui_bar_textures(u);
    u->bar = b;
    float cy = b.y + b.h / 2, nh = 0.46f * ui.unit, x = b.x;
    if (deal > 0) {
        float cw = 0.36f * ui.unit;
        ui_put_c(u->t_back, x + cw / 2, cy, cw, cw * SOLUI_ASPECT);
        x += cw + 0.14f * ui.unit;
        float dw = solui_digits_w(deal, 0.4f * ui.unit);
        ui_number(x + dw / 2, cy, 0.4f * ui.unit, deal, 1);
        x += dw + 0.45f * ui.unit;
    }
    float mw = solui_digits_w(u->rec->moves, nh);
    ui_number(x + mw / 2, cy, nh, u->rec->moves, 1);
    if (u->won) return;
    float hx, hy, r;
    solui_hint_at(u, &hx, &hy, &r);
    int lit = u->hint && (ui_ms() - u->hint_t0) / 300 % 2 == 0;
    if (lit) ui_mark_gold(hx, hy, 3.0f * r);
    ui_put_c(u->t_disc, hx, hy, 2 * r, 2 * r);
    ui_put_c(u->t_bulb, hx, hy, 1.32f * r, 1.32f * r);
}

static inline int solui_bar_press(solui *u, float x, float y)
{
    if (!u->bar.w || u->won) return 0;
    float hx, hy, r;
    solui_hint_at(u, &hx, &hy, &r);
    if (hypotf(x - hx, y - hy) > 1.15f * r) return 0;
    solmove m = u->R->hint(&u->rec->pos);
    if (m.kind == SM_NONE) { ui_sound(SND_DENY); return 1; }
    u->hm = m;
    u->hint = 1;
    u->hint_t0 = ui_ms();
    ui_sound(SND_TAP);
    ui_animate();
    return 1;
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
/* a card's picture w wide, centred on (cx, cy), painted over a picture of size x size (CARD_NONE: its back) */
static inline void solui_paint(uint8_t *rgba, int size, int deck, int card, float cx, float cy, int w)
{
    int h = (int)(w * cards_aspect(deck) + 0.5f);
    uint8_t *c = malloc((size_t)w * h * 4);
    if (!c) return;
    cards_paint(deck, card, w, h, c);
    art_blend(rgba, size, size, c, w, h, (int)(cx - w / 2.0f), (int)(cy - h / 2.0f));
    free(c);
}

/* a column in a picture: `down` backs, then the cards (fractions of size: its first card's centre, the card's width,
 * the steps) */
static inline void solui_paint_column(uint8_t *rgba, int size, int deck, float x, float y, float w, int down,
                                      const int *cards, int n, float dd, float du)
{
    for (int i = 0; i < down + n; i++)
        solui_paint(rgba, size, deck, i < down ? CARD_NONE : cards[i - down], x * size,
                    (y + (i < down ? i : down) * dd + (i > down ? i - down : 0) * du) * size, (int)(w * size));
}

/* the token: two cards, one over the other */
static inline void solui_token(int size, uint8_t *rgba, int under, int over)
{
    memset(rgba, 0, (size_t)size * size * 4);
    int w = (int)(size * 0.5f);
    solui_paint(rgba, size, DECK_FRENCH, under, size * 0.4f, size * 0.42f, w);
    solui_paint(rgba, size, DECK_FRENCH, over, size * 0.6f, size * 0.58f, w);
}

#endif
