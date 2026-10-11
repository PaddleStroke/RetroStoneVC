/*
 * Backgammon in RetroStone Board: the board (24 points inlaid in the plank, the bar a fold of the walnut, the tray
 * for the checkers borne off), the dice thrown with a tap, the checkers moved one die at a time by tap or drag. The
 * rules: backgammon_rules.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The board lies across the square: player 0's home board at the bottom right, player 1's at the top right (flipped:
 * the other way up), the tray on the right. The dice land in the middle of the board, on the right of the player who
 * threw them (the bottom player's on the right half, the top player's on the left), the opening roll on the bar.
 * The moves' notation (script): backgammon.h ("31: 8/5 6/5 64: 24/18 13/9").
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/backgammon.h"
#include "shell.h"

#define FRAME 0.64f                             /* the frame and half the bar, in points' widths */
#define TRAY 1.25f                              /* the tray's width */
#define POINT 5.5f                              /* a point's length (the plank is 14 high: a gap of 3 between) */
#define ANIM_MS 200

/* the board's geometry in pixels: w a point's width; the picture's top left at (x, y) */
typedef struct { int w, fr, tw; float x, y; int flip; } geo;

static bgame G;
static bmove legal[BG_MAXMOVES];
static int nlegal;
static geo S;                                   /* on the screen */
static int made = -1, pic_w, pic_h, cs;
static SDL_Texture *t_board, *t_chk[2];
static int sel = -1;                            /* the place (the mover's own numbering) of the checker in hand */
static int shown = -1;                          /* the throw (event) the dice show */
static int pass_snd;                            /* a throw with no move: the sound once the dice lie */
static struct { int on, moved; float x, y; } drag;
static struct { int on, player; bmove m; Uint32 t0; } anim;
static const int IDLE[2] = {5, 2};              /* the faces of the dice waiting to be thrown */

static int fr_of(int w) { return (int)(w * FRAME); }
static int tray_of(int w) { return (int)(w * TRAY); }
static int width_of(int w) { return 12 * w + 5 * fr_of(w) + tray_of(w); }
static geo geo_of(int w, float x, float y, int flip) { return (geo){w, fr_of(w), tray_of(w), x, y, flip}; }

/* ---- the position ------------------------------------------------------------------------------------------------ */
static int movable(int from)
{
    for (int i = 0; i < nlegal; i++)
        if (legal[i].from == from) return 1;
    return 0;
}

static void refresh(void)
{
    const bpos *p = bg_pos(&G);
    nlegal = bg_legal(p, legal);
    sel = nlegal && p->n[p->side][BG_BAR] ? BG_BAR : -1;    /* on the bar: those first, in hand already */
    drag.on = 0;
}

static int last_throw(void)
{
    int k = G.n - 1;
    while (k >= 0 && G.ev[k].kind != BG_EV_THROW) k--;
    return k;
}

/* the shell's dice show throw k, tumbling or at rest, in the thrower's colour (the opening: ivory's die, ebony's) */
static void show_throw(int k, int tumble)
{
    const bpos *q = &G.pos[k + 1];
    int v[4], who[4];
    for (int i = 0; i < q->nd; i++) {
        v[i] = q->die[i];
        who[i] = G.ev[k].player >= 0 ? G.ev[k].player : i;
    }
    (tumble ? ui_dice_throw_each : ui_dice_show_each)(q->nd, v, who);
    shown = k;
}

/* after undo or a script: the dice show the last throw (undo brings an earlier one back, never a new one) */
static void sync(void)
{
    int k = last_throw();
    if (k < 0) shown = -1;
    else if (k != shown) show_throw(k, 0);
}

static void start(int variant)
{
    (void)variant;
    bg_new(&G);
    shown = -1;
    anim.on = pass_snd = 0;
    refresh();
}

static int status(void)
{
    static const int map[] = {[BG_PLAY] = GS_PLAY, [BG_WIN0] = GS_WIN0, [BG_WIN1] = GS_WIN1};
    return map[bg_status(&G)];
}

static int turn(void) { return bg_pos(&G)->side; }

