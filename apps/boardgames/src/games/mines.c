/*
 * Mines in RetroStone Board, for one player: a walnut tray of wooden tiles; under each a hollow in the plank, empty or
 * holding a number in coloured ink, or one of the house's ebony stones (a mine). Brass flags stand on the tiles the
 * player marks. Beside the board, the shell's buttons: the flag (taps then plant flags), the lamp (a hint) and, on the
 * larger fields, the magnifying glass (near or whole) (the rules: mines_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Tap a tile: it lifts away, and an empty hollow lifts its neighbours, spreading. Hold a finger on a tile (or tap it
 * with the flag button on) to plant or pull up a flag. Tap a number whose flags are all planted: its other neighbours
 * lift (a chord). Near (the glass): the cells grow and a finger drags the field round; thin cream bars along the frame
 * show where the view is. A stone uncovered glows red: the field waits for undo (or the lamp, then lit, takes it
 * back). The hint uncovers a safe cell, or plants a sure flag, or pulls up a wrong one, in gold light. The bar: the
 * actions, and at its right end the mines left (the mines less the flags) by a stone. Cleared: every stone shows
 * under a flag, and a wave of gold light runs over the field from the last tile lifted.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/mines.h"
#include "games/puzzles.h"
#include "games/pztools.h"
#include "shell.h"

#define FRAME 0.035f
#define HOLD_MS 420                             /* a finger held this long plants a flag */
#define LIFT_MS 170                             /* a tile lifting away */
#define SPREAD_MS 22                            /* the next ring of an empty area a moment later */
#define POP_MS 200
#define HINT_MS 1200
#define WAVE_STEP 32
#define WAVE_LEN 520

enum { B_FLAG, B_HINT, B_ZOOM, BTNS };

static const uint8_t INK[9][3] = {{0, 0, 0},    {40, 72, 164}, {32, 112, 62}, {178, 42, 32}, {34, 34, 112},
                                  {122, 34, 22}, {22, 112, 122}, {34, 28, 24}, {110, 100, 90}};

static mns_game G;
static int flag_mode, zoom, zoomable, fresh;
static Uint32 open_at[MNS_CELLS];               /* when each tile began lifting away */
static struct { int cell; Uint32 t0; } pop = {-1, 0}, hint = {-1, 0};
static struct { int on, from; Uint32 t0; } win;
static struct { int on, cell, moved, held, btn; float x0, y0, lx, ly; Uint32 t0; } ptr;
static int last_cell = -1;

static box last_box;
static pzt_column col;
static float ox, oy;                            /* the view's offset into the field (pixels), near */
static float vx, vy;                            /* the view's top left on the screen */
static int cs, fr, vcols, vrows, fit_cs, near_cs, made_cs = -1, made_frame = -1, made_small = -1;
static SDL_Texture *t_hidden[4], *t_open[4], *t_flag, *t_stone, *t_frame, *t_small, *t_mine_small;
static puz_font f_num;

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* a covered cell: a wooden tile, bevelled, standing on the plank */
static void paint_tile(int q, int cut, uint8_t *o)
{
    pzt_plank(o, q, q, cut);
    for (int i = 0; i < q * q; i++) pzt_tint(o + 4 * i, (const uint8_t[3]){236, 200, 146}, 1.0f);
    uint8_t *t = calloc((size_t)q * q, 4);
    if (!t) return;
    memcpy(t, o, (size_t)q * q * 4);
    puz_bevel(t, q, q, 0.1f * q, fmaxf(1.5f, 0.11f * q), 0.4f);
    for (int i = 0; i < q * q; i++) {           /* the gap round it: the hollow's dark */
        uint8_t *p = o + 4 * i;
        for (int k = 0; k < 3; k++) p[k] = (uint8_t)(p[k] * 0.45f);
    }
    art_blend(o, q, q, t, q, q, 0, 0);
    free(t);
}

