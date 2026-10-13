/*
 * Backgammon in RetroStone Board, and plakoto, fevga and long nardy on its board: the board (24 points inlaid in the
 * plank, the bar a fold of the walnut, the tray for the checkers borne off), the dice thrown with a tap, the checkers
 * moved one die at a time by tap or drag. The rules: backgammon_rules.c. Plakoto's pinned checker lies under its
 * pinner, who stands a little up the point on it.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The board lies across the room the shell gives: player 0's home board at the bottom right, player 1's at the top
 * right (flipped: the other way up). Where the room is wide (landscape) the tray is on the board's right, the dice
 * land in the middle of the board, on the right of the player who threw them (the bottom player's on the right half,
 * the top player's on the left), the opening roll on the bar; the board as tall as the room, its points as wide as
 * the width allows. Where it is narrow (portrait) the tray leaves the board for two trays beyond its long sides, each
 * player's next to them by their home board, and each player's dice lie beside their tray (by their outer board): the
 * 24 points and the bar take the whole width, the board grows taller (longer points, higher stacks) with the height
 * left. The opening roll stays on the bar.
 * The moves' notation (script): backgammon.h ("31: 8/5 6/5 64: 24/18 13/9").
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/backgammon.h"
#include "shell.h"

#define FRAME 0.64f                             /* the frame and half the bar, in points' widths */
#define TRAY 1.25f                              /* the tray's width (in the board), its height (out of it) */
#define TRAY_GAP 0.25f                          /* a tray out of the board: so far from it */
#define ROW2 1.05f                              /* ...and a second row of checkers in it, where the room allows */
#define H_MIN 11.0f                             /* the plank's height, in points' widths (14 in the square) */
#define H_MAX 16.0f
#define ANIM_MS 200

/* the board's geometry in pixels: w a point's width (and a checker's size), wx a point's width on the board (wider
 * when the room is wide), hp the plank's height; the picture's top left at (x, y); v the variant (the ways); out: the
 * trays beyond the long sides (rows of checkers in them, their height th), the dice beside them */
typedef struct { int w, fr, tw; float x, y; int flip, v, wx, hp, out, rows, th; } geo;

static bgame G;
static bmove legal[BG_MAXMOVES];
static int nlegal;
static geo S;                                   /* on the screen */
static int made = -1, made_wx, made_hp, made_out, made_th, pic_w, pic_h, cs;
static SDL_Texture *t_board, *t_tray, *t_chk[2];
static int sel = -1;                            /* the place (the mover's own numbering) of the checker in hand */
static int shown = -1;                          /* the throw (event) the dice show */
static int pass_snd;                            /* a throw with no move: the sound once the dice lie */
static struct { int on, moved; float x, y; } drag;
static struct { int on, player; bmove m; Uint32 t0; } anim;
static const int IDLE[2] = {5, 2};              /* the faces of the dice waiting to be thrown */

static int fr_of(int w) { return (int)(w * FRAME); }
static int tray_of(int w) { return (int)(w * TRAY); }
static geo geo_of(int w, float x, float y, int flip, int v)   /* the board in the square (the cards) */
{
    return (geo){w, fr_of(w), tray_of(w), x, y, flip, v, w, 14 * w, 0, 1, 0};
}
static int board_w(const geo *g) { return 12 * g->wx + (g->out ? 4 : 5) * g->fr + g->tw; }
static int board_h(const geo *g) { return g->hp + 2 * g->fr; }

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
    bg_new(&G, variant);
    shown = -1;
    anim.on = pass_snd = 0;
    refresh();
}