static void throw_dice(void)
{
    int a = 1 + ui_random(6), b = 1 + ui_random(6);   /* the opening: ivory's die, ebony's */
    if (!bg_throw(&G, a, b)) return;
    show_throw(last_throw(), 1);
    pass_snd = bg_pos(&G)->passed >= 0;
    anim.on = 0;
    refresh();
}

static void play(bmove m, int animate)
{
    int side = bg_pos(&G)->side;
    if (!bg_play(&G, m)) return;
    anim.on = animate;
    anim.player = side;
    anim.m = m;
    anim.t0 = ui_ms();
    ui_sound(m.hit ? SND_CAPTURE : m.to == BG_OFF ? SND_PLACE : SND_MOVE);
    refresh();
    const bpos *p = bg_pos(&G);                 /* the checker stays in hand while it can go on */
    if (sel < 0 && p->phase == BG_MOVE && p->side == side && m.to != BG_OFF && movable(m.to)) sel = m.to;
}

static int undo(void)
{
    if (!bg_undo(&G)) return 0;
    anim.on = pass_snd = 0;
    sync();
    refresh();
    return 1;
}

static int script(const char *s)
{
    int ok = bg_moves(&G, s);
    anim.on = pass_snd = 0;
    sync();
    refresh();
    return ok;
}

/* ---- where things are -------------------------------------------------------------------------------------------- */
static int on_top(const geo *g, int player) { return player ^ g->flip; }

/* the middle of a column of points, s 0..11 from the left (6..11 past the bar) */
static float column_x(const geo *g, int s)
{
    return g->x + g->fr + s * g->w + (s >= 6 ? 2 * g->fr : 0) + g->w / 2.0f;
}

/* checker k (0: against the frame) of n on a player's place (their own numbering: 1..24, the bar, off) */
static void spot(const geo *g, int player, int place, int k, int n, float *cx, float *cy)
{
    float w = (float)g->w, top = g->y + g->fr, bottom = top + 14 * w, mid = top + 7 * w, step = w;
    if (place == BG_BAR) {                      /* on the bar, from the middle towards the player's side */
        if (n > 6) step = 5.4f * w / (n - 1);
        *cx = g->x + g->fr + 6 * w + g->fr;
        *cy = mid + (on_top(g, player) ? -1 : 1) * (0.6f * w + k * step);
        return;
    }
    if (place == BG_OFF) {                      /* in the tray, from the player's end */
        step = (7 * w - g->fr / 4.0f - w) / 14;
        *cx = g->x + g->fr + 12 * w + 3 * g->fr + g->tw / 2.0f;
        *cy = on_top(g, player) ? top + w / 2 + k * step : bottom - w / 2 - k * step;
        return;
    }
    int a = player ? 25 - place : place;        /* player 0's numbering: 1..6 bottom right, 13..18 top left */
    int up = (a >= 13) ^ g->flip, s = a >= 13 ? a - 13 : 12 - a;
    if (n > 5) step = 4 * w / (n - 1);
    *cx = column_x(g, s);
    *cy = up ? top + w / 2 + k * step : bottom - w / 2 - k * step;
}

/* where the dice of a player lie (-1: the opening roll, on the bar) */
static void dice_spot(int who, float *cx, float *cy)
{
    float w = (float)S.w;
    *cy = S.y + S.fr + 7 * w;
    *cx = who < 0 ? S.x + 6 * w + 2 * S.fr : on_top(&S, who) ? column_x(&S, 3) - w / 2 : column_x(&S, 9) - w / 2;
}

/* the place under the pointer, in the mover's numbering (-1: none) */
static int place_at(float x, float y)
{
    float w = (float)S.w, x0 = S.x + S.fr, y0 = S.y + S.fr, bar = x0 + 6 * w, right = bar + 2 * S.fr;
    if (y < y0 - S.fr || y >= y0 + 14 * w + S.fr) return -1;
    if (x >= bar && x < right) return BG_BAR;
    if (x >= right + 6 * w && x < right + 6 * w + S.fr + S.tw + S.fr) return BG_OFF;
    int s;
    if (x >= x0 && x < bar) s = (int)((x - x0) / w);
    else if (x >= right && x < right + 6 * w) s = 6 + (int)((x - right) / w);
    else return -1;
    int a = (y < y0 + 7 * w) ^ S.flip ? 13 + s : 12 - s;
    return bg_pos(&G)->side ? 25 - a : a;
}

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
static uint8_t *px(uint8_t *b, int w, int x, int y) { return b + 4 * ((size_t)y * w + x); }