/* an uncovered cell: the hollow, darker, in the shadow of its top and left edges */
static void paint_hollow(int q, int cut, uint8_t *o)
{
    pzt_plank(o, q, q, cut + 5);
    float sh = fmaxf(1.5f, 0.12f * q);
    for (int y = 0; y < q; y++)
        for (int x = 0; x < q; x++) {
            uint8_t *p = o + 4 * (y * q + x);
            float e = fminf((float)x, (float)y), k = 0.86f - 0.22f * clampf(1 - e / sh, 0, 1);
            if (x == q - 1 || y == q - 1) k *= 0.8f;   /* the seam to the next cell */
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)(p[i] * k);
        }
}

static void paint_btn(int kind, int size, uint8_t *o)
{
    if (kind == B_FLAG) pzt_pic_flag(o, size);
    else pzt_pic_glass(o, size, kind == B_ZOOM);
}

/* the token: a tile with a brass flag on it */
static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    int q = (int)(0.86f * size);
    uint8_t *t = calloc((size_t)q * q, 4);
    if (!t) return;
    paint_tile(q, 1, t);
    pzt_flag(t, q, q, 0.08f * q, 0.06f * q, 0.86f * q);
    art_blend(rgba, size, size, t, q, q, (size - q) / 2, (size - q) / 2);
    free(t);
}

static void stone_pic(int size, uint8_t *rgba)
{
    memset(rgba, 0, (size_t)size * size * 4);
    art_counter(COUNTER_STONE, 1, size, rgba);
}

/* ---- the geometry: whole (the field fitted) or near (larger cells, the view dragged round) ----------------------- */
static void clamp_view(void)
{
    float mx = fmaxf(0, (float)(G.w * cs - vcols * cs)), my = fmaxf(0, (float)(G.h * cs - vrows * cs));
    ox = clampf(ox, 0, mx);
    oy = clampf(oy, 0, my);
}

static void fit(void)
{
    box r = col.room;
    float S = fminf(r.w, r.h);
    fr = (int)(S * FRAME);
    fit_cs = (int)fminf((r.w - 2 * fr) / G.w, (r.h - 2 * fr) / G.h);
    zoomable = fit_cs < 0.62f * ui.unit;
    near_cs = (int)fmaxf((float)fit_cs, floorf(0.7f * ui.unit));
    if (!zoomable) zoom = 0;
    cs = zoom ? near_cs : fit_cs;
    vcols = (int)fminf((float)G.w, floorf((r.w - 2 * fr) / cs));
    vrows = (int)fminf((float)G.h, floorf((r.h - 2 * fr) / cs));
    vx = floorf(r.x + (r.w - vcols * cs) / 2);
    vy = floorf(r.y + (r.h - vrows * cs) / 2);
    clamp_view();
}

/* the view near, centred on a cell (or the field's middle) */
static void centre_on(int c)
{
    float cx = c >= 0 ? (c % G.w + 0.5f) * cs : G.w * cs / 2.0f, cy = c >= 0 ? (c / G.w + 0.5f) * cs : G.h * cs / 2.0f;
    ox = cx - vcols * cs / 2.0f;
    oy = cy - vrows * cs / 2.0f;
    clamp_view();
}

