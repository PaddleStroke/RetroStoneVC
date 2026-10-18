/*
 * Cross Math in RetroStone Board, for one player: a crossword of sums on the house plank. Its cells are inlaid in the
 * wood: the numbers given burned into maple, the operations and the equals signs in ink on honey wood, the missing
 * numbers as hollows; the missing numbers wait as wooden tiles in a tray beside the board (the rules:
 * mathcross_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Drag a tile from the tray to a hollow (a tile already there goes back to the tray), from a hollow to another (they
 * swap) or off the board (back to the tray). Or tap a tile, then a hollow (or a hollow, then a tile); a tap on a
 * placed tile sends it back. An equation filled and true lights its equals sign in gold; filled and false, its cells
 * turn red (a mistake, for the stars). The shell's tool: the hint (a wrong tile put right, else the next hollow
 * reasoning fills, its tile flying there). The keys: the arrows move a frame over the hollows and the tray, Return
 * (Enter, Space) taps what it frames. Undo takes back one action, all the way to the start. The bar: the moves.
 * Solved: a wave of gold light runs over the board; the stars count the hints and the mistakes (ui_logic_stars).
 * The shell's level screen chooses a level (150 in six chapters: mathcross_levels.h), or its die tab a puzzle made
 * at random (three variants).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/bench_ui.h"
#include "games/mathcross.h"
#include "games/mathcross_levels.h"
#include "games/mathsigns.h"
#include "shell.h"

enum { T_HINT, TOOLS };
#define FLY_MS 200
#define PITCH 1.1f                              /* the tray's slots, in cells */

static const float TONE_GIVEN[3] = {1.12f, 1.10f, 1.04f}, TONE_SIGN[3] = {0.84f, 0.76f, 0.66f},
                   TONE_TRAY[3] = {0.62f, 0.48f, 0.38f};
static const uint8_t SIGN_INK[3] = {58, 38, 26};

static mcr_game G;
static int serial;

static struct {
    box room;
    int q, made;                                /* a cell's side; the pictures' key */
    float bx, by, bw, bh, gx, gy;               /* the board's plank, the grid's corner */
    float tx, ty, tw, th;                       /* the tray's plank */
    float hx[MCR_GAPS], hy[MCR_GAPS];           /* each tile's home in the tray */
} L;
static SDL_Texture *t_board, *t_tray, *t_tile, *t_small;
static int made_small = -1;

static int sel_tile = -1, sel_gap = -1, cur = -1;   /* cur: the keys' frame (gaps, then the tray's tiles) */
static struct { int on, tile, gap, moved; float x0, y0, x, y; } drag;
typedef struct { float x, y; Uint32 t0; int on; } flight;
static flight fly[MCR_GAPS];
static bench_flash hint;
static bench_wave win;

/* ---- painting ---------------------------------------------------------------------------------------------------- */
/* a cell kind's sign (MCR_ADD..MCR_EQ) d across at (cx, cy) */
static void paint_sign(uint8_t *o, int w, int h, float cx, float cy, float d, int kind, const uint8_t ink[3])
{
    msg_sign(o, w, h, cx, cy, d, kind - MCR_ADD + MSG_ADD, ink);
}

static void paint_hollow(uint8_t *o, int w, int h, float x0, float y0, float s) { msg_hollow(o, w, h, x0, y0, s, s); }

/* a number's digits height for a cell q wide (three digits set smaller) */
static float digits_h(int v, float q) { return v >= 100 ? 0.4f * q : v >= 10 ? 0.5f * q : 0.56f * q; }

/* the board: the plank (w x h, frame fr), the grid's corner at (ox, oy), cells q; filled: tiles shown in the gaps (the
 * pictures of the menu: a share of them, 0..1) */