/* the board for points w pixels wide: the house plank and frame cut to the board's width (under the bar), the bar
 * and the strip before the tray made of the frame's walnut, the tray sunk, the points inlaid in two woods */
static uint8_t *paint(int w, int *pw, int *ph)
{
    int fr = fr_of(w), tw = tray_of(w), wa = 16 * w + 2 * fr, h = 14 * w + 2 * fr, bw = width_of(w);
    uint8_t *a = malloc((size_t)wa * h * 4), *o = malloc((size_t)bw * h * 4);
    if (!a || !o || fr < 2) { free(a); free(o); return NULL; }
    art_board_n(8, 7, 2 * w, fr, 0, a);
    int bar = fr + 6 * w, strip = bar + 2 * fr + 6 * w, tray = strip + fr, half = fr / 2, mid = fr + 7 * w;
    int q = fr / 4 > 0 ? fr / 4 : 1;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < bw; x++) memcpy(px(o, bw, x, y), px(a, wa, x < bar + fr ? x : x + wa - bw, y), 4);
    for (int y = fr; y < fr + 14 * w; y++) {
        for (int j = 0; j < 2 * fr; j++)        /* the bar: the frame's two sides back to back, a fold */
            memcpy(px(o, bw, bar + j, y), px(a, wa, j < fr ? wa - fr + j : j - fr, y), 4);
        for (int j = 0; j < fr; j++)            /* the same, narrower, before the tray */
            memcpy(px(o, bw, strip + j, y),
                   px(a, wa, j < half ? wa - fr + j * fr / half : (j - half) * fr / (fr - half), y), 4);
        for (int x = tray; x < tray + tw; x++) {
            uint8_t *p = px(o, bw, x, y);
            if (abs(y - mid) < q) {             /* walnut between the two players' ends */
                int j = y - mid + q, sy = j < q ? h - fr + j * fr / q : (j - q) * fr / q;
                memcpy(p, px(a, wa, x + wa - bw, sy), 4);
                continue;
            }
            float k = 0.64f + 0.14f * fminf(1, (x - tray) / (0.35f * tw));   /* sunk, in the shade of its side */
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)(p[i] * k);
        }
    }
    static const float tone[2][3] = {{1.10f, 1.22f, 1.45f}, {0.60f, 0.42f, 0.36f}};   /* maple, rosewood */
    int len = (int)(POINT * w);
    float m = 0.05f * w, lw = fmaxf(1, w / 30.0f);
    for (int s = 0; s < 12; s++) {
        int x0 = fr + s * w + (s >= 6 ? 2 * fr : 0);
        float cx = x0 + w / 2.0f;
        for (int up = 0; up < 2; up++) {
            const float *c = tone[(s + up) & 1];
            for (int i = 0; i < len; i++) {
                int y = up ? fr + i : fr + 14 * w - 1 - i;
                float hw = (1 - (i + 0.5f) / len) * (w / 2.0f - m);
                for (int x = x0; x < x0 + w; x++) {
                    float d = hw - fabsf(x + 0.5f - cx), cov = clampf(d + 0.5f, 0, 1);
                    if (cov <= 0) continue;
                    float edge = 1 - 0.3f * clampf(1 - d / lw, 0, 1);   /* a darker line along its sides */
                    uint8_t *p = px(o, bw, x, y);
                    for (int k = 0; k < 3; k++) p[k] = (uint8_t)clampf(p[k] * (1 - cov + c[k] * edge * cov), 0, 255);
                }
            }
        }
    }
    free(a);
    *pw = bw;
    *ph = h;
    return o;
}