static void layout(box b)
{
    last_box = b;
    pzt_column_fit(&col, b, BTNS);
    fit();
    if (fresh) {                                /* a new field: near at first when too small to tap whole */
        fresh = 0;
        zoom = zoomable && fit_cs < 0.45f * ui.unit;
        fit();
        centre_on(-1);
    }
    if (made_cs != cs) {
        made_cs = cs;
        for (int i = 0; i < 4; i++) {
            ui_free(&t_hidden[i]);
            ui_free(&t_open[i]);
            paint_tile(cs, i, ui_buf(cs, cs));
            t_hidden[i] = ui_tex(cs, cs, ui_buf(cs, cs));
            paint_hollow(cs, i, ui_buf(cs, cs));
            t_open[i] = ui_tex(cs, cs, ui_buf(cs, cs));
        }
        ui_free(&t_flag);
        memset(ui_buf(cs, cs), 0, (size_t)cs * cs * 4);
        pzt_flag(ui_buf(cs, cs), cs, cs, 0.1f * cs, 0.06f * cs, 0.82f * cs);
        t_flag = ui_tex(cs, cs, ui_buf(cs, cs));
        int d = (int)(0.8f * cs);
        ui_free(&t_stone);
        stone_pic(d, ui_buf(d, d));
        t_stone = ui_tex(d, d, ui_buf(d, d));
        puz_font_make(&f_num, (int)fmaxf(6, 0.62f * cs), 1);
    }
    int key = (vcols * 64 + vrows) * 512 + cs;
    if (made_frame != key) {
        made_frame = key;
        int w = vcols * cs + 2 * fr, h = vrows * cs + 2 * fr;
        ui_free(&t_frame);
        art_board_n(vcols, vrows, cs, fr, 0, ui_buf(w, h));
        t_frame = ui_tex(w, h, ui_buf(w, h));
    }
    int small = (int)(0.6f * ui.unit);
    if (made_small != small) {
        made_small = small;
        ui_free(&t_small);
        ui_free(&t_mine_small);
        memset(ui_buf(small, small), 0, (size_t)small * small * 4);
        paint_tile(small, 2, ui_buf(small, small));
        t_small = ui_tex(small, small, ui_buf(small, small));
        stone_pic(small, ui_buf(small, small));
        t_mine_small = ui_tex(small, small, ui_buf(small, small));
    }
}