static void paint_board(uint8_t *o, int w, int h, int fr, float ox, float oy, int q, const mcr_puzzle *p, float filled)
{
    art_plank(w - 2 * fr, h - 2 * fr, fr, o);
    float in = fmaxf(1, 0.04f * q), seam = fmaxf(1, 0.05f * q);
    int k = 0, gaps = 0;
    for (int c = 0; c < p->w * p->h; c++) gaps += p->kind[c] == MCR_NUM && p->gap[c];
    for (int c = 0; c < p->w * p->h; c++) {
        int kind = p->kind[c];
        if (kind == MCR_EMPTY) continue;
        float x = ox + (c % p->w) * q, y = oy + (c / p->w) * q, cx = x + q / 2.0f, cy = y + q / 2.0f;
        if (kind == MCR_NUM && p->gap[c]) {
            if (k++ < (int)(filled * gaps + 0.5f)) {   /* a tile in it (a picture) */
                art_inlay(o, w, h, x + in, y + in, q - 2 * in, q - 2 * in, 0.14f * q, BENCH_TONE[1], seam);
                puz_digits(o, w, h, p->val[c], cx, cy + 0.02f * q, digits_h(p->val[c], q), PUZ_INK, 1, 1);
            } else {
                paint_hollow(o, w, h, x + in, y + in, q - 2 * in);
            }
            continue;
        }
        art_inlay(o, w, h, x + in, y + in, q - 2 * in, q - 2 * in, 0.12f * q, kind == MCR_NUM ? TONE_GIVEN : TONE_SIGN,
                  seam);
        if (kind == MCR_NUM) puz_burn(o, w, h, p->val[c], cx, cy + 0.02f * q, digits_h(p->val[c], q));
        else paint_sign(o, w, h, cx, cy, (float)q, kind, SIGN_INK);
    }
}

/* the token: two cells, a plus sign and a tile of 7 */
static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    int fr = (int)fmaxf(1, 0.06f * size), q = (size - 2 * fr) / 2, w = 2 * q + 2 * fr;
    uint8_t *b = calloc((size_t)w * w, 4);
    if (!b || q < 2) { free(b); return; }
    art_plank(w - 2 * fr, w - 2 * fr, fr, b);
    art_inlay(b, w, w, fr + 1.0f, fr + 1.0f, q - 2.0f, q - 2.0f, 0.12f * q, TONE_SIGN, fmaxf(1, 0.05f * q));
    paint_sign(b, w, w, fr + q / 2.0f, fr + q / 2.0f, (float)q, MCR_ADD, SIGN_INK);
    art_inlay(b, w, w, fr + q + 1.0f, fr + q + 1.0f, q - 2.0f, q - 2.0f, 0.12f * q, BENCH_TONE[1], fmaxf(1, 0.05f * q));
    puz_digits(b, w, w, 7, fr + 1.5f * q, fr + 1.52f * q, 0.6f * q, PUZ_INK, 1, 1);
    art_blend(rgba, size, size, b, w, w, (size - w) / 2, (size - w) / 2);
    free(b);
}