static void layout(box b)
{
    int w = (int)floorf(b.w / 16.45f);
    while (w > 4 && width_of(w) > b.w) w--;
    if (w < 4) w = 4;
    if (made != w) {
        made = w;
        int bw, bh;
        uint8_t *pic = paint(w, &bw, &bh);
        ui_free(&t_board);
        if (pic) t_board = ui_tex(bw, bh, pic);
        free(pic);
        pic_w = bw;
        pic_h = bh;
        cs = (int)(1.12f * w);                  /* the disc fills 0.88 of its picture: a point's width */
        for (int c = 0; c < 2; c++) {
            ui_free(&t_chk[c]);
            art_counter(COUNTER_DISC, c, cs, ui_buf(cs, cs));
            t_chk[c] = ui_tex(cs, cs, ui_buf(cs, cs));
        }
    }
    S = geo_of(w, floorf(b.x + (b.w - pic_w) / 2), floorf(b.y + (b.h - pic_h) / 2), ui.flipped);
    if (shown >= 0 && !ui_dice_busy()) show_throw(shown, 0);   /* the shell's dice ours again (after another game) */
}

static void checker(int player, float cx, float cy, float k)
{
    float d = cs * k;
    ui_shadow_flat(cx + 0.3f * (k - 1) * cs, cy + 0.6f * (k - 1) * cs, d, d, 1);
    ui_put_c(t_chk[player], cx, cy, d, d);
}

/* a player's checkers on a place; skip: the top ones not drawn (moving); the count on a high stack */
static void stack(int player, int place, int skip)
{
    int n = bg_pos(&G)->n[player][place];
    float x, y;
    for (int k = 0; k < n - skip; k++) {
        spot(&S, player, place, k, n, &x, &y);
        checker(player, x, y, 1);
    }
    if (place != BG_OFF && n - skip > (place == BG_BAR ? 6 : 5)) {
        spot(&S, player, place, n - skip - 1, n, &x, &y);
        ui_number_rot(x, y, 0.42f * S.w, n - skip, player, ui_upside(player) ? 180 : 0);
    }
}

/* the moves of the latest turn that moved: where each checker left from and went */
static void mark_last(void)
{
    int k = G.n - 1;
    while (k >= 0 && G.ev[k].kind != BG_EV_MOVE) k--;
    const bpos *p = bg_pos(&G);
    for (; k >= 0 && G.ev[k].kind == BG_EV_MOVE; k--) {
        int pl = G.ev[k].player, from = G.ev[k].m.from, to = G.ev[k].m.to, nf = p->n[pl][from], nt = p->n[pl][to];
        float x, y, w = (float)S.w;
        spot(&S, pl, from, nf, nf + 1, &x, &y);
        ui_mark_last(x - w / 2, y - w / 2, w, w);
        spot(&S, pl, to, nt ? nt - 1 : 0, nt, &x, &y);
        ui_mark_last(x - w / 2, y - w / 2, w, w);
    }
}

static void dice(void)
{
    const bpos *p = bg_pos(&G);
    float x, y, w = (float)S.w, ds = 0.86f * w;
    int k = last_throw();
    if (shown >= 0 && shown == k) {             /* the last throw; dimmed once played, red when it could not be */
        int who = G.ev[k].player;
        unsigned used = p->phase == BG_OPENING ? 0 : p->phase == BG_MOVE ? p->used : 0xf;
        dice_spot(who, &x, &y);
        if (G.pos[k + 1].passed >= 0 && !ui_dice_busy()) ui_mark_glow(x, y, 3.4f * w);
        if (p->phase == BG_OPENING) ui_mark_gold(x, y, 3.4f * w);
        ui_dice_draw(x, y, ds, used);
    }
    if (pass_snd && !ui_dice_busy()) {
        pass_snd = 0;
        ui_sound(SND_DENY);
    }
    if (p->phase != BG_THROW && !(p->phase == BG_OPENING && shown < 0)) return;
    int opening = p->phase == BG_OPENING;       /* the dice to throw: a die each for the opening, else the mover's */
    dice_spot(opening ? -1 : p->side, &x, &y);
    ui_mark_gold(x, y, 3.4f * w);
    for (int i = 0; i < 2; i++)
        ui_die_at(x + (i ? 0.65f : -0.65f) * w, y, ds, opening ? i : p->side, IDLE[opening ? 0 : i], 0.8f);
}