static int status(void)
{
    static const int map[] = {[BG_PLAY] = GS_PLAY, [BG_WIN0] = GS_WIN0, [BG_WIN1] = GS_WIN1, [BG_DRAW] = GS_DRAW};
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

/* ---- the bot (backgammon_bot.c) ---------------------------------------------------------------------------------- */
static int busy(void);

const bgame *backgammon_live(void) { return &G; }

static int place_of(const char **s)             /* "bar", "off" or a point 1..24; -1: none */
{
    if (!strncmp(*s, "bar", 3)) { *s += 3; return BG_BAR; }
    if (!strncmp(*s, "off", 3)) { *s += 3; return BG_OFF; }
    char *end;
    long v = strtol(*s, &end, 10);
    if (end == *s || v < 1 || v > 24) return -1;
    *s = end;
    return (int)v;
}

int backgammon_show(const char *move)
{
    const bpos *p = bg_pos(&G);
    if (busy() || status() != GS_PLAY) return 0;
    if (!strcmp(move, "roll")) {
        if (p->phase != BG_OPENING && p->phase != BG_THROW) return 0;
        throw_dice();
        return 1;
    }
    const char *s = move;
    int from = place_of(&s), to = *s == '/' ? (s++, place_of(&s)) : -1;
    if (*s == '*') s++;
    bmove m;
    if (from < 0 || to < 0 || *s || !bg_find(p, from, to, &m)) return 0;
    play(m, 1);
    return 1;
}

/* ---- where things are -------------------------------------------------------------------------------------------- */
static int on_top(const geo *g, int player) { return player ^ g->flip; }

/* the middle of a column of points, s 0..11 from the left (6..11 past the bar) */
static float column_x(const geo *g, int s)
{
    return g->x + g->fr + s * g->wx + (s >= 6 ? 2 * g->fr : 0) + g->wx / 2.0f;
}

/* out: a player's tray beyond the board's long side on their side, under their home board (the right half): its
 * outer box */
static void tray_box(const geo *g, int player, float *x0, float *y0, float *x1, float *y1)
{
    float h = g->th + g->fr, gap = TRAY_GAP * g->w;    /* its frame: half the board's */
    *x0 = g->x + g->fr + 6 * g->wx + 2 * g->fr - g->fr / 2;
    *x1 = g->x + board_w(g);
    *y0 = on_top(g, player) ? g->y - gap - h : g->y + board_h(g) + gap;
    *y1 = *y0 + h;
}

/* a stack's checkers at full step up to this many, then closer (the middle of the board kept free) */
static float stack_room(const geo *g)
{
    float w = (float)g->w, gap = g->out ? 1.4f : fminf(2.0f, 1.6f + 0.15f * (g->hp / w - H_MIN));
    return g->hp / 2.0f - gap * w - w;          /* from the first checker's centre to the last's */
}

/* checker k (0: against the frame) of n on a player's place (their own numbering: 1..24, the bar, off) */
static void spot(const geo *g, int player, int place, float k, int n, float *cx, float *cy)
{
    float w = (float)g->w, top = g->y + g->fr, bottom = top + g->hp, mid = top + g->hp / 2.0f, step = w;
    if (place == BG_BAR) {                      /* on the bar, from the middle towards the player's side */
        float room = g->hp / 2.0f - 1.6f * w;
        if (n > 1 && (n - 1) * w > room) step = room / (n - 1);
        *cx = g->x + g->fr + 6 * g->wx + g->fr;
        *cy = mid + (on_top(g, player) ? -1 : 1) * (0.6f * w + k * step);
        return;
    }
    if (place == BG_OFF && g->out) {            /* in the player's tray, rows from the right end, the first nearest
                                                 * the board */
        float x0, y0, x1, y1, in = g->fr / 2.0f;
        int per = g->rows > 1 ? 8 : 15, r = (int)k / per;
        float i = k - r * per;
        tray_box(g, player, &x0, &y0, &x1, &y1);
        step = (x1 - x0 - 2 * in - 1.1f * w) / (per - 1);
        *cx = x1 - in - 0.55f * w - i * step;
        *cy = on_top(g, player) ? y1 - in - 0.6f * w - r * ROW2 * w : y0 + in + 0.6f * w + r * ROW2 * w;
        return;
    }
    if (place == BG_OFF) {                      /* in the tray, from the player's end */
        step = (g->hp / 2.0f - g->fr / 4.0f - w) / 14;
        *cx = g->x + g->fr + 12 * g->wx + 3 * g->fr + g->tw / 2.0f;
        *cy = on_top(g, player) ? top + w / 2 + k * step : bottom - w / 2 - k * step;
        return;
    }
    int a = player ? bg_opp(g->v, place) : place;   /* player 0's numbering: 1..6 bottom right, 13..18 top left */
    int up = (a >= 13) ^ g->flip, s = a >= 13 ? a - 13 : 12 - a;
    float room = stack_room(g);
    if (n > 1 && (n - 1) * w > room + 0.01f) step = room / (n - 1);
    *cx = column_x(g, s);
    *cy = up ? top + w / 2 + k * step : bottom - w / 2 - k * step;
}

/* a place's checkers at full step, at most (more: their count on the top one) */
static int stack_fits(const geo *g, int place)
{
    float room = place == BG_BAR ? g->hp / 2.0f - 1.6f * g->w : stack_room(g);
    return (int)(room / g->w + 0.01f) + 1;
}

/* the dice's size: beside a tray (out) bigger than in the board */
static float dice_size(int who)
{
    return S.out && who >= 0 ? fminf(1.3f * S.w, 0.9f * (S.th + S.fr)) : 0.86f * S.w;
}

/* where the dice of a player lie (-1: the opening roll, on the bar) */
static void dice_spot(int who, float *cx, float *cy)
{
    float w = (float)S.wx;
    if (S.out && who >= 0) {                    /* beside the player's tray, by their outer board */
        float x0, y0, x1, y1;
        tray_box(&S, who, &x0, &y0, &x1, &y1);
        *cx = column_x(&S, 3) - w / 2;
        *cy = (y0 + y1) / 2;
        return;
    }
    *cy = S.y + S.fr + S.hp / 2.0f;
    *cx = who < 0 ? S.x + 6 * w + 2 * S.fr : on_top(&S, who) ? column_x(&S, 3) - w / 2 : column_x(&S, 9) - w / 2;
}

/* the place under the pointer, in the mover's numbering (-1: none) */
static int place_at(float x, float y)
{
    float w = (float)S.wx, x0 = S.x + S.fr, y0 = S.y + S.fr, bar = x0 + 6 * w, right = bar + 2 * S.fr;
    if (S.out) {                                /* the mover's tray, a little round it */
        float tx0, ty0, tx1, ty1, m = 0.3f * S.w;
        tray_box(&S, bg_pos(&G)->side, &tx0, &ty0, &tx1, &ty1);
        if (x >= tx0 - m && x < tx1 + m && y >= ty0 - m && y < ty1 + m) return BG_OFF;
    }
    if (y < y0 - S.fr || y >= y0 + S.hp + S.fr) return -1;
    if (x >= bar && x < right) return BG_BAR;
    if (!S.out && x >= right + 6 * w && x < right + 6 * w + S.fr + S.tw + S.fr) return BG_OFF;
    int s;
    if (x >= x0 && x < bar) s = (int)((x - x0) / w);
    else if (x >= right && x < right + 6 * w) s = 6 + (int)((x - right) / w);
    else return -1;
    int a = (y < y0 + S.hp / 2.0f) ^ S.flip ? 13 + s : 12 - s;
    return bg_pos(&G)->side ? bg_opp(S.v, a) : a;
}

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
static uint8_t *px(uint8_t *b, int w, int x, int y) { return b + 4 * ((size_t)y * w + x); }

/* the board of g (its points wx pixels wide, its plank hp high; out: no tray): the house plank and frame cut to the
 * board's size (under the bar, and across the middle), the bar and the strip before the tray made of the frame's
 * walnut, the tray sunk, the points inlaid in two woods */
static uint8_t *paint(const geo *g, int *pw, int *ph)
{
    int w = g->w, wx = g->wx, fr = g->fr, tw = g->tw, hp = g->hp, h = board_h(g), bw = board_w(g);
    int nx = (bw - 2 * fr + 2 * w - 1) / (2 * w), ny = (hp + 2 * w - 1) / (2 * w);
    if (nx < 8) nx = 8;
    int wa = 2 * w * nx + 2 * fr, ha = 2 * w * ny + 2 * fr;
    uint8_t *a = malloc((size_t)wa * ha * 4), *o = malloc((size_t)bw * h * 4);
    if (!a || !o || fr < 2) { free(a); free(o); return NULL; }
    art_board_n(nx, ny, 2 * w, fr, 0, a);
    int bar = fr + 6 * wx, strip = bar + 2 * fr + 6 * wx, tray = strip + fr, half = fr / 2, mid = fr + hp / 2;
    int q = fr / 4 > 0 ? fr / 4 : 1;
#define SY(y) ((y) < h / 2 ? (y) : (y) + ha - h)  /* the plank's rows: its top half and its bottom half */
    for (int y = 0; y < h; y++)
        for (int x = 0; x < bw; x++) memcpy(px(o, bw, x, y), px(a, wa, x < bar + fr ? x : x + wa - bw, SY(y)), 4);
    for (int y = fr; y < fr + hp; y++) {
        for (int j = 0; j < 2 * fr; j++)        /* the bar: the frame's two sides back to back, a fold */
            memcpy(px(o, bw, bar + j, y), px(a, wa, j < fr ? wa - fr + j : j - fr, SY(y)), 4);
        if (g->out) continue;
        for (int j = 0; j < fr; j++)            /* the same, narrower, before the tray */
            memcpy(px(o, bw, strip + j, y),
                   px(a, wa, j < half ? wa - fr + j * fr / half : (j - half) * fr / (fr - half), SY(y)), 4);
        for (int x = tray; x < tray + tw; x++) {
            uint8_t *p = px(o, bw, x, y);
            if (abs(y - mid) < q) {             /* walnut between the two players' ends */
                int j = y - mid + q, sy = j < q ? ha - fr + j * fr / q : (j - q) * fr / q;
                memcpy(p, px(a, wa, x + wa - bw, sy), 4);
                continue;
            }
            float k = 0.64f + 0.14f * fminf(1, (x - tray) / (0.35f * tw));   /* sunk, in the shade of its side */
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)(p[i] * k);
        }
    }
#undef SY
    static const float tone[2][3] = {{1.10f, 1.22f, 1.45f}, {0.60f, 0.42f, 0.36f}};   /* maple, rosewood */
    int len = hp / 2 - (int)(1.5f * w);         /* 5.5 points' widths on the plank of 14 */
    float m = 0.05f * wx, lw = fmaxf(1, wx / 30.0f);
    for (int s = 0; s < 12; s++) {
        int x0 = fr + s * wx + (s >= 6 ? 2 * fr : 0);
        float cx = x0 + wx / 2.0f;
        for (int up = 0; up < 2; up++) {
            const float *c = tone[(s + up) & 1];
            for (int i = 0; i < len; i++) {
                int y = up ? fr + i : fr + hp - 1 - i;
                float hw = (1 - (i + 0.5f) / len) * (wx / 2.0f - m);
                for (int x = x0; x < x0 + wx; x++) {
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

/* a tray out of the board: a plank (iw x ih inside a frame of f), sunk, in the shade of its far side */
static void make_tray(int iw, int ih, int f, uint8_t *o)
{
    int w = iw + 2 * f;
    art_plank(iw, ih, f, o);
    for (int y = f; y < f + ih; y++)
        for (int x = f; x < f + iw; x++) {
            uint8_t *p = px(o, w, x, y);
            float k = 0.64f + 0.14f * fminf(1, (y - f) / (0.35f * ih));
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)(p[i] * k);
        }
}

/* the room: the board with its tray and the dice in it (as tall as the room, its points as wide as it lets them:
 * landscape), or the board alone across the whole width and the trays and dice beyond its long sides (the narrow
 * room of portrait), whichever gives the bigger checkers */
static void layout(box b)
{
    float zone = TRAY_GAP + TRAY + FRAME;      /* a tray out of the board, in points' widths */
    float w_in = fminf(b.w / (12 + 5 * FRAME + TRAY), b.h / (H_MIN + 2 * FRAME));
    float w_out = fminf(b.w / (12 + 4 * FRAME), b.h / (H_MIN + 2 * FRAME + 2 * zone));
    int out = w_out > 1.05f * w_in, w = (int)floorf(out ? w_out : w_in), rows = 1;
    if (w < 4) w = 4;
    geo g = geo_of(w, 0, 0, ui.flipped, bg_pos(&G)->variant);
    float room = b.h / w - 2 * FRAME - (out ? 2 * zone : 0);   /* the plank's height it may have */
    if (out && room - 2 * ROW2 >= 13) {         /* tall: two rows in the trays */
        rows = 2;
        room -= 2 * ROW2;
    }
    g.out = out;
    g.rows = rows;
    g.hp = (int)(clampf(room, H_MIN, H_MAX) * w);
    g.th = out ? (int)((TRAY + (rows - 1) * ROW2) * w) : 0;
    if (out) g.tw = 0;
    else {                                      /* the points wider where the width is long (up to a quarter) */
        int wx = (int)((b.w - 5 * g.fr - g.tw) / 12);
        g.wx = wx < w ? w : wx > 1.25f * w ? (int)(1.25f * w) : wx;
    }
    if (made != w || made_wx != g.wx || made_hp != g.hp || made_out != out || made_th != g.th) {
        made = w;
        made_wx = g.wx;
        made_hp = g.hp;
        made_out = out;
        made_th = g.th;
        int bw, bh;
        uint8_t *pic = paint(&g, &bw, &bh);
        ui_free(&t_board);
        if (pic) t_board = ui_tex(bw, bh, pic);
        free(pic);
        pic_w = bw;
        pic_h = bh;
        ui_free(&t_tray);
        if (out) {
            float x0, y0, x1, y1;
            tray_box(&g, 0, &x0, &y0, &x1, &y1);
            int f = g.fr / 2, iw = (int)(x1 - x0) - 2 * f, ih = (int)(y1 - y0) - 2 * f;
            make_tray(iw, ih, f, ui_buf(iw + 2 * f, ih + 2 * f));
            t_tray = ui_tex(iw + 2 * f, ih + 2 * f, ui_buf(iw + 2 * f, ih + 2 * f));
        }
        cs = (int)(1.12f * w);                  /* the disc fills 0.88 of its picture: a point's width */
        for (int c = 0; c < 2; c++) {
            ui_free(&t_chk[c]);
            art_counter(COUNTER_DISC, c, cs, ui_buf(cs, cs));
            t_chk[c] = ui_tex(cs, cs, ui_buf(cs, cs));
        }
    }
    float tall = (float)pic_h + (out ? 2 * (TRAY_GAP * w + g.th + g.fr) : 0);
    g.x = floorf(b.x + (b.w - pic_w) / 2);
    g.y = floorf(b.y + (b.h - tall) / 2 + (tall - pic_h) / 2);
    S = g;
    if (shown >= 0 && !ui_dice_busy()) show_throw(shown, 0);   /* the shell's dice ours again (after another game) */
}

static void checker(int player, float cx, float cy, float k)
{
    float d = cs * k;
    ui_shadow_flat(cx + 0.3f * (k - 1) * cs, cy + 0.6f * (k - 1) * cs, d, d, 1);
    ui_put_c(t_chk[player], cx, cy, d, d);
}

/* plakoto: a pinner's checkers stand on the checker they pin, a little up the point */
static float lift(int player, int place)
{
    const bpos *p = bg_pos(&G);
    return place >= 1 && place <= 24 && p->pin[!player][bg_opp(p->variant, place)] ? 0.6f : 0;
}

/* checker k of n of a player's on a place, as the board shows them now */
static void at(int player, int place, int k, int n, float *x, float *y)
{
    float l = lift(player, place);
    spot(&S, player, place, k + l, n + (l > 0), x, y);
}

/* a player's checkers on a place; skip: the top ones not drawn (moving); the count on a high stack */
static void stack(int player, int place, int skip)
{
    int n = bg_pos(&G)->n[player][place];
    float x, y;
    for (int k = 0; k < n - skip; k++) {
        at(player, place, k, n, &x, &y);
        checker(player, x, y, 1);
    }
    if (place != BG_OFF && n - skip > stack_fits(&S, place)) {
        at(player, place, n - skip - 1, n, &x, &y);
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
        at(pl, from, nf, nf + 1, &x, &y);
        ui_mark_last(x - w / 2, y - w / 2, w, w);
        at(pl, to, nt ? nt - 1 : 0, nt, &x, &y);
        ui_mark_last(x - w / 2, y - w / 2, w, w);
    }
}

static void dice(void)
{
    const bpos *p = bg_pos(&G);
    float x, y, ds;                             /* a die's size; its light round the dice ~4 times as wide */
    int k = last_throw();
    if (shown >= 0 && shown == k) {             /* the last throw; dimmed once played, red when it could not be */
        int who = G.ev[k].player;
        unsigned used = p->phase == BG_OPENING ? 0 : p->phase == BG_MOVE ? p->used : 0xf;
        dice_spot(who, &x, &y);
        ds = dice_size(who);
        if (G.pos[k + 1].passed >= 0 && !ui_dice_busy()) ui_mark_glow(x, y, 3.95f * ds);
        if (p->phase == BG_OPENING) ui_mark_gold(x, y, 3.95f * ds);
        ui_dice_draw(x, y, ds, used);
    }
    if (pass_snd && !ui_dice_busy()) {
        pass_snd = 0;
        ui_sound(SND_DENY);
    }
    if (p->phase != BG_THROW && !(p->phase == BG_OPENING && shown < 0)) return;
    int opening = p->phase == BG_OPENING;       /* the dice to throw: a die each for the opening, else the mover's */
    dice_spot(opening ? -1 : p->side, &x, &y);
    ds = dice_size(opening ? -1 : p->side);
    ui_mark_gold(x, y, 3.95f * ds);
    for (int i = 0; i < 2; i++)
        ui_die_at(x + (i ? 0.756f : -0.756f) * ds, y, ds, opening ? i : p->side, IDLE[opening ? 0 : i], 0.8f);
}

static void draw(void)
{
    const bpos *p = bg_pos(&G);
    float x, y, w = (float)S.w;
    ui_put(t_board, S.x, S.y, (float)pic_w, (float)pic_h);
    if (S.out && t_tray)                        /* the trays beyond the long sides */
        for (int pl = 0; pl < 2; pl++) {
            float x0, y0, x1, y1;
            tray_box(&S, pl, &x0, &y0, &x1, &y1);
            int tw, th;
            SDL_QueryTexture(t_tray, NULL, NULL, &tw, &th);
            ui_put(t_tray, floorf(x0), floorf(y0), (float)tw, (float)th);
        }
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
    for (int pass = 0; pass < 2; pass++)      /* the pinned checkers first: their pinners stand on them */
        for (int pl = 0; pl < 2; pl++)
            for (int place = 0; place <= BG_BAR; place++) {
                if (p->pin[pl][place] != !pass) continue;
                int skip = 0;
                if (anim.on && pl == anim.player && place == anim.m.to) skip = 1;
                if (anim.on && anim.m.hit && pl != anim.player && place == BG_BAR) skip = 1;
                if (drag.on && drag.moved && pl == mover && place == sel) skip = 1;
                stack(pl, place, skip);
            }
    if (in_hand && !drag.moved) {               /* the checker in hand, and where it can go: a ring on a blot to hit */
        int n = p->n[mover][sel];
        at(mover, sel, n - 1, n, &x, &y);
        ui_mark_select(x - w / 2, y - w / 2, w, w);
        for (int i = 0; i < nlegal; i++) {
            if (legal[i].from != sel) continue;
            int to = legal[i].to, there = p->n[mover][to];
            if (legal[i].hit) {                 /* a blot to hit, or (plakoto) a checker to pin */
                spot(&S, !mover, bg_opp(S.v, to), 0, 1, &x, &y);
                ui_mark_ring(x, y, 1.5f * w);
            } else {
                at(mover, to, there, there + 1, &x, &y);
                ui_mark_dot(x, y, 0.55f * w);
            }
        }
    }
    if (anim.on) {
        bmove m = anim.m;
        int pl = anim.player, nf = p->n[pl][m.from], nt = p->n[pl][m.to];
        float x0, y0, x1, y1;
        if (m.hit && S.v == BG_BACKGAMMON) {    /* the blot goes to the bar */
            int nb = p->n[!pl][BG_BAR];
            spot(&S, !pl, 25 - m.to, 0, 1, &x0, &y0);
            spot(&S, !pl, BG_BAR, nb - 1, nb, &x1, &y1);
            checker(!pl, x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, 1);
        }
        at(pl, m.from, nf, nf + 1, &x0, &y0);
        at(pl, m.to, nt - 1, nt, &x1, &y1);
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
        float ds = dice_size(p->phase == BG_OPENING ? -1 : p->side);
        if (fabsf(x - cx) < 2.56f * ds && fabsf(y - cy) < 1.63f * ds) throw_dice();
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

/* a checker on a card (c pixels, one: its scratch), its flat shadow under it */
static void card_checker(uint8_t *rgba, int size, uint8_t *one, int c, float x, float y, int player)
{
    int ox = (int)(x - c / 2.0f), oy = (int)(y - c / 2.0f);
    art_flat_shadow(c, one);
    art_blend(rgba, size, size, one, c, c, ox + c / 20, oy + c * 8 / 100);
    art_counter(COUNTER_DISC, player, c, one);
    art_blend(rgba, size, size, one, c, c, ox, oy);
}

/* the picker's cards: the whole board, a position telling the game (each player's checkers, "24:12 9:1" in their own
 * numbering; plakoto: ebony's checker pinned on its 16 under ivory's on 9), two dice: backgammon's start; plakoto's
 * stacks on the two right corners, a pin; fevga's diagonal corners, a runner each past the other's start; long
 * nardy's heads and a first 6-6 played, two checkers off each head */
static void card(int variant, int size, uint8_t *rgba)
{
    static const struct { const char *pos[2]; int pinned, dice[2]; } CARD[BG_VARIANTS] = {
        [BG_BACKGAMMON] = {{"24:2 13:5 8:3 6:5", "24:2 13:5 8:3 6:5"}, 0, {3, 1}},
        [BG_PLAKOTO] = {{"24:12 17:1 13:1 9:1", "24:12 21:1 16:1 11:1"}, 16, {4, 2}},
        [BG_FEVGA] = {{"24:14 9:1", "24:14 9:1"}, 0, {5, 2}},
        [BG_NARDY] = {{"24:13 18:2", "24:13 18:2"}, 0, {6, 6}},
    };
    int w = (int)(size / 16.6f), bw, bh, c = (int)(1.12f * w), d = (int)(1.3f * w);
    geo pg = geo_of(w, 0, 0, 0, variant);
    uint8_t *pic = paint(&pg, &bw, &bh), *one = malloc((size_t)d * d * 4);   /* the dice, larger than a checker */
    if (!pic || !one) { free(pic); free(one); return; }
    int ox = (size - bw) / 2, oy = (size - bh) / 2;
    art_blend(rgba, size, size, pic, bw, bh, ox, oy);
    free(pic);
    geo g = geo_of(w, (float)ox, (float)oy, 0, variant);
    for (int pass = 0; pass < 2; pass++)        /* the pinned checker first, its pinner on it */
        for (int pl = 0; pl < 2; pl++)
            for (const char *t = CARD[variant].pos[pl]; *t;) {
                char *e;
                int place = (int)strtol(t, &e, 10), n = (int)strtol(e + 1, &e, 10);
                t = *e ? e + 1 : e;
                int pinned = pl == 1 && place == CARD[variant].pinned;
                int pinner = pl == 0 && CARD[variant].pinned && bg_opp(variant, place) == CARD[variant].pinned;
                if (pinned != !pass) continue;
                for (int k = 0; k < n; k++) {
                    float x, y;
                    spot(&g, pl, place, k + (pinner ? 0.6f : 0), n + pinner, &x, &y);
                    card_checker(rgba, size, one, c, x, y, pl);
                }
            }
    float cy = g.y + g.fr + 7 * w;              /* the dice in the middle of the right half (ivory's) */
    for (int i = 0; i < 2; i++) {
        art_die(CARD[variant].dice[i], 0, d, one);
        int dx = (int)(column_x(&g, 9) + (i ? 0.15f : -1.45f) * w);
        art_blend(rgba, size, size, one, d, d, dx, (int)(cy - d / 2.0f));
    }
    free(one);
}

/* the menu's card: a corner of the board, from the bar and the middle: a home board with checkers, the tray, dice;
 * the picker's: card() */
static void thumb(int variant, int size, uint8_t *rgba)
{
    if (variant >= 0) {
        card(variant, size, rgba);
        return;
    }
    int w = (int)(size / 10.2f), bw, bh, c = (int)(1.12f * w), d = (int)(0.86f * w);
    geo pg = geo_of(w, 0, 0, 0, BG_BACKGAMMON);
    uint8_t *pic = paint(&pg, &bw, &bh), *one = malloc((size_t)c * c * 4);
    if (!pic || !one) { free(pic); free(one); return; }
    int fr = fr_of(w), x0 = fr + 6 * w, y0 = fr + 7 * w, cw = bw - x0, ch = bh - y0 + fr, ox = (size - cw) / 2;
    int oy = (size - ch) / 2;
    for (int y = 0; y < ch; y++) {              /* the frame's top, then the lower half of the board */
        int sy = y < fr ? y : y - fr + y0;
        memmove(pic + 4 * (size_t)y * cw, pic + 4 * ((size_t)sy * bw + x0), 4 * (size_t)cw);
    }
    art_blend(rgba, size, size, pic, cw, ch, ox, oy);
    free(pic);
    geo g = geo_of(w, (float)(ox - x0), (float)(oy + fr - y0), 0, BG_BACKGAMMON);
    static const int place[][3] = {{0, 6, 4}, {0, 4, 2}, {0, 5, 1}, {1, 22, 1}, {1, 24, 2}, {0, 0, 3}};
    for (unsigned i = 0; i < sizeof place / sizeof place[0]; i++)
        for (int k = 0; k < place[i][2]; k++) {
            float x, y;
            spot(&g, place[i][0], place[i][1], (float)k, place[i][2], &x, &y);
            card_checker(rgba, size, one, c, x, y, place[i][0]);
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
    .id = "backgammon", .order = 130, .variants = BG_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .fill = 1,
};
REGISTER_GAME(backgammon_game)