static void relayout(void)
{
    if (last_box.w > 0) layout(last_box);
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void calm(void)
{
    memset(open_at, 0, sizeof open_at);
    pop.cell = hint.cell = -1;
    win.on = 0;
    memset(&ptr, 0, sizeof ptr);
}

static void start(int variant)
{
    mns_new(&G, variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    calm();
    flag_mode = 0;
    last_cell = -1;
    zoom = 0;
    fresh = 1;
    relayout();
}

static int status(void) { return mns_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }

static Uint32 win_end(void) { return (Uint32)((G.w + G.h) * WAVE_STEP + WAVE_LEN); }

static int busy(void)
{
    Uint32 now = ui_ms();
    if (win.on && now - win.t0 < win_end()) return 1;
    for (int c = 0; c < G.w * G.h; c++)
        if (open_at[c] && now < open_at[c] + LIFT_MS) return 1;
    return 0;
}

static int dist(int a, int b)
{
    int dx = abs(a % G.w - b % G.w), dy = abs(a / G.w - b / G.w);
    return dx > dy ? dx : dy;
}

/* an action done: the tiles it uncovered lift away from where it began, the sounds, the win */
static void acted(const uint8_t *before, int origin, int kind)
{
    Uint32 now = ui_ms();
    int lifted = 0;
    for (int c = 0; c < G.w * G.h; c++)
        if (before[c] != MNS_OPEN && G.st[c] == MNS_OPEN) {
            open_at[c] = now + (Uint32)(dist(c, origin) * SPREAD_MS);
            lifted++;
        }
    last_cell = origin;
    if (G.blast >= 0) ui_sound(SND_CAPTURE);
    else if (kind == MNS_MARK) {
        pop.cell = origin;
        pop.t0 = now;
        ui_sound(SND_TAP);
    } else ui_sound(lifted > 3 ? SND_MOVE : SND_PLACE);
    if (mns_solved(&G)) {
        int far = 0;
        for (int c = 0; c < G.w * G.h; c++)
            if (open_at[c] && dist(c, origin) > far && before[c] != MNS_OPEN) far = dist(c, origin);
        win.on = 1;
        win.from = origin;
        win.t0 = now + (Uint32)(far * SPREAD_MS + LIFT_MS);
    }
    ui_animate();
}

static void act(int c, int kind)
{
    uint8_t before[MNS_CELLS];
    memcpy(before, G.st, sizeof before);
    int ok = kind == MNS_MARK ? mns_flag(&G, c) : mns_dig(&G, c);
    if (!ok) { ui_sound(SND_DENY); return; }
    acted(before, c, kind);
}

static void use_hint(void)
{
    int kind, c = mns_hint(&G, &kind);
    if (c < 0) { ui_sound(SND_DENY); return; }
    if (kind == MNS_UNDO) {
        mns_undo(&G);
        calm();
        ui_sound(SND_TAP);
        ui_animate();
        return;
    }
    uint8_t before[MNS_CELLS];
    memcpy(before, G.st, sizeof before);
    if (mns_play_hint(&G) < 0) { ui_sound(SND_DENY); return; }
    acted(before, c, kind);
    hint.cell = c;
    hint.t0 = ui_ms();
    if (zoom && zoomable) {                     /* the hint's cell brought into view */
        float cx = (c % G.w + 0.5f) * cs - ox, cy = (c / G.w + 0.5f) * cs - oy;
        if (cx < cs || cy < cs || cx > (vcols - 1) * cs || cy > (vrows - 1) * cs) centre_on(c);
    }
}

static int undo(void)
{
    if (!mns_undo(&G)) return 0;
    calm();
    return 1;
}

static int script(const char *moves)
{
    calm();
    while (*moves == ' ') moves++;
    if (*moves == 'z') {                        /* "z": the view whole (z0) or near (z1) */
        zoom = moves[1] == '1';
        moves += 2;
        relayout();
        centre_on(-1);
    }
    int ok = mns_script(&G, moves);
    if (G.na) last_cell = G.act[G.na - 1] & 0xfff;
    if (mns_solved(&G)) {
        win.on = 1;
        win.from = last_cell;
        win.t0 = ui_ms();
    }
    if (zoom && zoomable && last_cell >= 0) centre_on(last_cell);
    return ok;
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static void cell_xy(int c, float *x, float *y)  /* its top left on the screen */
{
    *x = vx + (c % G.w) * cs - ox;
    *y = vy + (c / G.w) * cs - oy;
}

static int cell_at(float x, float y)
{
    if (x < vx || y < vy || x >= vx + vcols * cs || y >= vy + vrows * cs) return -1;
    int cx = (int)((x - vx + ox) / cs), cy = (int)((y - vy + oy) / cs);
    if (cx < 0 || cy < 0 || cx >= G.w || cy >= G.h) return -1;
    return cx + cy * G.w;
}

static void draw_cell(int c, float x, float y, Uint32 now, int solved)
{
    int v = (c % G.w * 7 + c / G.w * 3) % 4, st = G.st[c];
    if (st == MNS_OPEN) {
        ui_put(t_open[v], x, y, (float)cs, (float)cs);
        if (G.mine[c]) {
            ui_mark_glow(x + cs / 2.0f, y + cs / 2.0f, 1.6f * cs);
            ui_put_c(t_stone, x + cs / 2.0f, y + cs / 2.0f, 0.8f * cs, 0.8f * cs);
        } else if (G.adj[c]) {
            puz_number(&f_num, x + cs / 2.0f, y + cs * 0.52f, 0.62f * cs, G.adj[c], INK[G.adj[c]], 255);
        }
        if (open_at[c] && now < open_at[c] + LIFT_MS) {   /* the tile lifting away: larger, fading */
            float t = now < open_at[c] ? 0 : (now - open_at[c]) / (float)LIFT_MS, s = 1 + 0.25f * puz_ease(t);
            SDL_SetTextureAlphaMod(t_hidden[v], (Uint8)(255 * (1 - puz_ease(t))));
            ui_put_c(t_hidden[v], x + cs / 2.0f - 0.08f * cs * t, y + cs / 2.0f - 0.12f * cs * t, cs * s, cs * s);
            SDL_SetTextureAlphaMod(t_hidden[v], 255);
            ui_animate();
        }
        return;
    }
    ui_put(t_hidden[v], x, y, (float)cs, (float)cs);
    if (solved && G.mine[c] && st != MNS_FLAG)  /* cleared: the stones show, under their flags */
        ui_put_c(t_stone, x + cs / 2.0f, y + cs / 2.0f, 0.62f * cs, 0.62f * cs);
    if (st == MNS_FLAG || (solved && G.mine[c])) {
        float s = 1;
        if (c == pop.cell && now - pop.t0 < POP_MS) {
            s = 0.6f + 0.4f * puz_ease((now - pop.t0) / (float)POP_MS);
            ui_animate();
        }
        if (solved && G.mine[c] && st != MNS_FLAG) s = 0.8f;
        ui_put(t_flag, x + cs * (1 - s) / 2, y + cs * (1 - s), cs * s, cs * s);
    }
}

static void draw(void)
{
    if (made_cs < 0) layout(last_box);
    Uint32 now = ui_ms();
    int solved = mns_solved(&G);
    ui_put(t_frame, vx - fr, vy - fr, (float)(vcols * cs + 2 * fr), (float)(vrows * cs + 2 * fr));
    SDL_Rect clip = {(int)vx, (int)vy, vcols * cs, vrows * cs};
    SDL_RenderSetClipRect(ui.r, &clip);
    int x0 = (int)(ox / cs), y0 = (int)(oy / cs);
    int x1 = (int)ceilf((ox + vcols * cs) / cs), y1 = (int)ceilf((oy + vrows * cs) / cs);
    if (x1 > G.w) x1 = G.w;
    if (y1 > G.h) y1 = G.h;
    float x, y;
    for (int cy = y0; cy < y1; cy++)
        for (int cx = x0; cx < x1; cx++) {
            int c = cx + cy * G.w;
            cell_xy(c, &x, &y);
            draw_cell(c, x, y, now, solved);
        }
    if (ptr.on && !ptr.moved && !ptr.held && ptr.cell >= 0 && !ptr.btn) {   /* a finger held: a ring grows */
        cell_xy(ptr.cell, &x, &y);
        float t = (now - ptr.t0) / (float)HOLD_MS;
        if (t > 0.25f) ui_mark_select_round(x + cs / 2.0f, y + cs / 2.0f, cs * (0.6f + 0.9f * clampf(t, 0, 1)));
        ui_animate();
    }
    if (hint.cell >= 0) {
        float p = puz_pulse((float)(now - hint.t0), 0, HINT_MS);
        if (p > 0) {
            cell_xy(hint.cell, &x, &y);
            ui_mark_gold(x + cs / 2.0f, y + cs / 2.0f, 3.0f * cs * p);
            ui_animate();
        } else {
            hint.cell = -1;
        }
    }
    if (win.on) {
        float wt = (float)now - (float)win.t0;
        if (wt >= 0)
            for (int cy = y0; cy < y1; cy++)
                for (int cx = x0; cx < x1; cx++) {
                    int c = cx + cy * G.w;
                    float p = puz_pulse(wt, dist(c, win.from) * WAVE_STEP, WAVE_LEN);
                    if (p <= 0) continue;
                    cell_xy(c, &x, &y);
                    ui_mark_gold(x + cs / 2.0f, y + cs / 2.0f, 2.2f * cs * p);
                }
        if (wt < (float)win_end()) ui_animate();
        else win.on = 0;
    }
    SDL_RenderSetClipRect(ui.r, NULL);
    if (zoom && zoomable) {                     /* where the view is: thin bars along the frame */
        float fw = (float)(G.w * cs), fh = (float)(G.h * cs), vw = (float)(vcols * cs), vh = (float)(vrows * cs);
        float t = fmaxf(2, 0.18f * fr);
        if (fw > vw) ui_fill(vx + vw * ox / fw, vy + vh + fr * 0.5f - t / 2, vw * vw / fw, t, 246, 238, 220, 190);
        if (fh > vh) ui_fill(vx + vw + fr * 0.5f - t / 2, vy + vh * oy / fh, t, vh * vh / fh, 246, 238, 220, 190);
    }
    ui_button_paint(col.x, col.y[B_FLAG], col.d, paint_btn, B_FLAG, 1, flag_mode ? UI_ON : 0);
    ui_button(col.x, col.y[B_HINT], col.d, ICON_HINT, G.blast >= 0 ? UI_LIT : solved ? UI_DIM : 0);
    if (zoomable) ui_button_paint(col.x, col.y[B_ZOOM], col.d, paint_btn, zoom ? B_ZOOM + 1 : B_ZOOM, 1, 0);
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    memset(&ptr, 0, sizeof ptr);
    int b = pzt_column_hit(&col, x, y);
    if (b >= 0 && (b < B_ZOOM || zoomable)) {
        ptr.btn = 1;
        if (b == B_ZOOM) {
            int keep = last_cell;
            float cx = ox + vcols * cs / 2.0f, cy = oy + vrows * cs / 2.0f;   /* the middle of the view kept */
            int mid = (int)(cx / cs) + (int)(cy / cs) * G.w;
            zoom = !zoom;
            relayout();
            centre_on(zoom ? (keep >= 0 ? keep : -1) : mid);
            ui_sound(SND_TAP);
        } else if (mns_solved(&G)) {
            ui_sound(SND_DENY);
        } else if (b == B_FLAG) {
            flag_mode = !flag_mode;
            ui_sound(SND_TAP);
        } else {
            use_hint();
        }
        ui_animate();
        return;
    }
    if (mns_solved(&G)) return;
    ptr.on = 1;
    ptr.cell = cell_at(x, y);
    ptr.x0 = ptr.lx = x;
    ptr.y0 = ptr.ly = y;
    ptr.t0 = ui_ms();
    ui_animate();
}

static void motion(float x, float y)
{
    if (!ptr.on) return;
    if (!ptr.moved && hypotf(x - ptr.x0, y - ptr.y0) > 0.22f * ui.unit) ptr.moved = 1;
    if (ptr.moved && zoom && zoomable) {        /* the field dragged round */
        ox -= x - ptr.lx;
        oy -= y - ptr.ly;
        clamp_view();
    }
    ptr.lx = x;
    ptr.ly = y;
    ui_animate();
}

/* the finger held: the other action (a flag, or with the flag button on, an uncovering) */
static void held(void)
{
    ptr.held = 1;
    int c = ptr.cell;
    if (c < 0 || G.blast >= 0) { ui_sound(SND_DENY); return; }
    if (G.st[c] == MNS_OPEN) { act(c, MNS_DIG); return; }   /* a number: its chord */
    act(c, flag_mode ? MNS_DIG : MNS_MARK);
}

static void release(float x, float y)
{
    if (!ptr.on) return;
    ptr.on = 0;
    ui_animate();
    if (ptr.moved || ptr.held) return;
    int c = cell_at(x, y);
    if (c < 0 || c != ptr.cell) return;
    if (G.blast >= 0) { ui_sound(SND_DENY); return; }
    if (G.st[c] == MNS_OPEN) { act(c, MNS_DIG); return; }
    act(c, flag_mode || G.st[c] == MNS_FLAG ? MNS_MARK : MNS_DIG);
}

/* the long press is seen while drawing (the finger still down) */
static void watch_hold(void)
{
    if (ptr.on && !ptr.moved && !ptr.held && !ptr.btn && ui_ms() - ptr.t0 >= HOLD_MS) held();
}

static void draw_all(void)
{
    watch_hold();
    draw();
}

/* ---- the bar: the actions; the mines left at the right ----------------------------------------------------------- */
static void bar(int player, box b)
{
    (void)player;
    int left = G.mines - G.flags;
    pzt_bar_two(b, t_small, G.moves, t_mine_small, left < 0 ? 0 : left, mns_solved(&G));
}

/* ---- the menu's and the picker's cards: a field of the size begun (a die on the classic ones) ------------------ */
static void thumb(int variant, int size, uint8_t *rgba)
{
    mns_game T;
    int v = variant < 0 ? 0 : variant;
    mns_new(&T, v, 3);
    mns_dig(&T, T.w / 2 + T.h / 2 * T.w);
    for (int i = 0; i < (variant < 0 ? 14 : 4); i++) mns_play_hint(&T);
    int side = variant < 0 ? size : (int)(0.9f * size);
    int f = (int)(side * FRAME), q = (int)fminf((side - 2.0f * f) / T.w, (side - 2.0f * f) / T.h);
    int w = T.w * q + 2 * f, h = T.h * q + 2 * f;
    uint8_t *b = calloc((size_t)w * h, 4), *tile = calloc((size_t)q * q, 4), *hol = calloc((size_t)q * q, 4);
    int d = (int)(0.8f * q);
    uint8_t *st = calloc((size_t)(d > 0 ? d : 1) * (d > 0 ? d : 1), 4);
    if (b && tile && hol && st) {
        art_board_n(T.w, T.h, q, f, 0, b);
        paint_tile(q, 0, tile);
        paint_hollow(q, 0, hol);
        if (d > 2) art_counter(COUNTER_STONE, 1, d, st);
        for (int c = 0; c < T.w * T.h; c++) {
            int x = f + c % T.w * q, y = f + c / T.w * q;
            art_blend(b, w, h, T.st[c] == MNS_OPEN ? hol : tile, q, q, x, y);
            if (T.st[c] == MNS_OPEN && T.adj[c]) {
                if (q >= 10) puz_digits(b, w, h, T.adj[c], x + q / 2.0f, y + q * 0.53f, 0.62f * q, INK[T.adj[c]], 1, 1);
                else puz_ink_rect(b, w, h, x + 0.3f * q, y + 0.3f * q, x + 0.7f * q, y + 0.7f * q, INK[T.adj[c]], 0.8f);
            }
            if (T.st[c] == MNS_FLAG) pzt_flag(b, w, h, x + 0.1f * q, y + 0.06f * q, 0.82f * q);
        }
        if (variant < 0 && d > 2)               /* the menu's: one stone uncovered */
            for (int c = 0; c < T.w * T.h; c++)
                if (T.mine[c] && T.st[c] == MNS_HIDDEN) {
                    int x = f + c % T.w * q, y = f + c / T.w * q;
                    art_blend(b, w, h, hol, q, q, x, y);
                    art_blend(b, w, h, st, d, d, x + (q - d) / 2, y + (q - d) / 2);
                    break;
                }
    }
    if (b) art_blend(rgba, size, size, b, w, h, (size - w) / 2, (size - h) / 2);
    free(b);
    free(tile);
    free(hol);
    free(st);
    if (variant >= MNS_SIZES) {                 /* classic: a die, the fields of pure chance */
        int dd = (int)(0.28f * size);
        uint8_t *die = calloc((size_t)dd * dd, 4);
        if (die) {
            art_die(5, 0, dd, die);
            art_blend(rgba, size, size, die, dd, dd, size - dd - (int)(0.02f * size), size - dd - (int)(0.02f * size));
            free(die);
        }
    }
}

static int score(void) { return G.moves; }   /* the end card's number */

static const game mines_game = {
    .id = "mines", .order = 440, .variants = MNS_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw_all,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .score = score,
};
REGISTER_GAME(mines_game)