static void draw(void)
{
    const bpos *p = bg_pos(&G);
    float x, y, w = (float)S.w;
    ui_put(t_board, S.x, S.y, (float)pic_w, (float)pic_h);
    mark_last();
    int rolling = ui_dice_busy(), mover = p->side, in_hand = sel >= 0 && !rolling;
    dice();
    float t = 1;
    if (anim.on) {
        t = (ui_ms() - anim.t0) / (float)ANIM_MS;
        if (t >= 1) t = 1, anim.on = 0;
        else ui_animate();
        t = 1 - (1 - t) * (1 - t) * (1 - t);
    }
    for (int pl = 0; pl < 2; pl++)
        for (int place = 0; place <= BG_BAR; place++) {
            int skip = 0;
            if (anim.on && pl == anim.player && place == anim.m.to) skip = 1;
            if (anim.on && anim.m.hit && pl != anim.player && place == BG_BAR) skip = 1;
            if (drag.on && drag.moved && pl == mover && place == sel) skip = 1;
            stack(pl, place, skip);
        }
    if (in_hand && !drag.moved) {               /* the checker in hand, and where it can go: a ring on a blot to hit */
        int n = p->n[mover][sel];
        spot(&S, mover, sel, n - 1, n, &x, &y);
        ui_mark_select(x - w / 2, y - w / 2, w, w);
        for (int i = 0; i < nlegal; i++) {
            if (legal[i].from != sel) continue;
            int to = legal[i].to, there = p->n[mover][to];
            if (legal[i].hit) {
                spot(&S, !mover, 25 - to, 0, 1, &x, &y);
                ui_mark_ring(x, y, 1.5f * w);
            } else {
                spot(&S, mover, to, there, there + 1, &x, &y);
                ui_mark_dot(x, y, 0.55f * w);
            }
        }
    }
    if (anim.on) {
        bmove m = anim.m;
        int pl = anim.player, nf = p->n[pl][m.from], nt = p->n[pl][m.to];
        float x0, y0, x1, y1;
        if (m.hit) {                            /* the blot goes to the bar */
            int nb = p->n[!pl][BG_BAR];
            spot(&S, !pl, 25 - m.to, 0, 1, &x0, &y0);
            spot(&S, !pl, BG_BAR, nb - 1, nb, &x1, &y1);
            checker(!pl, x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, 1);
        }
        spot(&S, pl, m.from, nf, nf + 1, &x0, &y0);
        spot(&S, pl, m.to, nt - 1, nt, &x1, &y1);
        checker(pl, x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, 1 + 0.12f * sinf(t * 3.14159265f));
    }
    if (drag.on && drag.moved) checker(mover, drag.x, drag.y - 0.5f * w, 1.18f);
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    if (status() != GS_PLAY) return;
    const bpos *p = bg_pos(&G);
    if (p->phase != BG_MOVE) {                  /* a tap on the dice throws them */
        float cx, cy;
        dice_spot(p->phase == BG_OPENING ? -1 : p->side, &cx, &cy);
        if (ui_dice_busy()) return;
        if (fabsf(x - cx) < 2.2f * S.w && fabsf(y - cy) < 1.4f * S.w) throw_dice();
        else ui_sound(SND_DENY);
        return;
    }
    int at = place_at(x, y);
    bmove m;
    if (sel >= 0 && at >= 0 && at != sel && bg_find(p, sel, at, &m)) {
        play(m, 1);
        return;
    }
    if (at > 0 && movable(at) && !(p->n[p->side][BG_BAR] && at != BG_BAR)) {
        sel = at;
        drag.on = 1;
        drag.moved = 0;
        drag.x = x;
        drag.y = y;
        return;
    }
    ui_sound(SND_DENY);
    if (!p->n[p->side][BG_BAR]) sel = -1;
}