/* ---- the layout: the board and the tray, as big as the room allows ---------------------------------------------- */
static void fit(box b)
{
    int W = G.p.w, H = G.p.h, n = G.ntray > 0 ? G.ntray : 1;
    float best = 0, M = 0.7f, GAP = 0.35f;      /* the planks' margins (both sides), the room between them */
    int bt = n, bside = 0, btr = 99;
    float cap = fminf(b.w, b.h) / 5.5f;
    for (int side = 0; side < 2; side++)
        for (int tc = 1; tc <= n; tc++) {
            int tr = (n + tc - 1) / tc;
            float tw = tc * PITCH + 0.5f, th = tr * PITCH + 0.5f, nw, nh;
            if (side) nw = W + M + GAP + tw, nh = fmaxf(H + M, th);
            else nw = fmaxf(W + M, tw), nh = H + M + GAP + th;
            float q = fminf(fminf(b.w / nw, b.h / nh), cap);   /* as big as can be, then the fewest rows */
            if (q > best * 1.02f || (q > best * 0.98f && tr < btr)) best = q, bt = tc, bside = side, btr = tr;
        }
    int q = (int)best;
    if (q < 4) q = 4;
    int tr = (n + bt - 1) / bt, mg = (int)(M / 2 * q);
    L.q = q;
    L.bw = (float)(W * q + 2 * mg);
    L.bh = (float)(H * q + 2 * mg);
    L.tw = floorf((bt * PITCH + 0.5f) * q);
    L.th = floorf((tr * PITCH + 0.5f) * q);
    float gap = floorf(GAP * q);
    if (bside) {
        float all = L.bw + gap + L.tw;
        L.bx = floorf(b.x + (b.w - all) / 2);
        L.by = floorf(b.y + (b.h - L.bh) / 2);
        L.tx = L.bx + L.bw + gap;
        L.ty = floorf(b.y + (b.h - L.th) / 2);
    } else {
        float all = L.bh + gap + L.th;
        L.bx = floorf(b.x + (b.w - L.bw) / 2);
        L.by = floorf(b.y + (b.h - all) / 2);
        L.tx = floorf(b.x + (b.w - L.tw) / 2);
        L.ty = L.by + L.bh + gap;
    }
    L.gx = L.bx + mg;
    L.gy = L.by + mg;
    for (int t = 0; t < G.ntray; t++) {
        int r = t / bt, in_row = r == tr - 1 ? G.ntray - r * bt : bt;
        L.hx[t] = L.tx + (0.25f + PITCH * (t % bt + 0.5f + (bt - in_row) / 2.0f)) * q;
        L.hy[t] = L.ty + (0.25f + PITCH * (r + 0.5f)) * q;
    }
}

static void layout(box b)
{
    L.room = b;
    if (!G.neq) return;
    fit(b);
    int key = (L.q * 64 + (int)L.tw % 64) * 4096 + serial % 4096;
    if (L.made != key || !t_board) {
        L.made = key;
        int w = (int)L.bw, h = (int)L.bh, fr = (int)fmaxf(2, 0.12f * L.q);
        uint8_t *o = ui_buf(w, h);
        paint_board(o, w, h, fr, L.gx - L.bx, L.gy - L.by, L.q, &G.p, 0);
        ui_free(&t_board);
        t_board = ui_tex(w, h, o);
        int tw = (int)L.tw, th = (int)L.th;     /* the tray: a darker plank, a hollow for each tile */
        o = ui_buf(tw, th);
        art_plank(tw - 2 * fr, th - 2 * fr, fr, o);
        for (int i = 0; i < tw * th; i++)
            for (int k = 0; k < 3; k++) o[4 * i + k] = (uint8_t)(o[4 * i + k] * TONE_TRAY[k] + 0.5f);
        float s = 0.92f * L.q;
        for (int t = 0; t < G.ntray; t++) paint_hollow(o, tw, th, L.hx[t] - L.tx - s / 2, L.hy[t] - L.ty - s / 2, s);
        ui_free(&t_tray);
        t_tray = ui_tex(tw, th, o);
        int ts = (int)(0.9f * L.q);             /* a tile: honey wood, bevelled */
        o = ui_buf(ts, ts);
        bench_tile(o, ts, ts, 0.16f * ts, BENCH_TONE[1]);
        ui_free(&t_tile);
        t_tile = ui_tex(ts, ts, o);
    }
    bench_small(&t_small, &made_small, token);
}

/* ---- where things are -------------------------------------------------------------------------------------------- */
static void cell_mid(int c, float *x, float *y)
{
    *x = L.gx + (c % G.p.w + 0.5f) * L.q;
    *y = L.gy + (c / G.p.w + 0.5f) * L.q;
}

static void tile_pos(int t, float *x, float *y)  /* where tile t rests: its gap, or its home in the tray */
{
    int i = mcr_holder(&G, t);
    if (i >= 0) cell_mid(G.gaps[i], x, y);
    else *x = L.hx[t], *y = L.hy[t];
}

static int gap_at(float x, float y)
{
    float ix = x - L.gx, iy = y - L.gy;
    if (ix < 0 || iy < 0 || ix >= G.p.w * L.q || iy >= G.p.h * L.q) return -1;
    return mcr_gap_of(&G, (int)(iy / L.q) * G.p.w + (int)(ix / L.q));
}

static int tray_at(float x, float y)            /* a free tile of the tray under the finger */
{
    for (int t = 0; t < G.ntray; t++)
        if (mcr_tile_free(&G, t) && fabsf(x - L.hx[t]) <= PITCH * L.q / 2 && fabsf(y - L.hy[t]) <= PITCH * L.q / 2)
            return t;
    return -1;
}

static int in_tray(float x, float y) { return x >= L.tx && y >= L.ty && x < L.tx + L.tw && y < L.ty + L.th; }

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void calm(void)
{
    memset(&drag, 0, sizeof drag);
    memset(fly, 0, sizeof fly);
    sel_tile = sel_gap = -1;
    hint.at = -1;
    win.on = 0;
}

static void begin(void)
{
    serial++;
    calm();
    cur = -1;
    if (L.room.w > 0) layout(L.room);
}

static void start(int level)
{
    mcr_puzzle p;
    if (!mcr_decode(mcr_levels[level < 0 || level >= MCR_LEVELS ? 0 : level], &p) || !mcr_load(&G, &p))
        mcr_new(&G, 0, 1);
    begin();
}

static void start_random(int variant)
{
    mcr_new(&G, variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int status(void) { return mcr_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int flying(void)
{
    for (int t = 0; t < G.ntray; t++)
        if (fly[t].on && ui_ms() - fly[t].t0 < FLY_MS) return 1;
    return 0;
}
static int busy(void) { return flying() || bench_wave_busy(&win); }
static int score(void) { return G.moves; }
static int stars(void) { return ui_logic_stars(G.hints, G.mistakes); }

/* an action: the tiles that moved fly from where they were (but the one the finger carried); solved: the wave */
typedef struct { float x[MCR_GAPS], y[MCR_GAPS]; } spots;

static void spots_now(spots *s)
{
    for (int t = 0; t < G.ntray; t++) tile_pos(t, &s->x[t], &s->y[t]);
}

static void after(const spots *was, int carried, int mistakes)
{
    for (int t = 0; t < G.ntray; t++) {
        float x, y;
        tile_pos(t, &x, &y);
        if (t != carried && (fabsf(x - was->x[t]) > 1 || fabsf(y - was->y[t]) > 1))
            fly[t] = (flight){was->x[t], was->y[t], ui_ms(), 1};
    }
    sel_tile = sel_gap = -1;
    if (G.mistakes > mistakes) ui_sound(SND_DENY);
    else ui_sound(SND_PLACE);
    if (mcr_solved(&G) && !win.on) {
        bench_wave_start(&win, G.p.w / 2.0f, G.p.h / 2.0f, G.p.w, G.p.h);
        cur = -1;
    }
    ui_animate();
}

/* do an action (a put, a take, a move), with its flights */
enum { A_PUT, A_TAKE, A_MOVE };
static int act(int what, int i, int j, int carried)
{
    spots was;
    spots_now(&was);
    int m = G.mistakes, ok = what == A_PUT ? mcr_put(&G, i, j) : what == A_TAKE ? mcr_take(&G, i) : mcr_move(&G, i, j);
    if (ok) after(&was, carried, m);
    else ui_sound(SND_DENY);
    return ok;
}

static int undo(void)
{
    spots was;
    spots_now(&was);
    if (!mcr_undo(&G)) return 0;
    int keep = cur;
    calm();
    cur = keep;
    for (int t = 0; t < G.ntray; t++) {
        float x, y;
        tile_pos(t, &x, &y);
        if (fabsf(x - was.x[t]) > 1 || fabsf(y - was.y[t]) > 1) fly[t] = (flight){was.x[t], was.y[t], ui_ms(), 1};
    }
    return 1;
}

static int script(const char *moves)
{
    calm();
    int ok = mcr_script(&G, moves);
    if (mcr_solved(&G)) bench_wave_start(&win, G.p.w / 2.0f, G.p.h / 2.0f, G.p.w, G.p.h);
    return ok;
}

static void do_hint(void)
{
    if (mcr_solved(&G)) return;
    spots was;
    spots_now(&was);
    int m = G.mistakes, i = mcr_hint(&G);
    if (i < 0) { ui_sound(SND_DENY); return; }
    bench_flash_start(&hint, G.gaps[i]);
    after(&was, -1, m);
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static void tile_at(int t, float x, float y, float s, Uint8 a)
{
    float d = 0.9f * L.q * s;
    ui_shadow_flat(x, y, d, d, a / 255.0f * 0.7f);
    SDL_SetTextureAlphaMod(t_tile, a);
    ui_put_c(t_tile, x, y, d, d);
    SDL_SetTextureAlphaMod(t_tile, 255);
    int v = G.tray[t];
    ui_number_ex(x, y + 0.02f * L.q * s, digits_h(v, (float)L.q) * s, v, PUZ_INK, a, UI_NUM_TIGHT);
}

/* the keys' frame: a gap, or a tile's home in the tray */
static void cur_xy(int f, float *x, float *y)
{
    if (f < G.ngap) cell_mid(G.gaps[f], x, y);
    else *x = L.hx[f - G.ngap], *y = L.hy[f - G.ngap];
}

static void draw(void)
{
    if (!t_board) layout(L.room);
    if (!t_board) return;
    int q = L.q, solved = mcr_solved(&G);
    ui_put(t_board, L.bx, L.by, L.bw, L.bh);
    ui_put(t_tray, L.tx, L.ty, L.tw, L.th);
    for (int e = 0; e < G.neq && !solved; e++) {   /* the equations filled: true (gold), false (red) */
        int st = mcr_eq_state(&G, e);
        const mcr_eq *eq = &G.eq[e];
        float x, y;
        if (st > 0) {
            cell_mid(eq->cell[eq->len - 2], &x, &y);
            ui_mark_gold_a(x, y, 1.3f * q, 0.8f);
        } else if (st < 0) {
            for (int i = 0; i < eq->len; i++) {
                cell_mid(eq->cell[i], &x, &y);
                ui_fill(x - q / 2.0f, y - q / 2.0f, (float)q, (float)q, 214, 60, 44, 70);
            }
        }
    }
    if (sel_gap >= 0) {
        float x, y;
        cell_mid(G.gaps[sel_gap], &x, &y);
        ui_mark_select(x - q / 2.0f, y - q / 2.0f, (float)q, (float)q);
    }
    if (hint.at >= 0) {
        float x, y;
        cell_mid(hint.at, &x, &y);
        bench_glow(&hint, x, y, 2.4f * q);
    }
    for (int c = 0; c < G.p.w * G.p.h && win.on; c++)
        if (G.p.kind[c] != MCR_EMPTY) {
            float x, y;
            cell_mid(c, &x, &y);
            bench_wave_at(&win, c % G.p.w + 0.5f, c / G.p.w + 0.5f, x, y, 2.2f * q);
        }
    bench_wave_tick(&win);
    Uint32 now = ui_ms();
    for (int t = 0; t < G.ntray; t++) {        /* the tiles: in the tray, in the gaps, flying */
        if (drag.on && drag.moved && drag.tile == t) continue;
        float x, y;
        tile_pos(t, &x, &y);
        if (fly[t].on) {
            float f = (now - fly[t].t0) / (float)FLY_MS;
            if (f >= 1) fly[t].on = 0;
            else {
                f = ui_ease(f);
                x = fly[t].x + (x - fly[t].x) * f;
                y = fly[t].y + (y - fly[t].y) * f;
                ui_animate();
            }
        }
        if (t == sel_tile) ui_mark_gold(x, y, 1.6f * q);
        tile_at(t, x, y, t == sel_tile ? 1.08f : 1, 255);
    }
    if (cur >= 0 && cur < G.ngap + G.ntray && !solved) {
        float x, y;
        cur_xy(cur, &x, &y);
        ui_mark_select(x - q * 0.55f, y - q * 0.55f, 1.1f * q, 1.1f * q);
    }
    if (drag.on && drag.moved) {               /* the tile carried, above the finger; the gap under it */
        float x = drag.x, y = drag.y - 0.55f * q;
        int i = gap_at(x, y);
        if (i >= 0) {
            float cx, cy;
            cell_mid(G.gaps[i], &cx, &cy);
            ui_mark_select(cx - q / 2.0f, cy - q / 2.0f, (float)q, (float)q);
        }
        tile_at(drag.tile, x, y, 1.12f, 255);
    }
    ui_tool(T_HINT, ICON_HINT, solved ? UI_DIM : 0);
}

/* ---- the finger -------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    memset(&drag, 0, sizeof drag);
    cur = -1;
    if (mcr_solved(&G)) return;
    if (ui_tool_hit(x, y) == T_HINT) { do_hint(); return; }
    int i = gap_at(x, y), t = tray_at(x, y);
    if (i >= 0 && G.at[i] >= 0) t = G.at[i];
    else if (i >= 0) t = -1;
    drag.on = 1;
    drag.tile = t;
    drag.gap = i;
    drag.x = drag.x0 = x;
    drag.y = drag.y0 = y;
    ui_animate();
}

static void motion(float x, float y)
{
    if (!drag.on) return;
    drag.x = x;
    drag.y = y;
    if (drag.tile >= 0 && hypotf(x - drag.x0, y - drag.y0) > 0.25f * L.q) drag.moved = 1;
    ui_animate();
}

/* a tap on a tile of the tray (t) or a gap (i) */
static void tap(int t, int i)
{
    if (t >= 0 && i < 0) {                      /* a tile of the tray */
        if (sel_gap >= 0) { act(A_PUT, sel_gap, t, -1); return; }
        sel_tile = sel_tile == t ? -1 : t;
        ui_sound(SND_TAP);
    } else if (i >= 0) {                        /* a gap */
        if (sel_tile >= 0) { act(A_PUT, i, sel_tile, -1); return; }
        if (G.at[i] >= 0) { act(A_TAKE, i, 0, -1); return; }
        sel_gap = sel_gap == i ? -1 : i;
        ui_sound(SND_TAP);
    } else {
        sel_tile = sel_gap = -1;
    }
    ui_animate();
}

static void release(float x, float y)
{
    if (!drag.on) return;
    drag.on = 0;
    ui_animate();
    if (!drag.moved) {
        tap(drag.gap >= 0 ? -1 : drag.tile, drag.gap);
        return;
    }
    int t = drag.tile, to = gap_at(x, y - 0.55f * L.q);
    if (to < 0) to = gap_at(x, y);
    if (drag.gap >= 0) {                        /* from a gap: to another (swapped), or back to the tray */
        if (to == drag.gap) return;
        if (to >= 0) act(A_MOVE, drag.gap, to, t);
        else act(A_TAKE, drag.gap, 0, t);
        return;
    }
    if (to >= 0) act(A_PUT, to, t, t);
    else if (!in_tray(x, y)) ui_sound(SND_DENY);
}

/* ---- the keys: the arrows move the frame to the nearest thing that way, Return taps it --------------------------- */
static void key(SDL_Keycode k)
{
    int n = G.ngap + G.ntray;
    if (mcr_solved(&G) || !n) return;
    int dx = (k == SDLK_RIGHT) - (k == SDLK_LEFT), dy = (k == SDLK_DOWN) - (k == SDLK_UP);
    if (dx || dy) {
        if (cur < 0) cur = 0;
        else {
            float x0, y0, best = 1e9f;
            int to = -1;
            cur_xy(cur, &x0, &y0);
            for (int f = 0; f < n; f++) {
                if (f == cur || (f >= G.ngap && !mcr_tile_free(&G, f - G.ngap))) continue;
                float x, y;
                cur_xy(f, &x, &y);
                float along = (x - x0) * dx + (y - y0) * dy, across = fabsf((x - x0) * dy) + fabsf((y - y0) * dx);
                if (along < 0.3f * L.q) continue;
                float score = along + 2.5f * across;
                if (score < best) best = score, to = f;
            }
            if (to < 0) { ui_sound(SND_DENY); return; }
            cur = to;
        }
        ui_sound(SND_TAP);
        ui_animate();
        return;
    }
    if (k != SDLK_RETURN && k != SDLK_KP_ENTER && k != SDLK_SPACE) return;
    if (cur < 0) { cur = 0; ui_animate(); return; }
    if (cur < G.ngap) tap(-1, cur);
    else if (mcr_tile_free(&G, cur - G.ngap)) {
        tap(cur - G.ngap, -1);
        for (int i = 0; i < G.ngap && sel_tile >= 0; i++)   /* a tile taken: the frame to the first empty gap */
            if (G.at[i] < 0) { cur = i; break; }
    }
}

static void bar(int player, box b)
{
    (void)player;
    bench_bar_moves(b, t_small, G.moves);
}

/* ---- the pictures for the shell ---------------------------------------------------------------------------------- */
static void paint_puzzle(const mcr_puzzle *p, int size, int side, float filled, int top, uint8_t *rgba)
{
    int big = p->w > p->h ? p->w : p->h, q = (int)(side / (big + 0.7f)), mg = (int)(0.35f * q);
    if (q < 3) return;
    int w = p->w * q + 2 * mg, h = p->h * q + 2 * mg, fr = (int)fmaxf(1, 0.12f * q);
    uint8_t *o = malloc((size_t)w * h * 4);
    if (!o) return;
    paint_board(o, w, h, fr, (float)mg, (float)mg, q, p, filled);
    art_blend(rgba, size, size, o, w, h, (size - w) / 2, top ? 0 : (size - h) / 2);
    free(o);
}

static void thumb(int variant, int size, uint8_t *rgba)
{
    mcr_puzzle p;
    if (variant < 0) {                          /* the menu's: a level of the fourth chapter, half done */
        if (mcr_decode(mcr_levels[80], &p)) paint_puzzle(&p, size, size, 0.5f, 0, rgba);
        return;
    }
    mcr_make m = mcr_variant_make(variant);
    mcr_make_puzzle(&m, 2026u + (uint32_t)variant, &p);
    paint_puzzle(&p, size, (int)(0.84f * size), 0, 1, rgba);
    bench_stones(rgba, size, (int)(0.84f * size), variant);
}

/* a chapter's tab: its sign on a tile (plus, minus, times, divided), a big board, then times and plus together */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    if (chapter == 4) {
        mcr_puzzle p;
        if (mcr_decode(mcr_levels[124], &p)) paint_puzzle(&p, size, (int)(0.96f * size), 0, 0, rgba);
        return;
    }
    int s = (int)(0.8f * size), o = (size - s) / 2;
    uint8_t *b = calloc((size_t)s * s, 4);
    if (!b) return;
    bench_tile(b, s, s, 0.16f * s, TONE_SIGN);
    if (chapter == 5) {
        paint_sign(b, s, s, 0.33f * s, 0.36f * s, 0.62f * s, MCR_MUL, SIGN_INK);
        paint_sign(b, s, s, 0.67f * s, 0.64f * s, 0.62f * s, MCR_ADD, SIGN_INK);
    } else {
        paint_sign(b, s, s, s / 2.0f, s / 2.0f, 1.1f * s, MCR_ADD + (chapter < 0 ? 0 : chapter % 4), SIGN_INK);
    }
    art_blend(rgba, size, size, b, s, s, o, o);
    free(b);
}

static const game mathcross_game = {
    .id = "mathcross", .order = 490, .variants = MCR_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .key = key,
    .levels = MCR_LEVELS, .chapters = MCR_CHAPTERS, .chapter_icon = chapter_icon,
    .start_random = start_random, .score = score, .level_stars = stars, .fill = 1, .tools = TOOLS,
};
REGISTER_GAME(mathcross_game)