static void motion(float x, float y)
{
    if (!drag.on) return;
    if (hypotf(x - drag.x, y - drag.y) > S.w * 0.25f) drag.moved = 1;
    drag.x = x;
    drag.y = y;
    ui_animate();
}

static void release(float x, float y)
{
    if (!drag.on) return;
    drag.on = 0;
    if (!drag.moved) return;                    /* a tap: the checker stays in hand */
    drag.moved = 0;
    int at = place_at(x, y);
    bmove m;
    if (at >= 0 && at != sel && bg_find(bg_pos(&G), sel, at, &m)) play(m, 0);
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
/* the checkers the player bore off; at the end, the winner's points in a gold light (2 a gammon, 3 a backgammon) */
static void bar(int player, box b)
{
    const bpos *p = bg_pos(&G);
    float u = ui.unit, cy = b.y + b.h / 2;
    ui_number(b.x + 0.45f * u, cy, 0.46f * u, p->n[player][BG_OFF], 1);
    if (p->phase != BG_OVER || p->side != player) return;
    float x = b.x + b.w - 0.5f * u;
    ui_mark_gold(x, cy, 1.3f * u);
    ui_number(x, cy, 0.56f * u, bg_points(p), 1);
}

static void token(int player, int size, uint8_t *rgba) { art_counter(COUNTER_DISC, player, size, rgba); }

/* the menu's card: a corner of the board, from the bar and the middle: a home board with checkers, the tray, dice */
static void thumb(int variant, int size, uint8_t *rgba)
{
    (void)variant;
    int w = (int)(size / 10.2f), bw, bh, c = (int)(1.12f * w), d = (int)(0.86f * w);
    uint8_t *pic = paint(w, &bw, &bh), *one = malloc((size_t)c * c * 4);
    if (!pic || !one) { free(pic); free(one); return; }
    int fr = fr_of(w), x0 = fr + 6 * w, y0 = fr + 7 * w, cw = bw - x0, ch = bh - y0 + fr, ox = (size - cw) / 2;
    int oy = (size - ch) / 2;
    for (int y = 0; y < ch; y++) {              /* the frame's top, then the lower half of the board */
        int sy = y < fr ? y : y - fr + y0;
        memmove(pic + 4 * (size_t)y * cw, pic + 4 * ((size_t)sy * bw + x0), 4 * (size_t)cw);
    }
    art_blend(rgba, size, size, pic, cw, ch, ox, oy);
    free(pic);
    geo g = geo_of(w, (float)(ox - x0), (float)(oy + fr - y0), 0);
    static const int place[][3] = {{0, 6, 4}, {0, 4, 2}, {0, 5, 1}, {1, 22, 1}, {1, 24, 2}, {0, 0, 3}};
    for (unsigned i = 0; i < sizeof place / sizeof place[0]; i++)
        for (int k = 0; k < place[i][2]; k++) {
            float x, y;
            spot(&g, place[i][0], place[i][1], k, place[i][2], &x, &y);
            int ox = (int)(x - c / 2.0f), oy = (int)(y - c / 2.0f);
            art_flat_shadow(c, one);
            art_blend(rgba, size, size, one, c, c, ox + c / 20, oy + c * 8 / 100);
            art_counter(COUNTER_DISC, place[i][0], c, one);
            art_blend(rgba, size, size, one, c, c, ox, oy);
        }
    int dy = oy + fr + (int)(0.3f * w);
    art_die(5, 0, d, one);
    art_blend(rgba, size, size, one, d, d, (int)(column_x(&g, 8) - d / 2.0f), dy);
    art_die(3, 0, d, one);
    art_blend(rgba, size, size, one, d, d, (int)(column_x(&g, 9) + 0.2f * w), dy);
    free(one);
}

static int busy(void) { return anim.on || ui_dice_busy(); }   /* the end waits for the last checker */

static const game backgammon_game = {
    .id = "backgammon", .order = 130, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy,
};
REGISTER_GAME(backgammon_game)
